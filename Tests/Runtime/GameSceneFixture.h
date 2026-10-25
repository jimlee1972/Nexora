#pragma once
#include "Nexora/Runtime/GameProjectPackage.h"
#include <stdexcept>

namespace nexora::testscene {
inline constexpr runtime::AssetUuid kTitle{1, 1}, kLevel{1, 2}, kMesh{2, 2}, kMaterial{3, 3};
inline runtime::RuntimeBlob Cook(runtime::AssetUuid id, std::string_view type,
                                 runtime::ByteBuffer bytes,
                                 std::vector<runtime::AssetUuid> dependencies = {}) {
  runtime::DerivedDataCache cache;
  auto blob = runtime::AssetCooker{}.Cook(
      {id, std::string(type), std::move(dependencies), std::move(bytes)}, "portable",
      "game-scenes-v1", cache);
  if (!blob)
    throw std::runtime_error("Scene fixture cooking failed");
  return std::move(*blob);
}
inline runtime::RuntimeBlob Scene(runtime::AssetUuid id, std::string name, double x, double local,
                                  bool invalid_native_camera = false) {
  runtime::World world;
  const auto scene = world.LoadScene(std::move(name));
  const auto parent = world.CreateEntity(scene).id;
  const auto child = world.CreateEntity(scene).id;
  const auto camera = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer edits;
  edits.SetTransform(parent, {x, 0, 0});
  edits.SetTransform(child, {local, 0, 0});
  edits.SetParent(child, parent, false);
  edits.SetMeshRenderer(child, runtime::MeshComponent{runtime::MeshResourceId(kMesh), {0}});
  edits.SetTransform(camera, {x, 0, 5});
  runtime::CameraComponent camera_data{};
  if (invalid_native_camera)
    camera_data.vertical_field_of_view = 179.9999999999;
  edits.SetCamera(camera, camera_data);
  if (!edits.Apply(world))
    throw std::runtime_error("Scene fixture edits failed");
  runtime::CookedScene source{
      *world.SaveScene(scene), {{child, kMesh, kMaterial, runtime::MeshResourceId(kMesh)}}, {}};
  const auto bytes = runtime::EncodeCookedScene(source);
  if (!bytes)
    throw std::runtime_error("Scene fixture encoding failed");
  return Cook(id, runtime::kCookedSceneType, bytes.Value(), {kMaterial, kMesh});
}
inline runtime::GameProjectPackage Package(bool invalid_title_camera = false) {
  runtime::CookedMesh mesh;
  mesh.vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}},
                   {{1, 0, 0}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}},
                   {{0, 1, 0}, {0, 0, 1}, {0, 1}, {1, 0, 0, 1}}};
  mesh.indices = {0, 1, 2};
  runtime::CookedScalarPbr material;
  material.base_color = {1, 0, 0};
  material.emission = {1, 0, 0};
  return {{9, 9},
          kLevel,
          {{kTitle, "Title.scene"}, {kLevel, "Level01.scene"}},
          {Scene(kTitle, "Title", 1, 1, invalid_title_camera), Scene(kLevel, "Level01", 10, -1),
           Cook(kMesh, runtime::kCookedMeshType, runtime::EncodeCookedMesh(mesh).Value()),
           Cook(kMaterial, runtime::kCookedScalarPbrType,
                runtime::EncodeCookedScalarPbr(material).Value())}};
}
inline std::shared_ptr<const runtime::LoadedGameProject> Project() {
  auto project = runtime::LoadGameProjectPackage(Package());
  if (!project)
    throw std::runtime_error("Scene fixture package resolution failed");
  return std::make_shared<const runtime::LoadedGameProject>(std::move(*project));
}
} // namespace nexora::testscene
