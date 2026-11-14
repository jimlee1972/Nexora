#include "Nexora/TwoD/GameplayGeneration.h"
#include <array>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

using namespace nexora;
namespace {
constexpr twod::AssetId first{11, 12}, second{13, 14}, clipId{15, 16};
void Check(bool value, const char *reason) {
  if (!value)
    throw std::runtime_error(reason);
}
void Move(runtime::World &world, runtime::Id entity, double x, double y) {
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(entity, {x, y, 0});
  Check(commands.Apply(world), "generation World transform transaction");
}
NexoraTwoDApiV1 Api(twod::GameplayGeneration &generation) {
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  Check(generation.Bridge().GetApi(&api) == 0, "generation API");
  return api;
}
std::shared_ptr<const twod::Clip> Clip() {
  return std::make_shared<const twod::Clip>(
      twod::Clip{clipId, {}, true, {{0, 0.25, "second", first}, {0, 0.5, "first", second}}});
}
struct Resolver {
  std::shared_ptr<const twod::Clip> clip;
  std::shared_ptr<bool> fail;
  std::shared_ptr<std::function<void()>> onCopy;
  Resolver(std::shared_ptr<const twod::Clip> c, std::shared_ptr<bool> f,
           std::shared_ptr<std::function<void()>> callback = {})
      : clip(std::move(c)), fail(std::move(f)), onCopy(std::move(callback)) {}
  Resolver(const Resolver &other) : clip(other.clip), fail(other.fail), onCopy(other.onCopy) {
    if (onCopy && *onCopy)
      (*onCopy)();
    if (*fail)
      throw std::bad_alloc();
  }
  std::shared_ptr<const twod::Clip> operator()(twod::AssetId id) const {
    return id == clipId ? clip : nullptr;
  }
};
std::uint32_t Hits(NexoraTwoDApiV1 api) {
  NexoraTwoDShape shape{};
  shape.geometry.offset = {2, 3};
  shape.geometry.radius = 1;
  shape.geometry.group = 1;
  shape.geometry.mask = UINT32_MAX;
  std::uint32_t count{};
  Check(api.overlap(api.context, 0, &shape, nullptr, 0, &count) == 0, "staged query snapshot");
  return count;
}
void SingleScene() {
  const auto fail = std::make_shared<bool>(false);
  auto live = twod::GameplayGeneration::CreateSingleScene("Main", 2, Resolver{Clip(), fail});
  Check(bool(live) && !live->Scenes() && live->Sprites() && live->Tiles(), "single owner");
  const auto scene = live->Sprites()->SceneId();
  Check(live->World().Activate(scene), "activate owner scene");
  const auto entity = live->World().CreateEntity(scene).id;
  Move(live->World(), entity, 2, 3);
  twod::Sprite sprite;
  sprite.source = first;
  sprite.order = 17;
  sprite.flipX = sprite.sortY = true;
  Check(live->Sprites()->Set(entity, sprite) && live->Sprites()->SetCollider(entity, {}),
        "generation Sprite/collider");
  const std::array instances{twod::Instance{{3, 2, 0, 0, 0, 0, 1, -1, 1, 1}, sprite},
                             twod::Instance{{4, 2, 0}, sprite}};
  const auto handles = live->Pool().Create(instances);
  Check(handles && handles->size() == 2 && live->Pool().Destroy(std::span(handles->data(), 1)),
        "prepare live/stale pool handles");
  const auto stale = (*handles)[0], retained = (*handles)[1];
  live->Tiles()->atlas = {7, 8};
  const twod::Tile tile{0, 0, 0, 1, true};
  Check(live->Tiles()->Paint(std::span(&tile, 1)) && live->Tiles()->SetChunkLoaded({0, 0}, false),
        "generation tile chunks");
  auto api = Api(*live);
  const NexoraTwoDCamera camera{{7, -3}, 12, 0.3F};
  Check(api.set_camera(api.context, &camera) == 0 && api.rebuild_queries(api.context, 0) == 0 &&
            Hits(api) == 1 &&
            api.play_animation(api.context, entity, clipId.high, clipId.low, 1) == 0,
        "live camera/query/animation");
  constexpr std::uint32_t capacity = 2;
  std::array<NexoraTwoDEvent, capacity> events{};
  NexoraTwoDAnimationResult result{};
  Check(api.advance_animation(api.context, entity, 0.125, events.data(), capacity, &result) == 0 &&
            result.event_count == 0 && api.pause_animation(api.context, entity, 1) == 0,
        "prepare partial paused clock");
  const auto world = live->World().SaveScene(scene);
  const auto metadata = live->Sprites()->Snapshot();
  const auto tiles = live->Tiles()->Encode();
  *fail = true;
  Check(!live->Stage() && live->World().SaveScene(scene) == world &&
            live->Sprites()->Snapshot() == metadata && live->Tiles()->Encode() == tiles &&
            live->Pool().Size() == 1 && live->Pool().Find(retained) && Hits(api) == 1,
        "allocation failure changed live generation");
  *fail = false;
  bool rejected{};
  std::thread other([&] { rejected = !live->Stage(); });
  other.join();
  Check(rejected, "wrong-thread Stage admitted");
  rejected = false;
  live->Pool().ForEach([&](auto &) { rejected = !live->Stage(); });
  Check(rejected, "Stage admitted during pool iteration");
  auto candidate = live->Stage();
  Check(candidate && &candidate->World() != &live->World() &&
            candidate->World().SaveScene(scene) == world &&
            candidate->Sprites()->Snapshot() == metadata && candidate->Tiles()->Encode() == tiles &&
            &candidate->Sprites()->WorldOwner() == &candidate->World() &&
            candidate->Pool().Find(retained) && !candidate->Pool().Find(stale),
        "staged values, World binding or handles changed");
  auto staged = Api(*candidate);
  Check(staged.context != api.context && Hits(staged) == 1,
        "staged bridge context/query readiness lost");
  NexoraTwoDCamera copied{};
  Check(staged.get_camera(staged.context, &copied) == 0 && copied.center.x == 7 &&
            copied.center.y == -3 && copied.vertical_size == 12 && copied.rotation_radians == 0.3F,
        "staged camera changed");
  Move(candidate->World(), entity, 42, 3);
  Check(Hits(staged) == 1 && staged.rebuild_queries(staged.context, 0) == 0 && Hits(staged) == 0 &&
            Hits(api) == 1 && live->World().FindEntity(entity)->transform.x == 2,
        "query/World generations alias");
  Check(staged.pause_animation(staged.context, entity, 0) == 0 &&
            staged.advance_animation(staged.context, entity, 0.125, events.data(), capacity,
                                     &result) == 0 &&
            result.event_count == 1 && candidate->Sprites()->Find(entity)->source == second &&
            candidate->Sprites()->Find(entity)->order == 17 &&
            candidate->Sprites()->Find(entity)->flipX &&
            api.advance_animation(api.context, entity, 0.125, events.data(), capacity, &result) ==
                0 &&
            result.event_count == 0 && live->Sprites()->Find(entity)->source == first,
        "staged clock/pause/style or live isolation lost");
  const std::array retainedHandles{retained};
  Check(candidate->Pool().Destroy(retainedHandles) && live->Pool().Find(retained) &&
            live->Pool().Size() == 1,
        "retired/candidate pool cleanup mutated live handles");
  const auto replacement = candidate->Pool().Create(std::span(instances).first(1));
  Check(replacement && !candidate->Pool().Find(retained) &&
            candidate->Pool().Find(replacement->front()) &&
            !live->Pool().Find(replacement->front()),
        "stale pool slot resurrected or escaped its generation");
  candidate.reset();
  Check(api.pause_animation(api.context, entity, 0) == 0 &&
            api.advance_animation(api.context, entity, 0.125, events.data(), capacity, &result) ==
                0 &&
            result.event_count == 1 && live->Sprites()->Find(entity)->source == second,
        "failed/discarded stages consumed live partial clock");
}
std::shared_ptr<const twod::AuthoredScene> Source() {
  runtime::World world;
  const auto sceneId = world.LoadScene("Source");
  const auto entity = world.CreateEntity(sceneId).id;
  twod::SpriteScene scene(world, sceneId);
  const twod::Image image{1, 1, 1, twod::Bytes(4, std::byte{255})};
  const auto atlas = twod::PackAtlas({7, 8}, std::span(&image, 1), 16);
  Check(bool(atlas), "catalog Atlas");
  twod::Sprite sprite;
  sprite.atlas = atlas->id;
  sprite.region = 1;
  Check(scene.Set(entity, sprite) && scene.SetCollider(entity, {}), "catalog Sprite");
  return twod::AuthoredScene::Decode(twod::EncodeAuthoredScene(scene, *atlas, {}, {}));
}
void Catalog() {
  const auto source = Source();
  Check(bool(source), "catalog source");
  auto live = twod::GameplayGeneration::CreateCatalog(
      {{"A.scene", {91, 1}, source}, {"B.scene", {91, 2}, source}}, 2);
  Check(live && live->Scenes() && !live->Sprites() && !live->Tiles(), "catalog owner mode");
  auto api = live->Scenes()->Api();
  std::uint64_t request{};
  Check(api.request_load(api.context, 91, 1, 0, 0, &request) == 0, "first scene request");
  bool rejected{};
  Check(live->Scenes()->Process([&](const auto &, auto) {
    rejected = !live->Stage();
    return true;
  }) && rejected,
        "Stage admitted during scene publication");
  const auto primary = live->Scenes()->Primary();
  const auto entity = live->Scenes()->Instances().front().sprites.Sprites().begin()->first;
  const auto snapshot = live->World().SaveScene(primary);
  const auto revision = live->Scenes()->Revision();
  const auto completed = request;
  Check(api.request_load(api.context, 91, 2, 1, 0, &request) == 0, "pending additive request");
  auto candidate = live->Stage();
  Check(candidate && candidate->Scenes()->Revision() == revision &&
            candidate->Scenes()->Primary() == primary &&
            candidate->World().SaveScene(primary) == snapshot,
        "catalog metadata changed on Stage");
  auto staged = candidate->Scenes()->Api();
  NexoraSceneTransitionV1 status{};
  status.struct_size = sizeof(status);
  Check(staged.context != api.context &&
            staged.get_status(staged.context, completed, &status) == 0 && status.state == 2 &&
            staged.get_status(staged.context, request, &status) == 0 && status.state == 1,
        "pending/completed scene request state lost");
  Check(candidate->Scenes()->Process() && candidate->World().ActiveSceneCount() == 2 &&
            live->World().ActiveSceneCount() == 1 &&
            api.get_status(api.context, request, &status) == 0 && status.state == 1,
        "candidate scene transition escaped into live generation");
  for (const auto &instance : candidate->Scenes()->Instances())
    Check(&instance.sprites.WorldOwner() == &candidate->World(),
          "catalog Sprite World not rebound");
  Move(candidate->World(), entity, 123, 0);
  Check(live->World().FindEntity(entity)->transform.x == 0,
        "catalog retained scene World state aliases");
  candidate->Scenes()->Stop();
  Check(!candidate->Stage() && live->World().SaveScene(primary) == snapshot,
        "stopped catalog admitted staging or altered live state");
}
void CallbackReentry() {
  twod::GameplayGeneration *owner{};
  bool rejected{};
  const auto clip = Clip();
  auto live = twod::GameplayGeneration::CreateSingleScene("Main", 1, [&](auto id) {
    rejected = owner && !owner->Stage();
    return id == clipId ? clip : nullptr;
  });
  Check(bool(live), "callback generation");
  owner = live.get();
  const auto scene = live->Sprites()->SceneId();
  Check(live->World().Activate(scene), "callback scene");
  const auto entity = live->World().CreateEntity(scene).id;
  twod::Sprite sprite;
  sprite.source = first;
  Check(live->Sprites()->Set(entity, sprite), "callback source");
  auto api = Api(*live);
  Check(api.play_animation(api.context, entity, clipId.high, clipId.low, 1) == 0 && rejected &&
            live->Stage(),
        "Stage callback reentry or subsequent quiescence failed");
}
void CopyReentry() {
  const auto callback = std::make_shared<std::function<void()>>();
  auto live = twod::GameplayGeneration::CreateSingleScene(
      "Main", 1, Resolver{Clip(), std::make_shared<bool>(false), callback});
  Check(bool(live), "copy-reentry owner");
  unsigned calls{};
  *callback = [&] {
    ++calls;
    Check(!live->Stage(), "resolver copy recursively admitted staging");
  };
  Check(bool(live->Stage()) && calls == 1 && bool(live->Stage()) && calls == 2,
        "resolver copy reentry guard or release failed");
  *callback = {};
}
} // namespace
int main() {
  try {
    Check(!twod::GameplayGeneration::CreateSingleScene("") &&
              !twod::GameplayGeneration::CreateSingleScene("Main", 0) &&
              !twod::GameplayGeneration::CreateCatalog({}),
          "invalid generation factory admitted");
    SingleScene();
    Catalog();
    CallbackReentry();
    CopyReentry();
    std::cout
        << "PASS isolated 2D generation values, clocks, queries, handles and pending scenes\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
