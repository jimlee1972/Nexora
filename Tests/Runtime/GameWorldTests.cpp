// API-M5 conformance: GameWorld's opaque-handle facade over World -- spawn/
// destroy, get/set, batch query, scene load, and the input-snapshot helper.
#include "Nexora/Game/GameWorld.h"

#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::game;
using namespace nexora::runtime;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int Run() {
  GameWorld world;
  const auto scene = world.LoadScene("GameWorldTestScene");
  Require(world.ActivateScene(scene), "scene must activate");

  // ---- Spawn / get / component flags ----
  const auto plain = world.SpawnEntity(scene);
  const auto snapshot = world.GetEntity(plain);
  Require(snapshot.has_value() && snapshot->id == plain, "GetEntity must find a spawned entity");
  Require(!snapshot->has_camera && !snapshot->has_light && !snapshot->has_mesh_renderer,
          "a plain spawn must have no components attached");

  EntitySpawnDescriptor descriptor;
  descriptor.transform = {1.0, 2.0, 3.0};
  descriptor.camera = CameraComponent{75.0, 0.05, 500.0};
  descriptor.mesh_renderer = MeshComponent{42, {7}};
  const auto camera_entity = world.SpawnEntity(scene, descriptor);
  const auto camera_snapshot = world.GetEntity(camera_entity);
  Require(camera_snapshot.has_value(), "camera entity must be found");
  Require(camera_snapshot->has_camera && !camera_snapshot->has_light &&
              camera_snapshot->has_mesh_renderer,
          "spawn descriptor must set exactly the requested component flags");
  Require(camera_snapshot->camera.vertical_field_of_view == 75.0 &&
              camera_snapshot->mesh.mesh == 42 && camera_snapshot->mesh.material.shader == 7,
          "spawn descriptor component data must round trip");
  Require(camera_snapshot->transform.x == 1.0 && camera_snapshot->transform.y == 2.0 &&
              camera_snapshot->transform.z == 3.0,
          "spawn descriptor transform must round trip");

  Require(!world.GetEntity(999999).has_value(), "GetEntity on a missing id must be nullopt");

  // ---- SetTransform ----
  Require(world.SetTransform(plain, {9.0, 8.0, 7.0}), "SetTransform must succeed on a live entity");
  Require(world.GetEntity(plain)->transform.x == 9.0, "SetTransform must be visible on read-back");
  Require(!world.SetTransform(999999, {}), "SetTransform on a missing id must fail");

  // ---- IsAlive / DestroyEntity ----
  Require(world.IsAlive(plain), "a spawned entity must be alive");
  Require(world.DestroyEntity(plain), "DestroyEntity must succeed on a live entity");
  Require(!world.IsAlive(plain), "a destroyed entity must not be alive");
  Require(!world.GetEntity(plain).has_value(), "a destroyed entity must not be found");
  Require(!world.DestroyEntity(plain), "destroying an already-destroyed entity must fail");

  // ---- Batch query (OR semantics) ----
  EntitySpawnDescriptor light_descriptor;
  light_descriptor.light = LightComponent{2.5F};
  const auto light_entity = world.SpawnEntity(scene, light_descriptor);
  const auto all = world.Query(scene);
  Require(all.size() == 2, "an unfiltered query must return every live entity in the scene");
  const auto cameras_or_lights =
      world.Query(scene, GameWorld::kQueryCamera | GameWorld::kQueryLight);
  Require(cameras_or_lights.size() == 2, "OR-mask query must match either flag, not both");
  const auto lights_only = world.Query(scene, GameWorld::kQueryLight);
  Require(lights_only.size() == 1 && lights_only.front() == light_entity,
          "a light-only mask must exclude the camera entity");
  const auto other_scene = world.LoadScene("OtherScene");
  Require(world.Query(other_scene).empty(), "a query against an empty scene must be empty");
  Require(world.Query(123456789).empty(),
          "a query against a missing scene must be empty, not throw");

  // ---- Input snapshot ----
  InputSystem input_system;
  Require(input_system.Assign(1, InputDeviceKind::Keyboard, 0), "device assignment must succeed");
  RawInputEvent event;
  event.device = InputDeviceKind::Keyboard;
  event.device_id = 0;
  event.kind = InputEventKind::Button;
  event.control = "Space";
  event.value = 1.0F;
  Require(input_system.Push(event), "pushing a raw input event must succeed");
  const auto captured = CaptureInput(input_system, 1);
  Require(captured.events.size() == 1 && captured.events.front().control == "Space",
          "CaptureInput must return exactly the events pushed for that user");
  const auto captured_again = CaptureInput(input_system, 1);
  Require(captured_again.events.empty(),
          "CaptureInput must consume events, not leave them for the next capture");

  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
