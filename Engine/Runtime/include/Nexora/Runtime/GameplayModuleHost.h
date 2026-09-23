#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Runtime/Api.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace nexora::runtime {

// Owns the callable lifetime of one gameplay module. Calls and replacement are
// serialized so a reload can never invalidate code while update is executing.
class NEXORA_RUNTIME_API GameplayModuleHost final {
public:
  struct QuiescenceBarrier final {
    void *context{};
    void (*wait)(void *context, std::uint64_t generation){};
  };

  struct ReloadStats final {
    std::uint64_t successful_reloads{};
    std::chrono::nanoseconds last_reload_duration{};
    std::uint32_t migrated_bytes{};
  };
  explicit GameplayModuleHost(NexoraGameplayHostV3 host) noexcept;
  GameplayModuleHost(NexoraGameplayHostV3 host, QuiescenceBarrier quiescence) noexcept;
  ~GameplayModuleHost();

  GameplayModuleHost(const GameplayModuleHost &) = delete;
  GameplayModuleHost &operator=(const GameplayModuleHost &) = delete;

  [[nodiscard]] bool Load(NexoraGameModuleLoadV3Fn load);
  [[nodiscard]] bool Reload(NexoraGameModuleLoadV3Fn load);
  [[nodiscard]] bool Load(const std::filesystem::path &library);
  [[nodiscard]] bool Reload(const std::filesystem::path &library);
  [[nodiscard]] static std::filesystem::path Discover(const std::filesystem::path &directory,
                                                      std::string_view module_name);
  [[nodiscard]] bool FixedUpdate(double fixed_delta_seconds);
  [[nodiscard]] bool Update(double delta_seconds);
  void Unload() noexcept;
  [[nodiscard]] bool IsLoaded() const noexcept;
  [[nodiscard]] std::uint64_t Generation() const noexcept;
  [[nodiscard]] ReloadStats GetReloadStats() const noexcept;

private:
  struct DynamicLibrary;
  [[nodiscard]] bool Create(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module) const;
  [[nodiscard]] bool ReloadLocked(NexoraGameModuleLoadV3Fn load, DynamicLibrary *candidate_library);
  void QuiesceLocked() const noexcept;
  void ShutdownLocked() noexcept;

  NexoraGameplayHostV3 host_{};
  QuiescenceBarrier quiescence_{};
  mutable std::mutex mutex_;
  NexoraGameModuleV3 module_{};
  DynamicLibrary *library_{};
  std::uint64_t generation_{};
  bool loaded_{};
  ReloadStats reload_stats_{};
};

} // namespace nexora::runtime
