#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Services = editor::preview::PlaySceneServices;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
NexoraGameplayHostV3 host;
std::uint64_t scene{};
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
std::uint64_t initial_entity{};
#endif
bool stopped{}, destroyed{};
NexoraEntitySpawnDescriptor Descriptor() {
  NexoraEntitySpawnDescriptor descriptor{};
  descriptor.struct_size = sizeof(descriptor);
  descriptor.components = NEXORA_SPAWN_PHYSICS;
  descriptor.bounds_minimum = {-1, -1, -1};
  descriptor.bounds_maximum = {1, 1, 1};
  descriptor.camera_fov_degrees = std::numeric_limits<double>::quiet_NaN();
  return descriptor;
}
constexpr NexoraRaycastRequest kRay{{0, 0, 3}, {0, 0, -1}, 10};
constexpr NexoraRaycastHit kSentinel{UINT64_MAX, 123, {4, 5, 6}};
bool Same(NexoraRaycastHit a, NexoraRaycastHit b) {
  return a.entity == b.entity && a.distance == b.distance && a.point.x == b.point.x &&
         a.point.y == b.point.y && a.point.z == b.point.z;
}
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = nullptr;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) {
  constexpr char name[] = "Physics from real V3 Start";
  if (host.load_scene(host.context, name, sizeof(name) - 1, 0, &scene) != NEXORA_GAMEPLAY_OK ||
      host.activate_scene(host.context, scene) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto descriptor = Descriptor();
  auto entity = std::uint64_t{999};
  const auto result = host.spawn_entity(host.context, scene, &descriptor, &entity);
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  if (!host.raycast || result != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  initial_entity = entity;
  auto ray = kRay;
  ray.direction.z = -std::numeric_limits<double>::max();
  NexoraRaycastHit hit = kSentinel;
  if (host.raycast(host.context, &ray, &hit) != NEXORA_GAMEPLAY_OK || hit.entity != entity ||
      hit.distance != 2 || hit.point.z != 1)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#else
  if (host.raycast || result != NEXORA_GAMEPLAY_ERROR_UNSUPPORTED || entity != 999)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#endif
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
void Stop(void *) {
  auto descriptor = Descriptor();
  std::uint64_t entity{};
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  NexoraRaycastHit hit{};
  stopped = host.spawn_entity(host.context, scene, &descriptor, &entity) == NEXORA_GAMEPLAY_OK &&
            host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_OK && hit.entity == entity &&
            host.despawn_entity(host.context, entity) == NEXORA_GAMEPLAY_OK;
#else
  stopped = host.spawn_entity(host.context, scene, &descriptor, &entity) ==
            NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#endif
}
void Destroy(void *) {
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  NexoraRaycastHit hit = kSentinel;
  destroyed = host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              Same(hit, kSentinel);
#else
  destroyed = !host.raycast;
#endif
}
int32_t Loader(std::uint32_t requested, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = requested;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
void VerifyQueries(runtime::World &world) {
  const auto reject_ray = [](NexoraRaycastRequest bad) {
    NexoraRaycastHit hit = kSentinel;
    Require(host.raycast(host.context, &bad, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                Same(hit, kSentinel),
            "bad/miss ray changed copied output");
  };
  auto ray = kRay;
  ray.direction = {};
  reject_ray(ray);
  ray = kRay;
  ray.origin.x = std::numeric_limits<double>::infinity();
  reject_ray(ray);
  ray = kRay;
  ray.direction.x = std::numeric_limits<double>::quiet_NaN();
  reject_ray(ray);
  ray = kRay;
  ray.distance = -1;
  reject_ray(ray);
  ray.distance = std::numeric_limits<double>::max();
  NexoraRaycastHit hit{};
  Require(host.raycast(host.context, &ray, &hit) == NEXORA_GAMEPLAY_OK && hit.distance == 2,
          "finite maximum ray distance rejected");
  ray.direction.z = -std::numeric_limits<double>::denorm_min();
  Require(host.raycast(host.context, &ray, &hit) == NEXORA_GAMEPLAY_OK && hit.distance == 2,
          "finite subnormal direction failed robust normalization");
  ray = kRay;
  ray.distance = 1;
  reject_ray(ray);
  ray = kRay;
  ray.origin.x = 20;
  reject_ray(ray);
  hit = kSentinel;
  Require(host.raycast(host.context, nullptr, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              Same(hit, kSentinel) &&
              host.raycast(host.context, &kRay, nullptr) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
          "null ray/output admission failed");

  auto descriptor = Descriptor();
  std::uint64_t tied{};
  Require(host.spawn_entity(host.context, scene, &descriptor, &tied) == NEXORA_GAMEPLAY_OK &&
              host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_OK &&
              hit.entity == initial_entity,
          "equal-distance Play hits did not select the lower entity ID");
  Require(host.despawn_entity(host.context, tied) == NEXORA_GAMEPLAY_OK,
          "tie fixture despawn failed");
  runtime::WorldCommandBuffer move;
  auto transform = world.FindEntity(initial_entity)->transform;
  transform.z = -2;
  move.SetTransform(initial_entity, transform);
  Require(move.Apply(world) && host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_OK &&
              hit.distance == 4 && hit.point.z == -1,
          "query used stale world transform");
  const auto parent = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer hierarchy;
  runtime::Transform parent_transform;
  parent_transform.z = -2;
  parent_transform.sx = -2;
  parent_transform.sy = 3;
  hierarchy.SetTransform(parent, parent_transform);
  hierarchy.SetParent(initial_entity, parent, false);
  Require(hierarchy.Apply(world) && host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_OK &&
              hit.distance == 6 && hit.point.z == -3,
          "query lost reparented mirrored/nonuniform affine transforms");
  auto rotated = world.FindEntity(initial_entity)->transform;
  rotated.qz = std::sin(0.39269908169872414);
  rotated.qw = std::cos(0.39269908169872414);
  rotated.sy = 2;
  runtime::WorldCommandBuffer shear;
  shear.SetTransform(initial_entity, rotated);
  ray = kRay;
  ray.origin.x = 4;
  ray.origin.y = 6;
  Require(shear.Apply(world) && host.raycast(host.context, &ray, &hit) == NEXORA_GAMEPLAY_OK &&
              hit.distance == 6 && hit.point.x == 4 && hit.point.y == 6,
          "eight-corner conservative query lost rotated/nonuniform inherited shear");
  const auto baseline = *world.SaveScene(scene);
  for (int invalid = 0; invalid < 4; ++invalid) {
    auto bad = Descriptor();
    if (invalid == 0)
      bad.bounds_minimum.y = 2;
    if (invalid == 1)
      bad.bounds_maximum.x = std::numeric_limits<double>::quiet_NaN();
    if (invalid == 2)
      bad.bounds_minimum.z = -std::numeric_limits<double>::infinity();
    if (invalid == 3) {
      bad.position.x = std::numeric_limits<double>::max();
      bad.bounds_minimum.x = bad.bounds_maximum.x = std::numeric_limits<double>::max();
    }
    std::uint64_t output = 999;
    Require(host.spawn_entity(host.context, scene, &bad, &output) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                output == 999 && world.SaveScene(scene) == baseline,
            "invalid physics descriptor partially published an entity");
  }
  auto extreme = world.FindEntity(initial_entity)->transform;
  extreme.sx = std::numeric_limits<double>::max();
  runtime::WorldCommandBuffer overflow;
  overflow.SetTransform(initial_entity, extreme);
  Require(overflow.Apply(world), "overflow world-transform fixture failed");
  hit = kSentinel;
  Require(host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              Same(hit, kSentinel),
          "invalid live collider silently returned a partial/wrong hit");
  Require(host.despawn_entity(host.context, parent) == NEXORA_GAMEPLAY_OK &&
              !world.FindEntity(initial_entity),
          "collider cascade deletion failed");
  reject_ray(kRay);
}

void VerifyBudgetAndUnload(runtime::World &world) {
  Services services;
  Require(services.Bind(world), "physics services bind failed");
  const auto inactive = world.LoadScene("Inactive collider");
  auto descriptor = Descriptor();
  std::uint64_t entity{};
  NexoraRaycastHit hit = kSentinel;
  Require(services.Spawn(inactive, &descriptor, &entity) == NEXORA_GAMEPLAY_OK &&
              services.Raycast(&kRay, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              Same(hit, kSentinel) && services.Activate(inactive) == NEXORA_GAMEPLAY_OK &&
              services.Raycast(&kRay, &hit) == NEXORA_GAMEPLAY_OK && hit.entity == entity,
          "inactive/active collider query lifecycle failed");
  Require(world.RequestUnload(inactive), "collider scene unload request failed");
  hit = kSentinel;
  Require(services.Raycast(&kRay, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              Same(hit, kSentinel),
          "unloading collider remained queryable");
  world.EndFrame();
  Require(services.Raycast(&kRay, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
              Same(hit, kSentinel),
          "unloaded collider remained queryable");
  const auto target = world.LoadScene("Budget");
  Require(world.Activate(target), "budget scene activation failed");
  std::array<std::uint64_t, Services::kMaximumColliders> ids{};
  for (auto &id : ids)
    Require(services.Spawn(target, &descriptor, &id) == NEXORA_GAMEPLAY_OK,
            "live collider budget rejected a valid entry");
  auto unchanged = std::uint64_t{999};
  const auto baseline = *world.SaveScene(target);
  Require(services.Spawn(target, &descriptor, &unchanged) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              unchanged == 999 && world.SaveScene(target) == baseline,
          "live collider overflow partially published an entity");
  Require(services.Despawn(ids[0]) == NEXORA_GAMEPLAY_OK &&
              services.Spawn(target, &descriptor, &unchanged) == NEXORA_GAMEPLAY_OK,
          "despawn did not release the live collider budget");
  runtime::WorldCommandBuffer external;
  external.DestroyEntity(ids[1]);
  Require(external.Apply(world) &&
              services.Spawn(target, &descriptor, &unchanged) == NEXORA_GAMEPLAY_OK,
          "external cascade did not prune stale owning collider records");
  services.Clear();
  hit = kSentinel;
  Require(services.Raycast(&kRay, &hit) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              Same(hit, kSentinel) && services.Bind(world) &&
              services.Raycast(&kRay, &hit) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
          "clear/rebind retained old colliders or changed failed output");

  runtime::World lifetime{runtime::WorldKind::Play};
  const auto lifetime_scene = lifetime.LoadScene("Lifetime budget");
  Services limited;
  Require(limited.Bind(lifetime), "lifetime collider bind failed");
  for (std::size_t i = 0; i < Services::kMaximumSpawns; ++i)
    Require(limited.Spawn(lifetime_scene, &descriptor, &entity) == NEXORA_GAMEPLAY_OK &&
                limited.Despawn(entity) == NEXORA_GAMEPLAY_OK,
            "collider lifetime budget rejected successful admission/despawn");
  unchanged = 999;
  Require(limited.Spawn(lifetime_scene, &descriptor, &unchanged) ==
                  NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              unchanged == 999 && limited.Bind(lifetime) &&
              limited.Spawn(lifetime_scene, &descriptor, &unchanged) == NEXORA_GAMEPLAY_OK,
          "despawn recycled lifetime spawn quota or Bind did not reset it");
}
#endif
} // namespace

int main() {
  try {
    runtime::World authored;
    const auto authored_scene = authored.LoadScene("Authored");
    authored.CreateEntity(authored_scene);
    const auto baseline = *authored.SaveScene(authored_scene);
    runtime::PlaySession play(authored);
    editor::preview::PlayGameplayModule module;
    Require(play.Start(.25, [](runtime::World &, double) { return true; }) &&
                module.Load(*play.PlayWorld(), Loader),
            "actual V3 physics Start failed");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
    VerifyQueries(*play.PlayWorld());
#else
    Services stripped;
    NexoraRaycastHit unchanged = kSentinel;
    Require(stripped.Bind(*play.PlayWorld()) &&
                stripped.Raycast(&kRay, &unchanged) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
                Same(unchanged, kSentinel),
            "feature-stripped direct query changed output");
#endif
    module.Unload();
    Require(stopped && destroyed, "Stop/Destroy lost their bound physics service lifetime");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
    NexoraRaycastHit hit = kSentinel;
    Require(host.raycast(host.context, &kRay, &hit) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                Same(hit, kSentinel),
            "Raycast service outlived module teardown");
    VerifyBudgetAndUnload(*play.PlayWorld());
#endif
    Require(play.Stop() && authored.SaveScene(authored_scene) == baseline,
            "Play physics mutated the authored Editor World");
    std::cout << "Actual V3 Play collider query services passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
