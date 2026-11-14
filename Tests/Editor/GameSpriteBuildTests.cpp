#include "../Runtime/GameSceneFixture.h"
#include "Nexora/Editor/GameBuildCapture.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SpriteSourceCatalog.h"
#include "Nexora/TwoD/GameSpriteAssets.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
constexpr runtime::AssetUuid atlasId{101, 1}, sourceId{102, 1};
constexpr runtime::AssetUuid otherSourceId{103, 1}, otherAtlasId{104, 1}, clipId{105, 1},
    legacyClipId{106, 1}, unusedClipId{107, 1};
static_assert(editor::kSpriteSourceReferenceType == runtime::kGameSpriteSourceReferenceType &&
              editor::kSpriteSourceReferenceName == runtime::kGameSpriteSourceReferenceName);
void Check(bool value, const char *text) {
  if (!value)
    throw std::runtime_error(text);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  out.close();
  Check(bool(out), "fixture write");
}
twod::Bytes Source(float pivot = 2) {
  twod::SpriteAsset source;
  source.id = sourceId;
  source.atlas = atlasId;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = 1;
  source.trimWidth = source.trimHeight = 2;
  source.pivotX = pivot;
  source.pivotY = 3;
  source.pixelsPerUnit = 1;
  return twod::EncodeSpriteAsset(source);
}
struct Fixture final {
  editor::ProjectWorkspace workspace;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  editor::MaterialAssetCatalog materials;
  runtime::AssetUuid titleId, levelId;
  explicit Fixture(const std::filesystem::path &root, bool animations = false) {
    Check(workspace.Create(root, "Sprite Build"), "workspace");
    std::filesystem::create_directories(root / "Content");
    // Asymmetric colors reveal crop, UV direction and negative scale independently.
    const twod::Image image{1,
                            2,
                            2,
                            {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
                             std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
                             std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
                             std::byte{255}, std::byte{255}, std::byte{0}, std::byte{255}}};
    const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 8, 8, true);
    Check(bool(atlas), "Atlas");
    Write(root / "Content/Color.nx2atlas", twod::EncodeAtlas(*atlas));
    Write(root / "Content/Source.nx2sprite", Source());
    if (animations) {
      auto other_atlas = *atlas;
      other_atlas.id = otherAtlasId;
      auto other_source = *twod::DecodeSpriteAsset(Source(3));
      other_source.id = otherSourceId;
      other_source.atlas = otherAtlasId;
      Write(root / "Content/Other.nx2atlas", twod::EncodeAtlas(other_atlas));
      Write(root / "Content/Other.nx2sprite", twod::EncodeSpriteAsset(other_source));
      const twod::Clip clip{
          clipId, {}, true, {{0, 0.25, "first", sourceId}, {0, 0.25, "other", otherSourceId}}};
      twod::Clip legacy{legacyClipId, atlasId, true, {{1, 0.25, "legacy"}}};
      Write(root / "Content/Animation.nx2clip", twod::EncodeClip(clip));
      Write(root / "Content/Legacy.nx2clip", twod::EncodeClip(legacy));
      legacy.id = unusedClipId;
      Write(root / "Content/Unselected.nx2clip", twod::EncodeClip(legacy));
    }
    constexpr std::string_view obj = "v -0.6 -0.6 0\nv 0.6 -0.6 0\nv 0 0.6 0\nf 1 2 3\n";
    Write(root / "Content/Triangle.obj", std::as_bytes(std::span(obj.data(), obj.size())));
    Check(
        assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "import");
    Check(content.Open(workspace, assets, 17), "Content");
    Check(meshes.PublishContent(content.Browser()) && materials.PublishContent(content.Browser()),
          "catalogs");
    runtime::World world;
    const auto id = world.LoadScene("Sources");
    editor::SceneDocument document(world, id);
    const auto left = document.Create("Source left"), right = document.Create("Source mirrored");
    Check(document.SetTransform(left, runtime::Transform{-4, 0, 0}) &&
              document.SetTransform(right, runtime::Transform{4, 0, 0, 0, 0, 0, 1, -1, 1, 1}) &&
              document.SetOpaqueComponent(*document.Key(left),
                                          editor::SpriteSourceReference(sourceId)) &&
              document.SetOpaqueComponent(*document.Key(right),
                                          editor::SpriteSourceReference(sourceId)),
          "source references");
    const auto mesh_item =
        std::ranges::find(content.Browser().Items(), ".obj", &editor::ContentItem::type);
    Check(mesh_item != content.Browser().Items().end(), "mixed mesh source");
    const auto mesh = document.Create("Retained 3D mesh");
    Check(document.SetMeshRenderer(
              *document.Key(mesh),
              runtime::MeshComponent{runtime::MeshResourceId(mesh_item->id), {0}}),
          "mixed mesh binding");
    Check(document.Save(root / "Title.scene") && document.Save(root / "Level.scene"), "scenes");
    editor::ProjectBuildSettingsSession settings(workspace);
    Check(settings.Reload(), "settings");
    const auto title = settings.Enroll("Title.scene", content.Browser());
    const auto level = settings.Enroll("Level.scene", content.Browser());
    editor::ProjectBuildSettings build{title ? title->id : runtime::AssetUuid{}, {}};
    if (title && level)
      build.scenes = {*title, *level};
    if (animations)
      build.animation_clips = {legacyClipId, clipId};
    Check(title && level && settings.Save(build, content.Browser()), "scene catalog");
    titleId = title->id;
    levelId = level->id;
  }
  std::optional<editor::GameBuildCapture> Capture(std::string *error = nullptr) const {
    return editor::CaptureGameBuild(workspace, content.Browser(), meshes, materials, error);
  }
};
void AnimationRoots(const std::filesystem::path &root) {
  std::optional<editor::GameBuildCapture> capture;
  {
    Fixture fixture(root, true);
    std::string error;
    capture = fixture.Capture(&error);
    Check(bool(capture), error.c_str());
    Check(capture->project.sprite_assets.size() == 6 && capture->sources.size() == 20 &&
              capture->project.animation_clips == std::vector{legacyClipId, clipId} &&
              std::ranges::none_of(capture->project.sprite_assets,
                                   [](const auto &blob) { return blob.id == unusedClipId; }) &&
              editor::GameBuildSourcesCurrent(root, capture->sources),
          "explicit animation closure or exact source baselines incorrect");
    editor::ProjectBuildSettingsSession settings(fixture.workspace);
    Check(settings.Reload(), "animation settings reopen");
    auto wrong = *settings.Settings();
    const auto valid = wrong;
    for (const auto &roots :
         {std::vector{atlasId}, std::vector{sourceId}, std::vector{runtime::AssetUuid{900, 900}}}) {
      wrong.animation_clips = roots;
      Check(settings.Save(wrong, fixture.content.Browser()) && !fixture.Capture(),
            "missing or wrong-typed root captured");
    }
    Check(settings.Save(valid, fixture.content.Browser()) && fixture.Capture().has_value(),
          "restored roots could not capture");
    const auto path = root / "Content/Animation.nx2clip";
    const auto clip =
        std::ranges::find(capture->project.sprite_assets, clipId, &runtime::RuntimeBlob::id);
    auto changed = *twod::DecodeClip(clip->payload);
    changed.frames[0].seconds = 0.5;
    Write(path, twod::EncodeClip(changed));
    Check(!fixture.Capture() && !editor::GameBuildSourcesCurrent(root, capture->sources),
          "unreimported animation escaped capture/final source guard");
    Write(path, clip->payload);
    const auto sidecar = root / "Content/Animation.nx2clip.meta";
    const auto backup = root / "Content/Animation.meta.saved";
    std::filesystem::rename(sidecar, backup);
    Check(!fixture.Capture(), "Clip without mandatory identity sidecar captured");
    std::filesystem::rename(backup, sidecar);
    Check(fixture.Capture().has_value() && editor::GameBuildSourcesCurrent(root, capture->sources),
          "restoring exact animation baselines failed");
  }
  const auto bytes = editor::CookGameProject(capture->project);
  auto loaded = bytes ? runtime::DecodeGameProjectPackage(*bytes) : std::nullopt;
  Check(loaded && loaded->Capability() == 4 && loaded->AnimationClips().size() == 2 &&
            loaded->SpriteAssets().size() == 6 && loaded->AssetCount() == 9,
        "source/legacy Clip roots absent from game wire");
  auto typed = twod::GameSpriteAssets::Load(loaded->SpriteAssets());
  Check(typed && typed->ResolveClip(clipId) && typed->ResolveClip(legacyClipId) &&
            !typed->ResolveClip(unusedClipId),
        "immutable Clip admission/resolution failed");
  auto reordered = capture->project;
  std::ranges::reverse(reordered.animation_clips);
  std::ranges::reverse(reordered.sprite_assets);
  Check(editor::CookGameProject(reordered) == bytes, "Clip root package is not canonical");
  auto clip_only = capture->project;
  for (auto &scene : clip_only.scenes)
    for (auto &node : scene.capture.nodes)
      node.opaque.clear();
  Check(editor::CookGameProject(clip_only).has_value(),
        "explicit animation roots require unrelated scene source references");
  clip_only.animation_clips = {legacyClipId};
  std::erase_if(clip_only.sprite_assets,
                [](const auto &blob) { return blob.id != atlasId && blob.id != legacyClipId; });
  const auto legacy_bytes = editor::CookGameProject(clip_only);
  const auto legacy_only =
      legacy_bytes ? runtime::DecodeGameProjectPackage(*legacy_bytes) : std::nullopt;
  Check(legacy_only && legacy_only->Capability() == 4 && legacy_only->SpriteAssets().size() == 2,
        "legacy-only animation root cannot retain Atlas without authored sources");
  for (unsigned mutation = 0; mutation < 12; ++mutation) {
    auto bad = capture->project;
    auto clip = std::ranges::find(bad.sprite_assets, clipId, &runtime::RuntimeBlob::id);
    auto legacy = std::ranges::find(bad.sprite_assets, legacyClipId, &runtime::RuntimeBlob::id);
    if (mutation == 0)
      bad.animation_clips.clear();
    if (mutation == 1)
      bad.animation_clips.push_back(clipId);
    if (mutation == 2)
      bad.animation_clips = {atlasId, legacyClipId};
    if (mutation == 3)
      clip->payload[0] ^= std::byte{1};
    if (mutation == 4)
      clip->dependencies = {sourceId};
    if (mutation == 5)
      bad.sprite_assets.erase(
          std::ranges::find(bad.sprite_assets, otherSourceId, &runtime::RuntimeBlob::id));
    if (mutation >= 6) {
      auto payload = *twod::DecodeClip(mutation == 9 ? legacy->payload : clip->payload);
      if (mutation == 6)
        payload.frames[1].source = runtime::AssetUuid{900, 900};
      if (mutation == 7)
        payload.id = {900, 900};
      if (mutation == 8)
        clip->dependencies = {sourceId, sourceId, otherSourceId};
      if (mutation == 9)
        payload.frames[0].region = 999;
      if (mutation == 10)
        payload.frames[0].source = atlasId;
      auto &target = mutation == 9 ? *legacy : *clip;
      if (mutation == 6 || mutation == 10)
        target.dependencies = twod::ClipDependencies(payload);
      auto malformed_payload = twod::EncodeClip(payload);
      if (mutation == 11)
        malformed_payload[0] ^= std::byte{1};
      runtime::DerivedDataCache cache;
      target = *runtime::AssetCooker{}.Cook(
          {target.id, target.type, target.dependencies, malformed_payload}, "portable",
          "invalid-animation", cache);
    }
    Check(!editor::CookGameProject(bad), "invalid typed/authoritative animation closure cooked");
  }
  const auto retained = typed->ResolveClip(clipId);
  std::filesystem::remove_all(root);
  loaded.reset();
  typed.reset();
  capture.reset();
  twod::Animator animator;
  Check(animator.Play(*retained) && animator.SourceId() == sourceId && animator.Advance(0.3) &&
            animator.SourceId() == otherSourceId,
        "retained Clip lost source UUID clock after all producer/source owners disappeared");
  std::cout << "Explicit animation roots, canonical capability4, source closure/baselines and "
               "retained Clip ownership passed\n";
}
void Run(const std::filesystem::path &root) {
  // Frozen against the actual pre-change ac15a642 encoder, built independently in cloud.
  const auto legacy = runtime::EncodeGameProjectPackage(testscene::Package());
  Check(legacy && legacy->size() == 1379 && (*legacy)[12] == std::byte{2},
        "legacy capability wire");
  std::uint64_t legacy_hash = 1469598103934665603ULL;
  for (const auto byte : *legacy)
    legacy_hash = (legacy_hash ^ std::to_integer<unsigned char>(byte)) * 1099511628211ULL;
  Check(legacy_hash == 0x7df5c4a79b4fc8edULL, "source-free game wire changed");
  std::optional<editor::GameBuildCapture> capture;
  runtime::AssetUuid titleId, levelId;
  {
    Fixture fixture(root);
    std::string error;
    capture = fixture.Capture(&error);
    Check(bool(capture), error.c_str());
    titleId = fixture.titleId;
    levelId = fixture.levelId;
    Check(capture->project.sprite_assets.size() == 2 && capture->project.meshes.size() == 1 &&
              capture->sources.size() == 12 &&
              editor::GameBuildSourcesCurrent(root, capture->sources),
          "owning source/Atlas closure");
    Write(root / "Content/Source.nx2sprite", Source(3));
    Check(!fixture.Capture() && !editor::GameBuildSourcesCurrent(root, capture->sources),
          "unreimported source escaped capture/publication guard");
    Write(root / "Content/Source.nx2sprite", Source());
    const auto metadata = root / "Content/Source.nx2sprite.meta";
    const auto backup = root / "Content/Source.sidecar.saved";
    std::filesystem::rename(metadata, backup);
    Check(!fixture.Capture(), "missing source sidecar admitted");
    std::filesystem::rename(backup, metadata);
    Check(bool(fixture.Capture()), "restored exact source baseline");
  }
  std::string error;
  const auto bytes = editor::CookGameProject(capture->project, &error);
  Check(bool(bytes), error.c_str());
  Check((*bytes)[12] == std::byte{3}, "source capability absent");
  auto decoded = runtime::DecodeGameProjectPackage(*bytes, &error);
  Check(decoded && decoded->AssetCount() == 5 && decoded->SpriteAssets().size() == 2,
        "shared source/Atlas union not deduplicated across scenes");
  auto typed = twod::GameSpriteAssets::Load(decoded->SpriteAssets(), &error);
  Check(bool(typed), error.c_str());
  // Correct NXAB integrity cannot substitute for optional typed Sprite/Atlas admission.
  auto malformed = capture->project;
  for (auto &blob : malformed.sprite_assets)
    if (blob.type == "sprite_source") {
      auto payload = blob.payload;
      payload[0] ^= std::byte{1};
      runtime::DerivedDataCache cache;
      blob = *runtime::AssetCooker{}.Cook({blob.id, blob.type, blob.dependencies, payload},
                                          "portable", "invalid-source", cache);
    }
  Check(!twod::GameSpriteAssets::Load(malformed.sprite_assets) &&
            !editor::CookGameProject(malformed),
        "typed admission accepted valid-hash malformed source");
  auto reordered = capture->project;
  std::ranges::reverse(reordered.scenes);
  std::ranges::reverse(reordered.sprite_assets);
  Check(editor::CookGameProject(reordered) == bytes, "noncanonical source package");
  for (unsigned mutation = 0; mutation < 6; ++mutation) {
    auto bad = capture->project;
    if (mutation == 0)
      bad.sprite_assets.pop_back();
    if (mutation == 1)
      bad.sprite_assets.front().payload[0] ^= std::byte{1};
    if (mutation == 2)
      bad.sprite_assets.push_back(bad.sprite_assets.front());
    if (mutation >= 3) {
      auto &record = bad.scenes.front().capture.nodes.front().opaque.front();
      if (mutation == 3)
        record.data[0] = 2;
      if (mutation == 4)
        record.type = 999;
      if (mutation == 5)
        record = editor::SpriteSourceReference({900, 900});
    }
    Check(!editor::CookGameProject(bad), "invalid source closure/reference cooked");
  }
  // Complete metadata closure survives document, Content, workspace and original source removal.
  std::filesystem::remove_all(root);
  auto project = std::make_shared<const runtime::LoadedGameProject>(std::move(*decoded));
  game::GameWorld world(runtime::WorldKind::Play);
  runtime::GameSceneSession session(world, project);
  std::uint64_t request{};
  Check(session.RequestLoad(titleId, runtime::GameSceneLoadMode::Replace, false, request) ==
                runtime::GameSceneError::None &&
            session.Process(),
        "source startup remap");
  const auto first = typed->Extract(session.View(), 320, 240);
  Check(first && first->statistics.visible == 2 && first->statistics.unavailable == 0 &&
            first->vertices.size() == 8,
        "source startup extraction");
  Check(session.RequestLoad(levelId, runtime::GameSceneLoadMode::Additive, false, request) ==
                runtime::GameSceneError::None &&
            session.Process(),
        "source additive remap");
  const auto additive = typed->Extract(session.View(), 320, 240);
  Check(additive && additive->statistics.visible == 4 && additive->vertices.size() == 16,
        "overlapping authored IDs did not map to independent live sources");
  const auto view = session.View();
  Check(view.instances[0].entities != view.instances[1].entities, "live entity identities reused");
  Check(runtime::DecodeGameProjectPackage(*bytes).has_value(), "retained source-free IO ownership");
  std::cout << "Owning main Game Build Sprite closure, exact baselines, capability3, remapped "
               "multi-scene extraction passed\n";
}
} // namespace
int main(int argc, char **argv) {
  const auto root =
      argc == 3 ? std::filesystem::absolute(argv[2])
                : std::filesystem::canonical(std::filesystem::temp_directory_path()) /
                      ("nexora-sprite-build-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-fixture") {
      Fixture fixture(root);
      return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--write-animation-fixture") {
      Fixture fixture(root, true);
      return 0;
    }
    Run(root);
    AnimationRoots(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 1;
  }
}
