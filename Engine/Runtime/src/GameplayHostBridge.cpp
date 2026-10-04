#include "Nexora/Game/GameplayHostBridge.h"

#include "Nexora/Foundation/Types.h"

#include <cstring>
#include <string>

namespace nexora::game {
namespace {

int32_t ReadComponent(void *context, uint64_t entity, uint64_t component_type, void *data,
                      uint32_t data_size) {
  if (context == nullptr)
    return -1;
  const auto &host_context = *static_cast<const GameplayHostContext *>(context);
  // The V2 table reports every failure as -1.
  return host_context.world != nullptr &&
                 ReadGameplayComponent(*host_context.world, entity, component_type, data,
                                       data_size) == NEXORA_GAMEPLAY_OK
             ? 0
             : -1;
}

int32_t WriteComponent(void *context, uint64_t entity, uint64_t component_type, const void *data,
                       uint32_t data_size) {
  if (context == nullptr)
    return -1;
  auto &host_context = *static_cast<GameplayHostContext *>(context);
  return host_context.world != nullptr &&
                 WriteGameplayComponent(*host_context.world, entity, component_type, data,
                                        data_size) == NEXORA_GAMEPLAY_OK
             ? 0
             : -1;
}

NexoraTransformV2 ToWire(const runtime::Transform &transform) {
  return {{transform.x, transform.y, transform.z},
          {transform.qx, transform.qy, transform.qz, transform.qw},
          {transform.sx, transform.sy, transform.sz}};
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

std::uint64_t TransformV2ComponentType() noexcept {
  return foundation::Name("Nexora.TransformV2").Value();
}
std::uint64_t WorldTransformComponentType() noexcept {
  return foundation::Name("Nexora.WorldTransform").Value();
}
std::uint64_t ParentComponentType() noexcept { return foundation::Name("Nexora.Parent").Value(); }

int32_t ReadGameplayComponent(const runtime::World &world, runtime::Id entity,
                              std::uint64_t component_type, void *data, std::uint32_t data_size) {
  if (data == nullptr)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  const auto snapshot = world.FindEntity(entity);
  if (!snapshot)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  const auto copy = [&](const auto &wire) {
    if (data_size < sizeof(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    std::memcpy(data, &wire, sizeof(wire));
    return NEXORA_GAMEPLAY_OK;
  };
  if (component_type == TransformComponentType())
    return copy(
        GameplayTransformWire{snapshot->transform.x, snapshot->transform.y, snapshot->transform.z});
  if (component_type == TransformV2ComponentType())
    return copy(ToWire(snapshot->transform));
  if (component_type == WorldTransformComponentType()) {
    const auto world_transform = world.WorldTransform(entity);
    if (!world_transform)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    return copy(ToWire(*world_transform));
  }
  if (component_type == ParentComponentType())
    return copy(NexoraParent{world.Parent(entity).value_or(0), 0, 0});
  if (component_type == CameraComponentType())
    return snapshot->camera ? copy(GameplayCameraWire{snapshot->camera_data.vertical_field_of_view,
                                                      snapshot->camera_data.near_plane,
                                                      snapshot->camera_data.far_plane})
                            : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  if (component_type == LightComponentType())
    return snapshot->light ? copy(GameplayLightWire{snapshot->light_data.intensity})
                           : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  if (component_type == MeshRendererComponentType())
    return snapshot->mesh_renderer
               ? copy(GameplayMeshRendererWire{snapshot->mesh_data.mesh,
                                               snapshot->mesh_data.material.shader})
               : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
}

int32_t ReadGameplayComponent(const GameWorld &world, runtime::Id entity,
                              std::uint64_t component_type, void *data, std::uint32_t data_size) {
  return ReadGameplayComponent(world.InternalWorld(), entity, component_type, data, data_size);
}

namespace {
template <typename WorldT>
int32_t WriteComponentWire(WorldT &world, runtime::Id entity, std::uint64_t component_type,
                           const void *data, std::uint32_t data_size) {
  if (data == nullptr)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  const auto read = [&](auto &wire) {
    if (data_size < sizeof(wire))
      return false;
    std::memcpy(&wire, data, sizeof(wire));
    return true;
  };
  const auto result = [](bool written) {
    return written ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  };
  if (component_type == TransformComponentType()) {
    GameplayTransformWire wire{};
    if (!read(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    // The wire carries a position only; keep the entity's rotation and scale instead of resetting
    // them to identity.
    const auto current = world.GetEntity(entity);
    return result(current &&
                  world.SetTransform(
                      entity, runtime::WithPosition(current->transform, wire.x, wire.y, wire.z)));
  }
  if (component_type == TransformV2ComponentType()) {
    NexoraTransformV2 wire{};
    if (!read(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    runtime::Transform transform{wire.position.x, wire.position.y, wire.position.z};
    transform.qx = wire.rotation.x;
    transform.qy = wire.rotation.y;
    transform.qz = wire.rotation.z;
    transform.qw = wire.rotation.w;
    transform.sx = wire.scale.x;
    transform.sy = wire.scale.y;
    transform.sz = wire.scale.z;
    // SetTransform validates and normalizes, and rejects an invalid transform.
    return result(world.SetTransform(entity, transform));
  }
  if (component_type == WorldTransformComponentType())
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  if (component_type == ParentComponentType()) {
    NexoraParent wire{};
    if (!read(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    return result(world.SetParent(entity, wire.parent, wire.keep_local == 0));
  }
  if (component_type == CameraComponentType()) {
    GameplayCameraWire wire{};
    if (!read(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    return result(
        world.SetCamera(entity, runtime::CameraComponent{wire.vertical_field_of_view,
                                                         wire.near_plane, wire.far_plane}));
  }
  if (component_type == LightComponentType()) {
    GameplayLightWire wire{};
    if (!read(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    return result(world.SetLight(entity, runtime::LightComponent{wire.intensity}));
  }
  if (component_type == MeshRendererComponentType()) {
    GameplayMeshRendererWire wire{};
    if (!read(wire))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    return result(world.SetMeshRenderer(
        entity, runtime::MeshComponent{wire.mesh, runtime::MaterialComponent{wire.shader}}));
  }
  return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
}

// A narrow adapter reuses the wire decoder with the same validated command operations as
// GameWorld. It never owns or caches an Entity borrow across a mutation.
struct WorldWrites final {
  runtime::World &world;
  const runtime::Entity *GetEntity(runtime::Id entity) const { return world.FindEntity(entity); }
  bool SetTransform(runtime::Id entity, runtime::Transform value) {
    runtime::WorldCommandBuffer commands;
    commands.SetTransform(entity, value);
    return commands.Apply(world);
  }
  bool SetParent(runtime::Id entity, runtime::Id parent, bool keep_world) {
    runtime::WorldCommandBuffer commands;
    commands.SetParent(entity, parent, keep_world);
    return commands.Apply(world);
  }
  bool SetCamera(runtime::Id entity, runtime::CameraComponent value) {
    runtime::WorldCommandBuffer commands;
    commands.SetCamera(entity, value);
    return commands.Apply(world);
  }
  bool SetLight(runtime::Id entity, runtime::LightComponent value) {
    runtime::WorldCommandBuffer commands;
    commands.SetLight(entity, value);
    return commands.Apply(world);
  }
  bool SetMeshRenderer(runtime::Id entity, runtime::MeshComponent value) {
    runtime::WorldCommandBuffer commands;
    commands.SetMeshRenderer(entity, value);
    return commands.Apply(world);
  }
};
} // namespace
int32_t WriteGameplayComponent(GameWorld &world, runtime::Id entity, std::uint64_t component_type,
                               const void *data, std::uint32_t data_size) {
  return WriteComponentWire(world, entity, component_type, data, data_size);
}
int32_t WriteGameplayComponent(runtime::World &world, runtime::Id entity,
                               std::uint64_t component_type, const void *data,
                               std::uint32_t data_size) {
  WorldWrites writes{world};
  return WriteComponentWire(writes, entity, component_type, data, data_size);
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
