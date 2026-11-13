#include "Nexora/Editor/TwoDPlayContext.h"
#include "Nexora/TwoD/AuthoredScene.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace nexora;
namespace {
constexpr twod::AssetId atlasId{71, 72}, sourceId{81, 82};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
twod::Atlas Atlas() {
  const twod::Image image{
      1, 1, 1, {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4, 4, true);
  Check(bool(atlas), "Atlas fixture");
  return *atlas;
}
twod::SpriteAsset Source(float pivot = 2) {
  twod::SpriteAsset source;
  source.id = sourceId;
  source.atlas = atlasId;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = source.trimWidth = source.trimHeight = 1;
  source.pivotX = pivot;
  source.pivotY = 2;
  source.pixelsPerUnit = 1;
  return source;
}
std::array<runtime::Id, 2> Populate(editor::TwoDDocument &doc) {
  Check(doc.ReplaceAtlas(Atlas()) && doc.ReplaceSpriteSource(Source()), "source assets");
  twod::Sprite sprite;
  sprite.source = sourceId;
  sprite.color = {1, 0, 0, 1};
  runtime::Transform pose;
  pose.x = 1;
  const auto red = doc.AddSprite(sprite, pose);
  sprite.color = {0, 0, 1, 1};
  pose.x = -1;
  pose.sx = -1;
  const auto blue = doc.AddSprite(sprite, pose);
  Check(red && blue, "source binding");
  return {*red, *blue};
}
void Run() {
  editor::TwoDDocument legacy;
  Check(legacy.ReplaceAtlas(Atlas()), "legacy Atlas");
  twod::Sprite plain;
  plain.atlas = atlasId;
  plain.region = 1;
  Check(bool(legacy.AddSprite(plain)), "legacy sprite");
  const auto v1 = legacy.Save();
  Check(v1[4] == std::byte{1} &&
            twod::EncodeAuthoredScene(legacy.Scene(), legacy.Atlas(), legacy.Clip(),
                                      legacy.Tiles()) == v1 &&
            twod::EncodeAuthoredScene(legacy.Scene(), legacy.Atlas(), legacy.Clip(), legacy.Tiles(),
                                      {}) == v1,
        "source-free v1 bytes changed");
  editor::TwoDDocument doc;
  const auto ids = Populate(doc);
  doc.MarkSaved();
  const auto original = doc.Save();
  Check(original[4] == std::byte{2} && !doc.Dirty(), "source schema");
  const auto decoded = twod::AuthoredScene::Decode(original);
  Check(
      decoded && decoded->sources == std::vector{Source()} &&
          decoded->scene->Find(ids[0])->source == sourceId &&
          twod::EncodeAuthoredScene(*decoded->scene, decoded->atlas, decoded->clip, decoded->tiles,
                                    decoded->sources) == original &&
          twod::EncodeAuthoredScene(*decoded->scene, decoded->atlas, decoded->clip, decoded->tiles)
              .empty(),
      "source canonical round trip or source-less rejection");
  auto retained = doc.SpriteSource(sourceId);
  Check(doc.ReplaceSpriteSource(Source(3)) && doc.Dirty() && retained->pivotX == 2 &&
            doc.SpriteSource(sourceId)->pivotX == 3,
        "source replacement ownership");
  const auto replaced = doc.Save();
  Check(doc.Undo() && doc.Save() == original && !doc.Dirty() && doc.Redo() &&
            doc.Save() == replaced,
        "source metadata Undo/Redo");
  const auto revision = doc.Revision();
  auto invalid = Source();
  invalid.trimWidth = 2;
  Check(!doc.ReplaceSpriteSource(invalid) && !doc.RemoveSpriteSource(sourceId) &&
            doc.Save() == replaced && doc.Revision() == revision,
        "invalid/referenced source publication");
  auto unknown = *doc.Scene().Find(ids[0]);
  unknown.source = twod::AssetId{90, 91};
  Check(!doc.EditSprite(ids[0], unknown, {}) && doc.Save() == replaced,
        "unknown source assignment");
  editor::TwoDDocument reopened;
  Check(reopened.Reopen(original) && reopened.Save() == original && !reopened.Dirty() &&
            !reopened.Undo(),
        "source reopen/history baseline");
  for (std::size_t i = 0; i < original.size(); ++i)
    Check(!twod::AuthoredScene::Decode(std::span(original).first(i)), "truncated source document");
  auto corrupt = original;
  corrupt.push_back(std::byte{0});
  Check(!reopened.Reopen(corrupt) && reopened.Save() == original, "trailing source document");
  // The catalog's trailing framing is count + length + the unchanged 96-byte source wire.
  const auto table = original.size() - 104;
  for (const auto count : {0U, 4097U}) {
    corrupt = original;
    for (unsigned i = 0; i < 4; ++i)
      corrupt[table + i] = std::byte((count >> (i * 8)) & 255);
    Check(!reopened.Reopen(corrupt) && reopened.Save() == original, "source catalog count bound");
  }
  corrupt = original;
  corrupt[table] = std::byte{2};
  corrupt.insert(corrupt.end(), original.end() - 100, original.end());
  Check(!reopened.Reopen(corrupt), "duplicate source catalog identity");
  for (const auto offset : {24U, 40U, 60U}) {
    corrupt = original;
    corrupt[table + 8 + offset] ^= std::byte{1};
    Check(!reopened.Reopen(corrupt) && reopened.Save() == original,
          "invalid source Atlas/region/trim accepted");
  }
  auto other = Source();
  other.id.low += 1;
  const std::array unordered{other, Source()};
  const auto ordered =
      twod::EncodeAuthoredScene(doc.Scene(), doc.Atlas(), doc.Clip(), doc.Tiles(), unordered);
  const auto orderedOwner = twod::AuthoredScene::Decode(ordered);
  Check(orderedOwner && orderedOwner->sources.front().id == sourceId, "catalog canonical ordering");
  const std::array duplicate{Source(), Source()};
  Check(twod::EncodeAuthoredScene(doc.Scene(), doc.Atlas(), doc.Clip(), doc.Tiles(), duplicate)
            .empty(),
        "duplicate source encoder");
  std::vector<twod::SpriteAsset> maximum(4096, Source());
  for (std::size_t i = 0; i < maximum.size(); ++i)
    maximum[i].id.low += i;
  const auto maximumBytes =
      twod::EncodeAuthoredScene(doc.Scene(), doc.Atlas(), doc.Clip(), doc.Tiles(), maximum);
  Check(twod::AuthoredScene::Decode(maximumBytes) && maximumBytes.size() < 64 * 1024 * 1024,
        "exact source count bound");
  // Exercise both catalog bounds together, including rejection of an unrelated overlapping
  // region. Per-source resolution must retain full Atlas validation without repeating it.
  auto largeAtlas = Atlas();
  largeAtlas.width = largeAtlas.height = 96;
  largeAtlas.rgba.resize(96 * 96 * 4);
  largeAtlas.regions.clear();
  for (std::uint32_t i = 0; i < 1024; ++i)
    largeAtlas.regions.push_back({i + 1, 1 + (i % 32) * 3, 1 + (i / 32) * 3, 1, 1});
  for (std::size_t i = 0; i < maximum.size(); ++i)
    maximum[i].region = std::uint32_t(i % 1024) + 1;
  const auto validationStart = std::chrono::steady_clock::now();
  Check(twod::ValidateSceneReferences(doc.Scene(), largeAtlas, doc.Clip(), doc.Tiles(), maximum),
        "maximum source and Atlas catalogs");
  std::cout << "Maximum-catalog reference validation: "
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                         validationStart)
                   .count()
            << " ms (4096 sources, 1024 regions)\n";
  largeAtlas.regions.back().x = largeAtlas.regions.front().x;
  largeAtlas.regions.back().y = largeAtlas.regions.front().y;
  Check(!twod::ValidateSceneReferences(doc.Scene(), largeAtlas, doc.Clip(), doc.Tiles(),
                                       std::span(maximum).first(1)),
        "unrelated overlapping Atlas region admitted");
  largeAtlas = Atlas();
  auto incompatible = Source();
  incompatible.trimWidth = 2;
  twod::SpriteInstance witness;
  witness.id = 1;
  witness.matrix = runtime::ToMatrix({});
  Check(!twod::ResolveSpriteAsset(incompatible, largeAtlas, witness) &&
            !twod::ValidateSceneReferences(doc.Scene(), largeAtlas, doc.Clip(), doc.Tiles(),
                                           std::span(&incompatible, 1)),
        "shared source trim rejection");
  maximum.push_back(other);
  Check(
      twod::EncodeAuthoredScene(doc.Scene(), doc.Atlas(), doc.Clip(), doc.Tiles(), maximum).empty(),
      "excess source count admitted");
  doc.SetReadOnly(true);
  Check(!doc.ReplaceSpriteSource(Source()) && !doc.RemoveSpriteSource(sourceId) && !doc.Undo() &&
            !doc.Reopen(original) && doc.Save() == replaced,
        "read-only source edit");
  std::unique_ptr<editor::TwoDPlayContext> oldPlay, newPlay;
  {
    editor::TwoDDocument temporary;
    Check(temporary.Reopen(original), "temporary source reopen");
    oldPlay = editor::TwoDPlayContext::Create(temporary);
    Check(temporary.ReplaceSpriteSource(Source(3)), "temporary source replacement");
    newPlay = editor::TwoDPlayContext::Create(temporary);
  }
  Check(oldPlay && newPlay && retained->pivotX == 2, "independent source owners");
  const auto extract = [](auto &play) {
    NexoraTwoDApiV1 api{};
    api.struct_size = sizeof(api);
    Check(play.GetApi(&api) == 0, "source Play API");
    const NexoraTwoDCamera camera{{0, 0}, 10, 0};
    Check(api.set_camera(api.context, &camera) == 0, "source Play camera");
    return play.ExtractFrame(320, 240, 7);
  };
  const auto oldFrame = extract(*oldPlay), newFrame = extract(*newPlay);
  Check(oldFrame && newFrame && oldFrame->statistics.visible == 2 &&
            oldFrame->vertices.size() == 8 && oldFrame->vertices[0].position[0] == 160 &&
            newFrame->vertices[0].position[0] == 136,
        "source Play retained crop/Pivot geometry");
  const auto again = extract(*oldPlay);
  Check(again && again->vertices[0].position[0] == oldFrame->vertices[0].position[0] &&
            again->vertices[0].position[1] == oldFrame->vertices[0].position[1],
        "new source mutated old Play capture");
  doc.SetReadOnly(false);
  for (const auto id : ids) {
    auto sprite = *doc.Scene().Find(id);
    sprite.source.reset();
    sprite.atlas = atlasId;
    sprite.region = 1;
    Check(doc.EditSprite(id, sprite, doc.World().FindEntity(id)->transform), "clear source");
  }
  Check(doc.RemoveSpriteSource(sourceId) && doc.Save()[4] == std::byte{1} && doc.Undo() &&
            doc.SpriteSource(sourceId),
        "unreferenced removal or schema downgrade Undo");
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(output), "write authored fixture");
}
} // namespace
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    Run();
    if (argc == 2) {
      const std::filesystem::path root = argv[1];
      std::filesystem::create_directories(root);
      editor::TwoDDocument doc;
      static_cast<void>(Populate(doc));
      Write(root / "initial.scene", doc.Save());
      Check(doc.ReplaceSpriteSource(Source(3)), "fixture pivot replacement");
      Write(root / "shifted.scene", doc.Save());
      Write(root / "atlas.nx2atlas", twod::EncodeAtlas(Atlas()));
      Write(root / "source.nx2sprite", twod::EncodeSpriteAsset(Source()));
    }
    std::cout << "PASS authored source catalog, v1 compatibility, Undo/reopen and isolated Play\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
