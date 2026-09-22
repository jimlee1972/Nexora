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

  // subscribe_event/set_tick_enabled are documented as unimplemented in this
  // pass; confirm they report that honestly rather than a false success.
  Require(host.subscribe_event(host.context, 1) != 0,
          "subscribe_event has no real implementation yet and must not report success");
  host.set_tick_enabled(host.context, 1); // must not crash; no observable state to assert on

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
