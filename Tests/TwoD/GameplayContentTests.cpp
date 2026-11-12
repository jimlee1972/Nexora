#include "Nexora/TwoD/GameplayContent.h"
#include "nexora/nexora.h"
#include <cassert>
#include <stdexcept>
#include <thread>
using namespace nexora;
namespace {
twod::Atlas Atlas(twod::AssetId id = {8, 9}, std::uint32_t region = 1) {
  twod::Image image{region, 2, 2, {}};
  image.rgba.assign(16, std::byte{255});
  auto atlas = twod::PackAtlas(id, std::span(&image, 1), 8, 8, true);
  assert(atlas);
  return std::move(*atlas);
}
twod::Clip Clip() { return {{3, 4}, {8, 9}, true, {{1, 0.1, "frame"}}}; }
NexoraGameplayContentApiV1 Api(twod::GameplayContent &content) {
  NexoraGameplayContentApiV1 api{};
  api.struct_size = sizeof(api);
  assert(content.GetApi(&api) == 0 && api.abi_version == 1);
  return api;
}
twod::SpriteAsset Source(twod::AssetId id = {1, 2}) {
  twod::SpriteAsset source;
  source.id = id;
  source.atlas = {8, 9};
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = 1;
  source.trimWidth = source.trimHeight = 2;
  source.pivotX = 2;
  source.pivotY = 0;
  source.pixelsPerUnit = 2;
  return source;
}
NexoraGameplaySpriteContentApiV1 SpriteApi(twod::GameplayContent &content) {
  NexoraGameplaySpriteContentApiV1 api{};
  api.struct_size = sizeof(api);
  assert(content.GetSpriteApi(&api) == 0 && api.abi_version == 1);
  return api;
}
void SpriteSources() {
  std::shared_ptr<const twod::SpriteAsset> retained;
  std::optional<twod::SpriteFrame> frame;
  {
    twod::GameplayContent content;
    auto api = Api(content);
    auto sources = SpriteApi(content);
    assert(content.GetSpriteApi(nullptr) == -1);
    NexoraGameplaySpriteContentApiV1 shortTable{};
    shortTable.struct_size = sizeof(shortTable) - 1;
    shortTable.abi_version = 99;
    assert(content.GetSpriteApi(&shortTable) == -2 && shortTable.abi_version == 99 &&
           !shortTable.register_sprite_source);
    // Oversized callers retain their tail; only the declared V1 prefix is written.
    struct Extended final {
      NexoraGameplaySpriteContentApiV1 api;
      std::uint64_t tail{123};
    } extended{};
    extended.api.struct_size = sizeof(extended);
    assert(content.GetSpriteApi(&extended.api) == 0 && extended.tail == 123 &&
           extended.api.struct_size == sizeof(sources));
    const auto bytes = twod::EncodeSpriteAsset(Source());
    std::uint64_t high = 55, low = 66;
    auto publish = [&](const twod::Bytes &value) {
      return sources.register_sprite_source(sources.context, value.data(), value.size(), &high,
                                            &low);
    };
    assert(publish(bytes) == -2 && high == 55 && low == 66);
    const auto atlas = twod::EncodeAtlas(Atlas());
    assert(api.register_atlas(api.context, atlas.data(), atlas.size(), &high, &low) == 0);
    high = 55;
    low = 66;
    assert(sources.register_sprite_source(nullptr, bytes.data(), bytes.size(), &high, &low) == -1);
    assert(sources.register_sprite_source(sources.context, nullptr, bytes.size(), &high, &low) ==
           -1);
    assert(sources.register_sprite_source(sources.context, bytes.data(), UINT64_MAX, &high, &low) ==
           -1);
    assert(sources.register_sprite_source(sources.context, bytes.data(), bytes.size(), &high,
                                          &high) == -1);
    auto bad = bytes;
    bad.pop_back();
    assert(publish(bad) == -1);
    bad = bytes;
    bad.push_back(std::byte{});
    assert(publish(bad) == -1);
    bad = bytes;
    bad[4] = std::byte{2};
    assert(publish(bad) == -1);
    auto source = Source();
    source.trimWidth = 1;
    assert(publish(twod::EncodeSpriteAsset(source)) == -1);
    source = Source();
    source.region = 2;
    assert(publish(twod::EncodeSpriteAsset(source)) == -1);
    assert(high == 55 && low == 66 && !content.ResolveSprite({1, 2}));
    assert(publish(bytes) == 0 && high == 1 && low == 2);
    retained = content.ResolveSprite({1, 2});
    assert(retained && *retained == Source());
    const auto usage = content.Statistics();
    assert(publish(bytes) == 0 && content.ResolveSprite({1, 2}) == retained &&
           content.Statistics().active_bytes == usage.active_bytes &&
           content.Statistics().retired_generations == 0);
    // UUID collisions reject in both directions, including Clip/source collisions.
    source = Source({8, 9});
    source.atlas = {10, 11};
    assert(!twod::EncodeSpriteAsset(source).empty() &&
           publish(twod::EncodeSpriteAsset(source)) == -1);
    auto clip = Clip();
    auto clipBytes = twod::EncodeClip(clip);
    assert(api.register_clip(api.context, clipBytes.data(), clipBytes.size(), &high, &low) == 0);
    assert(publish(twod::EncodeSpriteAsset(Source(clip.id))) == -1);
    clip.id = {1, 2};
    clipBytes = twod::EncodeClip(clip);
    assert(api.register_clip(api.context, clipBytes.data(), clipBytes.size(), &high, &low) == -1);
    const auto collision = twod::EncodeAtlas(Atlas({1, 2}));
    assert(api.register_atlas(api.context, collision.data(), collision.size(), &high, &low) == -1);
    assert(!content.ResolveSprite({8, 9}) && !content.ResolveSprite({3, 4}));
    NexoraContentAsset list[3]{};
    std::uint32_t count{};
    list[0].high = 99;
    assert(api.list_assets(api.context, list, 2, &count) == -2 && count == 3 && list[0].high == 99);
    assert(api.list_assets(api.context, list, 3, &count) == 0 && count == 3 &&
           list[0].kind == NEXORA_CONTENT_ATLAS && list[1].kind == NEXORA_CONTENT_CLIP &&
           list[2].kind == NEXORA_CONTENT_SPRITE_SOURCE && list[2].high == 1);
    NexoraContentAsset description{};
    assert(api.describe_asset(api.context, 1, 2, &description) == 0 &&
           description.kind == NEXORA_CONTENT_SPRITE_SOURCE && description.count == 1 &&
           description.width == 4 && description.height == 4 && description.atlas_high == 8 &&
           description.atlas_low == 9);
    // Resolve through the actual extraction path and preserve the authored World/Y anchor.
    twod::SpriteInstance instance;
    instance.id = 1;
    instance.matrix = runtime::ToMatrix({});
    instance.sprite.source = twod::AssetId{1, 2};
    const auto resolveAtlas = [&](auto id) { return content.ResolveAtlas(id); };
    const auto resolveSource = [&](auto id) { return content.ResolveSprite(id); };
    frame = twod::ExtractSprites(std::span(&instance, 1), {}, resolveAtlas, resolveSource);
    assert(frame && frame->statistics.visible == 1 && frame->retainedAtlases.size() == 1);
    const auto texture = content.ResolveAtlas({8, 9})->texture;
    const auto oldVertex = frame->vertices.front();
    // Removing the Clip alone does not permit removal of a source's Atlas.
    assert(content.Remove({3, 4}) == 0 && content.Remove({8, 9}) == -3);
    const auto missingRegion = twod::EncodeAtlas(Atlas({8, 9}, 2));
    assert(api.register_atlas(api.context, missingRegion.data(), missingRegion.size(), &high,
                              &low) == -1);
    auto incompatible = Atlas();
    incompatible.regions[0].width = 1;
    auto incompatibleBytes = twod::EncodeAtlas(incompatible);
    assert(!incompatibleBytes.empty());
    assert(api.register_atlas(api.context, incompatibleBytes.data(), incompatibleBytes.size(),
                              &high, &low) == -1);
    assert(content.ResolveAtlas({8, 9})->texture == texture);
    auto replacement = Atlas();
    replacement.rgba[0] = std::byte{128};
    const auto newAtlas = twod::EncodeAtlas(replacement);
    assert(api.register_atlas(api.context, newAtlas.data(), newAtlas.size(), &high, &low) == 0 &&
           content.ResolveAtlas({8, 9})->texture != texture);
    source = Source();
    source.pivotY = 1;
    assert(publish(twod::EncodeSpriteAsset(source)) == 0 && retained->pivotY == 0 &&
           content.ResolveSprite({1, 2})->pivotY == 1);
    const auto next =
        twod::ExtractSprites(std::span(&instance, 1), {}, resolveAtlas, resolveSource);
    assert(next && next->vertices.front().position[1] != oldVertex.position[1] &&
           frame->retainedAtlases[0].texture == texture &&
           frame->retainedAtlases[0].atlas->rgba[0] == std::byte{255});
    int wrongThread = 0;
    bool resolverRejected = false;
    high = 55;
    low = 66;
    std::thread worker([&] {
      wrongThread = publish(bytes);
      try {
        (void)content.ResolveSprite({1, 2});
      } catch (const std::logic_error &) {
        resolverRejected = true;
      }
    });
    worker.join();
    assert(wrongThread == -3 && resolverRejected && high == 55 && low == 66);
    assert(content.Remove({1, 2}) == 0 && !content.ResolveSprite({1, 2}));
    assert(content.Remove({8, 9}) == 0 && retained->region == 1);
  }
  assert(retained && retained->pivotY == 0 && frame && frame->retainedAtlases[0].atlas);
}
void SpriteBudgets() {
  const auto atlas = Atlas();
  const auto atlasBytes = twod::EncodeAtlas(atlas);
  const auto activeBytes =
      atlas.rgba.size() + atlas.regions.size() * sizeof(twod::Region) + sizeof(twod::SpriteAsset);
  twod::GameplayContent content({2, activeBytes, activeBytes + sizeof(twod::SpriteAsset)});
  auto api = Api(content);
  auto sources = SpriteApi(content);
  std::uint64_t high{}, low{};
  assert(api.register_atlas(api.context, atlasBytes.data(), atlasBytes.size(), &high, &low) == 0);
  auto publish = [&](const twod::SpriteAsset &source) {
    const auto bytes = twod::EncodeSpriteAsset(source);
    return sources.register_sprite_source(sources.context, bytes.data(), bytes.size(), &high, &low);
  };
  assert(publish(Source()) == 0 && content.Statistics().active_bytes == activeBytes);
  assert(publish(Source({11, 12})) == -2 && !content.ResolveSprite({11, 12}));
  auto retained = content.ResolveSprite({1, 2});
  auto changed = Source();
  changed.pivotY = 1;
  assert(publish(changed) == 0 && content.Statistics().retired_bytes == sizeof(changed));
  const auto current = content.ResolveSprite({1, 2});
  changed.pivotY = 2;
  assert(publish(changed) == -2 && content.ResolveSprite({1, 2}) == current);
  // Identical publication succeeds even at the live-byte ceiling.
  assert(publish(*current) == 0 && content.ResolveSprite({1, 2}) == current);
  assert(content.CollectRetiredTextures().empty() && content.Statistics().retired_generations == 1);
  retained.reset();
  assert(content.CollectRetiredTextures().empty() && content.Statistics().retired_generations == 0);
  assert(publish(changed) == 0 && current->pivotY == 1 &&
         content.ResolveSprite({1, 2})->pivotY == 2);
}
} // namespace
int main() {
  SpriteSources();
  SpriteBudgets();
  std::optional<twod::AtlasBinding> retained;
  std::shared_ptr<const twod::Clip> retainedClip;
  {
    twod::GameplayContent content;
    auto api = Api(content);
    const auto atlasBytes = twod::EncodeAtlas(Atlas()), clipBytes = twod::EncodeClip(Clip());
    assert(!atlasBytes.empty() && !clipBytes.empty());
    std::uint64_t high = 55, low = 66;
    assert(api.register_clip(api.context, clipBytes.data(), clipBytes.size(), &high, &low) == -2 &&
           high == 55 && low == 66);
    assert(api.register_atlas(api.context, nullptr, 1, &high, &low) == -1);
    assert(api.register_atlas(api.context, atlasBytes.data(), UINT64_MAX, &high, &low) == -1);
    assert(api.register_atlas(api.context, atlasBytes.data(), atlasBytes.size(), &high, &high) ==
           -1);
    assert(api.register_atlas(api.context, atlasBytes.data(), atlasBytes.size() - 1, &high, &low) ==
           -1);
    auto trailing = atlasBytes;
    trailing.push_back(std::byte{0});
    assert(api.register_atlas(api.context, trailing.data(), trailing.size(), &high, &low) == -1);
    assert(high == 55 && low == 66 && !content.ResolveAtlas({8, 9}));
    assert(api.register_atlas(api.context, atlasBytes.data(), atlasBytes.size(), &high, &low) ==
               0 &&
           high == 8 && low == 9);
    retained = content.ResolveAtlas({high, low});
    assert(retained && retained->texture >= (UINT64_C(1) << 62));
    assert(api.register_atlas(api.context, atlasBytes.data(), atlasBytes.size(), &high, &low) == 0);
    assert(content.ResolveAtlas({8, 9})->texture == retained->texture);
    auto badClip = Clip();
    badClip.frames[0].region = 2;
    auto badClipBytes = twod::EncodeClip(badClip);
    assert(api.register_clip(api.context, badClipBytes.data(), badClipBytes.size(), &high, &low) ==
               -1 &&
           high == 8 && low == 9);
    assert(api.register_clip(api.context, clipBytes.data(), clipBytes.size(), &high, &low) == 0 &&
           high == 3 && low == 4);
    retainedClip = content.ResolveClip({3, 4});
    assert(retainedClip && retainedClip->frames[0].event == "frame");
    // UUIDs have one type; a loaded clip cannot silently become an atlas.
    const auto conflict = twod::EncodeAtlas(Atlas({3, 4}));
    assert(api.register_atlas(api.context, conflict.data(), conflict.size(), &high, &low) == -1);
    // Replacing an atlas cannot invalidate any registered animation frame's region.
    auto missingRegion = twod::EncodeAtlas(Atlas({8, 9}, 2));
    assert(api.register_atlas(api.context, missingRegion.data(), missingRegion.size(), &high,
                              &low) == -1);
    assert(content.ResolveAtlas({8, 9})->texture == retained->texture);
    auto replacement = Atlas();
    replacement.rgba[0] = std::byte{128};
    const auto changed = twod::EncodeAtlas(replacement);
    assert(api.register_atlas(api.context, changed.data(), changed.size(), &high, &low) == 0);
    assert(content.ResolveAtlas({8, 9})->texture != retained->texture);
    assert(retained->atlas->rgba[0] == std::byte{255});
    std::uint32_t count = 99;
    assert(api.list_assets(api.context, nullptr, 0, &count) == 0 && count == 2);
    NexoraContentAsset list[2]{};
    list[0].high = 99;
    assert(api.list_assets(api.context, list, 1, &count) == -2 && count == 2 && list[0].high == 99);
    assert(api.list_assets(api.context, list, 2, &count) == 0 && count == 2);
    assert(list[0].kind == NEXORA_CONTENT_ATLAS && list[0].high == 8 && list[0].count == 1);
    assert(list[1].kind == NEXORA_CONTENT_CLIP && list[1].high == 3 && list[1].atlas_low == 9);
    NexoraContentAsset description{};
    description.high = 99;
    assert(api.describe_asset(api.context, 99, 99, &description) == -2 && description.high == 99);
    assert(api.describe_asset(api.context, 3, 4, &description) == 0 && description.count == 1);
    assert(!content.ResolveClip({8, 9}) && !content.ResolveAtlas({3, 4}));
    int wrongThread = 0;
    std::thread worker([&] { wrongThread = api.list_assets(api.context, nullptr, 0, &count); });
    worker.join();
    assert(wrongThread == -3);
    // Registry count budget rejects a new identity atomically.
    for (std::uint64_t i = 0; i < 254; ++i) {
      const auto bytes = twod::EncodeAtlas(Atlas({100, i + 1}));
      assert(api.register_atlas(api.context, bytes.data(), bytes.size(), &high, &low) == 0);
    }
    auto overflow = twod::EncodeAtlas(Atlas({101, 1}));
    high = 55;
    low = 66;
    assert(api.register_atlas(api.context, overflow.data(), overflow.size(), &high, &low) == -2 &&
           high == 55 && low == 66);
    assert(api.list_assets(api.context, nullptr, 0, &count) == 0 && count == 256);
  }
  assert(retained->atlas->id == twod::AssetId({8, 9}));
  assert(retainedClip->atlas == twod::AssetId({8, 9}));
}
