// API-M6 conformance: NexoraGameplayHostV2's read_component/write_component,
// built by nexora::game::MakeHost, actually reads and writes a real
// GameWorld entity's Transform -- not a test-only fake host. Also covers
// the `log` callback's real core::AsyncLogService wiring.
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::game;
using namespace nexora::runtime;
using nexora::core::AsyncLogService;
using nexora::core::LogLevel;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

struct ControlState final {
  std::uint64_t subscribed_event{};
  bool tick_enabled{true};
};

int32_t Subscribe(void *context, std::uint64_t event_type) {
  auto &state = *static_cast<ControlState *>(context);
  state.subscribed_event = event_type;
  return NEXORA_GAMEPLAY_OK;
}

void SetTick(void *context, bool enabled) {
  static_cast<ControlState *>(context)->tick_enabled = enabled;
}

int Run() {
  GameWorld world;
  const auto scene = world.LoadScene("BridgeTestScene");
  world.ActivateScene(scene);
  EntitySpawnDescriptor descriptor;
  descriptor.transform = {1.0, 2.0, 3.0};
  const auto entity = world.SpawnEntity(scene, descriptor);

  GameplayHostContext context{&world};
  const auto host = MakeHost(context);
  Require(host.struct_size == sizeof(NexoraGameplayHostV2) &&
              host.abi_version == NEXORA_GAMEPLAY_ABI_VERSION && host.context == &context,
          "MakeHost must fill in the versioned descriptor header and context");

  // ---- read_component: matches the live entity's real Transform ----
  GameplayTransformWire wire{};
  Require(
      host.read_component(host.context, entity, TransformComponentType(), &wire, sizeof(wire)) == 0,
      "read_component must succeed for a live entity and the Transform component type");
  Require(wire.x == 1.0 && wire.y == 2.0 && wire.z == 3.0,
          "read_component must return the entity's real transform, not test scaffolding");

  // ---- write_component: actually mutates GameWorld, not a private copy ----
  const GameplayTransformWire updated{9.0, 8.0, 7.0};
  Require(host.write_component(host.context, entity, TransformComponentType(), &updated,
                               sizeof(updated)) == 0,
          "write_component must succeed for a live entity and the Transform component type");
  const auto snapshot = world.GetEntity(entity);
  Require(snapshot.has_value() && snapshot->transform.x == 9.0 && snapshot->transform.y == 8.0 &&
              snapshot->transform.z == 7.0,
          "write_component must be visible through GameWorld itself, proving it mutated real "
          "state rather than a copy private to the bridge");

  GameplayCameraWire camera{72.0, 0.25, 750.0};
  Require(host.write_component(host.context, entity, CameraComponentType(), &camera,
                               sizeof(camera)) == 0,
          "write_component must attach camera data");
  GameplayCameraWire camera_read{};
  Require(host.read_component(host.context, entity, CameraComponentType(), &camera_read,
                              sizeof(camera_read)) == 0 &&
              camera_read.vertical_field_of_view == 72.0 && camera_read.near_plane == 0.25,
          "camera wire data must round trip through GameWorld");

  GameplayLightWire light{6.0F};
  Require(host.write_component(host.context, entity, LightComponentType(), &light, sizeof(light)) ==
              0,
          "write_component must attach light data");
  GameplayMeshRendererWire mesh{123, 456};
  Require(host.write_component(host.context, entity, MeshRendererComponentType(), &mesh,
                               sizeof(mesh)) == 0,
          "write_component must attach mesh-renderer data");
  const auto component_snapshot = world.GetEntity(entity);
  Require(component_snapshot->has_light && component_snapshot->light.intensity == 6.0F &&
              component_snapshot->has_mesh_renderer && component_snapshot->mesh.mesh == 123 &&
              component_snapshot->mesh.material.shader == 456,
          "all supported bridge components must update real entity state");

  // ---- error paths ----
  Require(
      host.read_component(host.context, 999999, TransformComponentType(), &wire, sizeof(wire)) != 0,
      "read_component on a missing entity must fail, not read garbage");
  Require(host.write_component(host.context, 999999, TransformComponentType(), &updated,
                               sizeof(updated)) != 0,
          "write_component on a missing entity must fail, not silently no-op");
  Require(host.read_component(host.context, entity, TransformComponentType() ^ 1, &wire,
                              sizeof(wire)) != 0,
          "read_component with the wrong component_type must fail rather than reinterpret bytes");
  Require(host.read_component(host.context, entity, TransformComponentType(), &wire,
                              sizeof(wire) - 1) != 0,
          "read_component with an undersized buffer must fail rather than under-read");
  Require(host.read_component(nullptr, entity, TransformComponentType(), &wire, sizeof(wire)) != 0,
          "read_component with a null context must fail rather than dereference it");
  GameplayHostContext null_world_context{};
  const auto null_world_host = MakeHost(null_world_context);
  Require(null_world_host.read_component(null_world_host.context, entity, TransformComponentType(),
                                         &wire, sizeof(wire)) != 0,
          "read_component with a null world must fail rather than dereference it");

  Require(host.subscribe_event(host.context, 1) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
          "subscribe_event must report unsupported when no embedding hook is installed");
  host.set_tick_enabled(host.context, 1); // optional missing hook remains safe

  ControlState control;
  GameplayHostContext controlled_context{&world, nullptr, &control, &Subscribe, &SetTick};
  const auto controlled_host = MakeHost(controlled_context);
  Require(controlled_host.subscribe_event(controlled_host.context, 42) == NEXORA_GAMEPLAY_OK &&
              control.subscribed_event == 42,
          "subscribe_event must delegate to the embedding event router");
  controlled_host.set_tick_enabled(controlled_host.context, 0);
  Require(!control.tick_enabled, "set_tick_enabled must delegate to the embedding scheduler");

  // ---- log: dropped when GameplayHostContext::log is null (unchanged
  // default behavior), forwarded to a real AsyncLogService when set ----
  const char message[] = "hello from gameplay";
  host.log(host.context, static_cast<uint32_t>(LogLevel::Info), message,
           static_cast<uint32_t>(sizeof(message) - 1));
  // No log service is attached above, so this must not have crashed or
  // gone anywhere observable -- there is nothing further to assert here
  // beyond "the call above returned".

  AsyncLogService log_service;
  log_service.Start();
  GameplayHostContext logging_context{&world, &log_service};
  const auto logging_host = MakeHost(logging_context);
  logging_host.log(logging_host.context, static_cast<uint32_t>(LogLevel::Warning), message,
                   static_cast<uint32_t>(sizeof(message) - 1));
  log_service.Flush();
  const auto records = log_service.CrashRingSnapshot();
  Require(records.size() == 1 && records.front().level == LogLevel::Warning &&
              records.front().category == "Gameplay" && records.front().message == message,
          "log must forward to the attached AsyncLogService with the right level/category/text");

  // An out-of-range level must be dropped, not reinterpreted as whichever
  // LogLevel that bit pattern happens to alias.
  logging_host.log(logging_host.context, 200, message, static_cast<uint32_t>(sizeof(message) - 1));
  log_service.Flush();
  Require(log_service.CrashRingSnapshot().size() == 1,
          "an out-of-range log level must be dropped rather than misinterpreted");
  const char embedded_nul[]{'a', 0, 'b'};
  const char invalid_utf8[]{'\xc0', '\xaf'};
  logging_host.log(logging_host.context, 1, embedded_nul, sizeof(embedded_nul));
  logging_host.log(logging_host.context, 1, invalid_utf8, sizeof(invalid_utf8));
  logging_host.log(logging_host.context, 1, nullptr, 1);
  logging_host.log(logging_host.context, 1, reinterpret_cast<const char *>(1), UINT32_MAX);
  const auto rejected = log_service.SnapshotSince(0);
  Require(rejected.records.size() == 1 && rejected.rejected_records == 5,
          "invalid gameplay wire data allocated/read payloads or concealed producer loss");
  logging_host.log(logging_host.context, 1, nullptr, 0);
  log_service.Flush();
  Require(log_service.CrashRingSnapshot().size() == 2 &&
              log_service.CrashRingSnapshot().back().message.empty(),
          "zero-length gameplay log requires a non-null pointer");
  log_service.Stop();

  return 0;
}

// "Nexora.TransformV2", "Nexora.WorldTransform", and "Nexora.Parent" through the shared path used
// by every host table (the V2 bridge above and the V3 Showcase host).
void TestHierarchyWires() {
  GameWorld world;
  const auto scene = world.LoadScene("Wires");
  EntitySpawnDescriptor parent_descriptor;
  parent_descriptor.transform = {10.0, 0.0, 0.0};
  parent_descriptor.transform.qy = parent_descriptor.transform.qw = 0.70710678118654752440;
  parent_descriptor.transform.sx = parent_descriptor.transform.sy = parent_descriptor.transform.sz =
      2.0;
  const auto parent = world.SpawnEntity(scene, parent_descriptor);
  EntitySpawnDescriptor child_descriptor;
  child_descriptor.transform = {10.0, 0.0, -2.0};
  const auto child = world.SpawnEntity(scene, child_descriptor);

  // Parent: write with keep_local == 0 keeps the world pose (Unity's default).
  const NexoraParent attach{parent, 0, 0};
  Require(WriteGameplayComponent(world, child, ParentComponentType(), &attach, sizeof(attach)) ==
              NEXORA_GAMEPLAY_OK,
          "writing Nexora.Parent must reparent");
  NexoraParent parent_read{};
  Require(ReadGameplayComponent(world, child, ParentComponentType(), &parent_read,
                                sizeof(parent_read)) == NEXORA_GAMEPLAY_OK &&
              parent_read.parent == parent,
          "reading Nexora.Parent must return the parent id");

  NexoraTransformV2 local{};
  NexoraTransformV2 world_pose{};
  Require(ReadGameplayComponent(world, child, TransformV2ComponentType(), &local, sizeof(local)) ==
                  NEXORA_GAMEPLAY_OK &&
              ReadGameplayComponent(world, child, WorldTransformComponentType(), &world_pose,
                                    sizeof(world_pose)) == NEXORA_GAMEPLAY_OK,
          "TransformV2 and WorldTransform must be readable");
  const auto near = [](double a, double b) { return a - b < 1e-9 && b - a < 1e-9; };
  Require(near(world_pose.position.x, 10.0) && near(world_pose.position.z, -2.0) &&
              near(world_pose.scale.x, 1.0) && near(local.position.x, 1.0) &&
              near(local.scale.x, 0.5),
          "the local transform is relative to the parent and the world pose did not move");
  GameplayTransformWire position{};
  Require(ReadGameplayComponent(world, child, TransformComponentType(), &position,
                                sizeof(position)) == NEXORA_GAMEPLAY_OK &&
              near(position.x, local.position.x),
          "the original Transform wire carries the local position");

  // TransformV2: a full write round trips; an invalid one is rejected and changes nothing.
  NexoraTransformV2 written{{1.0, 2.0, 3.0}, {0.0, 0.0, 0.0, 2.0}, {-1.0, 2.0, 3.0}};
  Require(WriteGameplayComponent(world, child, TransformV2ComponentType(), &written,
                                 sizeof(written)) == NEXORA_GAMEPLAY_OK,
          "writing a valid TransformV2 must succeed");
  const auto stored = world.GetEntity(child)->transform;
  Require(stored.qw == 1.0 && stored.sx == -1.0 && stored.sz == 3.0 && stored.y == 2.0,
          "TransformV2 must store rotation (normalized) and mirroring scale");
  written.scale.y = 0.0;
  Require(WriteGameplayComponent(world, child, TransformV2ComponentType(), &written,
                                 sizeof(written)) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              world.GetEntity(child)->transform == stored,
          "a zero scale must be rejected without changing the entity");

  // A position-only write keeps rotation and scale (this path also serves the V3 Showcase host,
  // which used to reset them to identity).
  const GameplayTransformWire moved{4.0, 5.0, 6.0};
  Require(WriteGameplayComponent(world, child, TransformComponentType(), &moved, sizeof(moved)) ==
                  NEXORA_GAMEPLAY_OK &&
              world.GetEntity(child)->transform.sx == -1.0 &&
              world.GetEntity(child)->transform.x == 4.0,
          "a Transform write must keep rotation and scale");

  // Read-only, unsupported, undersized, and rejected cases.
  Require(WriteGameplayComponent(world, child, WorldTransformComponentType(), &world_pose,
                                 sizeof(world_pose)) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
          "Nexora.WorldTransform must be read only");
  Require(ReadGameplayComponent(world, child, TransformV2ComponentType(), &local,
                                sizeof(local) - 1) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
              ReadGameplayComponent(world, child, ParentComponentType() ^ 1, &parent_read,
                                    sizeof(parent_read)) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
          "an undersized buffer or unknown component must be unsupported");
  const NexoraParent cycle{child, 0, 0};
  Require(WriteGameplayComponent(world, parent, ParentComponentType(), &cycle, sizeof(cycle)) ==
                  NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              ReadGameplayComponent(world, 999'999, ParentComponentType(), &parent_read,
                                    sizeof(parent_read)) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
          "a cycle or a missing entity must be rejected");
  // Keep-local detaches with the local values unchanged.
  const auto before = world.GetEntity(child)->transform;
  const NexoraParent detach{0, 1, 0};
  Require(WriteGameplayComponent(world, child, ParentComponentType(), &detach, sizeof(detach)) ==
                  NEXORA_GAMEPLAY_OK &&
              world.GetParent(child) == Id{0} && world.GetEntity(child)->transform == before,
          "keep_local must keep the local values");

  // The V2 table reaches the same wires.
  GameplayHostContext context{&world};
  const auto host = MakeHost(context);
  Require(host.read_component(host.context, child, TransformV2ComponentType(), &local,
                              sizeof(local)) == 0 &&
              host.write_component(host.context, child, WorldTransformComponentType(), &world_pose,
                                   sizeof(world_pose)) == -1,
          "the V2 bridge must serve the new wires and report failures as -1");
}
#if NEXORA_EDITOR_SDK_ENABLED
void TestPlayWorldWires() {
  World editor;
  const auto scene = editor.LoadScene("Play wires");
  Require(editor.Activate(scene), "activate failed");
  const auto parent = editor.CreateEntity(scene).id;
  const auto child = editor.CreateEntity(scene).id;
  WorldCommandBuffer setup;
  setup.SetTransform(parent, {3, 0, 0});
  setup.SetParent(child, parent, false);
  Require(setup.Apply(editor), "setup failed");
  PlaySession play(editor);
  Require(play.Start(1.0 / 60.0,
                     [child](World &world, double) {
                       GameplayTransformWire position{};
                       if (ReadGameplayComponent(world, child, TransformComponentType(), &position,
                                                 sizeof(position)) != NEXORA_GAMEPLAY_OK)
                         return false;
                       position.x += 1;
                       return WriteGameplayComponent(world, child, TransformComponentType(),
                                                     &position,
                                                     sizeof(position)) == NEXORA_GAMEPLAY_OK;
                     }),
          "start failed");
  Require(play.Tick() && play.Pause() && play.Step(), "fixed/step callback failed");
  auto &clone = *play.PlayWorld();
  Require(clone.FindEntity(child)->transform.x == 2 && editor.FindEntity(child)->transform.x == 0,
          "component callback escaped Play clone");
  NexoraTransformV2 pose{};
  Require(ReadGameplayComponent(clone, child, WorldTransformComponentType(), &pose, sizeof(pose)) ==
                  NEXORA_GAMEPLAY_OK &&
              pose.position.x == 5,
          "borrowed World did not resolve parent pose");
  const auto before = clone.SaveScene(scene);
  const NexoraParent cycle{child, 0, 0};
  Require(WriteGameplayComponent(clone, parent, ParentComponentType(), &cycle, sizeof(cycle)) ==
                  NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              clone.SaveScene(scene) == before,
          "failed wire reparent partially mutated Play clone");
  const GameplayCameraWire camera{80, 0.25, 200};
  const GameplayLightWire light{3};
  const GameplayMeshRendererWire mesh{UINT64_MAX, UINT64_MAX};
  Require(WriteGameplayComponent(clone, child, CameraComponentType(), &camera, sizeof(camera)) ==
                  NEXORA_GAMEPLAY_OK &&
              WriteGameplayComponent(clone, child, LightComponentType(), &light, sizeof(light)) ==
                  NEXORA_GAMEPLAY_OK &&
              WriteGameplayComponent(clone, child, MeshRendererComponentType(), &mesh,
                                     sizeof(mesh)) == NEXORA_GAMEPLAY_OK,
          "Play component writes failed");
  GameplayCameraWire read_camera{};
  GameplayLightWire read_light{};
  GameplayMeshRendererWire read_mesh{};
  Require(ReadGameplayComponent(clone, child, CameraComponentType(), &read_camera,
                                sizeof(read_camera)) == NEXORA_GAMEPLAY_OK &&
              read_camera.vertical_field_of_view == 80 &&
              ReadGameplayComponent(clone, child, LightComponentType(), &read_light,
                                    sizeof(read_light)) == NEXORA_GAMEPLAY_OK &&
              read_light.intensity == 3 &&
              ReadGameplayComponent(clone, child, MeshRendererComponentType(), &read_mesh,
                                    sizeof(read_mesh)) == NEXORA_GAMEPLAY_OK &&
              read_mesh.mesh == UINT64_MAX && read_mesh.shader == UINT64_MAX,
          "Play component wire payload differed");
  Require(!editor.FindEntity(child)->camera && !editor.FindEntity(child)->mesh_renderer &&
              play.Stop(),
          "Play wire edits leaked back to Editor");
}
#endif

} // namespace

int main() {
  try {
    TestHierarchyWires();
#if NEXORA_EDITOR_SDK_ENABLED
    TestPlayWorldWires();
#endif
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
