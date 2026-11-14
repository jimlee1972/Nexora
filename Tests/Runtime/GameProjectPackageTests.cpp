#include "GameSceneFixture.h"
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
void LegacySourceWire() {
  auto package = nexora::testscene::Package();
  constexpr AssetUuid atlas{50, 1}, source{51, 1};
  ByteBuffer data{std::byte{1}};
  for (const auto word : {source.high, source.low})
    for (unsigned i = 0; i < 8; ++i)
      data.push_back(static_cast<std::byte>(word >> (8 * i)));
  for (unsigned i = 0; i < 2; ++i) {
    auto scene = DecodeCookedScene(package.assets[i].payload);
    scene.Value().opaque.push_back({scene.Value().bindings[0].entity,
                                    kGameSpriteSourceReferenceType,
                                    std::string(kGameSpriteSourceReferenceName), data});
    package.assets[i] = nexora::testscene::Cook(
        package.assets[i].id, kCookedSceneType, EncodeCookedScene(scene.Value()).Value(),
        {nexora::testscene::kMesh, nexora::testscene::kMaterial, source});
  }
  package.assets.push_back(nexora::testscene::Cook(atlas, "sprite_atlas", {std::byte{1}}));
  package.assets.push_back(
      nexora::testscene::Cook(source, "sprite_source", {std::byte{2}}, {atlas}));
  const auto bytes = EncodeGameProjectPackage(package);
  Require(bytes && bytes->size() == 1712 && (*bytes)[12] == std::byte{3},
          "legacy source capability changed");
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto byte : *bytes)
    hash = (hash ^ std::to_integer<unsigned char>(byte)) * 1099511628211ULL;
  // Independently frozen using the pre-capability4 source-capable Runtime library.
  Require(hash == 0x602d2d4a2f78135bULL, "legacy source-capability wire bytes changed");
}
void AnimationRoots() {
  constexpr AssetUuid atlas{10, 1}, source{11, 1}, clip{12, 1}, legacy{13, 1};
  auto package = Fixture();
  // Runtime deliberately validates framing/graph without the optional TwoD payload codecs.
  package.assets.push_back(Cook(atlas, "sprite_atlas", {std::byte{1}}));
  package.assets.push_back(Cook(source, "sprite_source", {std::byte{2}}, {atlas}));
  package.assets.push_back(Cook(clip, "sprite_clip", {std::byte{3}}, {source}));
  package.assets.push_back(Cook(legacy, "sprite_clip", {std::byte{4}}, {atlas}));
  package.animation_clips = {legacy, clip};
  const auto bytes = EncodeGameProjectPackage(package);
  const auto loaded = bytes ? DecodeGameProjectPackage(*bytes) : std::nullopt;
  Require(loaded && loaded->Capability() == 4 && loaded->AnimationClips().size() == 2 &&
              loaded->AnimationClips()[0] == clip && loaded->SpriteAssets().size() == 4,
          "explicit source/legacy Clip roots lost in canonical package");
  auto reordered = package;
  std::ranges::reverse(reordered.animation_clips);
  std::ranges::reverse(reordered.assets);
  Require(EncodeGameProjectPackage(reordered) == bytes, "animation root order changed wire");
  for (unsigned mutation = 0; mutation < 11; ++mutation) {
    auto bad = package;
    if (mutation == 0)
      bad.animation_clips.clear();
    if (mutation == 1)
      bad.animation_clips.push_back(clip);
    if (mutation == 2)
      bad.animation_clips.push_back({});
    if (mutation == 3)
      bad.animation_clips.push_back({99, 99});
    if (mutation == 4)
      bad.animation_clips = {source, legacy};
    if (mutation == 5)
      bad.assets[6] = Cook(clip, "sprite_clip", {std::byte{3}}, {clip});
    if (mutation == 6)
      bad.assets[6] = Cook(clip, "sprite_clip", {std::byte{3}}, {source, atlas});
    if (mutation == 7)
      bad.assets[6] = Cook(clip, "sprite_clip", {std::byte{3}}, {source, source});
    if (mutation == 8)
      bad.assets[6] = Cook(clip, "sprite_clip", {std::byte{3}}, {});
    if (mutation == 9)
      bad.assets[6].payload[0] ^= std::byte{1};
    if (mutation == 10)
      bad.animation_clips.resize(4097, clip);
    Require(!LoadGameProjectPackage(bad), "invalid or unauthorized Clip closure admitted");
  }
  // A correctly checksummed capability4 wire must still enforce root ordering/count/bounds.
  const auto repair_checksum = [](ByteBuffer &wire) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t i = 0; i < wire.size() - 8; ++i)
      hash = (hash ^ std::to_integer<unsigned char>(wire[i])) * 1099511628211ULL;
    for (unsigned i = 0; i < 8; ++i)
      wire[wire.size() - 8 + i] = static_cast<std::byte>(hash >> (i * 8));
  };
  std::size_t roots_offset = 52;
  for (const auto &entry : loaded->Scenes())
    roots_offset += 20 + entry.path.size();
  for (unsigned mutation = 0; mutation < 5; ++mutation) {
    auto bad = *bytes;
    if (mutation == 0)
      bad[roots_offset] = std::byte{0};
    if (mutation == 1)
      std::copy_n(bad.begin() + static_cast<std::ptrdiff_t>(roots_offset + 4), 16,
                  bad.begin() + static_cast<std::ptrdiff_t>(roots_offset + 20));
    if (mutation == 2)
      bad[roots_offset + 1] = std::byte{0xff};
    if (mutation == 3)
      bad[12] = std::byte{3};
    if (mutation == 4)
      std::fill_n(bad.begin() + static_cast<std::ptrdiff_t>(roots_offset + 4), 16, std::byte{});
    repair_checksum(bad);
    Require(!DecodeGameProjectPackage(bad), "checksummed invalid animation root wire admitted");
  }
}
void Run() {
  LegacySourceWire();
  AnimationRoots();
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
