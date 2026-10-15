#include "Nexora/TwoD/GameplayContent.h"
#include "nexora/nexora.h"
#include <cassert>
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
} // namespace
int main() {
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
