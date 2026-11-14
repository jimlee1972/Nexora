#pragma once

#include "GenerationServices.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/TwoD/GameplayGeneration.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace nexora::player::detail {
struct PlayerContext final {
  runtime::World &world;
  twod::GameplayBridge &bridge;
  runtime::Id scene;
  runtime::GameplayServices &services;
  NexoraInputSnapshot input{};
  std::size_t &allocated;
  std::size_t &freed;
  std::string_view scene_name;
  twod::SceneSession *scenes{};
  const NexoraTwoDApiV1 &twod_api;
  GenerationAdmission &admission;
  std::int32_t GetTwoD(NexoraTwoDApiV1 *output) const noexcept {
    if (!output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (output->struct_size < NEXORA_2D_API_V1_BASE_SIZE)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    auto negotiated = twod_api;
    const auto bytes =
        output->struct_size >= sizeof(negotiated) ? sizeof(negotiated) : NEXORA_2D_API_V1_BASE_SIZE;
    negotiated.struct_size = static_cast<std::uint32_t>(bytes);
    std::memcpy(output, &negotiated, bytes);
    return NEXORA_GAMEPLAY_OK;
  }
};
// Private application factory shared by the actual Player and its embedding acceptance test.
NexoraGameplayHostV3 CreatePlayerHost(PlayerContext &);
// Stable owner lives through GameplayModuleHost::Unload and every dynamic-library finalizer.
// Each generation has a distinct Context, World/bridge/scene API and external service gates.
class GenerationHost final {
public:
  using HostFactory = NexoraGameplayHostV3 (*)(PlayerContext &);
  GenerationHost(std::unique_ptr<twod::GameplayGeneration> values,
                 runtime::GameplayServices &services, std::string scene, HostFactory factory)
      : services_(services), scene_(std::move(scene)), factory_(factory) {
    if (!values || !factory_)
      throw std::invalid_argument("Player generation needs owning values and host callbacks");
    active_ = std::make_unique<Generation>(*this, std::move(values), GenerationPhase::Active);
  }
  GenerationHost(const GenerationHost &) = delete;
  GenerationHost &operator=(const GenerationHost &) = delete;
  [[nodiscard]] NexoraGameplayHostV3 Host() {
    if (exposed_)
      throw std::logic_error("Player host tables cannot be rebound after initial admission");
    if (!active_->Bind(services_))
      throw std::logic_error("Player generation API preparation failed");
    exposed_ = true;
    return factory_(active_->context);
  }
  [[nodiscard]] twod::GameplayGeneration &Values() noexcept { return *active_->values; }
  [[nodiscard]] PlayerContext &Context() noexcept { return active_->context; }
  [[nodiscard]] const GenerationServiceStats &ServiceStats() const noexcept { return stats_; }
  [[nodiscard]] std::filesystem::path LoadedLibrary() const {
    return image_ ? image_->file : std::filesystem::path{};
  }
  bool Load(runtime::GameplayModuleHost &module, const std::filesystem::path &source) {
    if (!exposed_ || module.IsLoaded())
      return false;
    auto image = LibraryImage::Create(source);
    if (!image || !module.Load(image->file))
      return false;
    image_ = std::move(image);
    return true;
  }
  bool Reload(runtime::GameplayModuleHost &module, NexoraGameModuleLoadV3Fn source) {
    return Capture([&] { return module.Reload(source); });
  }
  bool Reload(runtime::GameplayModuleHost &module, const std::filesystem::path &source) {
    return Capture([&] {
      auto image = LibraryImage::Create(source);
      if (!image || !module.Reload(image->file))
        return false;
      // Runtime has finished old callbacks and closed the old image before returning. Keep the
      // new copy through final unload, then delete only the superseded host-owned directory.
      image_.swap(image);
      return true;
    });
  }
  void Unload(runtime::GameplayModuleHost &module) noexcept {
    module.Unload();
    image_.reset();
  }
  [[nodiscard]] runtime::GameplayModuleHost::ReloadTransaction Transaction() noexcept {
    return {this,
            [](void *p, NexoraGameplayHostV3 &host) noexcept {
              auto &owner = *static_cast<GenerationHost *>(p);
              if (!owner.exposed_ || !owner.reloading_ || owner.candidate_ || owner.retired_)
                return false;
              try {
                auto values = owner.active_->values->Stage();
                if (!values)
                  return false;
                owner.candidate_ = std::make_unique<Generation>(owner, std::move(values),
                                                                GenerationPhase::Candidate);
                owner.candidate_->context.input = owner.active_->context.input;
                if (!owner.candidate_->Bind(owner.services_))
                  return false;
                host = owner.factory_(owner.candidate_->context);
                host.capabilities |= NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE;
                return true;
              } catch (...) {
                return false;
              }
            },
            [](void *p) noexcept { static_cast<GenerationHost *>(p)->candidate_.reset(); },
            [](void *p) noexcept {
              auto &owner = *static_cast<GenerationHost *>(p);
              owner.active_->admission.phase = GenerationPhase::Retired;
              owner.retired_ = std::move(owner.active_);
            },
            [](void *p) noexcept {
              auto &owner = *static_cast<GenerationHost *>(p);
              owner.active_ = std::move(owner.candidate_);
              owner.active_->admission.phase = GenerationPhase::Active;
              owner.retired_.reset();
            }};
  }

private:
  struct LibraryImage final {
    std::filesystem::path root, file;
    ~LibraryImage() noexcept {
      if (root.empty())
        return;
      try {
        std::error_code error;
        std::filesystem::permissions(file, std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add, error);
        error.clear();
        std::filesystem::remove_all(root, error);
      } catch (...) {
      }
    }
    static std::unique_ptr<LibraryImage> Create(const std::filesystem::path &source) noexcept {
      try {
        if (!std::filesystem::is_regular_file(source))
          return {};
        const auto bytes = std::filesystem::file_size(source);
        const auto written = std::filesystem::last_write_time(source);
        auto result = std::make_unique<LibraryImage>();
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
          static std::atomic<std::uint64_t> next{1};
          const auto token =
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
              std::to_string(next.fetch_add(1));
          auto directory =
              std::filesystem::temp_directory_path() / ("nexora-player-image-" + token);
          // Never adopt or clean a preexisting directory, including another process's copy.
          if (!std::filesystem::create_directory(directory))
            continue;
          result->root = std::move(directory);
          std::filesystem::permissions(result->root, std::filesystem::perms::owner_all,
                                       std::filesystem::perm_options::replace);
          result->file = result->root / ("gameplay-" + token);
          result->file += source.extension();
          if (!std::filesystem::copy_file(source, result->file) ||
              std::filesystem::file_size(source) != bytes ||
              std::filesystem::last_write_time(source) != written ||
              std::filesystem::file_size(result->file) != bytes)
            return {};
          return result;
        }
      } catch (...) {
      }
      return {};
    }
  };
  template <class Function> bool Capture(Function &&function) {
    if (!exposed_ || reloading_)
      return false;
    reloading_ = true;
    active_->admission.phase = GenerationPhase::Capture;
    struct Finish final {
      GenerationHost &owner;
      ~Finish() {
        owner.active_->admission.phase = GenerationPhase::Active;
        owner.reloading_ = false;
      }
    } finish{*this};
    return function();
  }
  struct Generation final {
    std::unique_ptr<twod::GameplayGeneration> values;
    GenerationAdmission admission;
    GenerationServices services;
    ServiceGate<NexoraTwoDApiV1> twod_gate;
    ServiceGate<NexoraTwoDSourceApiV1> source_gate;
    NexoraTwoDApiV1 twod_api{};
    NexoraTwoDSourceApiV1 sources{};
    PlayerContext context;
    Generation(GenerationHost &owner, std::unique_ptr<twod::GameplayGeneration> state,
               GenerationPhase phase)
        : values(std::move(state)), admission{phase, owner.stats_}, services(admission),
          twod_gate(admission, true, this), source_gate(admission, true),
          context{values->World(),
                  values->Bridge(),
                  values->Sprites() ? values->Sprites()->SceneId() : 0,
                  services.Api(),
                  {},
                  owner.allocated_,
                  owner.freed_,
                  owner.scene_,
                  values->Scenes(),
                  twod_api,
                  admission} {}
    bool Bind(const runtime::GameplayServices &source) noexcept {
      services.Bind(source,
                    values->Scenes() ? values->Scenes()->Api() : NexoraGameplaySceneApiV1{});
      NexoraTwoDApiV1 raw{};
      raw.struct_size = sizeof(raw);
      NexoraTwoDSourceApiV1 source_api{};
      source_api.struct_size = sizeof(source_api);
      if (values->Bridge().GetApi(&raw) != 0 || values->Bridge().GetSourceApi(&source_api) != 0)
        return false;
      twod_api = GuardService(twod_gate, raw);
      sources = GuardService(source_gate, source_api);
      twod_api.get_source_api = [](void *p, NexoraTwoDSourceApiV1 *output) -> std::int32_t {
        if (!p || !output)
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        auto *owner =
            static_cast<Generation *>(static_cast<ServiceGate<NexoraTwoDApiV1> *>(p)->Owner());
        if (std::this_thread::get_id() != owner->admission.thread)
          return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
        if (output->struct_size < sizeof(owner->sources))
          return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
        *output = owner->sources;
        return NEXORA_GAMEPLAY_OK;
      };
      return true;
    }
  };
  runtime::GameplayServices &services_;
  std::string scene_;
  HostFactory factory_;
  std::size_t allocated_{}, freed_{};
  GenerationServiceStats stats_;
  std::unique_ptr<Generation> active_, candidate_, retired_;
  std::unique_ptr<LibraryImage> image_;
  bool exposed_{};
  bool reloading_{};
};
} // namespace nexora::player::detail
