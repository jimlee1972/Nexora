#pragma once
#include "Nexora/TwoD/Assets.h"
namespace nexora::twod {
struct EmissionRegion final {
  std::uint32_t base{}, mask{};
  friend bool operator==(const EmissionRegion &, const EmissionRegion &) = default;
};
// Separate metadata; historical Atlas/Clip codecs and Sprite C/Zig wires are unchanged.
struct EmissionAsset final {
  AssetId id, baseAtlas, maskAtlas;
  std::vector<EmissionRegion> regions;
};
[[nodiscard]] NEXORA_2D_API bool ValidateEmissionAsset(const EmissionAsset &) noexcept;
[[nodiscard]] NEXORA_2D_API bool
ValidateEmissionDependencies(const EmissionAsset &, const Atlas &base, const Atlas &mask) noexcept;
[[nodiscard]] NEXORA_2D_API Bytes EncodeEmissionAsset(const EmissionAsset &);
[[nodiscard]] NEXORA_2D_API std::optional<EmissionAsset>
    DecodeEmissionAsset(std::span<const std::byte>);
} // namespace nexora::twod
