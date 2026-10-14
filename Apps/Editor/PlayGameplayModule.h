#pragma once

#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "PlayInputState.h"
#include "PlaySceneServices.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>

namespace nexora::editor::preview {

// Owner-thread embedding of a component-oriented V3 gameplay library in the actual Play clone.
// Unload must precede PlaySession::Stop: destroy callbacks may still read/write that World.
class PlayGameplayModule final {
public:
  using LogSink = std::function<void(std::uint32_t, std::string)>;
  using LogRejectionSink = std::function<void()>;
  static constexpr std::size_t kMaximumLogMessageBytes = 4096;
  explicit PlayGameplayModule(LogSink log = {}, LogRejectionSink rejected_log = {})
      : log_(std::move(log)), rejected_log_(std::move(rejected_log)), module_(Host()) {}
  ~PlayGameplayModule() { Unload(); }
  PlayGameplayModule(const PlayGameplayModule &) = delete;
  PlayGameplayModule &operator=(const PlayGameplayModule &) = delete;

  bool LoadRelative(runtime::World &world, const std::filesystem::path &root,
                    std::string_view relative, std::string &error,
                    std::shared_ptr<const MeshAssetCatalog> assets = {}) try {
    Unload();
    if (assets && !assets->Generation()) {
      error = "Play mesh snapshot is unavailable.";
      return false;
    }
    const std::filesystem::path name{std::u8string(relative.begin(), relative.end())};
    if (name.empty() || name.is_absolute() || name.has_root_name()) {
      error = "Choose a gameplay library relative to the project root.";
      return false;
    }
    std::error_code ec;
    const auto directory = std::filesystem::canonical(root, ec);
    if (ec) {
      error = "Project root is unavailable.";
      return false;
    }
    const auto library = std::filesystem::canonical(directory / name, ec);
    const auto local = library.lexically_relative(directory);
    if (ec || local.empty() || local.is_absolute() || *local.begin() == ".." ||
        !std::filesystem::is_regular_file(library, ec) || ec) {
      error = "Gameplay library must be a file inside this project.";
      return false;
    }
    if (!scenes_.Bind(world)) {
      error = "Gameplay requires an isolated Play World.";
      return false;
    }
    world_ = &world;
    mesh_assets_ = std::move(assets);
    if (!module_.Load(library)) {
      Unload();
      error = "Gameplay library could not start (path, ABI, or lifecycle failure).";
      return false;
    }
    return true;
  } catch (const std::exception &) {
    Unload();
    error = "Gameplay library path or load failed.";
    return false;
  }
  // Static loader overload is useful for embedding tests; it has the same owning lifetime.
  bool Load(runtime::World &world, NexoraGameModuleLoadV3Fn loader,
            std::shared_ptr<const MeshAssetCatalog> assets = {}) {
    Unload();
    if (assets && !assets->Generation())
      return false;
    if (!scenes_.Bind(world))
      return false;
    world_ = &world;
    mesh_assets_ = std::move(assets);
    if (module_.Load(loader))
      return true;
    Unload();
    return false;
  }
  // One keyboard/mouse source has exactly one logical owner. Switching revokes held state;
  // the next focused batch is discarded by the existing acquisition handshake.
  bool SetInputUser(std::uint32_t user) noexcept {
    if (user >= PlayInputBindings::kMaximumUsers)
      return false;
    if (input_user_ != user) {
      input_.SetFocused(false);
      input_user_ = user;
    }
    return true;
  }
  [[nodiscard]] std::uint32_t InputUser() const noexcept { return input_user_; }
  void SetInputFocus(bool focused) noexcept { input_.SetFocused(focused); }
  bool SetInputBindings(const PlayInputBindings &bindings) noexcept {
    return input_.SetBindings(bindings);
  }
  void ProcessInput(std::span<const Nexora::Window::WindowEvent> events) noexcept {
    input_.Process(events);
  }
  [[nodiscard]] bool IsLoaded() const noexcept { return module_.IsLoaded(); }
  bool FixedUpdate(double seconds) {
    return !IsLoaded() || !module_.SupportsFixedUpdate() || module_.FixedUpdate(seconds);
  }
  bool Update(double seconds) {
    if (!IsLoaded())
      return true;
    if (!std::isfinite(seconds) || seconds < 0)
      return false;
    Increment(diagnostics_frame_);
    return module_.Update(seconds);
  }
  void Unload() noexcept {
    input_.SetFocused(false);
    input_user_ = 0;
    module_.Unload();
    scenes_.Clear();
    world_ = nullptr;
    mesh_assets_.reset();
    for (const auto &[pointer, allocation] : allocations_)
      ::operator delete(pointer, std::align_val_t(allocation.alignment));
    allocations_.clear();
    bytes_ = 0;
    diagnostics_frame_ = diagnostics_errors_ = 0;
  }

private:
  friend struct PlayGameplayDiagnosticsTestAccess;
  static void Increment(std::uint64_t &value) noexcept {
    if (value != std::numeric_limits<std::uint64_t>::max())
      ++value;
  }
  int32_t RecordResult(int32_t result) noexcept {
    if (result != NEXORA_GAMEPLAY_OK)
      Increment(diagnostics_errors_);
    return result;
  }
  struct Allocation final {
    std::uint64_t owner;
    std::size_t size, alignment;
  };
  static PlayGameplayModule &Self(void *context) {
    return *static_cast<PlayGameplayModule *>(context);
  }
  NexoraGameplayHostV3 Host() {
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.capabilities =
        NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR | NEXORA_GAMEPLAY_CAPABILITY_SCENE_API;
    host.context = this;
    host.log = [](void *context, std::uint32_t level, const char *message, std::uint32_t length) {
      try {
        auto &self = Self(context);
        // Reject before reading oversized/invalid pointer-length data; never truncate UTF-8.
        if (level > static_cast<std::uint32_t>(core::LogLevel::Fatal) ||
            length > kMaximumLogMessageBytes || (!message && length)) {
          if (self.rejected_log_)
            self.rejected_log_();
          return;
        }
        const std::string_view text =
            length ? std::string_view(message, length) : std::string_view{};
        if (text.find('\0') != std::string_view::npos || !foundation::IsValidUtf8(text)) {
          if (self.rejected_log_)
            self.rejected_log_();
          return;
        }
        if (self.log_)
          self.log_(level, std::string(text));
      } catch (...) {
      }
    };
    host.read_component = [](void *context, std::uint64_t entity, std::uint64_t type, void *data,
                             std::uint32_t size) -> int32_t {
      try {
        const auto *world = Self(context).world_;
        return Self(context).RecordResult(
            world ? game::ReadGameplayComponent(*world, entity, type, data, size)
                  : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      } catch (...) {
        return Self(context).RecordResult(NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
      }
    };
    host.write_component = [](void *context, std::uint64_t entity, std::uint64_t type,
                              const void *data, std::uint32_t size) -> int32_t {
      try {
        auto *world = Self(context).world_;
        return Self(context).RecordResult(
            world ? game::WriteGameplayComponent(*world, entity, type, data, size)
                  : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      } catch (...) {
        return Self(context).RecordResult(NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
      }
    };
    host.allocate = [](void *context, std::uint64_t owner, std::uint64_t size,
                       std::uint64_t alignment) -> void * {
      auto &self = Self(context);
      if (!size || size > 16 * 1024 * 1024 || !std::has_single_bit(alignment) ||
          alignment < alignof(void *) || alignment > 4096 || size > 64 * 1024 * 1024 - self.bytes_)
        return nullptr;
      auto *pointer =
          ::operator new(static_cast<std::size_t>(size), std::align_val_t(alignment), std::nothrow);
      if (!pointer)
        return nullptr;
      try {
        self.allocations_.emplace(pointer, Allocation{owner, static_cast<std::size_t>(size),
                                                      static_cast<std::size_t>(alignment)});
        self.bytes_ += size;
        return pointer;
      } catch (...) {
        ::operator delete(pointer, std::align_val_t(alignment));
        return nullptr;
      }
    };
    host.deallocate = [](void *context, std::uint64_t owner, void *pointer, std::uint64_t size,
                         std::uint64_t alignment) {
      auto &self = Self(context);
      const auto found = self.allocations_.find(pointer);
      if (found == self.allocations_.end() || found->second.owner != owner ||
          found->second.size != size || found->second.alignment != alignment)
        return;
      self.bytes_ -= found->second.size;
      ::operator delete(pointer, std::align_val_t(found->second.alignment));
      self.allocations_.erase(found);
    };
    host.capture_input = [](void *context, std::uint32_t user,
                            NexoraInputSnapshot *snapshot) -> int32_t {
      if (!snapshot || user >= PlayInputBindings::kMaximumUsers)
        return Self(context).RecordResult(NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      const auto &self = Self(context);
      auto copied = self.input_.Snapshot();
      if (user != self.input_user_) {
        copied.move_x = copied.move_y = 0;
        copied.buttons = 0;
      }
      *snapshot = copied;
      return NEXORA_GAMEPLAY_OK;
    };
    host.resolve_asset = [](void *context, std::uint64_t high, std::uint64_t low,
                            NexoraAssetHandle *output) -> int32_t {
      auto &self = Self(context);
      if (!output || (!high && !low))
        return self.RecordResult(NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      if (!self.world_)
        return self.RecordResult(NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
      if (!self.mesh_assets_)
        return self.RecordResult(NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
      const auto asset =
          self.mesh_assets_->ResolveAsset({high, low}, self.mesh_assets_->Generation());
      if (!asset)
        return self.RecordResult(NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      *output = {asset->resource};
      return self.RecordResult(NEXORA_GAMEPLAY_OK);
    };
    host.load_scene = [](void *context, const char *name, std::uint32_t length,
                         std::uint32_t persistent, std::uint64_t *scene) -> int32_t {
      return Self(context).RecordResult(
          Self(context).scenes_.Load(name, length, persistent, scene));
    };
    host.activate_scene = [](void *context, std::uint64_t scene) -> int32_t {
      return Self(context).RecordResult(Self(context).scenes_.Activate(scene));
    };
    host.spawn_entity = [](void *context, std::uint64_t scene,
                           const NexoraEntitySpawnDescriptor *descriptor,
                           std::uint64_t *entity) -> int32_t {
      return Self(context).RecordResult(Self(context).scenes_.Spawn(scene, descriptor, entity));
    };
    host.despawn_entity = [](void *context, std::uint64_t entity) -> int32_t {
      return Self(context).RecordResult(Self(context).scenes_.Despawn(entity));
    };
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
    host.raycast = [](void *context, const NexoraRaycastRequest *request,
                      NexoraRaycastHit *hit) -> int32_t {
      return Self(context).RecordResult(Self(context).scenes_.Raycast(request, hit));
    };
#endif
    host.get_diagnostics = [](void *context, NexoraFrameDiagnostics *output) -> int32_t {
      auto &self = Self(context);
      if (!output)
        return self.RecordResult(NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      if (!self.world_)
        return self.RecordResult(NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
      // No host mutex reentry, allocation or retained World/entity borrow.
      *output = {self.diagnostics_frame_, self.world_->ActiveEntityCount(), 0,
                 self.diagnostics_errors_};
      return NEXORA_GAMEPLAY_OK;
    };
    // Debug drawing remains unavailable.
    return host;
  }
  runtime::World *world_{};
  std::shared_ptr<const MeshAssetCatalog> mesh_assets_;
  LogSink log_;
  LogRejectionSink rejected_log_;
  PlayInputState input_;
  std::uint32_t input_user_{};
  PlaySceneServices scenes_;
  std::unordered_map<void *, Allocation> allocations_;
  std::uint64_t bytes_{};
  std::uint64_t diagnostics_frame_{}, diagnostics_errors_{};
  runtime::GameplayModuleHost module_;
};
} // namespace nexora::editor::preview
