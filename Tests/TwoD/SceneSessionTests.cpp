#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/SceneSession.h"
#include <cassert>
#include <thread>
using namespace nexora;
namespace {
std::shared_ptr<const twod::AuthoredScene> Source(float x) {
  runtime::World world;
  const auto sceneId = world.LoadScene("Authored scene");
  const auto parent = world.CreateEntity(sceneId).id;
  const auto child = world.CreateEntity(sceneId).id;
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(parent, {x, 0, 0});
  commands.SetTransform(child, {2, 0, 0});
  commands.SetParent(child, parent, false);
  assert(commands.Apply(world));
  twod::SpriteScene scene(world, sceneId);
  twod::Image image{1, 1, 1, twod::Bytes(4, std::byte{255})};
  auto atlas = twod::PackAtlas({17, 23}, std::span(&image, 1), 16);
  assert(atlas);
  twod::Sprite sprite;
  sprite.atlas = atlas->id;
  sprite.region = 1;
  assert(scene.Set(child, sprite) && scene.SetCollider(child, {}));
  twod::Tilemap tiles;
  tiles.atlas = atlas->id;
  const twod::Tile tile{0, 0, 0, 1, true};
  assert(tiles.Paint(std::span(&tile, 1)));
  const auto bytes = twod::EncodeAuthoredScene(scene, *atlas, {}, tiles);
  const auto source = twod::AuthoredScene::Decode(bytes);
  assert(source && twod::EncodeAuthoredScene(*source->scene, source->atlas, source->clip,
                                             source->tiles) == bytes);
  auto bad = bytes;
  bad.push_back(std::byte{0});
  assert(!twod::AuthoredScene::Decode(bad));
  for (std::size_t i = 0; i < bytes.size(); ++i)
    assert(!twod::AuthoredScene::Decode(std::span(bytes).first(i)));
  return source;
}
std::uint64_t Load(NexoraGameplaySceneApiV1 api, const char *path, std::uint32_t mode = 0,
                   std::uint32_t flags = 0) {
  std::uint64_t high{}, low{}, request{};
  assert(api.resolve_scene(api.context, path, uint32_t(std::char_traits<char>::length(path)), &high,
                           &low) == 0);
  assert(api.request_load(api.context, high, low, mode, flags, &request) == 0);
  return request;
}
NexoraSceneTransitionV1 Status(NexoraGameplaySceneApiV1 api, std::uint64_t request) {
  NexoraSceneTransitionV1 status{sizeof(status), 0, 0, 0, 0, 0};
  assert(api.get_status(api.context, request, &status) == 0);
  return status;
}
} // namespace
int main() {
  const auto start = Source(1), title = Source(10), main = Source(100);
  const std::vector<twod::SceneEntry> catalog{
      {"Start.scene", {1, 1}, start}, {"Title.scene", {1, 2}, title}, {"Main.scene", {1, 3}, main}};
  runtime::World world(runtime::WorldKind::Play);
  twod::SceneSession session(world, catalog);
  auto api = session.Api();
  twod::InstancePool pool(4);
  twod::GameplayBridge bridge(world, session, {}, &pool);
  NexoraTwoDApiV1 twoD{};
  twoD.struct_size = sizeof(twoD);
  assert(bridge.GetApi(&twoD) == 0);
  // Queueing does not publish, and Busy/unknown/invalid requests conserve output sentinels.
  const auto first = Load(api, "Start.scene");
  assert(Status(api, first).state == 1 && session.Primary() == 0);
  std::uint64_t sentinel = 999;
  assert(api.request_load(api.context, 1, 2, 0, 0, &sentinel) == -4 && sentinel == 999);
  assert(session.Process());
  auto initial = Status(api, first).scene;
  assert(initial == session.Primary() && Status(api, first).state == 2);
  const auto child = session.Instances()[0].entities.at(3);
  const auto parent = session.Instances()[0].entities.at(2);
  assert(world.Parent(child) == parent && world.WorldTransform(child)->x == 3);
  assert(session.FindEntity(child)->FindCollider(child));
  assert(api.request_load(api.context, 99, 99, 0, 0, &sentinel) == -5 && sentinel == 999);
  assert(api.request_load(api.context, 1, 1, 2, 0, &sentinel) == -1);
  assert(api.resolve_scene(api.context, "../Main.scene", 13, &sentinel, &sentinel) == -1);
  NexoraTwoDSprite sprite{};
  assert(twoD.get_sprite(twoD.context, child, &sprite) == 0 && sprite.region == 1);
  assert(twoD.rebuild_queries(twoD.context, 0) == 0);
  // A rejected host preparation retains transforms, metadata, tiles, primary and request status.
  const auto snapshot = world.SaveScene(initial);
  // A catalog bridge must not enter the legacy single-scene reload path: it owns no
  // single scene pointer, and dropping its catalog would lose routing/transition state.
  const auto revision = session.Revision();
  {
    auto candidate_world = world;
    auto metadata = session.FindEntity(child)->ForkForReload(candidate_world);
    auto candidate_pool = pool.ForkForReload();
    assert(metadata && candidate_pool);
    assert(!bridge.ForkForReload(candidate_world, *metadata, nullptr, candidate_pool.get()));
    assert(!bridge.RebindForReload(world, *session.FindEntity(child), nullptr, &pool));
  }
  assert(session.Revision() == revision && session.Primary() == initial &&
         world.SaveScene(initial) == snapshot &&
         twoD.get_sprite(twoD.context, child, &sprite) == 0);
  auto request = Load(api, "Title.scene");
  assert(!session.Process([](const auto &, auto) { return false; }));
  assert(Status(api, request).state == 3 && session.Primary() == initial &&
         world.SaveScene(initial) == snapshot && session.FindEntity(child));
  assert(twoD.get_sprite(twoD.context, child, &sprite) == 0);
  request = Load(api, "Title.scene");
  assert(!session.Process([](const auto &, auto) -> bool { throw std::runtime_error("prepare"); }));
  assert(Status(api, request).state == 3 && world.SaveScene(initial) == snapshot);
  // Replace allocates fresh IDs even for repeated imports of the same authored identities.
  request = Load(api, "Title.scene");
  assert(session.Process());
  const auto titleId = Status(api, request).scene;
  assert(titleId > initial && !world.FindScene(initial) && !world.FindEntity(child));
  assert(twoD.get_sprite(twoD.context, child, &sprite) == -1);
  assert(session.Instances()[0].entities.at(3) > child);
  // A stable API context now sees the next scene's authored Sprite and transformed collider.
  assert(twoD.context != nullptr && session.CollisionShapes()->at(0).center.x == 12);
  assert(session.PrimaryTiles()->Tiles().size() == 1);
  const auto prior = request;
  request = Load(api, "Main.scene", 1);
  assert(Status(api, prior).state == 2 && session.Process());
  const auto mainId = Status(api, request).scene;
  assert(session.Primary() == titleId && session.Instances().size() == 2 &&
         world.ActiveSceneCount() == 2 && session.CollisionShapes()->size() == 2);
  const auto mainChild = session.Instances()[1].entities.at(3);
  assert(twoD.get_sprite(twoD.context, mainChild, &sprite) == 0);
  assert(twoD.rebuild_queries(twoD.context, 0) == 0);
  assert(api.request_load(api.context, 1, 3, 1, 0, &sentinel) == -1);
  assert(api.request_unload(api.context, titleId, &request) == 0 && session.Process());
  assert(session.Primary() == mainId && !world.FindScene(titleId) &&
         session.Instances().size() == 1);
  assert(api.request_unload(api.context, mainId, &sentinel) == -1);
  // Persistent additive instances survive replacements, and cannot be unloaded explicitly.
  request = Load(api, "Start.scene", 1, 1);
  assert(session.Process());
  const auto persistent = Status(api, request).scene;
  request = Load(api, "Title.scene");
  assert(session.Process() && world.FindScene(persistent) && !world.FindScene(mainId));
  assert(api.request_unload(api.context, persistent, &sentinel) == -1);
  NexoraSceneInstanceV1 info{sizeof(info), 0, 0, 0, 0, 0};
  assert(api.get_scene_info(api.context, persistent, &info) == 0 &&
         (info.flags & NEXORA_SCENE_PERSISTENT) && !(info.flags & NEXORA_SCENE_PRIMARY));
  // Repeated replacements prune retired records and never recycle identities.
  auto previous = session.Primary();
  for (unsigned i = 0; i < 100; ++i) {
    request = Load(api, "Title.scene");
    assert(session.Process() && session.Primary() > previous && !world.FindScene(previous));
    assert(session.Instances().size() == 2 && world.ActiveSceneCount() == 2);
    previous = session.Primary();
  }
  std::thread other([&] {
    std::uint64_t value = 999;
    assert(api.get_active_scene(api.context, &value) == -3 && value == 999);
    assert(!session.Process());
  });
  other.join();
  request = Load(api, "Main.scene");
  session.Stop();
  assert(!session.Process());
  assert(Status(api, request).state == 4);
  assert(api.get_active_scene(api.context, &sentinel) == -3);
  // Generic snapshot instantiation also rejects malformed input without advancing live state.
  assert(!world.InstantiateScene("invalid"));
  return 0;
}
