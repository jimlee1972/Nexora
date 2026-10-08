#include "Nexora/Runtime/ProjectPackage.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
constexpr AssetUuid kScene{1, 1}, kMesh{2, 2}, kRed{3, 3}, kGreen{4, 4};
RuntimeBlob Cook(AssetUuid id, std::string_view type, const ByteBuffer &payload,
                 std::vector<AssetUuid> dependencies = {}) {
  DerivedDataCache cache;
  auto cooked = AssetCooker{}.Cook({id, std::string(type), std::move(dependencies), payload},
                                   "portable", "static-view-v1", cache);
  Require(cooked.has_value(), "real AssetCooker rejected fixture");
  return std::move(*cooked);
}
StaticProjectPackage Fixture() {
  CookedMesh mesh;
  mesh.vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}},
                   {{1, 0, 0}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}},
                   {{0, 1, 0}, {0, 0, 1}, {0, 1}, {1, 0, 0, 1}}};
  mesh.indices = {0, 1, 2};
  const auto mesh_bytes = EncodeCookedMesh(mesh);
  Require(bool(mesh_bytes), "cooked mesh fixture encode failed");
  CookedScalarPbr red, green;
  red.base_color = {1, 0, 0};
  green.base_color = {0, 1, 0};
  const auto red_bytes = EncodeCookedScalarPbr(red), green_bytes = EncodeCookedScalarPbr(green);
  Require(bool(red_bytes) && bool(green_bytes), "material fixture encode failed");
  World world;
  const auto scene = world.LoadScene("Current project static scene");
  const auto first = world.CreateEntity(scene).id, second = world.CreateEntity(scene).id;
  WorldCommandBuffer commands;
  commands.SetMeshRenderer(first, MeshComponent{MeshResourceId(kMesh), {UINT64_MAX - 7}});
  commands.SetMeshRenderer(second, MeshComponent{MeshResourceId(kMesh), {0}});
  commands.SetTransform(first, {1, 2, 3});
  commands.SetTransform(second, {4, 5, 6});
  commands.SetParent(second, first, false);
  commands.SetCamera(first, CameraComponent{});
  Require(commands.Apply(world), "real authored scene fixture failed");
  CookedScene payload;
  payload.world_snapshot = *world.SaveScene(scene);
  payload.bindings = {{first, kMesh, kRed, MeshResourceId(kMesh)},
                      {second, kMesh, kGreen, MeshResourceId(kMesh)}};
  payload.opaque = {{first, 111, "old.plugin.component", {std::byte{0}, std::byte{255}}}};
  const auto scene_bytes = EncodeCookedScene(payload);
  Require(bool(scene_bytes), "cooked scene fixture encode failed");
  return {{9, 9},
          kScene,
          {Cook(kGreen, kCookedScalarPbrType, green_bytes.Value()),
           Cook(kScene, kCookedSceneType, scene_bytes.Value(), {kRed, kMesh, kGreen}),
           Cook(kMesh, kCookedMeshType, mesh_bytes.Value()),
           Cook(kRed, kCookedScalarPbrType, red_bytes.Value())}};
}
RuntimeBlob &SceneBlob(StaticProjectPackage &package) {
  return *std::ranges::find(package.assets, kScene, &RuntimeBlob::id);
}
StaticProjectPackage LargeFixture(std::size_t count = 1000, bool chain = false) {
  auto package = Fixture();
  World world;
  const auto scene = world.LoadScene("One thousand instances of the current project mesh");
  CookedScene payload;
  WorldCommandBuffer commands;
  Id previous{};
  for (std::size_t index = 0; index < count; ++index) {
    const auto entity = world.CreateEntity(scene).id;
    commands.SetMeshRenderer(entity, MeshComponent{MeshResourceId(kMesh), {0}});
    if (chain) {
      commands.SetTransform(entity, {1, 0, 0});
      if (previous)
        commands.SetParent(entity, previous, false);
    }
    previous = entity;
    payload.bindings.push_back({entity, kMesh, std::nullopt, MeshResourceId(kMesh)});
  }
  Require(commands.Apply(world), "large authored scene failed");
  payload.world_snapshot = *world.SaveScene(scene);
  const auto encoded = EncodeCookedScene(payload);
  Require(bool(encoded), "large cooked scene failed");
  SceneBlob(package) = Cook(kScene, kCookedSceneType, encoded.Value(), {kMesh});
  std::erase_if(package.assets,
                [](const auto &blob) { return blob.id == kRed || blob.id == kGreen; });
  return package;
}
void ReplaceScene(StaticProjectPackage &package, const CookedScene &scene) {
  const auto bytes = EncodeCookedScene(scene);
  Require(bool(bytes), "modified scene fixture encode failed");
  const auto dependencies = SceneBlob(package).dependencies;
  SceneBlob(package) = Cook(kScene, kCookedSceneType, bytes.Value(), dependencies);
}
std::uint64_t Hash(std::span<const std::byte> bytes) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto byte : bytes) {
    hash ^= std::to_integer<std::uint8_t>(byte);
    hash *= 1099511628211ULL;
  }
  return hash;
}
void Rehash(ByteBuffer &bytes) {
  const auto hash = Hash(std::span(bytes).first(bytes.size() - 8));
  for (std::size_t index = 0; index < 8; ++index)
    bytes[bytes.size() - 8 + index] = static_cast<std::byte>(hash >> (8 * index));
}
void Contracts() {
  auto package = Fixture();
  std::string error = "stale";
  auto accepted = LoadStaticProjectPackage(package, &error);
  Require(bool(accepted) && error.empty() && accepted->WorldView().Kind() == WorldKind::Play &&
              accepted->RenderItems().size() == 2 && accepted->InactiveComponentCount() == 1,
          "real static candidate did not load");
  const auto items = accepted->RenderItems();
  Require(items[0].mesh == items[1].mesh && items[0].material != items[1].material &&
              items[0].material->base_color[0] == 1 && items[1].material->base_color[1] == 1 &&
              items[1].world_transform[12] == 5 && items[1].world_transform[13] == 7 &&
              accepted->WorldView().FindEntity(items[0].entity)->mesh_data.material.shader ==
                  UINT64_MAX - 7,
          "actual geometry/material/hierarchy/full shader identity was not resolved");
  Require(accepted->SceneData().opaque[0].data == ByteBuffer({std::byte{0}, std::byte{255}}),
          "inactive unknown component bytes changed");
  Require(accepted->SceneData().world_snapshot ==
              DecodeCookedScene(SceneBlob(package).payload).Value().world_snapshot,
          "exact saved Runtime scene snapshot text changed");
  const auto encoded = EncodeStaticProjectPackage(package, &error);
  Require(bool(encoded) && bool(DecodeStaticProjectPackage(*encoded, &error)),
          "real NXAB disk package did not round trip");
  std::ranges::reverse(package.assets);
  Require(EncodeStaticProjectPackage(package) == encoded, "UUID ordering is not deterministic");
  std::ranges::reverse(SceneBlob(package).dependencies);
  Require(EncodeStaticProjectPackage(package) == encoded,
          "equivalent dependency sets changed canonical package bytes");
  for (const auto length : {std::size_t{0}, std::size_t{7}, std::size_t{59}, encoded->size() - 1})
    Require(!DecodeStaticProjectPackage(std::span(*encoded).first(length), &error) &&
                !error.empty(),
            "truncated package accepted");
  auto damaged = *encoded;
  damaged[damaged.size() / 2] ^= std::byte{1};
  Require(!DecodeStaticProjectPackage(damaged), "corrupt checksum accepted");
  damaged = *encoded;
  damaged[8] = std::byte{2};
  Rehash(damaged);
  Require(!DecodeStaticProjectPackage(damaged), "unknown outer schema accepted");
  damaged = *encoded;
  // First NXAB begins at 60; dependency count follows magic/version/UUID/type string.
  const auto type_length = std::to_integer<std::uint8_t>(damaged[84]);
  const auto dependency_offset = std::size_t{88} + type_length;
  for (std::size_t index = 0; index < 4; ++index)
    damaged[dependency_offset + index] = std::byte{255};
  Rehash(damaged);
  Require(!DecodeStaticProjectPackage(damaged), "unbounded NXAB dependency allocation accepted");
  auto invalid = Fixture();
  invalid.assets.push_back(invalid.assets.front());
  Require(!LoadStaticProjectPackage(invalid), "duplicate full UUID accepted");
  invalid = Fixture();
  std::erase_if(invalid.assets, [](const auto &blob) { return blob.id == kRed; });
  Require(!LoadStaticProjectPackage(invalid), "missing referenced material accepted");
  invalid = Fixture();
  SceneBlob(invalid).dependencies.pop_back();
  Require(!LoadStaticProjectPackage(invalid), "incomplete dependency closure accepted");
  invalid = Fixture();
  invalid.assets[0].type = "unknown.asset.v1";
  Require(!LoadStaticProjectPackage(invalid), "unknown asset schema accepted");
  invalid = Fixture();
  auto scene = DecodeCookedScene(SceneBlob(invalid).payload).Value();
  scene.bindings[0].material.reset();
  ReplaceScene(invalid, scene);
  Require(!LoadStaticProjectPackage(invalid), "unsupported legacy shader silently invented");
  invalid = Fixture();
  scene = DecodeCookedScene(SceneBlob(invalid).payload).Value();
  scene.opaque.push_back({scene.bindings[0].entity, 0x45444d41544c0001ULL, "editor.material.asset",
                          ByteBuffer(17, std::byte{2})});
  ReplaceScene(invalid, scene);
  Require(!LoadStaticProjectPackage(invalid), "unknown material reference version accepted");
  invalid = Fixture();
  scene = DecodeCookedScene(SceneBlob(invalid).payload).Value();
  ByteBuffer reference{std::byte{1}};
  for (const auto value : {kRed.high, kRed.low})
    for (std::size_t index = 0; index < 8; ++index)
      reference.push_back(static_cast<std::byte>(value >> (8 * index)));
  scene.opaque.push_back(
      {scene.bindings[0].entity, 0x45444d41544c0001ULL, "editor.material.asset", reference});
  ReplaceScene(invalid, scene);
  const auto with_reference = LoadStaticProjectPackage(invalid);
  Require(bool(with_reference) && with_reference->InactiveComponentCount() == 1,
          "recognized material reference was lost or reported as inactive");
  scene.opaque.back().data[1] = std::byte{4};
  ReplaceScene(invalid, scene);
  Require(!LoadStaticProjectPackage(invalid), "mismatching material metadata accepted");
  invalid = Fixture();
  const auto collision_high = kMesh.high ^ 1;
  const AssetUuid collision{collision_high,
                            kMesh.low ^ std::rotl(kMesh.high, 23) ^ std::rotl(collision_high, 23)};
  Require(collision != kMesh && MeshResourceId(collision) == MeshResourceId(kMesh),
          "resource collision fixture failed");
  const auto mesh_blob = *std::ranges::find(invalid.assets, kMesh, &RuntimeBlob::id);
  invalid.assets.push_back(Cook(collision, kCookedMeshType, mesh_blob.payload));
  Require(!LoadStaticProjectPackage(invalid), "mesh resource collision accepted");
  const auto large = LoadStaticProjectPackage(LargeFixture());
  Require(bool(large) && large->RenderItems().size() == 1000 && !large->RenderItems()[0].material &&
              large->RenderItems()[0].mesh == large->RenderItems()[999].mesh,
          "neutral static material or large shared-geometry scene rejected");
  const auto deep = LoadStaticProjectPackage(LargeFixture(3000, true));
  Require(bool(deep) && deep->RenderItems().size() == 3000 &&
              deep->RenderItems().back().world_transform[12] == 3000 &&
              deep->RenderItems().back().mesh == deep->RenderItems().front().mesh,
          "deep static hierarchy did not resolve iteratively with exact parent/local matrices");
  auto sheared_package = Fixture();
  auto sheared_scene = DecodeCookedScene(SceneBlob(sheared_package).payload).Value();
  World transformed;
  const auto transformed_scene = transformed.LoadSceneSnapshot(sheared_scene.world_snapshot);
  Require(bool(transformed_scene), "mirrored hierarchy fixture load failed");
  Transform parent_transform{1, 2, 3}, child_transform{4, 5, 6};
  parent_transform.qy = std::sqrt(0.5);
  parent_transform.qw = std::sqrt(0.5);
  parent_transform.sx = -2;
  parent_transform.sy = 3;
  child_transform.qz = std::sin(0.39269908169872414);
  child_transform.qw = std::cos(0.39269908169872414);
  WorldCommandBuffer transforms;
  transforms.SetTransform(sheared_scene.bindings[0].entity, parent_transform);
  transforms.SetTransform(sheared_scene.bindings[1].entity, child_transform);
  Require(transforms.Apply(transformed), "mirrored/sheared fixture transform failed");
  sheared_scene.world_snapshot = *transformed.SaveScene(*transformed_scene);
  ReplaceScene(sheared_package, sheared_scene);
  const auto sheared = LoadStaticProjectPackage(sheared_package);
  Require(bool(sheared), "mirrored/sheared cooked candidate rejected");
  const auto &actual_matrix = sheared->RenderItems()[1].world_transform;
  const auto expected_matrix = sheared->WorldView().WorldMatrix(sheared->RenderItems()[1].entity);
  Require(bool(expected_matrix) && actual_matrix == *expected_matrix &&
              std::abs(actual_matrix[0] * actual_matrix[4] + actual_matrix[1] * actual_matrix[5] +
                       actual_matrix[2] * actual_matrix[6]) > 1,
          "exact sheared/mirrored matrix was replaced by lossy composed TRS");
  parent_transform = {};
  child_transform = {};
  parent_transform.sx = 1e200;
  child_transform.sx = 1e200;
  WorldCommandBuffer overflow;
  overflow.SetTransform(sheared_scene.bindings[0].entity, parent_transform);
  overflow.SetTransform(sheared_scene.bindings[1].entity, child_transform);
  Require(overflow.Apply(transformed), "finite local overflow fixture failed");
  sheared_scene.world_snapshot = *transformed.SaveScene(*transformed_scene);
  ReplaceScene(sheared_package, sheared_scene);
  Require(!LoadStaticProjectPackage(sheared_package), "nonfinite hierarchy matrix accepted");
  Require(accepted->WorldView().FindEntity(items[0].entity)->mesh_data.material.shader ==
                  UINT64_MAX - 7 &&
              accepted->SceneData().opaque[0].data[1] == std::byte{255},
          "failed candidate damaged previously accepted package");
  auto owned_mesh = accepted->ResolveMesh(MeshResourceId(kMesh));
  package.assets.clear();
  accepted.reset();
  Require(owned_mesh && owned_mesh->vertices.size() == 3 && owned_mesh->indices[2] == 2,
          "resolved geometry lost ownership after package replacement");
}
int Run(std::string_view action = {}, const std::filesystem::path &destination = {}) try {
  if (action == "--write-fixture" || action == "--write-large-fixture") {
    const auto encoded =
        EncodeStaticProjectPackage(action == "--write-fixture" ? Fixture() : LargeFixture());
    Require(bool(encoded), "fixture package encode failed");
    std::ofstream output(destination, std::ios::binary);
    output.write(reinterpret_cast<const char *>(encoded->data()),
                 static_cast<std::streamsize>(encoded->size()));
    Require(bool(output), "fixture package write failed");
    return 0;
  }
  Contracts();
  std::cout << "Static project package contracts passed\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  if (argc == 1)
    return Run();
  if (argc != 3)
    return 2;
  const auto action = std::wstring_view(argv[1]);
  if (action != L"--write-fixture" && action != L"--write-large-fixture")
    return 2;
  return Run(action == L"--write-fixture" ? "--write-fixture" : "--write-large-fixture",
             std::filesystem::path(argv[2]));
}
#else
int main(int argc, char **argv) {
  if (argc == 1)
    return Run();
  if (argc != 3)
    return 2;
  const auto action = std::string_view(argv[1]);
  if (action != "--write-fixture" && action != "--write-large-fixture")
    return 2;
  return Run(action, std::filesystem::path(std::u8string(
                         argv[2], argv[2] + std::char_traits<char>::length(argv[2]))));
}
#endif
