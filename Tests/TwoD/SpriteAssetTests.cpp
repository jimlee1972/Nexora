#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/Lifecycle.h"
#include "Nexora/TwoD/Scene.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace twod;
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Near(double a, double b) { Check(std::abs(a - b) < 1e-5, "independent geometry oracle"); }
SpriteAsset Source() { return {{11, 12}, {1, 2}, 7, 64, 64, 16, 16, 32, 48, 32, 0, 32, {}}; }
Atlas AtlasFixture(std::uint32_t width = 32, std::uint32_t height = 48) {
  Image image{7, width, height, Bytes(std::size_t(width) * height * 4, std::byte{255})};
  auto atlas = PackAtlas({1, 2}, std::span(&image, 1), 64, 128);
  Check(bool(atlas), "fixture atlas");
  return *atlas;
}
SpriteInstance Instance() {
  SpriteInstance instance{37, {}, runtime::ToMatrix({})};
  instance.sprite.sortY = true;
  instance.sprite.order = -3;
  return instance;
}
void Metadata() {
  const auto source = Source();
  const auto bytes = EncodeSpriteAsset(source);
  Check(bytes.size() == 96 && DecodeSpriteAsset(bytes) == source, "Sprite source roundtrip");
  for (std::size_t n = 0; n < bytes.size(); ++n)
    Check(!DecodeSpriteAsset(std::span(bytes).first(n)), "truncated Sprite source");
  auto badBytes = bytes;
  badBytes.push_back(std::byte{0});
  Check(!DecodeSpriteAsset(badBytes), "trailing bytes");
  badBytes = bytes;
  badBytes[4] = std::byte{2};
  Check(!DecodeSpriteAsset(badBytes), "unsupported source version");
  badBytes = bytes;
  badBytes[0] = std::byte{0};
  Check(!DecodeSpriteAsset(badBytes), "wrong source magic");
  for (float ppu :
       {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::denorm_min()}) {
    auto bad = source;
    bad.pixelsPerUnit = ppu;
    Check(!ValidateSpriteAsset(bad) && EncodeSpriteAsset(bad).empty(), "invalid PPU");
  }
  auto bad = source;
  bad.id = bad.atlas;
  Check(!ValidateSpriteAsset(bad), "self dependency");
  bad = source;
  bad.trimX = UINT32_MAX;
  Check(!ValidateSpriteAsset(bad), "trim coordinate overflow");
  bad = source;
  bad.trimWidth = UINT32_MAX;
  Check(!ValidateSpriteAsset(bad), "trim extent overflow");
  bad = source;
  bad.pivotX = std::numeric_limits<float>::quiet_NaN();
  Check(!ValidateSpriteAsset(bad), "nonfinite Pivot");
  bad = source;
  bad.pivotY = 24;
  bad.pixelsPerUnit = 0.000032F;
  Check(!ValidateSpriteAsset(bad), "geometry exceeds the existing Sprite size budget");
  bad = source;
  bad.border = {UINT32_MAX, UINT32_MAX, 0, 0};
  Check(!ValidateSpriteAsset(bad), "border sum overflow");
  bad = source;
  bad.border = {1, 1, 1, 1};
  Check(!ValidateSpriteAsset(bad), "bordered source must be untrimmed");
  bad.trimX = bad.trimY = 0;
  bad.trimWidth = bad.sourceWidth;
  bad.trimHeight = bad.sourceHeight;
  Check(ValidateSpriteAsset(bad), "valid untrimmed borders");
}
void Geometry() {
  auto atlas = std::make_shared<Atlas>(AtlasFixture());
  const auto source = Source();
  auto original = Instance();
  original.matrix[12] = 3;
  original.matrix[13] = 5;
  auto resolved = ResolveSpriteAsset(source, *atlas, original);
  Check(resolved && resolved->localQuad && resolved->matrix == original.matrix &&
            resolved->sprite.order == -3 && resolved->sprite.sortY,
        "source geometry must preserve world pose and sort anchor");
  Near(resolved->localQuad->minimum.x, -0.5);
  Near(resolved->localQuad->maximum.x, 0.5);
  Near(resolved->localQuad->minimum.y, 0);
  Near(resolved->localQuad->maximum.y, 1.5);
  Check(!original.localQuad && original.sprite.atlas == AssetId{}, "input mutation");
  auto frame = ExtractSprites(std::span(&*resolved, 1), {{}, 20, 0, 200, 200},
                              [&](auto) { return std::optional(AtlasBinding{atlas, 19}); });
  Check(frame && frame->statistics.visible == 1 && frame->vertices.size() == 4,
        "source geometry reaches existing extraction");
  Near(frame->vertices[0].position[0], 125);
  Near(frame->vertices[0].position[1], 50);
  Near(frame->vertices[2].position[0], 135);
  Near(frame->vertices[2].position[1], 35);

  auto next = source;
  next.trimX = 8;
  next.trimY = 8;
  next.trimWidth = 48;
  next.trimHeight = 56;
  auto nextAtlas = AtlasFixture(48, 56);
  auto nextFrame = ResolveSpriteAsset(next, nextAtlas, original);
  Check(bool(nextFrame), "differently trimmed next frame");
  Near(nextFrame->localQuad->minimum.y, 0);
  Near(nextFrame->localQuad->minimum.x, -0.75);
  Near(nextFrame->localQuad->maximum.y, 1.75);
  Check(nextFrame->matrix == resolved->matrix, "animation trim changed sort anchor");

  auto noncentral = source;
  noncentral.pivotX = 8;
  auto flipped = original;
  flipped.sprite.flipX = true;
  flipped.sprite.flipY = true;
  auto mirrored = ResolveSpriteAsset(noncentral, *atlas, flipped);
  Check(mirrored && mirrored->sprite.flipX && mirrored->sprite.flipY, "asset mirror flags");
  Near(mirrored->localQuad->minimum.x, -1.25);
  Near(mirrored->localQuad->maximum.x, -0.25);
  Near(mirrored->localQuad->minimum.y, -1.5);
  Near(mirrored->localQuad->maximum.y, 0);
  mirrored->matrix[0] = -1;
  auto negative = ExtractSprites(std::span(&*mirrored, 1), {{}, 20, 0, 200, 200},
                                 [&](auto) { return std::optional(AtlasBinding{atlas, 20}); });
  Check(negative && negative->vertices.size() == 4, "negative scale and Flip extraction");
  Near(negative->vertices[0].position[0], 142.5);
  Check(negative->vertices[0].uv[0] > negative->vertices[1].uv[0], "Flip UV orientation");

  auto invalid = *resolved;
  invalid.localQuad = SpriteLocalQuad{{0, 0}, {0, 1}};
  bool called = false;
  Check(!ExtractSprites(std::span(&invalid, 1), {},
                        [&](auto) -> std::optional<AtlasBinding> {
                          called = true;
                          return {};
                        }) &&
            !called,
        "invalid geometry should reject before resolver");
  Check(!ResolveSpriteAsset(source, nextAtlas, original), "mismatched region dimensions");
  auto wrong = *atlas;
  wrong.id = {8, 9};
  Check(!ResolveSpriteAsset(source, wrong, original), "wrong Atlas UUID");
  auto missing = source;
  missing.region = 2;
  Check(!ResolveSpriteAsset(missing, *atlas, original), "missing region");

  // A repack changes UVs/texture generation, not source identity or geometry.
  Image extra{1, 1, 1, Bytes(4, std::byte{128})};
  Image actor{7, 32, 48, Bytes(32 * 48 * 4, std::byte{255})};
  auto repacked = PackAtlas(source.atlas, std::array{extra, actor}, 64, 128);
  auto refreshed = ResolveSpriteAsset(source, *repacked, original);
  Check(refreshed && repacked->Find(7)->x != atlas->Find(7)->x &&
            EncodeSpriteAsset(source) ==
                EncodeSpriteAsset(*DecodeSpriteAsset(EncodeSpriteAsset(source))),
        "stable source survives atlas repack");
  Near(refreshed->localQuad->minimum.x, resolved->localQuad->minimum.x);
}
void Cook() {
  runtime::ImporterRegistry registry;
  Check(RegisterImporters(registry), "Sprite importer registration");
  const auto source = Source();
  auto canonical =
      registry.Import({source.id, "sprite_source", "actor.nx2sprite", EncodeSpriteAsset(source)});
  Check(canonical && canonical->dependencies == std::vector<AssetId>{source.atlas},
        "source retains Atlas dependency");
  Check(
      !registry.Import({{999, 999}, "sprite_source", "actor.nx2sprite", EncodeSpriteAsset(source)}),
      "embedded identity mismatch");
  runtime::DerivedDataCache cache;
  runtime::AssetCooker cooker;
  auto cooked = cooker.Cook(*canonical, "linux", "sprite-source-v1", cache);
  auto repeated = cooker.Cook(*canonical, "linux", "sprite-source-v1", cache);
  Check(cooked && repeated &&
            runtime::AssetCooker::Serialize(*cooked) == runtime::AssetCooker::Serialize(*repeated),
        "deterministic Sprite cook");
  auto atlas = AtlasFixture();
  auto atlasSource =
      registry.Import({atlas.id, "sprite_atlas", "atlas.nx2atlas", EncodeAtlas(atlas)});
  auto atlasCook = cooker.Cook(*atlasSource, "linux", "sprite-source-v1", cache);
  auto bundle = runtime::BundleBuilder::Build("source", 1, {}, {*cooked, *atlasCook});
  runtime::AssetGenerationStore store;
  Check(bundle && store.Stage({*bundle}) && store.ActivateStaged(),
        "Sprite dependency publication");
  auto resident = store.Load(source.id);
  Check(resident && DecodeSpriteAsset(resident->payload) == source, "cooked Sprite decode");
  store.Release(source.id);
  runtime::ImporterRegistry conflict;
  Check(
      conflict.Register(".nx2sprite",
                        [](const auto &) -> std::optional<runtime::CanonicalAsset> { return {}; }),
      "preexisting source importer");
  Check(!RegisterImporters(conflict) &&
            !conflict.Import({atlas.id, "sprite_atlas", "atlas.nx2atlas", EncodeAtlas(atlas)}),
        "failed importer registration publishes nothing");
}
void SourceBindings() {
  auto source = std::make_shared<SpriteAsset>(Source());
  auto atlas = std::make_shared<Atlas>(AtlasFixture());
  Sprite style;
  style.source = source->id;
  style.sortY = true;
  Check(ValidateSprite(style), "source-only Sprite does not require obsolete Atlas fields");
  runtime::World world;
  const auto sceneId = world.LoadScene("source bindings");
  const auto entity = world.CreateEntity(sceneId).id;
  const auto other = world.CreateEntity(sceneId).id;
  SpriteScene scene(world, sceneId);
  Sprite legacy;
  legacy.atlas = atlas->id;
  legacy.region = 7;
  Check(scene.Set(entity, legacy) && scene.Set(other, legacy), "legacy scene setup");
  const auto old = scene.Snapshot();
  Check(old && (*old)[4] == std::byte{2}, "legacy scene still emits schema 2");
  Check(scene.Set(entity, style), "persistent source binding");
  const auto bytes = scene.Snapshot();
  Check(bytes && (*bytes)[4] == std::byte{3} && *bytes != *old, "source scene emits schema 3");
  runtime::World isolated;
  auto loaded = SpriteScene::Load(isolated, *bytes);
  Check(loaded && loaded->Find(entity)->source == source->id &&
            loaded->Find(other)->source == AssetId{} && loaded->Snapshot() == bytes,
        "mixed source/legacy roundtrip in isolated World");
  for (std::size_t n = 0; n < bytes->size(); ++n)
    Check(!scene.Restore(std::span(*bytes).first(n)) && scene.Snapshot() == bytes,
          "truncated source snapshot restores atomically");
  auto trailing = *bytes;
  trailing.push_back(std::byte{0});
  Check(!scene.Restore(trailing) && scene.Snapshot() == bytes, "trailing source scene bytes");
  auto unknown = *bytes;
  unknown[4] = std::byte{4};
  Check(!scene.Restore(unknown) && scene.Snapshot() == bytes, "unsupported scene version");
  Check(scene.Restore(*old) && scene.Snapshot() == old && scene.Find(entity)->source == AssetId{},
        "backward reader and source removal on legacy restore");
  Check(scene.Set(entity, style), "restore source after legacy read");

  std::size_t sourceCalls{}, atlasCalls{};
  SpriteAssetResolver sources = [&](AssetId id) -> std::shared_ptr<const SpriteAsset> {
    ++sourceCalls;
    Check(id == source->id, "source resolver identity");
    return source;
  };
  AtlasResolver atlases = [&](AssetId id) -> std::optional<AtlasBinding> {
    ++atlasCalls;
    Check(id == atlas->id, "source selected Atlas identity");
    return AtlasBinding{atlas, 23};
  };
  Camera camera{{}, 20, 0, 200, 200};
  auto frame = loaded->Extract(camera, atlases, sources);
  Check(frame && frame->statistics.visible == 2 && sourceCalls == 1 && atlasCalls == 1,
        "mixed scene source/legacy share one Atlas resolution");
  Near(frame->vertices[0].position[0], 95);
  Near(frame->vertices[0].position[1], 100);
  Near(frame->vertices[2].position[1], 85);
  auto missing = loaded->Extract(camera, atlases);
  Check(missing && missing->statistics.visible == 1 && missing->statistics.unavailable == 1,
        "old overload reports missing source instead of legacy fallback");

  InstancePool pool(2);
  const twod::Instance value{{}, style};
  auto handles = pool.Create(std::array{value, value});
  Check(handles && pool.Size() == 2, "pool accepts source bindings");
  std::vector<SpriteInstance> inputs;
  pool.ForEach([&](auto &v) {
    inputs.push_back({inputs.size() + 1, v.sprite, runtime::ToMatrix(v.transform)});
  });
  inputs[1].sprite.flipX = true;
  inputs[1].matrix[0] = -1;
  sourceCalls = atlasCalls = 0;
  auto pooled = ExtractSprites(inputs, camera, atlases, sources);
  Check(pooled && pooled->statistics.visible == 2 && sourceCalls == 1 && atlasCalls == 1,
        "pool inputs cache source and Atlas once per extraction");
  Near(pooled->vertices[4].position[0], 105);
  Check(pooled->vertices[4].uv[0] > pooled->vertices[5].uv[0], "pooled source flips UVs");
  auto unavailable = ExtractSprites(inputs, camera, atlases, [&](auto) {
    ++sourceCalls;
    return std::shared_ptr<const SpriteAsset>{};
  });
  Check(unavailable && unavailable->statistics.unavailable == 2 && sourceCalls == 2,
        "negative source result cached within one frame");
  auto unavailableAtlas = ExtractSprites(
      inputs, camera, [&](auto) -> std::optional<AtlasBinding> { return {}; }, sources);
  Check(unavailableAtlas && unavailableAtlas->statistics.unavailable == 2,
        "missing source Atlas is unavailable");

  const auto assertRejected = [&](const std::shared_ptr<SpriteAsset> &bad) {
    Check(!ExtractSprites(inputs, camera, atlases, [bad](auto) { return bad; }),
          "invalid source closure rejects entire frame");
  };
  auto bad = std::make_shared<SpriteAsset>(*source);
  bad->id = {999, 999};
  assertRejected(bad);
  bad = std::make_shared<SpriteAsset>(*source);
  bad->pixelsPerUnit = 0;
  assertRejected(bad);
  bad = std::make_shared<SpriteAsset>(*source);
  bad->region = 999;
  assertRejected(bad);
  bad = std::make_shared<SpriteAsset>(*source);
  bad->trimWidth = 31;
  assertRejected(bad);
  auto invalidInputs = inputs;
  invalidInputs[1].id = invalidInputs[0].id;
  sourceCalls = atlasCalls = 0;
  Check(!ExtractSprites(invalidInputs, camera, atlases, sources) && !sourceCalls && !atlasCalls,
        "all draw identities validated before any source resolver");
  invalidInputs = inputs;
  invalidInputs[1].localQuad = SpriteLocalQuad{{0, 0}, {1, 1}};
  Check(!ExtractSprites(invalidInputs, camera, atlases, sources) && !sourceCalls && !atlasCalls,
        "ambiguous source and explicit quad reject before resolution");
  inputs[1].sprite.visible = false;
  inputs[0].sprite.color[3] = 0;
  auto hidden = ExtractSprites(inputs, camera, atlases, sources);
  Check(hidden && !hidden->statistics.visible && !sourceCalls && !atlasCalls,
        "hidden sources perform no asset resolution");
  inputs[0].sprite.color[3] = 1;
  inputs[1].sprite.visible = true;

  // Same UUID, new immutable source/Atlas generation: new geometry is not cached across frames.
  auto retained = frame->retainedAtlases[0].atlas;
  source = std::make_shared<SpriteAsset>(*source);
  source->trimX = 8;
  source->trimY = 8;
  source->trimWidth = 48;
  source->trimHeight = 56;
  atlas = std::make_shared<Atlas>(AtlasFixture(48, 56));
  auto refreshed = ExtractSprites(inputs, camera, atlases, sources);
  Check(refreshed && retained != atlas && frame->retainedAtlases[0].atlas == retained &&
            retained->Find(7)->width == 32,
        "old extracted frame retains old Atlas after generation replacement");
  Near(refreshed->vertices[0].position[0], 92.5);
  Near(refreshed->vertices[2].position[1], 82.5);
  Check(scene.Find(entity)->source == source->id && !pool.Find((*handles)[0])->sprite.region,
        "resolution does not mutate scene or pool authoring metadata");

  GameplayBridge bridge(world, scene, {}, nullptr, &pool);
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  Check(bridge.GetApi(&api) == NEXORA_GAMEPLAY_OK, "legacy API setup");
  NexoraTwoDSprite output{};
  output.region = 1234;
  Check(api.get_sprite(api.context, entity, &output) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
            output.region == 1234,
        "legacy getter cannot silently discard source identity");
  Check(api.play_animation(api.context, entity, 1, 2, 1) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
        "legacy region animation cannot override source-bound geometry");
  const NexoraTwoDInstanceHandle handle{(*handles)[0].owner, (*handles)[0].index,
                                        (*handles)[0].generation};
  NexoraTwoDInstance instanceOutput{};
  instanceOutput.sprite.region = 5678;
  Check(api.read_instances(api.context, &handle, 1, &instanceOutput) ==
                NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
            instanceOutput.sprite.region == 5678,
        "legacy pool read preserves outputs on unrepresentable source");
  Check(api.get_sprite(api.context, other, &output) == NEXORA_GAMEPLAY_OK && output.region == 7,
        "legacy API still reads ordinary Sprites");
}
} // namespace
int main() {
  try {
    Metadata();
    Geometry();
    Cook();
    SourceBindings();
    std::cout << "Sprite source metadata, trim/Pivot/flip extraction and cook passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
