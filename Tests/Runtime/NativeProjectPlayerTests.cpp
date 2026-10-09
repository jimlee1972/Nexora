#include "StaticView.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Options final {
  std::size_t instances{2}, materials{2}, vertices{3}, indices{3}, meshes{1};
  bool camera{true}, shear{};
  float intensity{1};
};
RuntimeBlob Cook(AssetUuid id, std::string_view type, ByteBuffer bytes,
                 std::vector<AssetUuid> dependencies = {}) {
  DerivedDataCache cache;
  auto blob = AssetCooker{}.Cook({id, std::string(type), std::move(dependencies), std::move(bytes)},
                                 "portable", "static-view-v1", cache);
  Require(bool(blob), "actual fixture cooking failed");
  return std::move(*blob);
}
StaticProjectPackage Fixture(Options options = {}) {
  StaticProjectPackage package{{1, 99}, {2, 99}, {}};
  std::vector<AssetUuid> dependencies;
  for (std::size_t index = 0; index < options.meshes; ++index) {
    CookedMesh mesh;
    mesh.vertices = {{{-0.7F, -0.7F, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}},
                     {{0.7F, -0.7F, 0}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}},
                     {{0, 0.7F, 0}, {0, 0, 1}, {0.5F, 1}, {1, 0, 0, 1}}};
    mesh.vertices.resize(options.vertices, mesh.vertices[0]);
    mesh.indices.resize(options.indices);
    for (std::size_t triangle = 0; triangle < mesh.indices.size(); triangle += 3) {
      mesh.indices[triangle] = 0;
      mesh.indices[triangle + 1] = 1;
      mesh.indices[triangle + 2] = 2;
    }
    const AssetUuid id{3, index + 1};
    const auto bytes = EncodeCookedMesh(mesh);
    Require(bool(bytes), "mesh fixture encoding failed");
    package.assets.push_back(Cook(id, kCookedMeshType, bytes.Value()));
    dependencies.push_back(id);
  }
  for (std::size_t index = 0; index < options.materials; ++index) {
    CookedScalarPbr material;
    material.base_color = index % 2 ? std::array<float, 3>{0, 1, 0} : std::array<float, 3>{1, 0, 0};
    material.emission = material.base_color;
    const AssetUuid id{4, index + 1};
    const auto bytes = EncodeCookedScalarPbr(material);
    Require(bool(bytes), "material fixture encoding failed");
    package.assets.push_back(Cook(id, kCookedScalarPbrType, bytes.Value()));
    dependencies.push_back(id);
  }
  World world;
  const auto scene = world.LoadScene("Native static acceptance");
  WorldCommandBuffer commands;
  if (options.camera) {
    const auto camera = world.CreateEntity(scene).id;
    commands.SetCamera(camera, CameraComponent{});
    commands.SetTransform(camera, {0, 0, 5});
  }
  const auto light = world.CreateEntity(scene).id;
  commands.SetLight(light, LightComponent{options.intensity});
  CookedScene cooked;
  Id parent{};
  for (std::size_t index = 0; index < options.instances; ++index) {
    const auto id = world.CreateEntity(scene).id;
    const AssetUuid mesh{3, index % options.meshes + 1};
    std::optional<AssetUuid> material;
    if (options.materials)
      material = AssetUuid{4, index % options.materials + 1};
    commands.SetMeshRenderer(id, MeshComponent{MeshResourceId(mesh), {material ? UINT64_MAX : 0}});
    Transform pose;
    pose.x = index % 2 ? 0.85 : -0.85;
    if (options.shear) {
      if (index == 0) {
        pose.sx = -2;
        pose.sy = 3;
        parent = id;
      } else {
        pose.qz = std::sin(0.39269908169872414);
        pose.qw = std::cos(0.39269908169872414);
        commands.SetParent(id, parent, false);
      }
    }
    commands.SetTransform(id, pose);
    cooked.bindings.push_back({id, mesh, material, MeshResourceId(mesh)});
  }
  Require(commands.Apply(world), "real World fixture authoring failed");
  cooked.world_snapshot = *world.SaveScene(scene);
  cooked.opaque = {
      {cooked.bindings[0].entity, 777, "absent.plugin", {std::byte{0}, std::byte{255}}}};
  const auto bytes = EncodeCookedScene(cooked);
  Require(bool(bytes), "scene fixture encoding failed");
  package.assets.push_back(Cook(package.scene, kCookedSceneType, bytes.Value(), dependencies));
  return package;
}
void Contracts() {
  auto package = Fixture({.shear = true});
  auto loaded = LoadStaticProjectPackage(package);
  Require(bool(loaded), "actual package fixture failed");
  auto view = nexora::player::PrepareStaticView(*loaded);
  Require(view && view->vertices.size() == 3 && view->indices.size() == 3 &&
              view->instances.size() == 2 && view->materials.size() == 3 &&
              view->batches[0].materialIndex == 1 && view->batches[1].materialIndex == 2 &&
              view->materials[1].baseColor[0] == 1 && view->materials[2].emission[1] == 1 &&
              view->vertices[0].tangent[3] == 1,
          "native geometry dedup/PBR/tangent conversion failed");
  for (std::size_t index = 0; index < loaded->RenderItems().size(); ++index)
    for (std::size_t row = 0; row < 4; ++row)
      for (std::size_t column = 0; column < 4; ++column)
        Require(
            (*view->instances[index].model_transform)[row * 4 + column] ==
                static_cast<float>(loaded->RenderItems()[index].world_transform[column * 4 + row]),
            "native exact mirrored/sheared affine matrix changed");
  const auto first = view->DrawData(*loaded, 1), resized = view->DrawData(*loaded, 2);
  Require(first && resized && first->pbr && !first->offscreen &&
              first->model_view_projection[0] != resized->model_view_projection[0] &&
              first->cameraPosition[2] == 5 && !view->DrawData(*loaded, 0) &&
              loaded->InactiveComponentCount() == 1,
          "native camera/resize/inactive contract failed");
  loaded.reset();
  package.assets.clear();
  Require(view->vertices[0].position[0] == -0.7F && view->materials[1].emission[0] == 1 &&
              view->instances[1].model_transform.has_value(),
          "native uploads borrowed destroyed package");
  const auto admit = [](Options options) {
    const auto candidate = LoadStaticProjectPackage(Fixture(options));
    Require(bool(candidate), "valid data rejected before native admission");
    std::string error;
    auto native = nexora::player::PrepareStaticView(*candidate, &error);
    Require(bool(native) == error.empty(), "native admission diagnostic missing");
    return native;
  };
  const auto instances = admit({.instances = 4096, .materials = 0});
  Require(instances && instances->instances.size() == 4096 &&
              !admit({.instances = 4097, .materials = 0}),
          "native exact instance budget failed");
  const auto palette = admit({.instances = 63, .materials = 63});
  Require(palette && palette->materials.size() == 64 && !admit({.instances = 64, .materials = 64}),
          "native exact palette budget failed");
  Require(admit({.vertices = 65535, .indices = 1048575}) &&
              !admit({.vertices = 32768, .meshes = 2}) && !admit({.indices = 524289, .meshes = 2}),
          "native shared/exact geometry budgets failed");
  Require(!admit({.camera = false}) && !admit({.intensity = 65505}),
          "native missing-camera/light failure hidden");
}
int Run(const std::filesystem::path &destination = {}) try {
  if (!destination.empty()) {
    const auto bytes = EncodeStaticProjectPackage(Fixture());
    Require(bool(bytes), "actual native fixture package encode failed");
    std::ofstream output(destination, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes->data()),
                 static_cast<std::streamsize>(bytes->size()));
    Require(bool(output), "actual native fixture file write failed");
    return 0;
  }
  Contracts();
  std::cout << "Actual native StaticView admission/ownership contracts passed\n";
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
  return argc == 3 && std::wstring_view(argv[1]) == L"--write-fixture"
             ? Run(std::filesystem::path(argv[2]))
             : 2;
}
#else
int main(int argc, char **argv) {
  if (argc == 1)
    return Run();
  return argc == 3 && std::string_view(argv[1]) == "--write-fixture"
             ? Run(std::filesystem::path(
                   std::u8string(argv[2], argv[2] + std::char_traits<char>::length(argv[2]))))
             : 2;
}
#endif
