#include "Nexora/Editor/TwoDPlayAssets.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace nexora;
namespace {
void Check(bool value, const char *reason) {
  if (!value)
    throw std::runtime_error(reason);
}
constexpr runtime::AssetUuid atlasId{17, 23}, clipId{19, 29}, otherId{31, 37};
twod::Atlas Atlas(std::byte color = std::byte{255}) {
  twod::Image image{1, 1, 1, {color, std::byte{0}, std::byte{0}, std::byte{255}}};
  auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
  Check(bool(atlas), "pack fixture");
  return *atlas;
}
runtime::CanonicalAsset AtlasSource(std::byte color = std::byte{255}) {
  runtime::ImporterRegistry registry;
  Check(twod::RegisterImporters(registry), "register importers");
  auto canonical =
      registry.Import({atlasId, ".nx2atlas", "atlas.nx2atlas", twod::EncodeAtlas(Atlas(color))});
  Check(bool(canonical), "atlas importer");
  return *canonical;
}
runtime::CanonicalAsset ClipSource() {
  runtime::ImporterRegistry registry;
  Check(twod::RegisterImporters(registry), "register clip importer");
  auto canonical =
      registry.Import({clipId, ".nx2clip", "clip.nx2clip",
                       twod::EncodeClip({clipId, atlasId, true, {{1, 0.1, "frozen"}}})});
  Check(bool(canonical), "clip importer");
  return *canonical;
}
void Activate(runtime::AssetGenerationStore &store, std::uint64_t generation,
              const std::vector<runtime::CanonicalAsset> &sources) {
  runtime::AssetCooker cooker;
  runtime::DerivedDataCache cache;
  std::vector<runtime::RuntimeBlob> blobs;
  for (const auto &source : sources) {
    auto blob = cooker.Cook(source, "linux", "play", cache);
    Check(bool(blob), "cook fixture");
    blobs.push_back(std::move(*blob));
  }
  auto bundle = runtime::BundleBuilder::Build("2D Play", generation, {}, blobs);
  Check(bool(bundle), "bundle fixture");
  Check(store.Stage({*bundle}) && store.ActivateStaged(), "activate fixture");
}
void Isolation() {
  std::shared_ptr<const editor::TwoDPlayAssets> frozen;
  std::shared_ptr<const twod::Atlas> retained;
  {
    runtime::AssetGenerationStore store;
    Activate(store, 1, {ClipSource(), AtlasSource()});
    const std::array roots{clipId};
    std::string error = "old";
    frozen = editor::TwoDPlayAssets::Capture(store, 1, roots, &error);
    Check(frozen && error.empty() && frozen->Size() == 2 && frozen->Generation() == 1,
          "capture clip closure");
    Check(frozen->PayloadBytes() == ClipSource().payload.size() + AtlasSource().payload.size(),
          "unique payload count");
    Check(!store.IsPinned(1) && store.ResidentBytes() == 0, "capture leaks residency or pin");
    Check(!frozen->Atlas(clipId) && !frozen->Clip(atlasId) && !frozen->Clip(otherId),
          "lookup type or identity collision");
    retained = frozen->Atlas(atlasId);
    Activate(store, 2, {ClipSource(), AtlasSource(std::byte{7})});
    auto replacement = editor::TwoDPlayAssets::Capture(store, 2, roots);
    Check(replacement && replacement->Atlas(atlasId)->rgba != retained->rgba,
          "new generation retains old pixels");
    Check(frozen->Atlas(atlasId) == retained && frozen->Clip(clipId)->frames[0].event == "frozen",
          "replacement mutates frozen owner");
    Activate(store, 3, {ClipSource(), AtlasSource(std::byte{9})});
    Check(!editor::TwoDPlayAssets::Capture(store, 1, roots), "collected generation resurrects");
    Check(frozen->Atlas(atlasId) == retained && store.ResidentBytes() == 0,
          "collection invalidates snapshot");
    Check(store.Rollback() && store.ActiveGeneration() == 2 &&
              replacement->Atlas(atlasId)->rgba != retained->rgba,
          "rollback invalidates independent snapshots");
  }
  Check(frozen->Clip(clipId)->atlas == atlasId && retained->Find(1), "store destruction borrow");
  runtime::World world(runtime::WorldKind::Play);
  const auto sceneId = world.LoadScene("Frozen 2D Play");
  const auto entity = world.CreateEntity(sceneId).id;
  twod::SpriteScene scene(world, sceneId);
  twod::GameplayBridge bridge(world, scene, [frozen](auto id) { return frozen->Clip(id); });
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  Check(bridge.GetApi(&api) == 0, "frozen resolver bridge");
  NexoraTwoDSprite sprite{atlasId.high, atlasId.low,  1, 1, {1, 1},
                          {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0};
  Check(api.set_sprite(api.context, entity, &sprite) == 0, "bridge sprite");
  Check(api.play_animation(api.context, entity, clipId.high, clipId.low, 1) == 0,
        "bridge consumes frozen clip after store destruction");
  frozen.reset();
  Check(retained->Find(1), "atlas owner borrows snapshot");
}
void BudgetsAndReferences() {
  runtime::AssetGenerationStore store;
  Activate(store, 10, {ClipSource(), AtlasSource()});
  const std::array roots{clipId};
  const auto bytes = ClipSource().payload.size() + AtlasSource().payload.size();
  Check(bool(editor::TwoDPlayAssets::Capture(store, 10, roots, nullptr, {2, bytes})),
        "exact limits reject");
  Check(store.Pin(10), "external pin");
  Check(store.Load(atlasId, 10) && store.Load(clipId, 10), "external loads");
  const auto baseline = store.ResidentBytes();
  const auto reject = [&](auto selected, std::uint64_t generation,
                          editor::TwoDPlayAssetLimits limits) {
    std::string error;
    Check(!editor::TwoDPlayAssets::Capture(store, generation, selected, &error, limits) &&
              !error.empty(),
          "invalid capture succeeds or lacks diagnostic");
    Check(store.ResidentBytes() == baseline && store.IsPinned(10), "failed capture steals owners");
  };
  reject(std::span(roots), 0, {});
  reject(std::span(roots), 999, {});
  reject(std::span(roots), 10, {1, bytes});
  reject(std::span(roots), 10, {2, bytes - 1});
  reject(std::span(roots), 10, {0, bytes});
  reject(std::span(roots), 10, {4097, bytes});
  reject(std::span(roots), 10, {2, 64 * 1024 * 1024 + 1});
  const std::array duplicate{clipId, clipId};
  const std::array zero{runtime::AssetUuid{}};
  const std::array missing{otherId};
  reject(std::span(duplicate), 10, {});
  reject(std::span(zero), 10, {});
  reject(std::span(missing), 10, {});
  const std::array explicitClosure{atlasId, clipId};
  auto success = editor::TwoDPlayAssets::Capture(store, 10, explicitClosure);
  Check(success && success->Size() == 2 && store.ResidentBytes() == baseline && store.IsPinned(10),
        "successful capture steals owners or double counts dependency");
  store.Release(clipId, 10);
  store.Release(atlasId, 10);
  Check(store.ResidentBytes() == 0 && store.Unpin(10) && !store.IsPinned(10),
        "temporary load or pin leak");
  Check(bool(editor::TwoDPlayAssets::Capture(store, 10, {})), "empty closure rejects");
}
void InvalidAssets() {
  const auto reject = [](std::vector<runtime::CanonicalAsset> sources) {
    runtime::AssetGenerationStore store;
    Activate(store, 5, sources);
    const std::array roots{clipId};
    std::string error;
    Check(!editor::TwoDPlayAssets::Capture(store, 5, roots, &error) && !error.empty(),
          "malformed asset capture succeeds");
    Check(store.ResidentBytes() == 0 && !store.IsPinned(5),
          "malformed asset leaks store ownership");
  };
  reject({ClipSource()});
  auto clip = ClipSource();
  clip.dependencies.clear();
  reject({clip, AtlasSource()});
  clip = ClipSource();
  clip.dependencies = {otherId};
  reject({clip, AtlasSource()});
  clip.dependencies = {atlasId, atlasId};
  reject({clip, AtlasSource()});
  clip = ClipSource();
  clip.type = "unsupported";
  reject({clip, AtlasSource()});
  clip = ClipSource();
  clip.payload.push_back(std::byte{0});
  reject({clip, AtlasSource()});
  clip = ClipSource();
  clip.payload = twod::EncodeClip({otherId, atlasId, true, {{1, 0.1, {}}}});
  reject({clip, AtlasSource()});
  clip.payload = twod::EncodeClip({clipId, atlasId, true, {{99, 0.1, {}}}});
  reject({clip, AtlasSource()});
  auto atlas = AtlasSource();
  atlas.dependencies = {otherId};
  reject({ClipSource(), atlas});
  atlas = AtlasSource();
  atlas.payload.resize(10);
  reject({ClipSource(), atlas});
  atlas = AtlasSource();
  auto wrong = Atlas();
  wrong.id = otherId;
  atlas.payload = twod::EncodeAtlas(wrong);
  reject({ClipSource(), atlas});
  atlas = {atlasId,
           "sprite_clip",
           {atlasId},
           twod::EncodeClip({atlasId, atlasId, true, {{1, 0.1, {}}}})};
  reject({ClipSource(), atlas});
}
void MaximumClosure() {
  runtime::AssetGenerationStore store;
  std::vector<runtime::CanonicalAsset> sources{AtlasSource()};
  std::vector<runtime::AssetUuid> roots;
  for (std::uint64_t index = 0; index < 4096; ++index) {
    const runtime::AssetUuid id{101, index + 1};
    roots.push_back(id);
    sources.push_back(
        {id, "sprite_clip", {atlasId}, twod::EncodeClip({id, atlasId, true, {{1, 0.1, {}}}})});
  }
  Activate(store, 42, sources);
  auto exact = editor::TwoDPlayAssets::Capture(store, 42, std::span(roots).first(4095));
  Check(exact && exact->Size() == 4096, "4096 asset closure rejects");
  Check(!editor::TwoDPlayAssets::Capture(store, 42, roots), "4097 asset closure succeeds");
  Check(store.ResidentBytes() == 0 && !store.IsPinned(42), "maximum closure leaks store owners");
}
} // namespace
int main() {
  try {
    Isolation();
    BudgetsAndReferences();
    InvalidAssets();
    MaximumClosure();
    std::cout
        << "2D cooked Play assets: generation isolation, closure, budgets and residency passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
