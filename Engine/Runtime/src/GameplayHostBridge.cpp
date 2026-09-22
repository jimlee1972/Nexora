#include "Nexora/Game/GameplayHostBridge.h"

#include "Nexora/Foundation/Types.h"

#include <cstring>
#include <string>

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

// Not implemented in this pass: the ABI has no callback slot for the host
// to invoke the module when a subscribed event later fires, and gating
// GameplayModuleHost::Update calls is a cross-namespace design decision
// (see this header's top comment) -- neither is a plain wiring gap, so they
// report failure/no-op rather than silently pretending to succeed.
int32_t SubscribeEvent(void * /*context*/, uint64_t /*event_type*/) { return -1; }
void SetTickEnabled(void * /*context*/, uint32_t /*enabled*/) {}

void Log(void *context, uint32_t level, const char *message, uint32_t message_length) {
  if (context == nullptr || message == nullptr)
    return;
  auto &host_context = *static_cast<const GameplayHostContext *>(context);
  if (host_context.log == nullptr)
    return;
  // Reject a level outside LogLevel's own range rather than
  // static_cast-ing an arbitrary uint32_t into the enum: a module built
  // against a future ABI version could pass a level this build doesn't
  // know about, and silently reinterpreting it as whichever LogLevel that
  // bit pattern happens to alias would be worse than just dropping it.
  if (level > static_cast<uint32_t>(core::LogLevel::Fatal))
    return;
  // message is a (pointer, length) pair, not necessarily NUL-terminated --
  // constructing std::string from both preserves an embedded NUL exactly
  // like Nexora/Foundation/Types.h's own byte-oriented views do, rather
  // than truncating at the first one.
  host_context.log->Write(static_cast<core::LogLevel>(level), "Gameplay",
                          std::string(message, message_length));
}

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
