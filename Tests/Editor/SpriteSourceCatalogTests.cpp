#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SpriteSourceCatalog.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace nexora;
namespace {
constexpr runtime::AssetUuid atlasId{17, 23}, sourceId{31, 37}, secondId{41, 43};
void Check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
twod::SpriteAsset Source(runtime::AssetUuid id = sourceId, float pivot = 2) {
  twod::SpriteAsset source;
  source.id = id;
  source.atlas = atlasId;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = source.trimWidth = source.trimHeight = 1;
  source.pivotX = pivot;
  source.pivotY = 2;
  source.pixelsPerUnit = 1;
  return source;
}
twod::Bytes Atlas(std::byte color = std::byte{255}) {
  const twod::Image image{1, 1, 1, {color, std::byte{0}, std::byte{0}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
  Check(bool(atlas), "Atlas fixture");
  return twod::EncodeAtlas(*atlas);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(output), "fixture write");
}
editor::ContentItem Item(std::string_view type, const twod::Bytes &bytes) {
  const auto imported = editor::ImportTwoDAsset(type, bytes);
  Check(bool(imported.payload), "typed fixture import");
  editor::ContentItem item;
  item.id = imported.payload->Id();
  item.path = item.id.ToString() + std::string(type);
  item.type = type;
  item.twod = imported.payload;
  return item;
}
void Catalog() {
  std::optional<editor::SpriteSourceSnapshot> retained;
  {
    editor::ContentBrowserModel content;
    editor::SpriteSourceCatalog catalog;
    std::vector items{Item(".nx2atlas", Atlas()),
                      Item(".nx2sprite", twod::EncodeSpriteAsset(Source()))};
    std::string error;
    Check(content.Reset({}, 0), "zero-generation fixture");
    Check(!catalog.PublishContent(content, &error) && !error.empty(), "zero generation accepted");
    Check(content.Reset(items, 7) && catalog.PublishContent(content, &error) && error.empty(),
          "initial closure publication");
    const auto version = catalog.Version();
    retained = catalog.ResolveAsset(sourceId, version);
    Check(retained && retained->source->pivotX == 2 && retained->atlas->id == atlasId &&
              !catalog.ResolveAsset(atlasId, version) && !catalog.ResolveAsset({}, version) &&
              !catalog.ResolveAsset(sourceId, {8, version.content_revision}) &&
              !catalog.ResolveAsset(sourceId, {7, version.content_revision + 1}),
          "typed lookup or version isolation");
    const auto reject = [&](std::vector<editor::ContentItem> candidate) {
      // The browser may reject a malformed identity before catalog publication.
      const bool reset = content.Reset(std::move(candidate), 8);
      Check((!reset || !catalog.PublishContent(content, &error)) && catalog.Version() == version &&
                catalog.ResolveAsset(sourceId, version)->source == retained->source &&
                catalog.ResolveAsset(sourceId, version)->atlas == retained->atlas,
            "rejected candidate replaced last-good owners/version");
    };
    reject({items[1]});
    auto source = Source();
    source.region = 99;
    reject({items[0], Item(".nx2sprite", twod::EncodeSpriteAsset(source))});
    source = Source();
    source.trimWidth = 2;
    reject({items[0], Item(".nx2sprite", twod::EncodeSpriteAsset(source))});
    auto wrong = items;
    wrong[0].id = {99, 1};
    reject(wrong);
    wrong = items;
    wrong[1].type = ".nx2atlas";
    reject(wrong);
    wrong = items;
    wrong.push_back(items[1]);
    reject(wrong);
    std::vector<editor::ContentItem> excess{items[0]};
    for (std::size_t i = 0; i < editor::kMaximumWorkspaceTwoDAssets; ++i)
      excess.push_back(Item(".nx2sprite", twod::EncodeSpriteAsset(Source({99, i + 1}))));
    reject(std::move(excess));
    // Many sources share the same Atlas without repeated full Atlas validation.
    items.pop_back();
    for (std::uint64_t i = 1; i <= 1024; ++i)
      items.push_back(Item(".nx2sprite", twod::EncodeSpriteAsset(Source({99, i}))));
    Check(content.Reset(std::move(items), 9) && catalog.PublishContent(content) &&
              catalog.ResolveAsset({99, 1024}, catalog.Version()),
          "shared Atlas closure publication");
    catalog.Clear();
    Check(catalog.Version() == editor::SpriteSourceContentVersion{} &&
              !catalog.ResolveAsset(sourceId, version),
          "catalog clear left a resolvable entry");
  }
  Check(retained->source->pivotX == 2 && retained->atlas->Find(1)->width == 1 &&
            twod::EncodeAtlas(*retained->atlas) == Atlas(),
        "snapshot borrowed destroyed catalog/content storage");
}
void Transactions(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Sprite reference transactions"), "project fixture");
  Write(root / "Content/Atlas.nx2atlas", Atlas());
  Write(root / "Content/Source.nx2sprite", twod::EncodeSpriteAsset(Source()));
  Write(root / "Content/Second.nx2sprite", twod::EncodeSpriteAsset(Source(secondId, 3)));
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "persistent typed index");
  editor::ProjectContentSession content;
  Check(content.Open(workspace, assets, 7), "Content fixture");
  editor::SpriteSourceCatalog catalog;
  Check(catalog.PublishContent(content.Browser()), "project catalog publication");
  auto version = catalog.Version();
  const auto retained = catalog.ResolveAsset(sourceId, version);
  runtime::World world;
  const auto id = world.LoadScene("Sprite reference scene");
  Check(world.Activate(id), "scene activation");
  editor::SceneDocument scene(world, id);
  const auto first = scene.Create("First"), second = scene.Create("Second");
  auto keys = std::array{*scene.Key(first), *scene.Key(second)};
  const editor::OpaqueComponent plugin{991, "unrelated.plugin", {7, 9, 11}};
  Check(scene.SetOpaqueComponent(keys[1], plugin) && scene.Select(keys), "selected scene fixture");
  const auto runtimeBefore = world.SaveScene(id);
  const auto initial = scene.PrepareSave()->Bytes();
  Check(editor::AssignSpriteSources(scene, keys, sourceId, version, content, catalog, true) &&
            editor::ReadSpriteSourceReference(scene, keys[0]) == sourceId &&
            editor::ReadSpriteSourceReference(scene, keys[1]) == sourceId &&
            world.SaveScene(id) == runtimeBefore && scene.Undo() &&
            scene.PrepareSave()->Bytes() == initial && scene.Redo(),
        "batch assignment was not one metadata-only Undo/Redo");
  const auto saved = root / "References.scene";
  Check(scene.Save(saved) && !scene.Dirty(), "wrapped scene save");
  Check(editor::AssignSpriteSources(scene, keys, secondId, version, content, catalog, true) &&
            scene.Dirty() && scene.Undo() && !scene.Dirty(),
        "replacement did not preserve saved baseline");
  const auto stable = scene.PrepareSave()->Bytes();
  auto stale = keys;
  ++stale[1].entity_generation;
  Check(
      !editor::AssignSpriteSources(scene, stale, secondId, version, content, catalog, true) &&
          !editor::AssignSpriteSources(scene, std::span(keys).first(1), secondId, version, content,
                                       catalog, true) &&
          !editor::AssignSpriteSources(scene, keys, secondId, version, content, catalog, false) &&
          !editor::AssignSpriteSources(scene, keys, atlasId, version, content, catalog, true) &&
          !editor::ClearSpriteSources(scene, keys, {8, version.content_revision}, content, true) &&
          !editor::ClearSpriteSources(scene, keys, version, content, false) &&
          editor::AssignSpriteSources(scene, keys, sourceId, version, content, catalog, true) &&
          scene.PrepareSave()->Bytes() == stable && scene.Redo() && scene.Undo(),
      "rejection changed bytes or consumed pending Redo");
  editor::ProjectContentSession readonly;
  Check(readonly.Open(workspace, assets, 7, false) &&
            !editor::AssignSpriteSources(scene, keys, sourceId, version, readonly, catalog, true) &&
            !editor::ClearSpriteSources(scene, keys, version, readonly, true),
        "read-only reference authoring accepted");
  // Same project, unchanged source UUID: Atlas reimport must still invalidate a pending action.
  Write(root / "Content/Atlas.nx2atlas", Atlas(std::byte{127}));
  Check(content.Reimport(atlasId) && content.Browser().ProjectGeneration() == 7 &&
            !editor::AssignSpriteSources(scene, keys, sourceId, version, content, catalog, true) &&
            !editor::ClearSpriteSources(scene, keys, version, content, true) &&
            scene.PrepareSave()->Bytes() == stable && catalog.PublishContent(content.Browser()),
        "same-generation stale Atlas revision accepted or changed scene");
  version = catalog.Version();
  Write(root / "Content/Source.nx2sprite", twod::EncodeSpriteAsset(Source(sourceId, 4)));
  Check(content.Reimport(sourceId) &&
            !editor::AssignSpriteSources(scene, keys, sourceId, version, content, catalog, true) &&
            catalog.PublishContent(content.Browser()),
        "stale source revision accepted");
  version = catalog.Version();
  Check(catalog.ResolveAsset(sourceId, version)->source->pivotX == 4 &&
            retained->source->pivotX == 2 && retained->atlas->rgba[0] == std::byte{255},
        "reimport mutated retained source/Atlas owners");
  Check(scene.Reload(saved), "wrapped scene reopen");
  Check(!editor::ReadSpriteSourceReference(scene, keys[0]), "reopen accepted an old NodeKey");
  keys = {*scene.Key(first), *scene.Key(second)};
  Check(scene.Select(keys) && editor::ReadSpriteSourceReference(scene, keys[0]) == sourceId &&
            editor::ReadSpriteSourceReference(scene, keys[1]) == sourceId &&
            scene.OpaqueComponents(keys[1])->front() == plugin,
        "wrapped reopen lost source UUID or unrelated plugin bytes");
  // Mixed references unify atomically and restore individually through one Undo.
  Check(scene.SetOpaqueComponent(keys[1], editor::SpriteSourceReference(secondId)) &&
            editor::AssignSpriteSources(scene, keys, sourceId, version, content, catalog, true) &&
            scene.Undo() && editor::ReadSpriteSourceReference(scene, keys[1]) == secondId &&
            editor::ReadSpriteSourceReference(scene, keys[0]) == sourceId && scene.Redo(),
        "mixed assignment lost per-node history");
  for (unsigned mutation = 0; mutation < 6; ++mutation) {
    auto unsupported = editor::SpriteSourceReference(sourceId);
    if (mutation == 0)
      unsupported.data[0] = 2;
    if (mutation == 1)
      unsupported.type_name = "foreign.reference";
    if (mutation == 2)
      unsupported.type = 997;
    if (mutation == 3)
      unsupported.data.resize(18);
    if (mutation == 4)
      unsupported = editor::SpriteSourceReference({});
    if (mutation == 5)
      unsupported.data.clear();
    Check(scene.SetOpaqueComponent(keys[1], unsupported), "unsupported reference fixture");
    const auto before = scene.PrepareSave()->Bytes();
    Check(!editor::AssignSpriteSources(scene, keys, secondId, version, content, catalog, true) &&
              !editor::ClearSpriteSources(scene, keys, version, content, true) &&
              scene.PrepareSave()->Bytes() == before && scene.Undo(),
          "late unsupported reference partially mutated selection or Undo");
  }
  const auto reference = editor::SpriteSourceReference(sourceId);
  const editor::OpaqueComponentInfo oversized{reference.type, reference.type_name, 1024 * 1024,
                                              reference.data, first};
  Check(!editor::ReadSpriteSourceReference(oversized), "oversized valid-looking prefix decoded");
  // A deleted source is unresolved, but its supported UUID can still be cleared and restored.
  Check(content.Delete(std::array{sourceId}) && catalog.PublishContent(content.Browser()),
        "source deletion fixture");
  version = catalog.Version();
  Check(!editor::AssignSpriteSources(scene, keys, sourceId, version, content, catalog, true) &&
            editor::ClearSpriteSources(scene, keys, version, content, true) &&
            !editor::ReadSpriteSourceReference(scene, keys[0]) &&
            !editor::ReadSpriteSourceReference(scene, keys[1]) && scene.Undo() &&
            editor::ReadSpriteSourceReference(scene, keys[0]) == sourceId &&
            editor::ReadSpriteSourceReference(scene, keys[1]) == sourceId && scene.Redo() &&
            scene.OpaqueComponents(keys[1])->front() == plugin,
        "missing-asset clear/Undo/Redo lost supported references or unrelated bytes");
  Check(scene.SetTransform(first, runtime::Transform{9, 0, 0}) && scene.Undo() &&
            editor::ClearSpriteSources(scene, keys, version, content, true) && scene.Redo() &&
            scene.Transform(first)->x == 9,
        "absent reference clear consumed unrelated Runtime Redo");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-sprite-reference-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Catalog();
    Transactions(root);
    std::filesystem::remove_all(root);
    std::cout << "Sprite source catalog and main Editor reference transactions passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::filesystem::remove_all(root);
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
