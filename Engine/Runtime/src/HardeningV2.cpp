#include "Nexora/Runtime/HardeningV2.h"

#include <array>
#include <limits>

namespace nexora::runtime::hardening_v2 {
namespace {
constexpr std::array kProjects{
    ReferenceProjectSpec{
        ReferenceProjectKind::MassiveOutdoor, "Massive Outdoor",
        ReferenceCapability::Streaming | ReferenceCapability::MemoryPressure |
            ReferenceCapability::SaveRecovery,
    },
    ReferenceProjectSpec{
        ReferenceProjectKind::IndoorPortalDungeon, "Indoor Portal Dungeon",
        ReferenceCapability::Streaming | ReferenceCapability::PortalAudio |
            ReferenceCapability::SaveRecovery,
    },
    ReferenceProjectSpec{
        ReferenceProjectKind::NetworkArena, "Network Arena",
        ReferenceCapability::Network | ReferenceCapability::PatchRollback |
            ReferenceCapability::Reconnect,
    },
    ReferenceProjectSpec{
        ReferenceProjectKind::CrowdCity, "Crowd City",
        ReferenceCapability::Streaming | ReferenceCapability::Crowd |
            ReferenceCapability::MemoryPressure,
    },
    ReferenceProjectSpec{
        ReferenceProjectKind::MobileStress, "Mobile Stress",
        ReferenceCapability::Streaming | ReferenceCapability::MobileThermal |
            ReferenceCapability::MemoryPressure | ReferenceCapability::PatchRollback,
    },
};

std::uint64_t SaveChecksum(std::uint64_t generation, std::span<const std::byte> payload) noexcept {
  constexpr std::uint64_t offset = 14695981039346656037ULL;
  constexpr std::uint64_t prime = 1099511628211ULL;
  std::uint64_t hash = offset;
  for (unsigned shift = 0; shift < 64; shift += 8U) {
    hash ^= (generation >> shift) & 0xffU;
    hash *= prime;
  }
  for (const auto value : payload) {
    hash ^= std::to_integer<std::uint8_t>(value);
    hash *= prime;
  }
  return hash;
}
} // namespace

std::span<const ReferenceProjectSpec> ReferenceProjects() noexcept { return kProjects; }

SaveImage MakeSaveImage(std::uint64_t generation, std::span<const std::byte> payload) {
  SaveImage image;
  image.generation = generation;
  image.payload.assign(payload.begin(), payload.end());
  image.checksum = SaveChecksum(generation, image.payload);
  return image;
}

bool VerifySaveImage(const SaveImage &image) noexcept {
  return image.generation != 0 && image.checksum == SaveChecksum(image.generation, image.payload);
}

ThermalDecision ThermalPolicy::Evaluate(std::uint32_t millicelsius) const noexcept {
  return throttle_millicelsius_ != 0 && millicelsius >= throttle_millicelsius_
             ? ThermalDecision::Throttle
             : ThermalDecision::Normal;
}

bool ValidateThermalEvidence(const ThermalPolicy &policy,
                             std::span<const ThermalEvidence> evidence) {
  if (evidence.empty())
    return false;
  std::uint64_t previous{};
  bool first = true;
  for (const auto &sample : evidence) {
    if ((!first && sample.sequence <= previous) ||
        (policy.Evaluate(sample.millicelsius) == ThermalDecision::Throttle &&
         !sample.throttled))
      return false;
    first = false;
    previous = sample.sequence;
  }
  return true;
}

bool FootprintWithinBudget(std::uint64_t v1_like_bytes, std::uint64_t v2_disabled_bytes,
                           std::uint64_t allowed_growth_bytes) noexcept {
  if (v2_disabled_bytes <= v1_like_bytes)
    return true;
  return v2_disabled_bytes - v1_like_bytes <= allowed_growth_bytes;
}

} // namespace nexora::runtime::hardening_v2
