#include "Nexora/Game/GameplayHostBridge.h"

#include "Nexora/Foundation/Types.h"

#include <cstring>
#include <string>

namespace nexora::game {
namespace {

int32_t ReadComponent(void *context, uint64_t entity, uint64_t component_type, void *data,
                      uint32_t data_size) {
  if (context == nullptr || data == nullptr) {
    return -1;
  }
  const auto &host_context = *static_cast<const GameplayHostContext *>(context);
  if (host_context.world == nullptr)
    return -1;
  const auto snapshot = host_context.world->GetEntity(entity);
  if (!snapshot)
    return -1;
  if (component_type == TransformComponentType() && data_size >= sizeof(GameplayTransformWire)) {
    const GameplayTransformWire wire{snapshot->transform.x, snapshot->transform.y,
                                     snapshot->transform.z};
    std::memcpy(data, &wire, sizeof(wire));
  } else if (component_type == CameraComponentType() && snapshot->has_camera &&
             data_size >= sizeof(GameplayCameraWire)) {
    const GameplayCameraWire wire{snapshot->camera.vertical_field_of_view,
                                  snapshot->camera.near_plane, snapshot->camera.far_plane};
    std::memcpy(data, &wire, sizeof(wire));
  } else if (component_type == LightComponentType() && snapshot->has_light &&
             data_size >= sizeof(GameplayLightWire)) {
    const GameplayLightWire wire{snapshot->light.intensity};
    std::memcpy(data, &wire, sizeof(wire));
  } else if (component_type == MeshRendererComponentType() && snapshot->has_mesh_renderer &&
             data_size >= sizeof(GameplayMeshRendererWire)) {
    const GameplayMeshRendererWire wire{snapshot->mesh.mesh, snapshot->mesh.material.shader};
    std::memcpy(data, &wire, sizeof(wire));
  } else {
    return -1;
  }
  return 0;
}

int32_t WriteComponent(void *context, uint64_t entity, uint64_t component_type, const void *data,
                       uint32_t data_size) {
  if (context == nullptr || data == nullptr) {
    return -1;
  }
  auto &host_context = *static_cast<GameplayHostContext *>(context);
  if (host_context.world == nullptr)
    return -1;
  bool written = false;
  if (component_type == TransformComponentType() && data_size >= sizeof(GameplayTransformWire)) {
    GameplayTransformWire wire{};
    std::memcpy(&wire, data, sizeof(wire));
    written = host_context.world->SetTransform(entity, {wire.x, wire.y, wire.z});
  } else if (component_type == CameraComponentType() && data_size >= sizeof(GameplayCameraWire)) {
    GameplayCameraWire wire{};
    std::memcpy(&wire, data, sizeof(wire));
    written = host_context.world->SetCamera(
        entity,
        runtime::CameraComponent{wire.vertical_field_of_view, wire.near_plane, wire.far_plane});
  } else if (component_type == LightComponentType() && data_size >= sizeof(GameplayLightWire)) {
    GameplayLightWire wire{};
    std::memcpy(&wire, data, sizeof(wire));
    written = host_context.world->SetLight(entity, runtime::LightComponent{wire.intensity});
  } else if (component_type == MeshRendererComponentType() &&
             data_size >= sizeof(GameplayMeshRendererWire)) {
    GameplayMeshRendererWire wire{};
    std::memcpy(&wire, data, sizeof(wire));
    written = host_context.world->SetMeshRenderer(
        entity, runtime::MeshComponent{wire.mesh, runtime::MaterialComponent{wire.shader}});
  }
  return written ? 0 : -1;
}

int32_t SubscribeEvent(void *context, uint64_t event_type) {
  if (context == nullptr)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  const auto &host_context = *static_cast<const GameplayHostContext *>(context);
  if (host_context.subscribe_event == nullptr)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  return host_context.subscribe_event(host_context.control_context, event_type);
}

void SetTickEnabled(void *context, uint32_t enabled) {
  if (context == nullptr)
    return;
  const auto &host_context = *static_cast<const GameplayHostContext *>(context);
  if (host_context.set_tick_enabled != nullptr)
    host_context.set_tick_enabled(host_context.control_context, enabled != 0);
}

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
std::uint64_t CameraComponentType() noexcept { return foundation::Name("Nexora.Camera").Value(); }
std::uint64_t LightComponentType() noexcept { return foundation::Name("Nexora.Light").Value(); }
std::uint64_t MeshRendererComponentType() noexcept {
  return foundation::Name("Nexora.MeshRenderer").Value();
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
