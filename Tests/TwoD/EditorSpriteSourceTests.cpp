#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace nexora;
namespace {
constexpr runtime::AssetUuid atlasId{17, 23}, sourceId{31, 37}, otherId{41, 43};
const std::array roots{sourceId};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
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
twod::Bytes Atlas(std::byte color = std::byte{255}) {
  const twod::Image image{1, 1, 1, {color, std::byte{0}, std::byte{0}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
  Check(bool(atlas), "Atlas fixture");
  return twod::EncodeAtlas(*atlas);
}
runtime::CanonicalAsset Canonical(const twod::SpriteAsset &source) {
  runtime::ImporterRegistry importers;
  Check(twod::RegisterImporters(importers), "source importer");
  const auto result = importers.Import(
      {source.id, ".nx2sprite", "Source.nx2sprite", twod::EncodeSpriteAsset(source)});
  Check(bool(result), "canonical source");
  return *result;
}
runtime::CanonicalAsset CanonicalAtlas(std::byte color = std::byte{255}) {
  return {atlasId, "sprite_atlas", {}, Atlas(color)};
}
void Activate(runtime::AssetGenerationStore &store, std::uint64_t generation,
              const std::vector<runtime::CanonicalAsset> &sources) {
  runtime::AssetCooker cooker;
  runtime::DerivedDataCache cache;
  std::vector<runtime::RuntimeBlob> blobs;
  for (const auto &source : sources) {
    auto blob = cooker.Cook(source, "editor-source-test", "v1", cache);
    Check(bool(blob), "cook source");
    blobs.push_back(std::move(*blob));
  }
  auto bundle = runtime::BundleBuilder::Build("Editor sources", generation, {}, blobs);
  Check(bundle && store.Stage({*bundle}) && store.ActivateStaged(), "activate source generation");
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(output), "write fixture");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Decode() {
  const auto bytes = twod::EncodeSpriteAsset(Source());
  const auto imported = editor::ImportTwoDAsset(".nx2sprite", bytes, sourceId);
  Check(imported.payload && imported.payload->SpriteSource() && !imported.payload->Atlas() &&
            !imported.payload->Clip() && imported.payload->Id() == sourceId &&
            imported.payload->Type() == ".nx2sprite" &&
            imported.payload->Dependencies() == std::vector{atlasId} &&
            *imported.payload->SpriteSource() == Source(),
        "owning typed Sprite metadata");
  Check(imported.payload->RetainedBytes() ==
            sizeof(editor::TwoDAssetPayload) + sizeof(twod::SpriteAsset),
        "source capacity accounting");
  Check(!editor::ImportTwoDAsset(".nx2sprite", bytes, otherId).payload &&
            !editor::ImportTwoDAsset(".nx2clip", bytes).payload,
        "source identity/type mismatch");
  auto corrupt = bytes;
  corrupt.push_back(std::byte{0});
  Check(!editor::ImportTwoDAsset(".nx2sprite", corrupt).payload, "malformed source decoded");
  unsigned polls{};
  const auto cancelled =
      editor::ImportTwoDAsset(".nx2sprite", bytes, {}, [&] { return ++polls == 2; });
  Check(cancelled.cancelled && !cancelled.payload && cancelled.error.empty(),
        "cancelled source escaped staging");
  Check(imported.payload->SpriteSource()->pivotX == 2, "rejected decode mutated old owner");
}
void Frozen() {
  std::shared_ptr<const editor::TwoDPlayAssets> first, second;
  {
    runtime::AssetGenerationStore store;
    Activate(store, 1, {Canonical(Source()), CanonicalAtlas()});
    first = editor::TwoDPlayAssets::Capture(store, 1, roots);
    const auto bytes = twod::EncodeSpriteAsset(Source()).size() + Atlas().size();
    Check(first && first->Size() == 2 && first->PayloadBytes() == bytes &&
              first->SpriteSource(sourceId)->atlas == atlasId && !first->SpriteSource(atlasId) &&
              !first->Atlas(sourceId) && !first->Clip(sourceId) && !store.IsPinned(1) &&
              store.ResidentBytes() == 0,
          "frozen source closure or ownership");
    Check(editor::TwoDPlayAssets::Capture(store, 1, roots, nullptr, {2, bytes}) &&
              !editor::TwoDPlayAssets::Capture(store, 1, roots, nullptr, {1, bytes}) &&
              !editor::TwoDPlayAssets::Capture(store, 1, roots, nullptr, {2, bytes - 1}),
          "source closure budgets");
    Activate(store, 2, {Canonical(Source(3)), CanonicalAtlas(std::byte{7})});
    second = editor::TwoDPlayAssets::Capture(store, 2, roots);
    Check(second && second->SpriteSource(sourceId)->pivotX == 3 &&
              first->SpriteSource(sourceId)->pivotX == 2 &&
              second->Atlas(atlasId)->rgba != first->Atlas(atlasId)->rgba,
          "replacement changed retained source/Atlas owners");
    Check(store.ResidentBytes() == 0 && !store.IsPinned(2), "source capture leaked residency");
  }
  runtime::World world(runtime::WorldKind::Play);
  const auto sceneId = world.LoadScene("Captured source");
  const auto entity = world.CreateEntity(sceneId).id;
  twod::SpriteScene scene(world, sceneId);
  twod::GameplayBridge bridge(world, scene);
  NexoraTwoDSourceApiV1 api{};
  api.struct_size = sizeof(api);
  Check(bridge.GetSourceApi(&api) == 0, "source gameplay acquisition");
  NexoraTwoDSourceSpriteV1 sprite{
      {0, 0, 0, 8, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0}, sourceId.high, sourceId.low};
  Check(api.set_sprite(api.context, entity, &sprite) == 0, "source gameplay binding");
  const auto frame = [&](const auto &assets) {
    return scene.Extract(
        {},
        [&](auto id) -> std::optional<twod::AtlasBinding> {
          const auto atlas = assets->Atlas(id);
          return atlas ? std::optional{twod::AtlasBinding{atlas, 7}} : std::nullopt;
        },
        [&](auto id) { return assets->SpriteSource(id); });
  };
  const auto oldFrame = frame(first), newFrame = frame(second);
  Check(oldFrame && newFrame && oldFrame->statistics.visible == 1 &&
            newFrame->statistics.visible == 1 &&
            oldFrame->vertices[0].position[0] != newFrame->vertices[0].position[0],
        "captured source geometry was not resolved after store destruction");
  first.reset();
  Check(oldFrame->retainedAtlases[0].atlas->rgba[0] == std::byte{255},
        "frame borrowed destroyed snapshot");
}
void RejectedClosures() {
  const auto reject = [](std::vector<runtime::CanonicalAsset> assets) {
    runtime::AssetGenerationStore store;
    Activate(store, 3, assets);
    std::string error;
    Check(!editor::TwoDPlayAssets::Capture(store, 3, roots, &error) && !error.empty() &&
              !store.IsPinned(3) && store.ResidentBytes() == 0,
          "invalid source closure published or leaked temporary ownership");
  };
  reject({Canonical(Source())});
  for (const auto &dependencies :
       {std::vector<runtime::AssetUuid>{}, std::vector{otherId}, std::vector{atlasId, atlasId}}) {
    auto source = Canonical(Source());
    source.dependencies = dependencies;
    reject({source, CanonicalAtlas()});
  }
  auto source = Canonical(Source());
  source.payload.push_back(std::byte{0});
  reject({source, CanonicalAtlas()});
  source = Canonical(Source());
  source.payload = twod::EncodeSpriteAsset(Source());
  source.payload[8] ^= std::byte{1};
  reject({source, CanonicalAtlas()});
  auto bad = Source();
  bad.region = 99;
  reject({Canonical(bad), CanonicalAtlas()});
  bad = Source();
  bad.trimWidth = 2;
  reject({Canonical(bad), CanonicalAtlas()});
  auto atlas = CanonicalAtlas();
  atlas.type = "unsupported";
  reject({Canonical(Source()), atlas});
}
void Project(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Sprite sources"), "create source project");
  Write(root / "Content/Atlas.NX2ATLAS", Atlas());
  const auto path = root / "Content/Source.NX2SPRITE";
  Write(path, twod::EncodeSpriteAsset(Source()));
  editor::AssetWorkspace index;
  Check(index.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "index source");
  const auto sidecar = Read(editor::AssetWorkspace::IdentitySidecar(path));
  Check(sidecar.find("uuid=" + sourceId.ToString()) != std::string::npos &&
            sidecar.find("type=.nx2sprite") != std::string::npos,
        "source embedded identity discovery");
  editor::ProjectContentSession content;
  Check(content.Open(workspace, index, 7), "open source Content");
  Check(content.Browser().Find(sourceId)->twod->SpriteSource() &&
            content.Dependencies().Forward(sourceId) == std::vector{atlasId},
        "indexed typed source/dependency");
  const auto capture = [&] {
    return editor::ProjectTwoDPlayAssets::Capture(workspace, content, 7, 9, roots, true);
  };
  const auto first = capture();
  Check(first && first->Assets()->Size() == 2 && first->Matches(workspace, content, true) &&
            !content.CanUndo(),
        "project source capture");
  Check(content.Rename(sourceId, "Renamed.asset") && capture(),
        "authoritative Sprite importer lost after rename");
  Check(content.Undo() && capture(), "source rename Undo lost metadata");
  Write(path, twod::EncodeSpriteAsset(Source(3)));
  Check(!capture() && !first->Matches(workspace, content, true), "stale source admitted");
  Check(content.Reimport(sourceId), "source reimport");
  const auto second = capture();
  Check(second && second->Assets()->SpriteSource(sourceId)->pivotX == 3 &&
            first->Assets()->SpriteSource(sourceId)->pivotX == 2,
        "source reimport mutated frozen generation");
  {
    core::JobSystem jobs{1};
    jobs.Start();
    editor::AssetImportQueue imports{jobs};
    Write(path, twod::EncodeSpriteAsset(Source(4)));
    Check(content.BeginReimport(imports, sourceId) && !capture(), "pending source publication");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (content.ReimportBusy() && std::chrono::steady_clock::now() < deadline) {
      static_cast<void>(content.PollReimport());
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(!content.ReimportBusy() && capture() &&
              content.Browser().Find(sourceId)->twod->SpriteSource()->pivotX == 4 &&
              second->Assets()->SpriteSource(sourceId)->pivotX == 3,
          "queued source reimport or immutable ownership");
  }
  Check(content.Dependencies().Set(sourceId, std::array{otherId}) && !capture(),
        "source graph drift admitted");
  Check(content.Dependencies().Set(sourceId, std::array{atlasId}) && capture(),
        "source graph restore");
  auto invalid = Source();
  invalid.trimWidth = 2;
  Write(path, twod::EncodeSpriteAsset(invalid));
  Check(content.Reimport(sourceId) && !capture() &&
            first->Assets()->SpriteSource(sourceId)->trimWidth == 1,
        "incompatible reimport replaced last-good snapshot");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-editor-sources-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Decode();
    Frozen();
    RejectedClosures();
    Project(root);
    std::filesystem::remove_all(root);
    std::cout << "Editor Sprite sources: index/reimport, frozen closure and provenance passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove_all(root);
    return 1;
  }
}
