#include "Nexora/Game/GameplayHostBridge.h"

#include "Nexora/Foundation/Types.h"

#include <cstring>

namespace nexora::game {
namespace {

int32_t ReadComponent(void *context, uint64_t entity, uint64_t component_type, void *data,
                      uint32_t data_size) {
  if (context == nullptr || data == nullptr || component_type != TransformComponentType() ||
      data_size < sizeof(GameplayTransformWire)) {
    return -1;
  }
  const auto &host_context = *static_cast<const GameplayHostContext *>(context);
  const auto snapshot = host_context.world->GetEntity(entity);
  if (!snapshot)
    return -1;
  const GameplayTransformWire wire{snapshot->transform.x, snapshot->transform.y,
                                   snapshot->transform.z};
  std::memcpy(data, &wire, sizeof(wire));
  return 0;
}

int32_t WriteComponent(void *context, uint64_t entity, uint64_t component_type, const void *data,
                       uint32_t data_size) {
  if (context == nullptr || data == nullptr || component_type != TransformComponentType() ||
      data_size < sizeof(GameplayTransformWire)) {
    return -1;
  }
  auto &host_context = *static_cast<GameplayHostContext *>(context);
  GameplayTransformWire wire{};
  std::memcpy(&wire, data, sizeof(wire));
  const runtime::Transform transform{wire.x, wire.y, wire.z};
  return host_context.world->SetTransform(entity, transform) ? 0 : -1;
}

// Not implemented in this pass: no EventBus/tick-gating integration exists
// yet to route these through, so they report failure rather than silently
// pretending to succeed.
int32_t SubscribeEvent(void * /*context*/, uint64_t /*event_type*/) { return -1; }
void SetTickEnabled(void * /*context*/, uint32_t /*enabled*/) {}
void Log(void * /*context*/, uint32_t /*level*/, const char * /*message*/,
         uint32_t /*message_length*/) {}

} // namespace

std::uint64_t TransformComponentType() noexcept {
  return foundation::Name("Nexora.Transform").Value();
}

NexoraGameplayHostV2 MakeHost(GameplayHostContext &context) noexcept {
  NexoraGameplayHostV2 host{};
  host.struct_size = sizeof(NexoraGameplayHostV2);
  host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  host.context = &context;
  host.log = &Log;
  host.read_component = &ReadComponent;
  host.write_component = &WriteComponent;
  host.subscribe_event = &SubscribeEvent;
  host.set_tick_enabled = &SetTickEnabled;
  return host;
}

} // namespace nexora::game
