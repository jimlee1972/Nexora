#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Runtime/Api.h"

#include <mutex>

namespace nexora::runtime {

// Owns the callable lifetime of one gameplay module. Calls and replacement are
// serialized so a reload can never invalidate code while update is executing.
class NEXORA_RUNTIME_API GameplayModuleHost final {
public:
  explicit GameplayModuleHost(NexoraGameplayHostV1 host) noexcept;
  ~GameplayModuleHost();

  GameplayModuleHost(const GameplayModuleHost &) = delete;
  GameplayModuleHost &operator=(const GameplayModuleHost &) = delete;

  [[nodiscard]] bool Load(NexoraGameModuleLoadFn load);
  [[nodiscard]] bool Reload(NexoraGameModuleLoadFn load);
  [[nodiscard]] bool Update(double delta_seconds);
  void Unload() noexcept;
  [[nodiscard]] bool IsLoaded() const noexcept;

private:
  [[nodiscard]] bool Create(NexoraGameModuleLoadFn load, NexoraGameModuleV1 &module) const;
  void ShutdownLocked() noexcept;

  NexoraGameplayHostV1 host_{};
  mutable std::mutex mutex_;
  NexoraGameModuleV1 module_{};
  bool loaded_{};
};

} // namespace nexora::runtime
