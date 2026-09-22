#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Runtime/Api.h"

#include <chrono>
#include <cstdint>
#include <mutex>

namespace nexora::runtime {

// Owns the callable lifetime of one gameplay module. Calls and replacement are
// serialized so a reload can never invalidate code while update is executing.
class NEXORA_RUNTIME_API GameplayModuleHost final {
public:
  struct ReloadStats final {
    std::uint64_t successful_reloads{};
    std::chrono::nanoseconds last_reload_duration{};
    std::uint32_t migrated_bytes{};
  };
  explicit GameplayModuleHost(NexoraGameplayHostV3 host) noexcept;
  ~GameplayModuleHost();

  GameplayModuleHost(const GameplayModuleHost &) = delete;
  GameplayModuleHost &operator=(const GameplayModuleHost &) = delete;

  [[nodiscard]] bool Load(NexoraGameModuleLoadV3Fn load);
  [[nodiscard]] bool Reload(NexoraGameModuleLoadV3Fn load);
  [[nodiscard]] bool FixedUpdate(double fixed_delta_seconds);
  [[nodiscard]] bool Update(double delta_seconds);
  void Unload() noexcept;
  [[nodiscard]] bool IsLoaded() const noexcept;
  [[nodiscard]] ReloadStats GetReloadStats() const noexcept;

private:
  [[nodiscard]] bool Create(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module) const;
  void ShutdownLocked() noexcept;

  NexoraGameplayHostV3 host_{};
  mutable std::mutex mutex_;
  NexoraGameModuleV3 module_{};
  bool loaded_{};
  ReloadStats reload_stats_{};
};

} // namespace nexora::runtime
