#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>

using namespace nexora;
namespace {
NexoraTwoDSprite SpriteWire() { return {1, 2, 1, 8, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0}; }
NexoraTwoDCollider ColliderWire() { return {0, {0, 0}, {0.5F, 0.5F}, 0.5F, 1, UINT32_MAX}; }
NexoraTwoDApiV1 Get(twod::GameplayBridge &bridge) {
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  assert(bridge.GetApi(&api) == 0);
  assert(api.abi_version == 1);
  return api;
}
void ReloadResources() {
  runtime::World world(runtime::WorldKind::Play);
  const auto id = world.LoadScene("Reload resources");
  const auto entity = world.CreateEntity(id).id;
  const auto source_entity = world.CreateEntity(id).id;
  auto scene = std::make_unique<twod::SpriteScene>(world, id);
  auto pool = std::make_unique<twod::InstancePool>(4);
  auto tiles = std::make_unique<twod::Tilemap>();
  tiles->atlas = {1, 2};
  auto clip = std::make_shared<const twod::Clip>(
      twod::Clip{{3, 4}, {1, 2}, true, {{1, .1, "a"}, {2, .1, "b"}}});
  auto bridge = std::make_unique<twod::GameplayBridge>(
      world, *scene, [clip](auto asset) { return asset == clip->id ? clip : nullptr; }, tiles.get(),
      pool.get());
  auto api = Get(*bridge);
  auto sprite = SpriteWire();
  sprite.pivot = {.25F, .75F};
  sprite.order = 19;
  assert(api.set_sprite(api.context, entity, &sprite) == 0);
  twod::Sprite source;
  source.atlas = {1, 2};
  source.region = 1;
  source.source = twod::AssetId{5, 6};
  source.pivot = {.125F, .875F};
  source.sortY = true;
  assert(scene->Set(source_entity, source));
  const auto collider = ColliderWire();
  assert(api.set_collider(api.context, entity, &collider) == 0 &&
         api.rebuild_queries(api.context, 0) == 0);
  const NexoraTwoDCamera camera{{3, -4}, 17, .35F};
  assert(api.set_camera(api.context, &camera) == 0 &&
         api.play_animation(api.context, entity, 3, 4, 1) == 0);
  NexoraTwoDInstance value{{{7, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, SpriteWire()};
  NexoraTwoDInstanceHandle handle{};
  assert(api.create_instances(api.context, &value, 1, &handle) == 0);
  runtime::World editor_world;
  assert(!scene->ForkForReload(world) && !scene->ForkForReload(editor_world) &&
         !bridge->ForkForReload(world, *scene, tiles.get(), pool.get()));
  for (unsigned generation = 1; generation <= 32; ++generation) {
    auto staged_world = world;
    auto staged_metadata = scene->ForkForReload(staged_world);
    assert(staged_metadata);
    auto staged_scene = std::make_unique<twod::SpriteScene>(std::move(*staged_metadata));
    auto staged_pool = pool->ForkForReload();
    auto staged_tiles = std::make_unique<twod::Tilemap>(*tiles);
    twod::InstancePool foreign_pool(4);
    assert(!bridge->ForkForReload(staged_world, *staged_scene, staged_tiles.get(), &foreign_pool));
    std::thread foreign([&] {
      assert(!bridge->ForkForReload(staged_world, *staged_scene, staged_tiles.get(),
                                    staged_pool.get()) &&
             !bridge->RebindForReload(world, *scene, tiles.get(), pool.get()));
    });
    foreign.join();
    auto candidate =
        bridge->ForkForReload(staged_world, *staged_scene, staged_tiles.get(), staged_pool.get());
    assert(candidate);
    auto next = Get(*candidate);
    const auto context = next.context;
    NexoraTwoDCamera restored_camera{};
    assert(next.get_camera(context, &restored_camera) == 0 &&
           restored_camera.vertical_size == camera.vertical_size &&
           restored_camera.rotation_radians == camera.rotation_radians);
    NexoraTwoDShape query{0, ColliderWire()};
    std::uint32_t count{};
    assert(next.overlap(context, 0, &query, nullptr, 0, &count) == 0 && count == 1);
    NexoraTwoDAnimationResult predicted{}, advanced{};
    std::array<NexoraTwoDEvent, 4> events{};
    const auto previous = *scene->Find(entity);
    assert(api.advance_animation(api.context, entity, .037, nullptr, 0, &predicted) == 0 &&
           next.advance_animation(context, entity, .037, events.data(), 4, &advanced) == 0 &&
           predicted.region == advanced.region && predicted.event_count == advanced.event_count);
    assert(staged_scene->Find(entity)->pivot.x == .25F && staged_scene->Find(entity)->order == 19 &&
           scene->Find(entity)->region == previous.region);
    value.transform.position.x = generation + 7;
    assert(next.write_instances(context, &handle, 1, &value) == 0);
    NexoraTwoDInstance original{};
    assert(api.read_instances(api.context, &handle, 1, &original) == 0 &&
           original.transform.position.x == generation + 6);

    // Reproduce the embedding's stable World address, isolated retirement and context rebinding.
    auto retired_world = std::move(world);
    assert(scene->RebindForReload(retired_world) &&
           bridge->RebindForReload(retired_world, *scene, tiles.get(), pool.get()));
    world = std::move(staged_world);
    assert(staged_scene->RebindForReload(world) &&
           candidate->RebindForReload(world, *staged_scene, staged_tiles.get(), staged_pool.get()));
    bridge->ForgetEntity(entity);
    assert(api.destroy_instances(api.context, &handle, 1) == 0 && staged_scene->Find(entity) &&
           staged_scene->Find(source_entity)->source == source.source &&
           staged_scene->Find(source_entity)->pivot.y == .875F &&
           next.read_instances(context, &handle, 1, &original) == 0 &&
           original.transform.position.x == generation + 7);
    bridge = std::move(candidate);
    scene = std::move(staged_scene);
    pool = std::move(staged_pool);
    tiles = std::move(staged_tiles);
    api = Get(*bridge);
    assert(api.context == context && &scene->WorldOwner() == &world);
  }
}
} // namespace
int main() {
  ReloadResources();
  runtime::World world(runtime::WorldKind::Play);
  const auto id = world.LoadScene("2D gameplay");
  const auto entity = world.CreateEntity(id).id;
  const auto otherScene = world.LoadScene("Other");
  const auto other = world.CreateEntity(otherScene).id;
  twod::SpriteScene scene(world, id);
  twod::Tilemap tiles;
  tiles.atlas = {1, 2};
  twod::InstancePool pool(4);
  auto clip = std::make_shared<const twod::Clip>(
      twod::Clip{{3, 4}, {1, 2}, true, {{1, 0.1, "a"}, {2, 0.1, "b"}}});
  twod::GameplayBridge *bridgePointer{};
  bool reentry{};
  twod::GameplayBridge bridge(
      world, scene,
      [&](auto asset) -> std::shared_ptr<const twod::Clip> {
        if (asset == twod::AssetId{9, 9})
          throw std::runtime_error("resolver failure");
        if (reentry) {
          NexoraTwoDApiV1 nested{};
          nested.struct_size = sizeof(nested);
          assert(bridgePointer->GetApi(&nested) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
        }
        return asset == clip->id ? clip : nullptr;
      },
      &tiles, &pool);
  bridgePointer = &bridge;
  auto api = Get(bridge);
  auto *p = api.context;
  assert(bridge.GetApi(nullptr) == -1);
  NexoraTwoDApiV1 shortApi{};
  shortApi.struct_size = sizeof(shortApi) - 1;
  const auto unchanged = shortApi;
  assert(bridge.GetApi(&shortApi) == -2 &&
         std::memcmp(&unchanged, &shortApi, sizeof(shortApi)) == 0);
  std::thread worker([&] {
    NexoraTwoDCamera c{};
    assert(api.get_camera(p, &c) == -3);
  });
  worker.join();
  runtime::World foreign;
  const auto foreignScene = foreign.LoadScene("Foreign");
  twod::SpriteScene wrongScene(foreign, foreignScene);
  bool mismatch{};
  try {
    twod::GameplayBridge rejected(world, wrongScene);
  } catch (const std::invalid_argument &) {
    mismatch = true;
  }
  assert(mismatch);

  auto sprite = SpriteWire();
  assert(api.set_sprite(p, entity, &sprite) == 0);
  assert(api.set_sprite(p, other, &sprite) == -1);
  auto badSprite = sprite;
  badSprite.flags = 16;
  assert(api.set_sprite(p, entity, &badSprite) == -1 && scene.Find(entity)->region == 1U);
  badSprite = sprite;
  badSprite.size.x = std::numeric_limits<float>::quiet_NaN();
  assert(api.set_sprite(p, entity, &badSprite) == -1);
  auto c = ColliderWire();
  assert(api.set_collider(p, entity, &c) == 0);
  c.kind = 256;
  assert(api.set_collider(p, entity, &c) == -1);
  c.kind = 0;
  NexoraTwoDCollider readCollider{};
  assert(api.get_collider(p, entity, &readCollider) == 0 && readCollider.radius == 0.5F);
  NexoraTwoDCamera camera{{2, -3}, 10, 0.4F};
  assert(api.set_camera(p, &camera) == 0);
  auto badCamera = camera;
  badCamera.rotation_radians = std::numeric_limits<float>::infinity();
  assert(api.set_camera(p, &badCamera) == -1);
  NexoraTwoDCamera readCamera{};
  assert(api.get_camera(p, &readCamera) == 0);
  assert(readCamera.rotation_radians == camera.rotation_radians);
  assert(bridge.RenderCamera(320, 240).width == 320U);

  reentry = true;
  assert(api.play_animation(p, entity, 3, 4, 1) == 0);
  reentry = false;
  assert(api.play_animation(p, entity, 9, 9, 1) == -3);
  assert(api.play_animation(p, entity, 5, 6, 1) == -2);
  NexoraTwoDAnimationResult result{};
  std::array<NexoraTwoDEvent, 4> events{};
  assert(api.advance_animation(p, entity, 0.25, nullptr, 0, &result) == 0 &&
         result.event_count == 2U);
  assert(scene.Find(entity)->region == 1U); // Probe does not advance.
  events[0].length = 99;
  assert(api.advance_animation(p, entity, 0.25, events.data(), 1, &result) == -2 &&
         events[0].length == 99U);
  assert(api.advance_animation(p, entity, 0.25, events.data(), 4, &result) == 0 &&
         result.event_count == 2U);
  assert(events[0].length == 1U && events[0].name[0] == 'b' && events[1].name[0] == 'a');
  assert(api.pause_animation(p, entity, 1) == 0);
  assert(api.advance_animation(p, entity, 1, events.data(), 4, &result) == 0 &&
         result.event_count == 0U);
  assert(api.pause_animation(p, entity, 0) == 0);
  assert(api.advance_animation(p, entity, -1, events.data(), 4, &result) == -1);
  assert(api.advance_animation(p, entity, 0.05, events.data(), 4, &result) == 0 &&
         result.region == 2U);

  NexoraTwoDShape query{0, ColliderWire()};
  std::uint32_t count = 99;
  std::array<uint64_t, 2> hits{42, 43};
  assert(api.overlap(p, 0, &query, hits.data(), 2, &count) == -3 && count == 99U);
  assert(api.rebuild_queries(p, 0) == 0);
  assert(api.overlap(p, 0, &query, nullptr, 0, &count) == 0 && count == 1U);
  assert(api.overlap(p, 0, &query, hits.data(), 0, &count) == -2 && hits[0] == 42U);
  assert(api.overlap(p, 0, &query, hits.data(), 2, &count) == 0 && hits[0] == entity);
  query.exclude_id = entity;
  assert(api.overlap(p, 0, &query, hits.data(), 2, &count) == 0 && count == 0U);
  query.exclude_id = 0;
  query.geometry.mask = 0;
  assert(api.overlap(p, 0, &query, hits.data(), 2, &count) == 0 && count == 0U);
  query.geometry.mask = UINT32_MAX;
  NexoraTwoDSweep sweep{{-2, 0}, {2, 0}, 0, 1, UINT32_MAX};
  std::array<NexoraTwoDHit, 2> swept{};
  assert(api.sweep(p, 0, &sweep, swept.data(), 2, &count) == 0 && count == 1U &&
         swept[0].fraction == 0.375);
  const NexoraTransformV2 transform{{5, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
  assert(game::WriteGameplayComponent(world, entity, game::TransformV2ComponentType(), &transform,
                                      sizeof(transform)) == 0);
  assert(api.overlap(p, 0, &query, hits.data(), 2, &count) == 0 &&
         count == 1U); // Explicit snapshot.
  assert(api.rebuild_queries(p, 0) == 0);
  assert(api.overlap(p, 0, &query, hits.data(), 2, &count) == 0 && count == 0U);
  sweep.radius = std::numeric_limits<float>::quiet_NaN();
  count = 42;
  assert(api.sweep(p, 0, &sweep, swept.data(), 2, &count) == -1 && count == 42U);
  assert(api.rebuild_queries(p, 2) == -1);

  const NexoraTwoDTile tile{-17, -2, -3, 1, 1};
  assert(api.paint_tiles(p, &tile, 1) == 0);
  NexoraTwoDTile readTile{};
  assert(api.get_tile(p, -17, -2, -3, &readTile) == 0 && readTile.x == -17 && readTile.y == -2);
  const std::array duplicate{tile, tile};
  assert(api.paint_tiles(p, duplicate.data(), 2) == -1 && tiles.Tiles().size() == 1U);
  assert(api.rebuild_queries(p, 1) == 0);
  query.geometry.offset = {-16.5F, -1.5F};
  assert(api.overlap(p, 1, &query, hits.data(), 2, &count) == 0 && count == 1U);
  assert(api.set_chunk_loaded(p, -2, -1, 0) == 0);
  assert(api.overlap(p, 1, &query, hits.data(), 2, &count) == -3);
  assert(api.rebuild_queries(p, 1) == 0);
  assert(api.overlap(p, 1, &query, hits.data(), 2, &count) == 0 && count == 0U);
  auto collected = tiles.Collect({});
  assert(collected && collected->instances.empty());
  assert(api.set_chunk_loaded(p, -2, -1, 1) == 0);

  const NexoraTwoDInstance value{{{0, 0, 0}, {0, 0, 0, 2}, {1, 1, 1}}, SpriteWire()};
  std::array values{value, value};
  std::array<NexoraTwoDInstanceHandle, 2> handles{};
  assert(api.create_instances(p, values.data(), 2, handles.data()) == 0 && pool.Size() == 2U);
  auto invalidValues = values;
  invalidValues[1].transform.scale.x = 0;
  assert(api.create_instances(p, invalidValues.data(), 2, handles.data()) == -1 &&
         pool.Size() == 2U);
  const auto originalHandles = handles;
  assert(api.read_instances(p, handles.data(), 2, values.data()) == 0 &&
         values[0].transform.rotation.w == 1);
  auto badHandles = handles;
  ++badHandles[1].owner;
  values[0].transform.position.x = 20;
  assert(api.write_instances(p, badHandles.data(), 2, values.data()) == -1);
  std::array<NexoraTwoDInstance, 2> current{};
  assert(api.read_instances(p, handles.data(), 2, current.data()) == 0 &&
         current[0].transform.position.x == 0);
  assert(api.write_instances(p, handles.data(), 2, values.data()) == 0);
  assert(api.read_instances(p, handles.data(), 2, current.data()) == 0 &&
         current[0].transform.position.x == 20);
  badHandles = {handles[0], handles[0]};
  assert(api.write_instances(p, badHandles.data(), 2, values.data()) == -1);
  assert(api.destroy_instances(p, badHandles.data(), 2) == -1 && pool.Size() == 2U);
  assert(api.destroy_instances(p, handles.data(), 2) == 0 && pool.Size() == 0U);
  assert(api.create_instances(p, values.data(), 2, handles.data()) == 0);
  assert(api.read_instances(p, originalHandles.data(), 2, current.data()) == -1);
  assert(api.create_instances(p, nullptr, 0, nullptr) == 0);
  assert(api.destroy_instances(p, nullptr, 0) == 0);

  twod::GameplayBridge minimal(world, scene);
  auto unsupported = Get(minimal);
  assert(unsupported.rebuild_queries(unsupported.context, 1) == -2);
  assert(unsupported.create_instances(unsupported.context, values.data(), 2, handles.data()) == -2);
  assert(api.set_sprite(p, entity, nullptr) == 0);
  assert(api.pause_animation(p, entity, 0) == -1);
  runtime::WorldCommandBuffer destroy;
  destroy.DestroyEntity(entity);
  assert(destroy.Apply(world));
  for (auto removed : destroy.LastDestroyed())
    bridge.ForgetEntity(removed);
  assert(api.get_collider(p, entity, &readCollider) == -1);
  assert(scene.Snapshot());
}
