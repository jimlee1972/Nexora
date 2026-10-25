#include "GameplayPlayer.h"
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
void LegacyInput() {
  nexora::player::PlayerGameplay context;
  using Nexora::Window::Key;
  using Nexora::Window::WindowEvent;
  using Nexora::Window::WindowEventType;
  std::array<WindowEvent, 3> pressed{};
  const std::array keys{Key::D, Key::W, Key::Space};
  for (std::size_t index = 0; index < pressed.size(); ++index) {
    pressed[index].type = WindowEventType::Key;
    pressed[index].value0 = static_cast<std::int32_t>(keys[index]);
    pressed[index].value1 = 1;
  }
  context.CaptureEvents(pressed, true, 7);
  const auto host = context.Host();
  NexoraInputSnapshot captured{};
  Require(host.capture_input(host.context, 0, &captured) == NEXORA_GAMEPLAY_OK &&
              captured.sequence == 7 && captured.move_x == 1 && captured.move_y == 1 &&
              captured.buttons == 1 && captured.reserved == 0,
          "legacy V3 WASD/Space snapshot changed meaning");
  context.CaptureEvents({}, true, 8);
  Require(context.input.move_x == 1 && context.input.buttons == 1,
          "legacy held keys lost between native events");
  pressed[0].value1 = 0;
  context.CaptureEvents(std::span(pressed).first(1), true, 9);
  Require(context.input.move_x == 0 && context.input.move_y == 1,
          "legacy key release did not clear movement");
  context.CaptureEvents({}, false, 10);
  Require(context.input.move_x == 0 && context.input.move_y == 0 && context.input.buttons == 0 &&
              context.input.reserved == 0,
          "legacy input did not clear held controls on focus loss");
  context.CaptureEvents({}, true, 11);
  Require(context.input.move_y == 0 && context.input.buttons == 0,
          "legacy held controls revived on focus regain");
  pressed[0].value0 = static_cast<std::int32_t>(Key::A);
  pressed[0].value1 = 1;
  pressed[1].value0 = static_cast<std::int32_t>(Key::S);
  context.CaptureEvents(std::span(pressed).first(2), true, 12);
  Require(context.input.move_x == -1 && context.input.move_y == -1 && context.input.buttons == 0,
          "legacy A/S movement signs changed");
  std::array<WindowEvent, 3> pointer_events{};
  pointer_events[0].type = WindowEventType::Pointer;
  pointer_events[0].value0 = 123;
  pointer_events[0].value1 = 456;
  pointer_events[1].type = WindowEventType::Wheel;
  pointer_events[1].value0 = -120;
  pointer_events[1].value1 = 240;
  pointer_events[2].type = WindowEventType::PointerButton;
  pointer_events[2].value1 = 1;
  context.CaptureEvents(pointer_events, true, 13);
  const auto input_api = context.actions.Api();
  NexoraGameplayPointerStateV1 pointer{};
  pointer.struct_size = sizeof(pointer);
  pointer.abi_version = 1;
  Require(input_api.read_pointer(input_api.context, 0, &pointer) == NEXORA_GAMEPLAY_OK &&
              pointer.x == 123 && pointer.y == 456 && pointer.wheel_x == -120 &&
              pointer.wheel_y == 240 && pointer.buttons == 1 && pointer.focused == 1,
          "native pointer/wheel events lost coordinates or wheel axes");
  Require(input_api.read_pointer(input_api.context, 0, &pointer) == NEXORA_GAMEPLAY_OK &&
              pointer.wheel_y == 240,
          "native pointer capture consumed wheel input");
  pointer_events[0].value0 = 130;
  pointer_events[0].value1 = 450;
  context.CaptureEvents(std::span(pointer_events).first(1), true, 14);
  Require(input_api.read_pointer(input_api.context, 0, &pointer) == NEXORA_GAMEPLAY_OK &&
              pointer.delta_x == 7 && pointer.delta_y == -6 && pointer.wheel_x == 0 &&
              pointer.wheel_y == 0 && pointer.buttons == 1,
          "native pointer motion did not preserve coordinates/clear per-frame wheel");
  context.CaptureEvents({}, false, 15);
  Require(input_api.read_pointer(input_api.context, 0, &pointer) == NEXORA_GAMEPLAY_OK &&
              pointer.buttons == 0 && pointer.focused == 0 && pointer.delta_x == 0 &&
              pointer.delta_y == 0,
          "native pointer focus loss did not clear buttons and motion");
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
  World clone{WorldKind::Play};
  const auto snapshot = loaded->WorldView().SaveScene(loaded->SceneId());
  Require(snapshot && clone.LoadSceneSnapshot(*snapshot), "gameplay world clone failed");
  Require(view->RefreshWorld(*loaded, clone), "cloned authored scene did not refresh");
  const auto entity = loaded->RenderItems()[0].entity;
  const auto before = (*view->instances[0].model_transform)[3];
  auto transform = clone.FindEntity(entity)->transform;
  transform.x += 2;
  WorldCommandBuffer changed;
  changed.SetTransform(entity, transform);
  Require(changed.Apply(clone) && view->RefreshWorld(*loaded, clone) &&
              (*view->instances[0].model_transform)[3] != before,
          "gameplay pose write did not refresh admitted instance");
  WorldCommandBuffer removed;
  removed.DestroyEntity(entity);
  Require(removed.Apply(clone) && !view->RefreshWorld(*loaded, clone),
          "destroyed authored instance silently drew stale geometry");
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
  const auto camera_less = LoadStaticProjectPackage(Fixture({.camera = false}));
  const auto game_view = nexora::player::PrepareSceneView(
      camera_less->RenderItems(), camera_less->WorldView(), camera_less->SceneId(), nullptr, true);
  Require(game_view && !game_view->camera && game_view->DrawData(camera_less->WorldView(), 1),
          "game default camera fallback failed");
  World empty_world{WorldKind::Play};
  const auto empty_scene = empty_world.LoadScene("Empty game scene");
  Require(empty_world.Activate(empty_scene), "empty scene activation");
  const auto empty_view =
      nexora::player::PrepareSceneView({}, empty_world, empty_scene, nullptr, true);
  Require(empty_view && empty_view->instances.empty() && empty_view->DrawData(empty_world, 1) &&
              !nexora::player::PrepareSceneView({}, empty_world, empty_scene),
          "game empty/default view changed legacy StaticView admission");
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
  LegacyInput();
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
