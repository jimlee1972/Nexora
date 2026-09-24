#include "Nexora/Runtime/GameplayModuleHost.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace nexora::runtime {
namespace {

constexpr std::size_t kRequiredHostSize = sizeof(NexoraGameplayHostV3);
constexpr std::size_t kRequiredModuleSize = sizeof(NexoraGameModuleV3);
constexpr auto kLoaderSymbol = "NexoraGameModuleLoad";

template <typename Function> Function FunctionCast(void *symbol) noexcept {
  static_assert(sizeof(Function) == sizeof(symbol));
  Function function{};
  std::memcpy(&function, &symbol, sizeof(function));
  return function;
}

} // namespace

struct GameplayModuleHost::DynamicLibrary final {
#if defined(_WIN32)
  HMODULE handle{};
#else
  void *handle{};
#endif

  ~DynamicLibrary() {
    if (handle == nullptr)
      return;
#if defined(_WIN32)
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
  }

  static std::unique_ptr<DynamicLibrary> Open(const std::filesystem::path &path) {
    auto result = std::make_unique<DynamicLibrary>();
#if defined(_WIN32)
    result->handle = LoadLibraryW(path.c_str());
#else
    result->handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    return result->handle == nullptr ? nullptr : std::move(result);
  }

  NexoraGameModuleLoadV3Fn Loader() const noexcept {
#if defined(_WIN32)
    return FunctionCast<NexoraGameModuleLoadV3Fn>(
        reinterpret_cast<void *>(GetProcAddress(handle, kLoaderSymbol)));
#else
    return FunctionCast<NexoraGameModuleLoadV3Fn>(dlsym(handle, kLoaderSymbol));
#endif
  }
};

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV3 host) noexcept : host_(host) {}

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV3 host,
                                       QuiescenceBarrier quiescence) noexcept
    : host_(host), quiescence_(quiescence) {}

GameplayModuleHost::~GameplayModuleHost() { Unload(); }

bool GameplayModuleHost::Create(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module) const {
  if (load == nullptr || host_.struct_size < kRequiredHostSize ||
      host_.abi_version != NEXORA_GAMEPLAY_ABI_VERSION)
    return false;

  module = {};
  module.struct_size = sizeof(module);
  module.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  if (load(NEXORA_GAMEPLAY_ABI_VERSION, &module) != 0 || module.struct_size < kRequiredModuleSize ||
      module.abi_version != NEXORA_GAMEPLAY_ABI_VERSION || module.create == nullptr ||
      module.on_start == nullptr || module.update == nullptr || module.on_stop == nullptr ||
      module.destroy == nullptr)
    return false;

  void *state = nullptr;
  if (module.create(&state, &host_) != NEXORA_GAMEPLAY_OK)
    return false;
  module.module_state = state;
  if (module.on_start(state) != NEXORA_GAMEPLAY_OK) {
    module.destroy(state);
    module = {};
    return false;
  }
  return true;
}

bool GameplayModuleHost::Load(NexoraGameModuleLoadV3Fn load) {
  std::scoped_lock lock(mutex_);
  if (loaded_)
    return false;
  NexoraGameModuleV3 candidate{};
  if (!Create(load, candidate))
    return false;
  module_ = candidate;
  loaded_ = true;
  generation_ = 1;
  return true;
}

bool GameplayModuleHost::Load(const std::filesystem::path &library) {
  auto candidate_library = DynamicLibrary::Open(library);
  if (!candidate_library)
    return false;
  const auto load = candidate_library->Loader();
  std::scoped_lock lock(mutex_);
  if (loaded_)
    return false;
  NexoraGameModuleV3 candidate{};
  if (!Create(load, candidate))
    return false;
  module_ = candidate;
  library_ = candidate_library.release();
  loaded_ = true;
  generation_ = 1;
  return true;
}

bool GameplayModuleHost::Reload(NexoraGameModuleLoadV3Fn load) {
  const auto started = std::chrono::steady_clock::now();
  std::scoped_lock lock(mutex_);
  if (!ReloadLocked(load, nullptr))
    return false;
  reload_stats_.last_reload_duration = std::chrono::steady_clock::now() - started;
  return true;
}

bool GameplayModuleHost::Reload(const std::filesystem::path &library) {
  return Reload(library, {});
}

bool GameplayModuleHost::Reload(const std::filesystem::path &library,
                                FileStabilization stabilization) {
  const auto started = std::chrono::steady_clock::now();
  if (stabilization.required_stable_samples == 0 || stabilization.maximum_samples == 0 ||
      stabilization.required_stable_samples > stabilization.maximum_samples ||
      stabilization.poll_interval.count() < 0)
    return false;

  std::error_code error;
  std::uintmax_t previous_size{};
  std::filesystem::file_time_type previous_write{};
  std::uint32_t stable_samples{};
  for (std::uint32_t sample = 0; sample < stabilization.maximum_samples; ++sample) {
    const auto size = std::filesystem::file_size(library, error);
    if (error)
      return false;
    const auto write = std::filesystem::last_write_time(library, error);
    if (error)
      return false;
    if (sample != 0 && size == previous_size && write == previous_write)
      ++stable_samples;
    else
      stable_samples = 1;
    if (stable_samples >= stabilization.required_stable_samples)
      break;
    previous_size = size;
    previous_write = write;
    if (sample + 1 == stabilization.maximum_samples)
      return false;
    std::this_thread::sleep_for(stabilization.poll_interval);
  }
  auto candidate_library = DynamicLibrary::Open(library);
  if (!candidate_library)
    return false;
  const auto load = candidate_library->Loader();
  std::scoped_lock lock(mutex_);
  if (!ReloadLocked(load, candidate_library.get()))
    return false;
  candidate_library.release();
  reload_stats_.last_reload_duration = std::chrono::steady_clock::now() - started;
  return true;
}

bool GameplayModuleHost::ReloadLocked(NexoraGameModuleLoadV3Fn load,
                                      DynamicLibrary *candidate_library) {
  if (!loaded_ || (library_ != nullptr && candidate_library == nullptr))
    return false;

  std::vector<std::byte> saved_state;
  if (module_.save_state != nullptr) {
    const auto required = module_.save_state(module_.module_state, nullptr, 0);
    if (required != 0) {
      saved_state.resize(required);
      if (module_.save_state(module_.module_state, saved_state.data(), required) != required)
        return false;
    }
  }

  NexoraGameModuleV3 candidate{};
  if (!Create(load, candidate))
    return false;

  if (!saved_state.empty()) {
    if (candidate.load_state == nullptr ||
        candidate.load_state(candidate.module_state, saved_state.data(),
                             static_cast<std::uint32_t>(saved_state.size())) != 0) {
      candidate.on_stop(candidate.module_state);
      candidate.destroy(candidate.module_state);
      return false;
    }
  }

  QuiesceLocked();
  auto *retired_library = library_;
  module_.on_stop(module_.module_state);
  module_.destroy(module_.module_state);
  module_ = {};
  module_ = candidate;
  library_ = candidate_library;
  loaded_ = true;
  ++generation_;
  failure_state_ = {};
  delete retired_library;
  ++reload_stats_.successful_reloads;
  reload_stats_.migrated_bytes = static_cast<std::uint32_t>(saved_state.size());
  return true;
}

std::filesystem::path GameplayModuleHost::Discover(const std::filesystem::path &directory,
                                                   std::string_view module_name) {
  if (module_name.empty() || !std::filesystem::is_directory(directory))
    return {};
#if defined(_WIN32)
  const std::string filename = std::string(module_name) + ".dll";
#elif defined(__APPLE__)
  const std::string filename = "lib" + std::string(module_name) + ".dylib";
#else
  const std::string filename = "lib" + std::string(module_name) + ".so";
#endif
  const auto candidate = directory / filename;
  return std::filesystem::is_regular_file(candidate) ? candidate : std::filesystem::path{};
}

bool GameplayModuleHost::Update(double delta_seconds) {
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0)
    return false;
  std::scoped_lock lock(mutex_);
  if (!loaded_)
    return false;
  const auto result = module_.update(module_.module_state, delta_seconds);
  if (result != NEXORA_GAMEPLAY_OK)
    failure_state_ = {CallbackFailure::Update, result, generation_};
  return result == NEXORA_GAMEPLAY_OK;
}

bool GameplayModuleHost::FixedUpdate(double fixed_delta_seconds) {
  if (!std::isfinite(fixed_delta_seconds) || fixed_delta_seconds <= 0.0)
    return false;
  std::scoped_lock lock(mutex_);
  if (!loaded_ || module_.fixed_update == nullptr ||
      (module_.capabilities & NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE) == 0)
    return false;
  const auto result = module_.fixed_update(module_.module_state, fixed_delta_seconds);
  if (result != NEXORA_GAMEPLAY_OK)
    failure_state_ = {CallbackFailure::FixedUpdate, result, generation_};
  return result == NEXORA_GAMEPLAY_OK;
}

void GameplayModuleHost::ShutdownLocked() noexcept {
  if (loaded_) {
    QuiesceLocked();
    module_.on_stop(module_.module_state);
    module_.destroy(module_.module_state);
  }
  module_ = {};
  loaded_ = false;
  delete library_;
  library_ = nullptr;
}

void GameplayModuleHost::QuiesceLocked() const noexcept {
  if (quiescence_.wait != nullptr)
    quiescence_.wait(quiescence_.context, generation_);
}

void GameplayModuleHost::Unload() noexcept {
  std::scoped_lock lock(mutex_);
  ShutdownLocked();
}

bool GameplayModuleHost::IsLoaded() const noexcept {
  std::scoped_lock lock(mutex_);
  return loaded_;
}

std::uint64_t GameplayModuleHost::Generation() const noexcept {
  std::scoped_lock lock(mutex_);
  return generation_;
}

GameplayModuleHost::ReloadStats GameplayModuleHost::GetReloadStats() const noexcept {
  std::scoped_lock lock(mutex_);
  return reload_stats_;
}

GameplayModuleHost::FailureState GameplayModuleHost::GetFailureState() const noexcept {
  std::scoped_lock lock(mutex_);
  return failure_state_;
}

} // namespace nexora::runtime
