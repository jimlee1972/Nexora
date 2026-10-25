#include "Nexora/Runtime/GameProjectPackage.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
constexpr AssetUuid kTitle{1, 1}, kLevel{1, 2}, kMesh{2, 2}, kMaterial{3, 3};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
RuntimeBlob Cook(AssetUuid id, std::string_view type, ByteBuffer payload,
                 std::vector<AssetUuid> dependencies = {}) {
  DerivedDataCache cache;
  auto result =
      AssetCooker{}.Cook({id, std::string(type), std::move(dependencies), std::move(payload)},
                         "portable", "game-scenes-v1", cache);
  Require(result.has_value(), "real cooker fixture failed");
  return std::move(*result);
}
RuntimeBlob Scene(AssetUuid id, std::string name, double x) {
  World world;
  const auto scene = world.LoadScene(std::move(name));
  const auto entity = world.CreateEntity(scene).id;
  WorldCommandBuffer commands;
  commands.SetMeshRenderer(entity, MeshComponent{MeshResourceId(kMesh), {0}});
  commands.SetCamera(entity, CameraComponent{});
  commands.SetTransform(entity, {x, 0, 0});
  Require(commands.Apply(world), "scene fixture edit failed");
  CookedScene cooked{
      *world.SaveScene(scene), {{entity, kMesh, kMaterial, MeshResourceId(kMesh)}}, {}};
  const auto bytes = EncodeCookedScene(cooked);
  Require(bool(bytes), "scene fixture encode failed");
  return Cook(id, kCookedSceneType, bytes.Value(), {kMaterial, kMesh});
}
GameProjectPackage Fixture() {
  CookedMesh mesh;
  mesh.vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}},
                   {{1, 0, 0}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}},
                   {{0, 1, 0}, {0, 0, 1}, {0, 1}, {1, 0, 0, 1}}};
  mesh.indices = {0, 1, 2};
  return {{9, 9},
          kLevel,
          {{kTitle, "Scenes/Title.scene"}, {kLevel, "Scenes/Level01.scene"}},
          {Scene(kTitle, "Title", 1), Scene(kLevel, "Level01", 10),
           Cook(kMesh, kCookedMeshType, EncodeCookedMesh(mesh).Value()),
           Cook(kMaterial, kCookedScalarPbrType, EncodeCookedScalarPbr({}).Value())}};
}
void Run() {
  const auto package = Fixture();
  std::string error;
  const auto encoded = EncodeGameProjectPackage(package, &error);
  Require(encoded.has_value(), error.c_str());
  auto loaded = DecodeGameProjectPackage(*encoded, &error);
  Require(loaded && loaded->StartupScene() == kLevel && loaded->Scenes().size() == 2 &&
              loaded->AssetCount() == 4,
          "non-Main catalog startup/counts incorrect");
  const auto title = loaded->ResolveScene(kTitle),
             level = loaded->ResolveScene("Scenes/Level01.scene");
  Require(title && level && title->RenderItems().size() == 1 && level->RenderItems().size() == 1 &&
              title->RenderItems()[0].mesh == level->RenderItems()[0].mesh &&
              title->RenderItems()[0].material == level->RenderItems()[0].material,
          "shared cooked assets were duplicated in resolved scene templates");
  Require(title->RenderItems()[0].entity == level->RenderItems()[0].entity,
          "fixture must exercise overlapping authored entity IDs");
  auto permutation = package;
  std::ranges::reverse(permutation.scenes);
  std::ranges::reverse(permutation.assets);
  for (auto &blob : permutation.assets)
    std::ranges::reverse(blob.dependencies);
  Require(EncodeGameProjectPackage(permutation) == encoded, "game wire is not canonical");
  Require(!DecodeStaticProjectPackage(*encoded), "game package masquerades as StaticView");
  auto bad = package;
  bad.start_scene = {99, 99};
  Require(!LoadGameProjectPackage(bad), "missing startup accepted");
  bad = package;
  bad.scenes.push_back(bad.scenes.front());
  Require(!LoadGameProjectPackage(bad), "duplicate catalog ID accepted");
  bad = package;
  bad.scenes[1].path = "scenes/TITLE.scene";
  Require(!LoadGameProjectPackage(bad), "case-alias catalog accepted");
  bad = package;
  bad.scenes[1].path = "../Level.scene";
  Require(!LoadGameProjectPackage(bad), "traversal locator accepted");
  bad = package;
  bad.assets[0].dependencies.push_back(kLevel);
  Require(!LoadGameProjectPackage(bad), "cross-scene dependency accepted");
  bad = package;
  bad.assets.pop_back();
  Require(!LoadGameProjectPackage(bad), "missing shared material accepted");
  bad = package;
  bad.assets.push_back(Cook({77, 88}, kCookedScalarPbrType, EncodeCookedScalarPbr({}).Value()));
  Require(!LoadGameProjectPackage(bad), "unused extra asset accepted");
  bad = package;
  bad.assets[0].content_hash = "0000000000000000";
  Require(!LoadGameProjectPackage(bad), "corrupt scene integrity accepted");
  bad = package;
  auto opaque = DecodeCookedScene(bad.assets[0].payload);
  Require(bool(opaque), "opaque fixture decode");
  opaque.Value().opaque.push_back(
      {opaque.Value().bindings[0].entity, 91, "missing.gameplay.component", {std::byte{1}}});
  const auto opaque_bytes = EncodeCookedScene(opaque.Value());
  Require(bool(opaque_bytes), "opaque fixture must be a valid cooked scene");
  bad.assets[0] = Cook(kTitle, kCookedSceneType, opaque_bytes.Value(), {kMaterial, kMesh});
  Require(!LoadGameProjectPackage(bad, &error) && error.find("opaque") != error.npos,
          "inactive required gameplay behavior accepted");
  bad = package;
  const AssetUuid collision{kMesh.high ^ 1, kMesh.low ^ std::rotl(std::uint64_t{1}, 23)};
  Require(MeshResourceId(collision) == MeshResourceId(kMesh), "collision fixture invalid");
  bad.assets.push_back(Cook(collision, kCookedMeshType, package.assets[2].payload));
  Require(!LoadGameProjectPackage(bad), "global mesh resource collision accepted");
  auto broken = *encoded;
  broken[12] = std::byte{1};
  Require(!DecodeGameProjectPackage(broken), "bad capability/checksum accepted");
  broken = *encoded;
  broken.push_back(std::byte{});
  Require(!DecodeGameProjectPackage(broken) &&
              !DecodeGameProjectPackage(std::span(*encoded).first(encoded->size() - 1)),
          "trailing/truncated game package accepted");
  loaded.reset();
  Require(title->RenderItems()[0].mesh->vertices.size() == 3 &&
              level->SceneData().world_snapshot.find("Level01") != std::string::npos,
          "owning template/resource did not outlive catalog");
  std::cout
      << "Multi-scene catalog, canonical wire, shared ownership and failure admission passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write") {
      const auto bytes = EncodeGameProjectPackage(Fixture());
      Require(bytes.has_value(), "fixture package encode");
      const auto path = std::filesystem::path(
          std::u8string(argv[2], argv[2] + std::char_traits<char>::length(argv[2])));
      std::ofstream output(path, std::ios::binary);
      output.write(reinterpret_cast<const char *>(bytes->data()),
                   static_cast<std::streamsize>(bytes->size()));
      Require(static_cast<bool>(output), "fixture package write");
    } else
      Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
