#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace nexora::runtime::hardening_v2 {

enum class ReferenceProjectKind : std::uint8_t {
  MassiveOutdoor,
  IndoorPortalDungeon,
  NetworkArena,
  CrowdCity,
  MobileStress,
};

enum class ReferenceCapability : std::uint32_t {
  None = 0,
  Streaming = 1U << 0U,
  PortalAudio = 1U << 1U,
  Network = 1U << 2U,
  Crowd = 1U << 3U,
  MobileThermal = 1U << 4U,
  MemoryPressure = 1U << 5U,
  PatchRollback = 1U << 6U,
  SaveRecovery = 1U << 7U,
  Reconnect = 1U << 8U,
};

[[nodiscard]] constexpr ReferenceCapability operator|(ReferenceCapability left,
                                                      ReferenceCapability right) noexcept {
  return static_cast<ReferenceCapability>(static_cast<std::uint32_t>(left) |
                                          static_cast<std::uint32_t>(right));
}
[[nodiscard]] constexpr bool HasCapability(ReferenceCapability value,
                                           ReferenceCapability capability) noexcept {
  return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(capability)) != 0;
}

struct ReferenceProjectSpec final {
  ReferenceProjectKind kind{};
  std::string_view name;
  ReferenceCapability capabilities{ReferenceCapability::None};
};

[[nodiscard]] NEXORA_RUNTIME_API std::span<const ReferenceProjectSpec> ReferenceProjects() noexcept;

struct SaveImage final {
  std::uint64_t generation{};
  std::uint64_t checksum{};
  std::vector<std::byte> payload;
};

[[nodiscard]] NEXORA_RUNTIME_API SaveImage MakeSaveImage(std::uint64_t generation,
                                                         std::span<const std::byte> payload);
[[nodiscard]] NEXORA_RUNTIME_API bool VerifySaveImage(const SaveImage &image) noexcept;

enum class ThermalDecision : std::uint8_t { Normal, Throttle };
class NEXORA_RUNTIME_API ThermalPolicy final {
public:
  explicit ThermalPolicy(std::uint32_t throttle_millicelsius)
      : throttle_millicelsius_(throttle_millicelsius) {}
  [[nodiscard]] ThermalDecision Evaluate(std::uint32_t millicelsius) const noexcept;
  [[nodiscard]] std::uint32_t Threshold() const noexcept { return throttle_millicelsius_; }

private:
  std::uint32_t throttle_millicelsius_{};
};

struct ThermalEvidence final {
  std::uint64_t sequence{};
  std::uint32_t millicelsius{};
  bool throttled{};
};

[[nodiscard]] NEXORA_RUNTIME_API bool
ValidateThermalEvidence(const ThermalPolicy &policy, std::span<const ThermalEvidence> evidence);

[[nodiscard]] NEXORA_RUNTIME_API bool
FootprintWithinBudget(std::uint64_t v1_like_bytes, std::uint64_t v2_disabled_bytes,
                      std::uint64_t allowed_growth_bytes) noexcept;

} // namespace nexora::runtime::hardening_v2
