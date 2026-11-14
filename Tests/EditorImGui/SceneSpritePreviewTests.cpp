#include "Nexora/Editor/ProjectContent.h"
#include "SceneSpriteTextures.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
constexpr runtime::AssetUuid atlasId{209, 1}, sourceId{209, 2};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Near(double actual, double expected) {
  Check(std::abs(actual - expected) < 0.00001, "source geometry differs from independent oracle");
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(file), "source geometry fixture write failed");
}
twod::SpriteAsset Source(float pivot = 3) {
  twod::SpriteAsset source;
  source.id = sourceId;
  source.atlas = atlasId;
  source.region = 1;
  source.sourceWidth = 10;
  source.sourceHeight = 8;
  source.trimX = 2;
  source.trimY = 1;
  source.trimWidth = 4;
  source.trimHeight = 2;
  source.pivotX = pivot;
  source.pivotY = 4;
  source.pixelsPerUnit = 2;
  return source;
}
void NativeAtlas(const std::filesystem::path &root, bool alternate = false) {
  twod::Image image{1, 4, 2, {}};
  std::array<std::array<unsigned, 4>, 8> colors{{{255, 0, 0, 0},
                                                 {255, 0, 0, 255},
                                                 {0, 255, 0, 255},
                                                 {0, 255, 0, 255},
                                                 {0, 0, 255, 255},
                                                 {0, 0, 255, 255},
                                                 {255, 255, 0, 255},
                                                 {255, 255, 0, 255}}};
  if (alternate)
    for (auto &color : colors)
      std::swap(color[0], color[2]);
  for (const auto &color : colors)
    for (const auto channel : color)
      image.rgba.push_back(std::byte(channel));
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 8, 16, true);
  Check(bool(atlas), "native atlas fixture failed");
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
}
void NativeFixture(const std::filesystem::path &root) {
  editor::ProjectWorkspace project;
  Check(project.Create(root, "Native Scene Sprite"), "native project fixture failed");
  const auto text = [&](const std::filesystem::path &relative, std::string_view value) {
    Write(root / relative, std::as_bytes(std::span(value)));
  };
  NativeAtlas(root);
  Write(root / "Content/Source.nx2sprite", twod::EncodeSpriteAsset(Source()));
  text("Content/Triangle.obj", "v -.6 -.4 0\nv .6 -.4 0\nv 0 .6 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
  text("Content/Triangle.obj.meta",
       "schema=1\nuuid=12345678-9abc-def0-fedc-ba9876543210\ntype=.obj\n");
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "native import failed");
  runtime::World world;
  const auto sceneId = world.LoadScene("Sprite authoring");
  editor::SceneDocument scene(world, sceneId);
  for (const bool mirrored : {false, true}) {
    const auto entity = scene.Create(mirrored ? "Mirrored source" : "Source");
    runtime::Transform pose;
    pose.x = mirrored ? 2 : -2;
    pose.y = 2;
    pose.sx = mirrored ? -1 : 1;
    Check(scene.SetTransform(entity, pose) &&
              scene.SetOpaqueComponent(*scene.Key(entity), editor::SpriteSourceReference(sourceId)),
          "native source assignment failed");
  }
  const auto mesh =
      runtime::MeshResourceId(*runtime::AssetUuid::Parse("12345678-9abc-def0-fedc-ba9876543210"));
  Check(scene.CreateMesh("Landmark", {mesh, {}}) &&
            scene.Save(root / ".nexora/scenes/Main.scene") && scene.Save(root / "Title.scene"),
        "native scene Save failed");
  text(
      "project.build.json",
      R"({"schema":1,"scenes":[{"id":"00000000-0000-0001-0000-000000000001","path":"Title.scene","enabled":true}],"start_scene":"00000000-0000-0001-0000-000000000001"})");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-native-fixture") {
      NativeFixture(argv[2]);
      return 0;
    }
    if (argc == 4 && std::string_view(argv[1]) == "--replace-native-atlas") {
      NativeAtlas(argv[2], std::string_view(argv[3]) == "1");
      return 0;
    }
    Check(argc == 1, "unexpected fixture arguments");
    std::optional<editor::preview::SceneSpritePlanes> retained;
    std::weak_ptr<const twod::Atlas> weakAtlas;
    const auto root = std::filesystem::temp_directory_path() /
                      ("nexora-scene-source-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
      editor::test::TemporaryDirectoryCleanup cleanup(root);
      editor::ProjectWorkspace project;
      Check(project.Create(root, "Scene Sprite planes"), "project fixture failed");
      twod::Image image{1, 4, 2, twod::Bytes(4 * 2 * 4, std::byte{255})};
      image.rgba[3] = std::byte{0};
      const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 8, 16, true);
      Check(bool(atlas), "atlas fixture failed");
      Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
      Write(root / "Content/Source.nx2sprite", twod::EncodeSpriteAsset(Source()));
      editor::AssetWorkspace assets;
      Check(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite),
            "asset fixture import failed");
      editor::ProjectContentSession content;
      Check(content.Open(project, assets, 11), "Content fixture failed");
      runtime::World world;
      const auto sceneId = world.LoadScene("Source geometry");
      editor::SceneDocument scene(world, sceneId);
      const auto parent = scene.Create("Parent"), entity = scene.Create("Sprite", parent);
      const auto empty = editor::preview::CaptureSceneSpritePlanes(scene, content.Browser());
      Check(empty && empty->entities.empty() && !empty->unavailable,
            "source-free fast path failed");
      const auto key = *scene.Key(entity);
      Check(scene.SetOpaqueComponent(key, editor::SpriteSourceReference(sourceId)),
            "source assignment failed");
      const auto runtimeBefore = world.SaveScene(sceneId);
      const auto authoringBefore = scene.PrepareSave()->Bytes();
      retained = editor::preview::CaptureSceneSpritePlanes(scene, content.Browser());
      Check(retained && retained->entities.size() == 1 && !retained->unavailable,
            "source plane capture failed");
      Check(editor::preview::SceneSpritePlanesMatch(*retained, scene, content.Browser()),
            "matching metadata/content capture became stale");
      const auto &plane = *retained->entities.at(entity);
      weakAtlas = plane.assets.atlas;
      Near(plane.geometry.minimum[0], -.5);
      Near(plane.geometry.minimum[1], .5);
      Near(plane.geometry.maximum[0], 1.5);
      Near(plane.geometry.maximum[1], 1.5);
      const auto *region = atlas->Find(1);
      Near(plane.geometry.vertices[0].uv[0], double(region->x) / atlas->width);
      Near(plane.geometry.vertices[0].uv[1], double(region->y + 2) / atlas->height);
      Near(plane.geometry.vertices[2].uv[1], double(region->y) / atlas->height);
      const auto identity = runtime::ToMatrix(runtime::Transform{});
      Check(!editor::preview::PickSceneSprite({{-.25, 1.25, 3}, {0, 0, -1}}, plane, identity) &&
                editor::preview::PickSceneSprite({{.25, 1.25, 3}, {0, 0, -1}}, plane, identity) &&
                !editor::preview::PickSceneSprite({{1.75, 1.25, 3}, {0, 0, -1}}, plane, identity),
            "source cutout/triangle picking disagrees with visible geometry");
      runtime::Transform mirrored;
      mirrored.sx = -1;
      Check(!editor::preview::PickSceneSprite({{.25, 1.25, 3}, {0, 0, -1}}, plane,
                                              runtime::ToMatrix(mirrored)) &&
                editor::preview::PickSceneSprite({{-.25, 1.25, 3}, {0, 0, -1}}, plane,
                                                 runtime::ToMatrix(mirrored)),
            "cutout picking lost the mirrored source anchor");
      Check(world.SaveScene(sceneId) == runtimeBefore &&
                scene.PrepareSave()->Bytes() == authoringBefore,
            "preview changed authoring/runtime bytes");
      Check(scene.Undo() &&
                editor::preview::CaptureSceneSpritePlanes(scene, content.Browser())
                    ->entities.empty() &&
                scene.Redo(),
            "capture changed metadata Undo/Redo history");
      runtime::Transform parentPose;
      parentPose.x = 10;
      parentPose.y = 20;
      parentPose.z = 3;
      parentPose.qz = parentPose.qw = std::sqrt(.5);
      parentPose.sx = 2;
      parentPose.sy = 3;
      runtime::Transform childPose;
      childPose.qz = std::sin(3.141592653589793 / 8);
      childPose.qw = std::cos(3.141592653589793 / 8);
      childPose.sx = -1;
      Check(scene.SetTransform(parent, parentPose) && scene.SetTransform(entity, childPose),
            "hierarchical mirror/shear fixture failed");
      editor::MeshAssetCatalog meshes;
      Check(meshes.PublishContent(content.Browser()), "mesh palette fixture failed");
      const auto prepared = editor::preview::PrepareNativeSceneMeshes(scene, meshes, content, 11);
      Check(prepared.sprites && prepared.sprite_ranges.size() == 1,
            "native source geometry missing");
      editor::preview::SceneSpriteTextures textures;
      Check(textures.Prepare(prepared) && textures.Size() == 1, "source texture staging failed");
      const auto texture = textures.Resolve(*plane.assets.atlas);
      auto foreign = *plane.assets.atlas;
      Check(texture && !textures.Resolve(foreign) && textures.Prepare(prepared) &&
                textures.Resolve(*plane.assets.atlas) == texture,
            "texture identity depended on UUID alone or churned without an owner change");
      auto oversized = prepared;
      for (std::uint64_t extra = 1; extra <= 16; ++extra) {
        auto copy = std::make_shared<editor::preview::SceneSpritePlane>(plane);
        auto replacement = std::make_shared<twod::Atlas>(*plane.assets.atlas);
        replacement->id.low += extra;
        copy->assets.atlas = replacement;
        oversized.sprites->entities.emplace(1000 + extra, copy);
        oversized.sprite_ranges.emplace(1000 + extra, editor::preview::MeshRange{});
      }
      Check(!textures.Prepare(oversized) && textures.Size() == 1 &&
                textures.Resolve(*plane.assets.atlas) == texture,
            "failed complete texture admission changed the active owner");
      Check(textures.Clear() && textures.Prepare(prepared) &&
                textures.Resolve(*plane.assets.atlas) > texture,
            "source texture IDs were reused across owner replacement");
      const auto candidates = editor::preview::NativeSceneProxyCandidates(scene, &prepared);
      const auto found =
          std::ranges::find_if(candidates, [&](const auto &v) { return v.entity == entity; });
      Check(found != candidates.end(), "source picking/framing candidate missing");
      const double c = std::sqrt(.5);
      Near(found->min.x, 10 - 6 * c);
      Near(found->max.x, 10 + 3 * c);
      Near(found->min.y, 20 - 6 * c);
      Near(found->max.y, 20);
      Near(found->min.z, 3);
      Near(found->max.z, 3);
      auto combined = prepared;
      auto meshBounds = std::make_shared<editor::MeshGeometry>();
      meshBounds->minimum = {-.1F, -1, -.5F};
      meshBounds->maximum = {.1F, 1, .5F};
      combined.entities.emplace(entity, editor::MeshAssetSnapshot{{777, 1}, 777, meshBounds});
      const auto unionCandidates = editor::preview::NativeSceneProxyCandidates(scene, &combined);
      const auto united =
          std::ranges::find_if(unionCandidates, [&](const auto &v) { return v.entity == entity; });
      Check(united != unionCandidates.end(), "combined source/mesh candidate missing");
      Near(united->min.x, 10 - 6 * c);
      Near(united->max.x, 10 + 3.3 * c);
      Near(united->min.y, 20 - 6 * c);
      Near(united->max.y, 20 + 2.2 * c);
      Near(united->min.z, 2.5);
      Near(united->max.z, 3.5);
      Write(root / "Content/Source.nx2sprite", twod::EncodeSpriteAsset(Source(1)));
      Check(content.Reimport(sourceId), "source reimport failed");
      const auto updated = editor::preview::CaptureSceneSpritePlanes(scene, content.Browser());
      Check(updated && updated->entities.size() == 1 &&
                updated->entities.at(entity)->assets.source != plane.assets.source,
            "source reimport reused a stale immutable owner");
      Check(!editor::preview::SceneSpritePlanesMatch(*retained, scene, content.Browser()) &&
                editor::preview::SceneSpritePlanesMatch(*updated, scene, content.Browser()),
            "content reimport was not part of source capture identity");
      Near(updated->entities.at(entity)->geometry.minimum[0], .5);
      Near(plane.geometry.minimum[0], -.5);
      auto broken = editor::SpriteSourceReference(sourceId);
      broken.data[0] = 0;
      Check(scene.SetOpaqueComponent(key, broken) &&
                !editor::preview::CaptureSceneSpritePlanes(scene, content.Browser()),
            "unsupported source metadata became a fabricated partial preview");
      Check(scene.SetOpaqueComponent(key, editor::SpriteSourceReference({909, 909})),
            "missing source fixture failed");
      const auto missing = editor::preview::CaptureSceneSpritePlanes(scene, content.Browser());
      Check(!editor::preview::SceneSpritePlanesMatch(*updated, scene, content.Browser()),
            "reference-only authoring change was not detected before drawing");
      Check(missing && missing->entities.empty() && missing->unavailable == 1,
            "missing source was silently rendered");
    }
    Check(!std::filesystem::exists(root) && !weakAtlas.expired(),
          "retained preview borrowed project files");
    Check(retained->entities.begin()->second->assets.atlas->rgba.size() > 0,
          "retained pixels were lost");
    retained.reset();
    Check(weakAtlas.expired(), "preview leaked its immutable asset owner");
    std::cout
        << "PASS:Scene source trim/Pivot/UV, exact mirrored sheared world bounds, Undo/Redo, "
           "immutable reimport/lifetime, missing/unsupported rejection and read-only capture\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
