#include "Nexora/Runtime/GameplayModuleHost.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#if defined(__APPLE__)
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#endif

namespace nexora::runtime {
namespace {

constexpr std::size_t kRequiredHostSize = NEXORA_GAMEPLAY_HOST_V3_BASE_SIZE;
constexpr std::size_t kRequiredModuleSize = sizeof(NexoraGameModuleV3);
constexpr auto kLoaderSymbol = "NexoraGameModuleLoad";

NexoraGameplayHostV3 OwnedHost(NexoraGameplayHostV3 host) noexcept {
  // This C++ owner copies a known-size value, never an unknown future suffix allocation.
  host.struct_size = std::min(host.struct_size, static_cast<std::uint32_t>(sizeof(host)));
  return host;
}

bool ReadModuleTable(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module) {
  if (!load)
    return false;
  module = {};
  module.struct_size = sizeof(module);
  module.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  return load(NEXORA_GAMEPLAY_ABI_VERSION, &module) == 0 &&
         module.struct_size >= kRequiredModuleSize &&
         module.abi_version == NEXORA_GAMEPLAY_ABI_VERSION && module.create && module.on_start &&
         module.update && module.on_stop && module.destroy;
}

template <typename Function> Function FunctionCast(void *symbol) noexcept {
  static_assert(sizeof(Function) == sizeof(symbol));
  Function function{};
  std::memcpy(&function, &symbol, sizeof(function));
  return function;
}

} // namespace

struct GameplayModuleHost::DynamicLibrary final {
  bool unloadable{true};
#if defined(_WIN32)
  HMODULE handle{};
#else
  void *handle{};
#endif

  ~DynamicLibrary() { Close(); }

  void Close() noexcept {
    if (handle == nullptr)
      return;
#if defined(_WIN32)
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    handle = nullptr;
  }

  static std::unique_ptr<DynamicLibrary> Open(const std::filesystem::path &path) {
    auto result = std::make_unique<DynamicLibrary>();
#if defined(_WIN32)
    result->handle = LoadLibraryW(path.c_str());
#else
    result->handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
#if defined(__APPLE__)
    // Reject known dyld-pinned images before handing them a host/context. Releasing
    // such an image does not run its finalizers, so they cannot participate in a
    // bounded generation transaction. Static-loader callers own their code lifetime.
    if (result->handle) {
      Dl_info info{};
      const auto symbol = dlsym(result->handle, kLoaderSymbol);
      if (symbol) {
        result->unloadable = dladdr(symbol, &info) && info.dli_fbase;
      }
      if (symbol && result->unloadable) {
        const auto *header = static_cast<const mach_header_64 *>(info.dli_fbase);
        result->unloadable =
            header->magic == MH_MAGIC_64 && !(header->flags & MH_HAS_TLV_DESCRIPTORS);
        if (result->unloadable)
          for (const auto *segment : {"__DATA", "__DATA_CONST", "__DATA_DIRTY"}) {
            unsigned long size{};
            if (getsectiondata(header, segment, "__objc_imageinfo", &size))
              result->unloadable = false;
          }
      }
    }
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

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV3 host) : host_(OwnedHost(host)) {}

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV3 host, QuiescenceBarrier quiescence)
    : host_(OwnedHost(host)), quiescence_(quiescence) {}

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV3 host, QuiescenceBarrier quiescence,
                                       ReloadTransaction transaction)
    : host_(OwnedHost(host)), quiescence_(quiescence), transaction_(transaction),
      strict_transaction_(true) {}

GameplayModuleHost::WorkLease::~WorkLease() { Release(); }

GameplayModuleHost::WorkLease::WorkLease(WorkLease &&other) noexcept
    : state_(std::move(other.state_)), generation_(other.generation_) {}

GameplayModuleHost::WorkLease &
GameplayModuleHost::WorkLease::operator=(WorkLease &&other) noexcept {
  if (this != &other) {
    Release();
    state_ = std::move(other.state_);
    generation_ = other.generation_;
  }
  return *this;
}

void GameplayModuleHost::WorkLease::Release() noexcept {
  if (auto state = std::move(state_)) {
    std::scoped_lock lock(state->mutex);
    --state->pending;
    // The local shared owner keeps the mutex/condition alive until after this lock releases.
    state->changed.notify_all();
  }
}

GameplayModuleHost::WorkLease
GameplayModuleHost::AcquireWork(std::uint64_t expected_generation) noexcept {
  std::scoped_lock lock(work_->mutex);
  if (!work_->open || work_->generation != expected_generation ||
      work_->pending == std::numeric_limits<std::size_t>::max())
    return {};
  ++work_->pending;
  return WorkLease(work_, expected_generation);
}

void GameplayModuleHost::OpenWorkLocked() noexcept {
  std::scoped_lock lock(work_->mutex);
  work_->generation = generation_;
  work_->open = loaded_;
}

GameplayModuleHost::~GameplayModuleHost() { Unload(); }

bool GameplayModuleHost::Create(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module,
                                const NexoraGameplayHostV3 &host) const {
  if (load == nullptr || host.struct_size < kRequiredHostSize ||
      host.abi_version != NEXORA_GAMEPLAY_ABI_VERSION)
    return false;

  if (!ReadModuleTable(load, module))
    return false;

  void *state = nullptr;
  if (module.create(&state, &host) != NEXORA_GAMEPLAY_OK)
    return false;
  module.module_state = state;
  return true;
}

bool GameplayModuleHost::Inspect(const std::filesystem::path &path) {
  auto library = DynamicLibrary::Open(path);
  NexoraGameModuleV3 module{};
  return library && ReadModuleTable(library->Loader(), module);
}

bool GameplayModuleHost::Load(NexoraGameModuleLoadV3Fn load) {
  std::scoped_lock lock(mutex_);
  return LoadLocked(load, nullptr);
}

bool GameplayModuleHost::Load(const std::filesystem::path &library) {
  auto candidate_library = DynamicLibrary::Open(library);
  if (!candidate_library)
    return false;
  std::scoped_lock lock(mutex_);
  if (!LoadLocked(candidate_library->Loader(), candidate_library.get()))
    return false;
  static_cast<void>(candidate_library.release());
  return true;
}

bool GameplayModuleHost::LoadLocked(NexoraGameModuleLoadV3Fn load, DynamicLibrary *library) {
  if (loaded_ || generation_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  if (strict_transaction_ && library && !library->unloadable) {
    reload_failure_ = ReloadFailure::RestartRequired;
    return false;
  }
  auto candidate_host = std::make_unique<NexoraGameplayHostV3>(host_);
  struct CloseCandidate final {
    DynamicLibrary *library;
    bool active{true};
    ~CloseCandidate() {
      if (active && library)
        library->Close();
    }
  } close_candidate{library};
  NexoraGameModuleV3 candidate{};
  if (!Create(load, candidate, *candidate_host))
    return false;
  if (candidate.on_start(candidate.module_state) != NEXORA_GAMEPLAY_OK) {
    candidate.destroy(candidate.module_state);
    return false;
  }
  module_ = candidate;
  module_host_ = std::move(candidate_host);
  library_ = library;
  loaded_ = true;
  close_candidate.active = false;
  ++generation_;
  reload_failure_ = ReloadFailure::None;
  OpenWorkLocked();
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
  const auto reject = [&](ReloadFailure failure) {
    std::scoped_lock lock(mutex_);
    reload_failure_ = failure;
    return false;
  };
  if (stabilization.required_stable_samples == 0 || stabilization.maximum_samples == 0 ||
      stabilization.required_stable_samples > stabilization.maximum_samples ||
      stabilization.poll_interval.count() < 0)
    return reject(ReloadFailure::InvalidCandidate);

  std::error_code error;
  std::uintmax_t previous_size{};
  std::filesystem::file_time_type previous_write{};
  std::uint32_t stable_samples{};
  for (std::uint32_t sample = 0; sample < stabilization.maximum_samples; ++sample) {
    const auto size = std::filesystem::file_size(library, error);
    if (error)
      return reject(ReloadFailure::Unavailable);
    const auto write = std::filesystem::last_write_time(library, error);
    if (error)
      return reject(ReloadFailure::Unavailable);
    if (sample != 0 && size == previous_size && write == previous_write)
      ++stable_samples;
    else
      stable_samples = 1;
    if (stable_samples >= stabilization.required_stable_samples)
      break;
    previous_size = size;
    previous_write = write;
    if (sample + 1 == stabilization.maximum_samples)
      return reject(ReloadFailure::Unavailable);
    std::this_thread::sleep_for(stabilization.poll_interval);
  }
  std::unique_ptr<DynamicLibrary> candidate_library;
  try {
    candidate_library = DynamicLibrary::Open(library);
  } catch (const std::exception &) {
    return reject(ReloadFailure::ResourceExhausted);
  }
  if (!candidate_library)
    return reject(ReloadFailure::Unavailable);
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
  const auto reject = [&](ReloadFailure failure) {
    reload_failure_ = failure;
    return false;
  };
  if (!loaded_)
    return reject(ReloadFailure::NotLoaded);
  if (library_ != nullptr && candidate_library == nullptr)
    return reject(ReloadFailure::InvalidCandidate);
  if (strict_transaction_ && library_ && candidate_library &&
      library_->handle == candidate_library->handle)
    return reject(ReloadFailure::RestartRequired);
  if (generation_ == std::numeric_limits<std::uint64_t>::max())
    return reject(ReloadFailure::ResourceExhausted);
  try {
    if (strict_transaction_ && ((library_ && !library_->unloadable) ||
                                (candidate_library && !candidate_library->unloadable)))
      return reject(ReloadFailure::RestartRequired);
    NexoraGameModuleV3 candidate{};
    if (!ReadModuleTable(load, candidate))
      return reject(ReloadFailure::InvalidCandidate);
    const auto migrates = [](const NexoraGameModuleV3 &module) {
      return (module.capabilities & NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION) &&
             module.save_state && module.load_state;
    };
    if (!migrates(module_) || !migrates(candidate))
      return reject(ReloadFailure::RestartRequired);
    const bool isolated =
        transaction_.begin && transaction_.rollback && transaction_.retire && transaction_.commit;
    if (strict_transaction_ && !isolated)
      return reject(ReloadFailure::RestartRequired);

    // Close new work and drain actual jobs/callbacks BEFORE reading module state. Update/Fixed
    // are already serialized by mutex_; registered work releases through a separate mutex.
    QuiesceLocked();
    struct Reopen final {
      GameplayModuleHost &host;
      ~Reopen() { host.OpenWorkLocked(); }
    } reopen{*this};
    const auto required = module_.save_state(module_.module_state, nullptr, 0);
    if (!required || required > kMaximumStateBytes)
      return reject(ReloadFailure::SaveState);
    std::vector<std::byte> saved_state(required);
    if (module_.save_state(module_.module_state, saved_state.data(), required) != required)
      return reject(ReloadFailure::SaveState);

    // A raw V3 module may retain the host table pointer, not just a copy of its fields. Keep
    // every generation's table at a stable address through destroy and library finalizers.
    auto candidate_host = std::make_unique<NexoraGameplayHostV3>(host_);
    struct TransactionGuard final {
      ReloadTransaction &hooks;
      bool active;
      ~TransactionGuard() {
        if (active)
          hooks.rollback(hooks.context);
      }
    } transaction{transaction_, isolated};
    struct CloseCandidate final {
      DynamicLibrary *library;
      bool active{true};
      ~CloseCandidate() {
        if (active && library)
          library->Close();
      }
    } close_candidate{candidate_library};
    if (isolated && !transaction_.begin(transaction_.context, *candidate_host))
      return reject(ReloadFailure::Isolation);
    *candidate_host = OwnedHost(*candidate_host);
    if ((candidate_host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE) &&
        (!(module_.capabilities & NEXORA_GAMEPLAY_CAPABILITY_RELOAD_LIFECYCLE) ||
         !(candidate.capabilities & NEXORA_GAMEPLAY_CAPABILITY_RELOAD_LIFECYCLE)))
      return reject(ReloadFailure::RestartRequired);
    if (candidate_host->struct_size < kRequiredHostSize ||
        candidate_host->abi_version != NEXORA_GAMEPLAY_ABI_VERSION)
      return reject(ReloadFailure::Isolation);
    void *state{};
    if (candidate.create(&state, candidate_host.get()) != NEXORA_GAMEPLAY_OK)
      return reject(ReloadFailure::Initialization);
    candidate.module_state = state;
    struct CandidateGuard final {
      NexoraGameModuleV3 &module;
      bool active{true};
      ~CandidateGuard() {
        if (active)
          module.destroy(module.module_state);
      }
    } candidate_guard{candidate};
    if (candidate.load_state(state, saved_state.data(), required) != NEXORA_GAMEPLAY_OK)
      return reject(ReloadFailure::RestoreState);
    if (candidate.on_start(state) != NEXORA_GAMEPLAY_OK)
      return reject(ReloadFailure::Initialization);

    // From this point only nonthrowing publication/retirement operations remain. Keep retired
    // World/resources isolated until both module callbacks and library finalizers have ended.
    if (isolated)
      transaction_.retire(transaction_.context);
    auto retired_host = std::move(module_host_);
    auto *retired_library = library_;
    module_.on_stop(module_.module_state);
    module_.destroy(module_.module_state);
    delete retired_library;
    module_ = candidate;
    module_host_ = std::move(candidate_host);
    library_ = candidate_library;
    ++generation_;
    failure_state_ = {};
    reload_failure_ = ReloadFailure::None;
    ++reload_stats_.successful_reloads;
    reload_stats_.migrated_bytes = required;
    candidate_guard.active = false;
    close_candidate.active = false;
    if (isolated)
      transaction_.commit(transaction_.context);
    host_ = *module_host_; // A retired generation's embedding context may have been released.
    // Keep the current context, but a future fresh Load must not inherit candidate creation mode.
    // The live generation retains its original host table through destruction/finalizers.
    host_.capabilities &= ~static_cast<std::uint64_t>(NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE);
    transaction.active = false;
    return true;
  } catch (const std::exception &) {
    return reject(ReloadFailure::ResourceExhausted);
  }
}

std::filesystem::path GameplayModuleHost::Discover(const std::filesystem::path &directory,
                                                   std::string_view module_name) {
  if (module_name.empty() || !std::filesystem::is_directory(directory))
    return {};
#if defined(__APPLE__)
  const std::string basename = "lib" + std::string(module_name);
  const auto dylib = directory / (basename + ".dylib");
  if (std::filesystem::is_regular_file(dylib))
    return dylib;
  const auto bundle = directory / (basename + ".so");
  return std::filesystem::is_regular_file(bundle) ? bundle : std::filesystem::path{};
#else
#if defined(_WIN32)
  const std::string filename = std::string(module_name) + ".dll";
#else
  const std::string filename = "lib" + std::string(module_name) + ".so";
#endif
  const auto candidate = directory / filename;
  return std::filesystem::is_regular_file(candidate) ? candidate : std::filesystem::path{};
#endif
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
  module_host_.reset();
}

void GameplayModuleHost::QuiesceLocked() const noexcept {
  {
    std::scoped_lock lock(work_->mutex);
    work_->open = false;
  }
  if (quiescence_.wait != nullptr)
    quiescence_.wait(quiescence_.context, generation_);
  std::unique_lock lock(work_->mutex);
  work_->changed.wait(lock, [&] { return work_->pending == 0; });
}

void GameplayModuleHost::Unload() noexcept {
  std::scoped_lock lock(mutex_);
  ShutdownLocked();
}

bool GameplayModuleHost::IsLoaded() const noexcept {
  std::scoped_lock lock(mutex_);
  return loaded_;
}

bool GameplayModuleHost::SupportsFixedUpdate() const noexcept {
  std::scoped_lock lock(mutex_);
  return loaded_ && module_.fixed_update &&
         (module_.capabilities & NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE) != 0;
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

GameplayModuleHost::ReloadFailure GameplayModuleHost::GetReloadFailure() const noexcept {
  std::scoped_lock lock(mutex_);
  return reload_failure_;
}

} // namespace nexora::runtime
