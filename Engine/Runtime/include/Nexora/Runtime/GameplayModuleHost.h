#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Runtime/Api.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

namespace nexora::runtime {

// Owns the callable lifetime of one gameplay module. Calls and replacement are
// serialized so a reload can never invalidate code while update is executing.
class NEXORA_RUNTIME_API GameplayModuleHost final {
  struct WorkState final {
    std::mutex mutex;
    std::condition_variable changed;
    std::uint64_t generation{};
    std::size_t pending{};
    bool open{};
  };

public:
  enum class CallbackFailure : std::uint8_t { None, FixedUpdate, Update };
  enum class ReloadFailure : std::uint8_t {
    None,
    NotLoaded,
    InvalidCandidate,
    RestartRequired,
    SaveState,
    RestoreState,
    Initialization,
    Isolation,
    Unavailable,
    ResourceExhausted
  };
  static constexpr std::uint32_t kMaximumStateBytes = 1024 * 1024;

  // A submitted job/queued callback retains this lease until its final module call returns.
  // The host's reload/unload waits for every lease. No function pointer is owned by the lease.
  class NEXORA_RUNTIME_API WorkLease final {
  public:
    WorkLease() = default;
    ~WorkLease();
    WorkLease(WorkLease &&other) noexcept;
    WorkLease &operator=(WorkLease &&other) noexcept;
    WorkLease(const WorkLease &) = delete;
    WorkLease &operator=(const WorkLease &) = delete;
    [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
    [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

  private:
    friend class GameplayModuleHost;
    WorkLease(std::shared_ptr<WorkState> state, std::uint64_t generation) noexcept
        : state_(std::move(state)), generation_(generation) {}
    void Release() noexcept;
    std::shared_ptr<WorkState> state_;
    std::uint64_t generation_{};
  };

  // Strict World/resource transaction. begin stages an unpublished candidate host; rollback
  // tolerates partial begin and runs after candidate destroy. retire quarantines old callbacks
  // and library finalizers; commit publishes the candidate after all old code has retired.
  // All hooks are required, serialized, nonthrowing and embedding-owned through Unload.
  struct ReloadTransaction final {
    void *context{};
    bool (*begin)(void *, NexoraGameplayHostV3 &) noexcept {};
    void (*rollback)(void *) noexcept {};
    void (*retire)(void *) noexcept {};
    void (*commit)(void *) noexcept {};
  };

  struct FailureState final {
    CallbackFailure callback{CallbackFailure::None};
    std::int32_t result{NEXORA_GAMEPLAY_OK};
    std::uint64_t generation{};
  };

  struct FileStabilization final {
    std::chrono::milliseconds poll_interval{25};
    std::uint32_t required_stable_samples{2};
    std::uint32_t maximum_samples{40};
  };

  struct QuiescenceBarrier final {
    void *context{};
    void (*wait)(void *context, std::uint64_t generation){};
  };

  struct ReloadStats final {
    std::uint64_t successful_reloads{};
    std::chrono::nanoseconds last_reload_duration{};
    std::uint32_t migrated_bytes{};
  };
  explicit GameplayModuleHost(NexoraGameplayHostV3 host);
  GameplayModuleHost(NexoraGameplayHostV3 host, QuiescenceBarrier quiescence);
  // World/resource embeddings use this strict overload. Missing isolation rejects reload with
  // RestartRequired. The older overloads retain the module-state-only embedding contract.
  GameplayModuleHost(NexoraGameplayHostV3 host, QuiescenceBarrier quiescence,
                     ReloadTransaction transaction);
  ~GameplayModuleHost();

  GameplayModuleHost(const GameplayModuleHost &) = delete;
  GameplayModuleHost &operator=(const GameplayModuleHost &) = delete;

  [[nodiscard]] bool Load(NexoraGameModuleLoadV3Fn load);
  [[nodiscard]] bool Reload(NexoraGameModuleLoadV3Fn load);
  [[nodiscard]] bool Load(const std::filesystem::path &library);
  // Opens a candidate and negotiates its V3 table without create/start/update/stop/destroy.
  // Synchronous, no host/world borrow. The loader and platform library initializers may execute;
  // this is admission of an authorized module, not a sandbox or gameplay acceptance test.
  [[nodiscard]] static bool Inspect(const std::filesystem::path &library);
  [[nodiscard]] bool Reload(const std::filesystem::path &library);
  [[nodiscard]] bool Reload(const std::filesystem::path &library, FileStabilization stabilization);
  [[nodiscard]] static std::filesystem::path Discover(const std::filesystem::path &directory,
                                                      std::string_view module_name);
  [[nodiscard]] bool FixedUpdate(double fixed_delta_seconds);
  [[nodiscard]] bool Update(double delta_seconds);
  void Unload() noexcept;
  [[nodiscard]] bool IsLoaded() const noexcept;
  // True only for a loaded module advertising a non-null optional fixed_update callback.
  [[nodiscard]] bool SupportsFixedUpdate() const noexcept;
  [[nodiscard]] std::uint64_t Generation() const noexcept;
  [[nodiscard]] ReloadStats GetReloadStats() const noexcept;
  [[nodiscard]] FailureState GetFailureState() const noexcept;
  [[nodiscard]] ReloadFailure GetReloadFailure() const noexcept;
  // Register before publishing work, including queued callbacks. Rejects closed/stale
  // generations, including while a reload barrier runs. May be called from a module callback;
  // it does not re-enter lifecycle serialization. Never call after host owner teardown begins.
  [[nodiscard]] WorkLease AcquireWork(std::uint64_t expected_generation) noexcept;

private:
  struct DynamicLibrary;
  [[nodiscard]] bool Create(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module,
                            const NexoraGameplayHostV3 &host) const;
  [[nodiscard]] bool LoadLocked(NexoraGameModuleLoadV3Fn load, DynamicLibrary *library);
  [[nodiscard]] bool ReloadLocked(NexoraGameModuleLoadV3Fn load, DynamicLibrary *candidate_library);
  void QuiesceLocked() const noexcept;
  void ShutdownLocked() noexcept;
  void OpenWorkLocked() noexcept;

  NexoraGameplayHostV3 host_{};
  QuiescenceBarrier quiescence_{};
  ReloadTransaction transaction_{};
  bool strict_transaction_{};
  mutable std::mutex mutex_;
  const std::shared_ptr<WorkState> work_{std::make_shared<WorkState>()};
  NexoraGameModuleV3 module_{};
  std::unique_ptr<NexoraGameplayHostV3> module_host_;
  DynamicLibrary *library_{};
  std::uint64_t generation_{};
  bool loaded_{};
  ReloadStats reload_stats_{};
  FailureState failure_state_{};
  ReloadFailure reload_failure_{};
};

} // namespace nexora::runtime
