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
} // namespace
int main() {
  try {
    Metadata();
    Geometry();
    Cook();
    std::cout << "Sprite source metadata, trim/Pivot/flip extraction and cook passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
