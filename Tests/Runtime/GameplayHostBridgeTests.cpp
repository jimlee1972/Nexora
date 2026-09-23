// API-M6 conformance: NexoraGameplayHostV2's read_component/write_component,
// built by nexora::game::MakeHost, actually reads and writes a real
// GameWorld entity's Transform -- not a test-only fake host. Also covers
// the `log` callback's real core::AsyncLogService wiring.
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Game/GameplayHostBridge.h"

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
  log_service.Stop();

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
