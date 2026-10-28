#include "Nexora/TwoD/Scene.h"
#include "SpriteGeometry.h"
#include "Wire.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace nexora::twod {
namespace {
constexpr std::uint32_t spriteMagic = 0x5032444e;
bool ValidQuad(const SpriteLocalQuad &quad) {
  for (const auto value : {quad.minimum.x, quad.minimum.y, quad.maximum.x, quad.maximum.y})
    if (!std::isfinite(value) || std::abs(value) > 1000000)
      return false;
  return quad.minimum.x < quad.maximum.x && quad.minimum.y < quad.maximum.y;
}
SpriteLocalQuad Quad(const SpriteAsset &a) {
  const double bottom = double(a.sourceHeight) - a.trimY - a.trimHeight;
  return {{float((double(a.trimX) - a.pivotX) / a.pixelsPerUnit),
           float((bottom - a.pivotY) / a.pixelsPerUnit)},
          {float((double(a.trimX) + a.trimWidth - a.pivotX) / a.pixelsPerUnit),
           float((bottom + a.trimHeight - a.pivotY) / a.pixelsPerUnit)}};
}
} // namespace

bool ValidateSpriteAsset(const SpriteAsset &a) noexcept {
  if (!detail::ValidId(a.id) || !detail::ValidId(a.atlas) || a.id == a.atlas || !a.region ||
      !a.sourceWidth || !a.sourceHeight || a.sourceWidth > 4096 || a.sourceHeight > 4096 ||
      std::uint64_t(a.sourceWidth) * a.sourceHeight > 4 * 1024 * 1024 || !a.trimWidth ||
      !a.trimHeight || a.trimX >= a.sourceWidth || a.trimY >= a.sourceHeight ||
      a.trimWidth > a.sourceWidth - a.trimX || a.trimHeight > a.sourceHeight - a.trimY ||
      !std::isfinite(a.pivotX) || !std::isfinite(a.pivotY) || !std::isfinite(a.pixelsPerUnit) ||
      a.pixelsPerUnit <= 0 || !ValidQuad(Quad(a)))
    return false;
  if (double(a.trimWidth) / a.pixelsPerUnit > 1000000 ||
      double(a.trimHeight) / a.pixelsPerUnit > 1000000)
    return false;
  if (std::uint64_t(a.border[0]) + a.border[1] > a.sourceWidth ||
      std::uint64_t(a.border[2]) + a.border[3] > a.sourceHeight)
    return false;
  // Sliced source metadata must retain the whole canvas. Sliced rendering is a later gate.
  const bool bordered =
      std::any_of(a.border.begin(), a.border.end(), [](auto v) { return v != 0; });
  return !bordered ||
         (!a.trimX && !a.trimY && a.trimWidth == a.sourceWidth && a.trimHeight == a.sourceHeight);
}

Bytes EncodeSpriteAsset(const SpriteAsset &a) {
  if (!ValidateSpriteAsset(a))
    return {};
  detail::Writer w;
  w.U32(spriteMagic);
  w.U32(1);
  w.Id(a.id);
  w.Id(a.atlas);
  for (auto v :
       {a.region, a.sourceWidth, a.sourceHeight, a.trimX, a.trimY, a.trimWidth, a.trimHeight})
    w.U32(v);
  w.Float(a.pivotX);
  w.Float(a.pivotY);
  w.Float(a.pixelsPerUnit);
  for (auto v : a.border)
    w.U32(v);
  return std::move(w.bytes);
}

std::optional<SpriteAsset> DecodeSpriteAsset(std::span<const std::byte> bytes) {
  // A fixed-size schema bounds malformed inputs before any allocation or field access.
  if (bytes.size() != 96)
    return std::nullopt;
  detail::Reader r(bytes);
  if (r.U32() != spriteMagic || r.U32() != 1)
    return std::nullopt;
  SpriteAsset a;
  a.id = r.Id();
  a.atlas = r.Id();
  a.region = r.U32();
  a.sourceWidth = r.U32();
  a.sourceHeight = r.U32();
  a.trimX = r.U32();
  a.trimY = r.U32();
  a.trimWidth = r.U32();
  a.trimHeight = r.U32();
  a.pivotX = r.Float();
  a.pivotY = r.Float();
  a.pixelsPerUnit = r.Float();
  for (auto &v : a.border)
    v = r.U32();
  return r.Done() && ValidateSpriteAsset(a) ? std::optional(a) : std::nullopt;
}

namespace {
std::optional<SpriteInstance> ResolveGeometry(const SpriteAsset &a, const Atlas &atlas,
                                              SpriteInstance instance) {
  if (!ValidateSpriteAsset(a) || atlas.id != a.atlas || !instance.id)
    return std::nullopt;
  const auto *region = atlas.Find(a.region);
  if (!region || region->width != a.trimWidth || region->height != a.trimHeight)
    return std::nullopt;
  return detail::SpriteGeometry(a, std::move(instance));
}

std::optional<SpriteInstance> detail::SpriteGeometry(const SpriteAsset &a,
                                                     SpriteInstance instance) {
  if (!ValidateSpriteAsset(a) || !instance.id)
    return std::nullopt;
  for (auto value : instance.matrix)
    if (!std::isfinite(value) || std::abs(value) > 1e12)
      return std::nullopt;
  if (instance.matrix[3] != 0 || instance.matrix[7] != 0 || instance.matrix[11] != 0 ||
      instance.matrix[15] != 1)
    return std::nullopt;
  auto &sprite = instance.sprite;
  sprite.source.reset();
  sprite.atlas = a.atlas;
  sprite.region = a.region;
  sprite.size = {float(double(a.trimWidth) / a.pixelsPerUnit),
                 float(double(a.trimHeight) / a.pixelsPerUnit)};
  sprite.pivot = {0.5F, 0.5F};
  if (!ValidateSprite(sprite))
    return std::nullopt;
  auto quad = Quad(a);
  if (sprite.flipX) {
    const auto oldMinimum = quad.minimum.x;
    quad.minimum.x = -quad.maximum.x;
    quad.maximum.x = -oldMinimum;
  }
  if (sprite.flipY) {
    const auto oldMinimum = quad.minimum.y;
    quad.minimum.y = -quad.maximum.y;
    quad.maximum.y = -oldMinimum;
  }
  instance.localQuad = quad;
  return instance;
}
} // namespace

std::optional<SpriteInstance> ResolveSpriteAsset(const SpriteAsset &a, const Atlas &atlas,
                                                 SpriteInstance instance) {
  if (!ValidateAtlas(atlas))
    return std::nullopt;
  return ResolveGeometry(a, atlas, std::move(instance));
}

std::optional<SpriteFrame> ExtractSprites(std::span<const SpriteInstance> instances,
                                          const Camera &camera, const AtlasResolver &resolve,
                                          const SpriteAssetResolver &sources) {
  // Preserve the legacy extraction hot path and validate every input before callbacks.
  if (instances.size() > 100000)
    return std::nullopt;
  if (std::none_of(instances.begin(), instances.end(),
                   [](const auto &i) { return i.sprite.source.has_value(); }))
    return ExtractSprites(instances, camera, resolve);
  if (!resolve || !ExtractSprites({}, camera, resolve))
    return std::nullopt;
  std::vector<std::uint64_t> ids;
  ids.reserve(instances.size());
  for (const auto &i : instances) {
    if (!i.id || !ValidateSprite(i.sprite) || (i.sprite.source && i.localQuad))
      return std::nullopt;
    ids.push_back(i.id);
    for (auto value : i.matrix)
      if (!std::isfinite(value) || std::abs(value) > 1e12)
        return std::nullopt;
    if (i.matrix[3] != 0 || i.matrix[7] != 0 || i.matrix[11] != 0 || i.matrix[15] != 1 ||
        (i.localQuad && !ValidQuad(*i.localQuad)))
      return std::nullopt;
  }
  std::sort(ids.begin(), ids.end());
  if (std::adjacent_find(ids.begin(), ids.end()) != ids.end())
    return std::nullopt;
  // Callbacks may mutate caller storage; geometry/styles must use the validated snapshot.
  const std::vector<SpriteInstance> snapshot(instances.begin(), instances.end());
  instances = snapshot;
  std::vector<std::shared_ptr<const SpriteAsset>> sourceOwners;
  std::unordered_map<AssetId, std::optional<AtlasBinding>, runtime::AssetUuidHash> atlasCache;
  std::unordered_map<AssetId, std::optional<SpriteInstance>, runtime::AssetUuidHash> geometry;
  std::set<std::uint64_t> textures;
  bool invalid = false;
  const AtlasResolver cachedAtlas = [&](AssetId id) -> std::optional<AtlasBinding> {
    if (auto found = atlasCache.find(id); found != atlasCache.end())
      return found->second;
    auto binding = resolve(id);
    if (binding &&
        (!binding->atlas || binding->atlas->id != id || !ValidateAtlas(*binding->atlas) ||
         !binding->texture || binding->texture >= UINT64_MAX - 1 || textures.size() >= 64 ||
         !textures.insert(binding->texture).second)) {
      invalid = true;
      binding.reset();
    }
    atlasCache.emplace(id, binding);
    return binding;
  };
  std::vector<SpriteInstance> prepared;
  prepared.reserve(instances.size());
  std::size_t unavailable = 0;
  for (const auto &i : instances) {
    if (!i.sprite.source) {
      prepared.push_back(i);
      continue;
    }
    // Hidden sources do not cause IO or inflate unavailable counts.
    if (!i.sprite.visible || i.sprite.color[3] == 0)
      continue;
    const auto id = *i.sprite.source;
    auto found = geometry.find(id);
    if (found == geometry.end()) {
      auto source = sources ? sources(id) : nullptr;
      sourceOwners.push_back(source);
      std::optional<SpriteInstance> templateInstance;
      if (source) {
        if (source->id != id || !ValidateSpriteAsset(*source))
          return std::nullopt;
        auto atlas = cachedAtlas(source->atlas);
        if (invalid)
          return std::nullopt;
        if (atlas) {
          templateInstance =
              ResolveGeometry(*source, *atlas->atlas, SpriteInstance{1, {}, runtime::ToMatrix({})});
          if (!templateInstance)
            return std::nullopt;
        }
      }
      found = geometry.emplace(id, std::move(templateInstance)).first;
    }
    if (!found->second) {
      ++unavailable;
      continue;
    }
    auto instance = i;
    const auto &resolved = *found->second;
    instance.sprite.source.reset();
    instance.sprite.atlas = resolved.sprite.atlas;
    instance.sprite.region = resolved.sprite.region;
    instance.sprite.size = resolved.sprite.size;
    instance.sprite.pivot = resolved.sprite.pivot;
    auto quad = *resolved.localQuad;
    if (instance.sprite.flipX) {
      const auto minimum = quad.minimum.x;
      quad.minimum.x = -quad.maximum.x;
      quad.maximum.x = -minimum;
    }
    if (instance.sprite.flipY) {
      const auto minimum = quad.minimum.y;
      quad.minimum.y = -quad.maximum.y;
      quad.maximum.y = -minimum;
    }
    instance.localQuad = quad;
    prepared.push_back(std::move(instance));
  }
  auto frame = ExtractSprites(prepared, camera, cachedAtlas);
  if (invalid || !frame)
    return std::nullopt;
  frame->statistics.input = instances.size();
  frame->statistics.unavailable += unavailable;
  return frame;
}
} // namespace nexora::twod
