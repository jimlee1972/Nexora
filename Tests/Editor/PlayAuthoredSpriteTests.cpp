#include "PlayGameplayModule.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace nexora;
namespace {
constexpr runtime::AssetUuid atlasId{17, 23}, sourceId{31, 37};
std::array<runtime::Id, 2> entities;
unsigned creates{}, stops{}, destroys{};
bool observed_sources{};
NexoraTwoDSourceApiV1 sourceApi{};
void Check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(output), "authored Play fixture write");
}
twod::Bytes Source(float pivot = 2) {
  twod::SpriteAsset value;
  value.id = sourceId;
  value.atlas = atlasId;
  value.region = 1;
  value.sourceWidth = value.sourceHeight = 4;
  value.trimX = value.trimY = value.trimWidth = value.trimHeight = 1;
  value.pivotX = pivot;
  value.pivotY = 2;
  value.pixelsPerUnit = 1;
  return twod::EncodeSpriteAsset(value);
}
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  ++creates;
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  sourceApi = {};
  sourceApi.struct_size = sizeof(sourceApi);
  if (host->get_twod_api(host->context, &api) || NexoraGetTwoDSourcesV1(&api, &sourceApi))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  for (const auto entity : entities) {
    NexoraTwoDSourceSpriteV1 sprite{};
    if (sourceApi.get_sprite(sourceApi.context, entity, &sprite) ||
        sprite.source_high != sourceId.high || sprite.source_low != sourceId.low ||
        sprite.sprite.color[0] != 1 || sprite.sprite.color[1] != 1 || sprite.sprite.color[2] != 1)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  observed_sources = true;
  *state = &sourceApi;
  return 0;
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = [](void *) -> int32_t { return 0; };
  module->update = [](void *, double) -> int32_t { return 0; };
  module->on_stop = [](void *) { ++stops; };
  module->destroy = [](void *) {
    NexoraTwoDSourceSpriteV1 sprite{};
    if (sourceApi.get_sprite(sourceApi.context, entities[0], &sprite) == 0)
      ++destroys;
  };
  return 0;
}
void LargeMetadataProbe() {
  std::optional<std::vector<editor::OpaqueComponentInfo>> retained;
  {
    constexpr std::size_t count = 100000;
    std::string source = "NEXORA_EDITOR_SCENE 3\n";
    for (std::size_t index = 0; index < count; ++index)
      source += "node " + std::to_string(100 + index) + " 0 Node\n";
    source += "world\nNEXORA_SCENE 3 \"Large references\" 0 " + std::to_string(count) + '\n';
    for (std::size_t index = 0; index < count; ++index)
      source += std::to_string(100 + index) + " 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
    runtime::World world;
    const auto id = world.LoadScene("Large references");
    Check(world.Activate(id), "large probe scene");
    editor::SceneDocument document(world, id);
    Check(document.ReloadBytes(source), "large probe ingestion");
    std::string error;
    const auto start = std::chrono::steady_clock::now();
    const auto empty = editor::preview::CapturePlaySourceReferences(document, error);
    Check(empty && empty->empty(), "large source-free reference probe");
    std::cout << "100000-node reference probe ms="
              << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                     .count()
              << '\n';
    const auto first = *document.Key(100), last = *document.Key(100 + count - 1);
    const editor::OpaqueComponent unrelated{991, "unrelated.plugin",
                                            std::vector<std::uint8_t>(1024 * 1024, 7)};
    auto oversized = editor::SpriteSourceReference(sourceId);
    oversized.data.resize(1024 * 1024);
    auto alias = editor::SpriteSourceReference(sourceId);
    alias.type = 997;
    Check(document.SetOpaqueComponent(first, unrelated) &&
              document.SetOpaqueComponent(first, oversized) &&
              document.SetOpaqueComponent(last, alias),
          "large metadata fixtures");
    retained = document.InspectOpaqueComponents(editor::kSpriteSourceReferenceType,
                                                editor::kSpriteSourceReferenceName);
    Check(retained && retained->size() == 2 && retained->front().entity == first.id &&
              retained->front().byte_count == 1024 * 1024 &&
              retained->front().preview.size() == 64 && retained->back().entity == last.id &&
              retained->back().type == 997 && retained->back().preview.size() == 17 &&
              !document.InspectOpaqueComponents(0, editor::kSpriteSourceReferenceName) &&
              !document.InspectOpaqueComponents(editor::kSpriteSourceReferenceType, "") &&
              !editor::preview::CapturePlaySourceReferences(document, error),
          "bulk matching lost type/name aliases, preview bounds or unsupported rejection");
    runtime::WorldCommandBuffer commands;
    commands.DestroyEntity(last.id);
    Check(commands.Apply(world), "stale bulk metadata fixture");
    Check(!document.InspectOpaqueComponents(editor::kSpriteSourceReferenceType,
                                            editor::kSpriteSourceReferenceName),
          "stale metadata entity passed bulk inspection");
  }
  Check(retained && retained->front().preview.front() == 1 &&
            retained->back().type_name == editor::kSpriteSourceReferenceName,
        "bulk inspection borrowed destroyed document metadata");
}
void Run(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Authored Play"), "project fixture");
  const twod::Image white{
      1, 1, 1, {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&white, 1), 4);
  Check(bool(atlas), "Atlas fixture");
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
  Write(root / "Content/Source.nx2sprite", Source());
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "typed index");
  editor::ProjectContentSession content;
  Check(content.Open(workspace, assets, 7), "Content fixture");
  runtime::World world;
  const auto id = world.LoadScene("Authored Play");
  Check(world.Activate(id), "scene activation");
  editor::SceneDocument document(world, id);
  entities = {document.Create("First Sprite"), document.Create("Second Sprite")};
  auto keys = std::array{*document.Key(entities[0]), *document.Key(entities[1])};
  editor::SpriteSourceCatalog catalog;
  Check(catalog.PublishContent(content.Browser()) && document.Select(keys) &&
            editor::AssignSpriteSources(document, keys, sourceId, catalog.Version(), content,
                                        catalog, true),
        "source authoring");
  std::string error;
  const auto before = document.CaptureRuntimeScene();
  auto captured = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
  Check(captured && captured->authoring && error.empty() &&
            editor::preview::MatchesPlayTwoD(*captured, document, error) &&
            document.CaptureRuntimeScene() == before &&
            !editor::preview::PlayTwoDServices::Bind(world, *captured),
        "capture mutated authoring or bound its Editor World");
  Check(!editor::preview::CapturePlayTwoD(workspace, content, document, false, error) &&
            !error.empty(),
        "disabled project authority captured authored sources");
  runtime::PlaySession play(world);
  const auto start = [&] {
    Check(play.Start(.1, [](runtime::World &, double) { return true; }), "Play clone");
  };
  start();
  const auto clone = play.PlayWorld()->SaveScene(id);
  editor::preview::PlayGameplayModule module;
  Check(module.Load(*play.PlayWorld(), Loader, {}, *captured) && creates == 1 && observed_sources &&
            play.PlayWorld()->SaveScene(id) == clone,
        "authored sources were absent before Create or changed Runtime bytes");
  const auto frame = module.TwoDFrame({0, 0, 320, 240});
  Check(frame && frame->sprites.statistics.visible == 2 &&
            frame->sprites.retainedAtlases.size() == 1,
        "authored source frame unavailable");
  module.Unload();
  Check(stops == 1 && destroys == 1 && play.Stop() && document.CaptureRuntimeScene() == before,
        "teardown lost services or changed authoring");
  const auto reject = [&](editor::preview::PlayTwoDBinding bad) {
    start();
    const auto bytes = play.PlayWorld()->SaveScene(id);
    const auto callbacks = creates;
    Check(!module.Load(*play.PlayWorld(), Loader, {}, bad) && creates == callbacks &&
              play.PlayWorld()->SaveScene(id) == bytes && play.Stop(),
          "invalid binding reached Create or changed its Play World");
  };
  for (unsigned mutation = 0; mutation < 4; ++mutation) {
    auto bad = *captured;
    auto capture = *bad.authoring;
    if (mutation == 0)
      ++capture.scene;
    if (mutation == 1)
      capture.nodes[0].key.id = UINT64_MAX;
    if (mutation == 2)
      capture.nodes.push_back(capture.nodes[0]);
    if (mutation == 3)
      capture.nodes[0].opaque[0] = editor::SpriteSourceReference({99, 101});
    bad.authoring =
        std::make_shared<const editor::SceneDocument::RuntimeSceneCapture>(std::move(capture));
    reject(std::move(bad));
  }
  auto excess = *captured;
  auto excess_capture = *captured->authoring;
  excess_capture.nodes.resize(4097, excess_capture.nodes[0]);
  excess.authoring =
      std::make_shared<const editor::SceneDocument::RuntimeSceneCapture>(std::move(excess_capture));
  reject(std::move(excess));
  Check(document.SetTransform(entities[0], runtime::Transform{3, 0, 0}) &&
            !editor::preview::MatchesPlayTwoD(*captured, document, error),
        "changed Runtime authoring passed activation recheck");
  reject(*captured);
  Check(document.Undo() && editor::preview::MatchesPlayTwoD(*captured, document, error),
        "Undo did not restore matching capture");
  Check(editor::ClearSpriteSources(document, keys, catalog.Version(), content, true) &&
            !editor::preview::MatchesPlayTwoD(*captured, document, error) && document.Undo() &&
            editor::preview::MatchesPlayTwoD(*captured, document, error),
        "reference-only authoring drift was not detected");
  for (unsigned mutation = 0; mutation < 5; ++mutation) {
    auto reference = editor::SpriteSourceReference(sourceId);
    if (mutation == 0)
      reference.data[0] = 2;
    if (mutation == 1)
      reference.type_name = "foreign.reference";
    if (mutation == 2)
      reference.type = 997;
    if (mutation == 3)
      reference.data.resize(1024 * 1024);
    if (mutation == 4)
      reference = editor::SpriteSourceReference({99, 101});
    Check(document.SetOpaqueComponent(keys[1], std::move(reference)), "bad reference fixture");
    const auto stable = document.PrepareSave()->Bytes();
    Check(!editor::preview::CapturePlayTwoD(workspace, content, document, true, error) &&
              !error.empty() && document.PrepareSave()->Bytes() == stable && document.Undo(),
          "unsupported/missing source was captured or its bytes/history changed");
  }
  Write(root / "Content/Source.nx2sprite", Source(3));
  Check(!captured->assets->Matches(workspace, content, true) && content.Reimport(sourceId),
        "stale source passed production asset recheck");
  const auto fresh = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
  Check(fresh && fresh->assets->Assets()->SpriteSource(sourceId)->pivotX == 3 &&
            captured->assets->Assets()->SpriteSource(sourceId)->pivotX == 2,
        "reimport mutated frozen source owner");
  const auto saved = root / "Authored.scene";
  Check(document.Save(saved) && document.Reload(saved) &&
            !editor::preview::MatchesPlayTwoD(*fresh, document, error),
        "reopened document reused a stale document generation");
  captured = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
  Check(captured && editor::preview::MatchesPlayTwoD(*captured, document, error),
        "reopened source reference capture");
  // An empty 3D project keeps its historical asset-free path, but cannot hide a source reference.
  Check(content.Browser().Reset({}, 7) &&
            !editor::preview::CapturePlayTwoD(workspace, content, document, true, error),
        "empty Content silently discarded authored references");
  keys = {*document.Key(entities[0]), *document.Key(entities[1])};
  Check(document.Select(keys) &&
            editor::ClearSpriteSources(document, keys, {7, content.Browser().Revision()}, content,
                                       true),
        "missing asset clear fixture");
  const auto empty = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
  Check(empty && !empty->assets && !empty->authoring &&
            editor::preview::MatchesPlayTwoD(*empty, document, error),
        "source-free empty project lost legacy Play path");
  Check(document.SetOpaqueComponent(keys[0], editor::SpriteSourceReference(sourceId)) &&
            !editor::preview::MatchesPlayTwoD(*empty, document, error) && document.Undo() &&
            editor::preview::MatchesPlayTwoD(*empty, document, error),
        "new reference silently crossed an empty-capture activation boundary");
  // Release the project writer lease before deletion. Windows correctly refuses unlinking the
  // live lease file; retained Play snapshots must survive the owners themselves, not just unlink.
  catalog.Clear();
  content = editor::ProjectContentSession{};
  assets = editor::AssetWorkspace{};
  workspace = editor::ProjectWorkspace{};
  std::filesystem::remove_all(root);
  Check(!std::filesystem::exists(root), "project teardown did not remove authoring sources");
  start();
  Check(module.Load(*play.PlayWorld(), Loader, {}, *captured),
        "retained binding depended on deleted project sources or owners");
  module.Unload();
  Check(play.Stop(), "retained Play stop");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-authored-play-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    LargeMetadataProbe();
    Run(root);
    std::filesystem::remove_all(root);
    std::cout << "Authored Sprite Play capture, pre-Create binding and rejection passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
