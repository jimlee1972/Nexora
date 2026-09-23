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

  // ---- Component mutation/removal ----
  Require(world.SetLight(plain, LightComponent{4.0F}), "SetLight must attach a light");
  Require(world.GetEntity(plain)->has_light && world.GetEntity(plain)->light.intensity == 4.0F,
          "SetLight data must round trip");
  Require(world.SetLight(plain, std::nullopt), "SetLight(nullopt) must remove a light");
  Require(!world.GetEntity(plain)->has_light, "removed light must not remain queryable");
  Require(world.SetCamera(plain, CameraComponent{80.0, 0.2, 900.0}),
          "SetCamera must attach a camera");
  Require(world.SetMeshRenderer(plain, MeshComponent{88, {99}}),
          "SetMeshRenderer must attach a renderer");

  // ---- Deferred mutation batch ----
  DeferredCommands deferred;
  deferred.SetTransform(plain, {5.0, 6.0, 7.0});
  deferred.SetCamera(plain, std::nullopt);
  deferred.SetLight(plain, LightComponent{3.0F});
  Require(deferred.Size() == 3, "deferred command buffer must report queued commands");
  Require(world.Submit(deferred), "a valid deferred batch must apply");
  Require(deferred.Size() == 0, "a successful submit must consume the batch");
  Require(world.GetEntity(plain)->transform.x == 5.0 && !world.GetEntity(plain)->has_camera &&
              world.GetEntity(plain)->has_light,
          "deferred component mutations must become visible together");

  DeferredCommands invalid_batch;
  invalid_batch.SetTransform(plain, {100.0, 0.0, 0.0});
  invalid_batch.SetTransform(999999, {});
  Require(!world.Submit(invalid_batch), "a batch containing a stale entity must fail");
  Require(world.GetEntity(plain)->transform.x == 5.0,
          "a rejected deferred batch must not partially mutate the world");

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

  // ---- Entity-bound audio ----
  EntitySpawnDescriptor audio_descriptor;
  audio_descriptor.audio = AudioVoice{700, 0.5F, true};
  const auto audio_entity = world.SpawnEntity(scene, audio_descriptor);
  Require(world.GetEntity(audio_entity)->has_audio &&
              world.GetEntity(audio_entity)->audio.resource == 700,
          "audio binding must round trip through the entity snapshot");
  Require(world.PlayAudio(audio_entity) && world.ActiveAudioVoices() == 1,
          "an entity-bound audio voice must play");
  Require(!world.SetAudio(camera_entity, AudioVoice{700, 1.0F, false}),
          "one audio resource must not ambiguously bind to two entities");
  Require(world.StopAudio(audio_entity) && world.ActiveAudioVoices() == 0,
          "an entity-bound audio voice must stop");
  Require(world.Query(scene, GameWorld::kQueryAudio) == std::vector<Id>{audio_entity},
          "audio entities must participate in batch queries");
  Require(world.PlayAudio(audio_entity), "audio must restart before deferred destruction");
  DeferredCommands destroy_audio;
  destroy_audio.DestroyEntity(audio_entity);
  Require(world.Submit(destroy_audio) && world.ActiveAudioVoices() == 0,
          "deferred destruction must release an entity's audio voice");

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  // ---- Entity-bound physics and character ----
  EntitySpawnDescriptor actor_descriptor;
  actor_descriptor.physics = PhysicsBody{0, {-0.5, 0.0, -0.5}, {0.5, 2.0, 0.5}, false, false, {}};
  actor_descriptor.character = CharacterControllerConfig{};
  const auto actor = world.SpawnEntity(scene, actor_descriptor);
  const auto actor_snapshot = world.GetEntity(actor);
  Require(actor_snapshot->has_physics && actor_snapshot->has_character,
          "physics and character bindings must appear in the entity snapshot");
  const auto hit = world.RaycastEntity({{0.0, 1.0, -2.0}, {0.0, 0.0, 1.0}, 10.0});
  Require(hit && *hit == actor, "physics raycasts must resolve back to the owning entity");
  CharacterInput character_input;
  character_input.move_x = 1.0;
  const auto movement = world.TickCharacter(actor, character_input, 0.1);
  Require(movement.has_value(), "an entity-bound character must tick");
  const auto character = world.GetCharacter(actor);
  Require(character.has_value() && world.GetEntity(actor)->transform.x == character->position.x,
          "character movement must synchronize the entity transform");
  Require(world.Query(scene, GameWorld::kQueryPhysics | GameWorld::kQueryCharacter) ==
              std::vector<Id>{actor},
          "simulation bindings must participate in batch queries");
  DeferredCommands destroy_actor;
  destroy_actor.DestroyEntity(actor);
  Require(world.Submit(destroy_actor) &&
              !world.RaycastEntity({{0.0, 1.0, -2.0}, {0.0, 0.0, 1.0}, 10.0}),
          "deferred destruction must remove physics and character bindings");
#endif

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
