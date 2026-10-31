#include "Nexora/TwoD/EmissionAssets.h"
#include <algorithm>
namespace nexora::twod {
bool ValidateEmissionAsset(const EmissionAsset &v) noexcept {
  const auto valid = [](AssetId id) { return id.high || id.low; };
  if (!valid(v.id) || !valid(v.baseAtlas) || !valid(v.maskAtlas) || v.id == v.baseAtlas ||
      v.id == v.maskAtlas || v.baseAtlas == v.maskAtlas || v.regions.empty() ||
      v.regions.size() > 1024)
    return false;
  for (std::size_t i = 0; i < v.regions.size(); ++i) {
    if (!v.regions[i].base || !v.regions[i].mask)
      return false;
    for (std::size_t j = 0; j < i; ++j)
      if (v.regions[j].base == v.regions[i].base)
        return false;
  }
  return true;
}
bool ValidateEmissionDependencies(const EmissionAsset &v, const Atlas &base,
                                  const Atlas &mask) noexcept {
  if (!ValidateEmissionAsset(v) || !ValidateAtlas(base) || !ValidateAtlas(mask) ||
      base.id != v.baseAtlas || mask.id != v.maskAtlas || mask.nearest)
    return false;
  for (const auto &region : v.regions) {
    const auto *a = base.Find(region.base), *b = mask.Find(region.mask);
    if (!a || !b || a->width != b->width || a->height != b->height)
      return false;
  }
  return true;
}
Bytes EncodeEmissionAsset(const EmissionAsset &v) {
  if (!ValidateEmissionAsset(v))
    return {};
  Bytes bytes;
  bytes.reserve(60 + v.regions.size() * 8);
  const auto put = [&](uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
      bytes.push_back(std::byte((value >> (8 * i)) & 255));
  };
  put(0x4532444e, 4);
  put(1, 4);
  for (const auto id : {v.id, v.baseAtlas, v.maskAtlas}) {
    put(id.high, 8);
    put(id.low, 8);
  }
  put(v.regions.size(), 4);
  auto regions = v.regions;
  std::sort(regions.begin(), regions.end(), [](auto a, auto b) { return a.base < b.base; });
  for (auto region : regions) {
    put(region.base, 4);
    put(region.mask, 4);
  }
  return bytes;
}
std::optional<EmissionAsset> DecodeEmissionAsset(std::span<const std::byte> bytes) {
  if (bytes.size() < 60 || bytes.size() > 60 + 1024 * 8)
    return std::nullopt;
  std::size_t cursor{};
  const auto get = [&](unsigned size) {
    uint64_t value{};
    for (unsigned i = 0; i < size; ++i)
      value |= uint64_t(std::to_integer<unsigned>(bytes[cursor++])) << (8 * i);
    return value;
  };
  if (get(4) != 0x4532444e || get(4) != 1)
    return std::nullopt;
  EmissionAsset asset;
  for (auto *id : {&asset.id, &asset.baseAtlas, &asset.maskAtlas}) {
    id->high = get(8);
    id->low = get(8);
  }
  const auto count = get(4);
  if (!count || count > 1024 || bytes.size() != 60 + count * 8)
    return std::nullopt;
  for (uint64_t i = 0; i < count; ++i)
    asset.regions.push_back({uint32_t(get(4)), uint32_t(get(4))});
  if (!ValidateEmissionAsset(asset))
    return std::nullopt;
  return asset;
}
} // namespace nexora::twod
