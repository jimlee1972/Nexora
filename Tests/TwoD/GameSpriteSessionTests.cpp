#include "../../Apps/ProjectPlayer/GameplayPlayer.h"
#include "../Runtime/GameSceneFixture.h"
#include "Nexora/TwoD/GameSpriteSession.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
constexpr runtime::AssetUuid atlas_id{101, 1}, first_id{102, 1}, second_id{103, 1}, clip_id{105, 1};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::shared_ptr<const runtime::LoadedGameProject> Project() {
  auto package = testscene::Package();
  const twod::Image image{1, 2, 2, twod::Bytes(16, std::byte{255})};
  const auto atlas = twod::PackAtlas(atlas_id, std::span(&image, 1), 8);
  Check(bool(atlas), "Atlas fixture");
  twod::SpriteAsset source;
  source.id = first_id;
  source.atlas = atlas_id;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = 1;
  source.trimWidth = source.trimHeight = 2;
  source.pivotX = 2;
  source.pivotY = 3;
  source.pixelsPerUnit = 1;
  const auto first = twod::EncodeSpriteAsset(source);
  source.id = second_id;
  source.pivotX = 3;
  const auto second = twod::EncodeSpriteAsset(source);
  const twod::Clip clip{
      clip_id, {}, true, {{0, .25, "first", first_id}, {0, .25, "second", second_id}}};
  runtime::ByteBuffer reference{std::byte{1}};
  for (const auto word : {first_id.high, first_id.low})
    for (unsigned i = 0; i < 8; ++i)
      reference.push_back(static_cast<std::byte>(word >> (8 * i)));
  for (unsigned i = 0; i < 2; ++i) {
    auto scene = runtime::DecodeCookedScene(package.assets[i].payload);
    Check(bool(scene), "scene decode");
    scene.Value().opaque.push_back(
        {scene.Value().bindings[0].entity, runtime::kGameSpriteSourceReferenceType,
         std::string(runtime::kGameSpriteSourceReferenceName), reference});
    package.assets[i] = testscene::Cook(package.assets[i].id, runtime::kCookedSceneType,
                                        runtime::EncodeCookedScene(scene.Value()).Value(),
                                        {testscene::kMesh, testscene::kMaterial, first_id});
  }
  package.assets.push_back(testscene::Cook(atlas_id, "sprite_atlas", twod::EncodeAtlas(*atlas)));
  package.assets.push_back(testscene::Cook(first_id, "sprite_source", first, {atlas_id}));
  package.assets.push_back(testscene::Cook(second_id, "sprite_source", second, {atlas_id}));
  package.assets.push_back(testscene::Cook(clip_id, "sprite_clip", twod::EncodeClip(clip),
                                           twod::ClipDependencies(clip)));
  package.animation_clips = {clip_id};
  auto project = runtime::LoadGameProjectPackage(package);
  Check(bool(project), "source game package");
  return std::make_shared<const runtime::LoadedGameProject>(std::move(*project));
}
runtime::Id SpriteEntity(const runtime::GameSceneInstance &instance) {
  for (const auto &record : instance.source->SceneData().opaque)
    if (record.type == runtime::kGameSpriteSourceReferenceType)
      return instance.entities.at(record.entity);
  throw std::runtime_error("authored source fixture missing");
}
NexoraTwoDSourceSpriteV1 Read(const NexoraTwoDSourceApiV1 &api, runtime::Id id) {
  NexoraTwoDSourceSpriteV1 value{};
  Check(api.get_sprite(api.context, id, &value) == 0, "read live source Sprite");
  return value;
}
void Run() {
  auto project = Project();
  auto assets = twod::GameSpriteAssets::Load(project->SpriteAssets());
  Check(bool(assets), "typed source/Clip admission");
  player::PlayerGameplay gameplay;
  auto &game = gameplay.world;
  auto &world = game.InternalWorld();
  runtime::GameSceneSession scenes(game, project);
  twod::GameSpriteSession sprites(world, std::move(*assets), 4);
  const auto legacy_host = gameplay.Host();
  Check(!(legacy_host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) &&
            !legacy_host.get_twod_api,
        "source-free host unexpectedly advertises TwoD");
  gameplay.get_twod = [&](NexoraTwoDApiV1 *output) { return sprites.Bridge().GetApi(output); };
  const auto host = gameplay.Host();
  Check((host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) && host.get_twod_api,
        "main Player did not negotiate TwoD");
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  Check(host.get_twod_api(host.context, &api) == 0, "main 2D API");
  NexoraTwoDApiV1 prefix{};
  prefix.struct_size = NEXORA_2D_API_V1_BASE_SIZE;
  Check(host.get_twod_api(host.context, &prefix) == 0 &&
            prefix.struct_size == NEXORA_2D_API_V1_BASE_SIZE && !prefix.get_source_api,
        "legacy TwoD prefix negotiation");
  NexoraTwoDSourceApiV1 sources{};
  sources.struct_size = sizeof(sources);
  Check(api.get_source_api(api.context, &sources) == 0, "main source API");
  auto transition = [&](runtime::AssetUuid asset, runtime::GameSceneLoadMode mode, bool persistent,
                        bool refuse = false) {
    std::uint64_t request{};
    Check(scenes.RequestLoad(asset, mode, persistent, request) == runtime::GameSceneError::None,
          "queue main scene transition");
    const auto revision = sprites.Revision();
    bool prepared{};
    const auto processed =
        scenes.Process([&](const runtime::GameSceneView &view,
                           std::string &error) -> std::unique_ptr<runtime::GameScenePresentation> {
          auto candidate = sprites.Prepare(view, &error);
          Check(bool(candidate), error.c_str());
          Check(!sprites.Available() && !sprites.Prepare(view) && !sprites.Reconcile(),
                "outstanding metadata preparation admitted another operation");
          NexoraTwoDApiV1 blocked{};
          blocked.struct_size = sizeof(blocked);
          Check(sprites.Bridge().GetApi(&blocked) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE,
                "module entry admitted during native preparation");
          const auto frame = candidate->Extract(view.world, 960, 640);
          Check(frame && !frame->statistics.unavailable, "candidate source frame");
          Check(!candidate->Extract(world, 960, 640), "candidate accepted a foreign World borrow");
          Check(sprites.Revision() == revision, "candidate published before World commit");
          prepared = true;
          if (refuse) {
            error = "Injected native admission rejection";
            return {};
          }
          return candidate;
        });
    Check(prepared && processed != refuse && sprites.Available(), "native admission rollback");
    Check(sprites.Revision() == revision + (refuse ? 0 : 1), "metadata commit count");
  };
  transition(testscene::kLevel, runtime::GameSceneLoadMode::Replace, true);
  const auto level_scene = scenes.View().primary;
  const auto entity = SpriteEntity(scenes.View().instances.front());
  Check(sprites.Primary() == level_scene && sprites.FindEntity(entity) &&
            &sprites.FindEntity(entity)->WorldOwner() == &world,
        "main metadata is not bound to the same remapped World");
  auto value = Read(sources, entity);
  Check(value.source_high == first_id.high && value.source_low == first_id.low,
        "initial authored source changed");
  value.sprite.flags |= 1U | 4U;
  value.sprite.color[0] = .5F;
  Check(sources.set_sprite(sources.context, entity, &value) == 0, "live source style");
  runtime::WorldCommandBuffer edit;
  auto transform = world.FindEntity(entity)->transform;
  transform.sx = -1;
  edit.SetTransform(entity, transform);
  Check(edit.Apply(world), "negative World scale");
  const auto matrix = world.WorldMatrix(entity);
  Check(api.play_animation(api.context, entity, clip_id.high, clip_id.low, 1) == 0,
        "main source animation");
  NexoraTwoDAnimationResult result{};
  std::array<NexoraTwoDEvent, 4> events{};
  const auto initial = sprites.Extract(960, 640);
  Check(initial && initial->statistics.input == 1, "initial mutable extraction");
  Check(api.advance_animation(api.context, entity, .25, nullptr, 0, &result) == 0 &&
            result.event_count == 1 && Read(sources, entity).source_high == first_id.high,
        "count probe advanced the live source clock");
  Check(api.advance_animation(api.context, entity, .25, events.data(), 4, &result) == 0,
        "advance actual main animation");
  value = Read(sources, entity);
  Check(value.source_high == second_id.high && value.source_low == second_id.low &&
            value.sprite.flags == (8U | 1U | 4U) && value.sprite.color[0] == .5F &&
            world.WorldMatrix(entity) == matrix,
        "source frame changed style or World/Pivot anchor");
  const auto changed = sprites.Extract(960, 640);
  Check(changed && changed->vertices.front().position[0] != initial->vertices.front().position[0],
        "main frame extraction replayed the immutable authored source");
  Check(api.pause_animation(api.context, entity, 1) == 0 &&
            api.advance_animation(api.context, entity, 1, events.data(), 4, &result) == 0 &&
            Read(sources, entity).source_high == second_id.high,
        "paused source clock changed");
  NexoraTwoDSourceInstanceV1 pooled{};
  pooled.transform.rotation.w = 1;
  pooled.transform.scale = {-1, 1, 1};
  pooled.sprite = value;
  NexoraTwoDInstanceHandle handle{};
  Check(sources.create_instances(sources.context, &pooled, 1, &handle) == 0 && handle.owner,
        "main source pool creation");
  // The main Game router is not a single-scene reload owner, even with valid cloned inputs.
  auto cloned_world = world;
  twod::SpriteScene cloned_scene(cloned_world, level_scene);
  auto cloned_pool = sprites.Pool().ForkForReload();
  const auto revision = sprites.Revision();
  NexoraTwoDApiV1 rebound_api{};
  rebound_api.struct_size = sizeof(rebound_api);
  NexoraTwoDSourceInstanceV1 retained_pool{};
  Check(
      cloned_pool &&
          !sprites.Bridge().ForkForReload(cloned_world, cloned_scene, nullptr, cloned_pool.get()) &&
          !sprites.Bridge().RebindForReload(cloned_world, cloned_scene, nullptr,
                                            cloned_pool.get()) &&
          sprites.Available() && sprites.Revision() == revision &&
          sprites.Bridge().GetApi(&rebound_api) == 0 && rebound_api.context == api.context &&
          &sprites.FindEntity(entity)->WorldOwner() == &world &&
          world.WorldMatrix(entity) == matrix &&
          Read(sources, entity).source_high == second_id.high &&
          sources.read_instances(sources.context, &handle, 1, &retained_pool) == 0 &&
          retained_pool.sprite.source_high == second_id.high,
      "single-scene reload changed the main Game World/animation/pool owner");
  auto frame = sprites.Extract(960, 640);
  Check(frame && frame->statistics.input == 2 && frame->statistics.visible == 2,
        "live pool missing from main extraction");
  const NexoraTwoDCollider collider{0, {0, 0}, {.5F, .5F}, .5F, 1, UINT32_MAX};
  Check(api.set_collider(api.context, entity, &collider) == 0 &&
            api.rebuild_queries(api.context, 0) == 0 &&
            api.rebuild_queries(api.context, 1) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
        "main collider query or unsupported Tilemap contract");
  transition(testscene::kTitle, runtime::GameSceneLoadMode::Additive, false);
  Check(sprites.Primary() == level_scene && Read(sources, entity).source_high == second_id.high,
        "Additive lost retained source state");
  const auto before = world.SaveScene(level_scene);
  transition(testscene::kTitle, runtime::GameSceneLoadMode::Replace, false, true);
  Check(world.SaveScene(level_scene) == before && sprites.Primary() == level_scene &&
            Read(sources, entity).source_high == second_id.high,
        "failed candidate changed published World/source state");
  transition(testscene::kTitle, runtime::GameSceneLoadMode::Replace, false);
  Check(sprites.Primary() != level_scene && Read(sources, entity).source_high == second_id.high,
        "Replace discarded persistent source metadata");
  NexoraTwoDSourceInstanceV1 read{};
  Check(sources.read_instances(sources.context, &handle, 1, &read) == 0 &&
            read.sprite.source_high == second_id.high,
        "scene transition replaced a live pool handle");
  Check(api.pause_animation(api.context, entity, 0) == 0 &&
            api.advance_animation(api.context, entity, .25, events.data(), 4, &result) == 0 &&
            Read(sources, entity).source_high == first_id.high &&
            world.WorldMatrix(entity) == matrix,
        "persistent animator clock/style was lost during Replace");
  const auto title_scene = scenes.View().primary;
  const auto title =
      std::ranges::find(scenes.View().instances, title_scene, &runtime::GameSceneInstance::scene);
  const auto title_entity = SpriteEntity(*title);
  std::uint64_t request{};
  Check(scenes.RequestUnload(title_scene, request) == runtime::GameSceneError::None &&
            scenes.Process([&](const auto &view, std::string &error) {
              return sprites.Prepare(view, &error);
            }),
        "unload main scene metadata");
  Check(sprites.Primary() == level_scene &&
            sources.get_sprite(sources.context, title_entity, &value) ==
                NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
            sources.read_instances(sources.context, &handle, 1, &read) == 0,
        "unloaded metadata or pool handle lifetime");
  auto instances = std::vector(scenes.View().instances.begin(), scenes.View().instances.end());
  instances.front().entities.clear();
  Check(!sprites.Prepare({world, instances, level_scene, 0}) && sprites.Available(),
        "incomplete remapping admitted");
  bool guarded{};
  std::thread foreign([&] {
    NexoraTwoDApiV1 output{};
    output.struct_size = sizeof(output);
    guarded = sprites.Bridge().GetApi(&output) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              !sprites.Prepare(scenes.View()) && !sprites.Extract(960, 640);
  });
  foreign.join();
  Check(guarded, "main metadata owner-thread guards");
  // Destroying the authored parent removes its source-bound mesh descendant too.
  const auto instance = scenes.View().instances.front();
  const auto parent = world.Parent(entity);
  Check(parent && *parent && game.DestroyEntity(*parent) && sprites.Reconcile(),
        "destroy authored subtree");
  Check(!sprites.FindEntity(entity) &&
            api.pause_animation(api.context, entity, 1) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
        "destroyed descendant retained live animation metadata");
  frame = sprites.Extract(960, 640);
  Check(frame && frame->statistics.input == 1 && frame->statistics.visible == 1,
        "destroyed authored reference was replayed or pooled instance lost");
  Check(api.destroy_instances(api.context, &handle, 1) == 0 &&
            sources.read_instances(sources.context, &handle, 1, &read) ==
                NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
        "main pool stale handle accepted");
}
void ReloadBranches() {
  auto project = Project();
  auto assets = twod::GameSpriteAssets::Load(project->SpriteAssets());
  Check(bool(assets), "reload assets");
  game::GameWorld game{runtime::WorldKind::Play};
  auto &world = game.InternalWorld();
  runtime::GameSceneSession scenes(game, project);
  auto live = std::make_unique<twod::GameSpriteSession>(world, std::move(*assets), 4);
  std::uint64_t request{};
  Check(scenes.RequestLoad(project->StartupScene(), runtime::GameSceneLoadMode::Replace, false,
                           request) == runtime::GameSceneError::None &&
            scenes.Process(
                [&](const auto &view, std::string &error) { return live->Prepare(view, &error); }),
        "reload startup");
  const auto entity = SpriteEntity(scenes.View().instances.front());
  const auto primary = live->Primary();
  const auto world_bytes = world.SaveScene(primary);
  const auto matrix = world.WorldMatrix(entity);
  NexoraTwoDApiV1 api{};
  NexoraTwoDSourceApiV1 sources{};
  const auto tables = [&](twod::GameSpriteSession &owner) {
    api.struct_size = sizeof(api);
    sources.struct_size = sizeof(sources);
    Check(owner.Bridge().GetApi(&api) == 0 && NexoraGetTwoDSourcesV1(&api, &sources) == 0,
          "reload source tables");
  };
  tables(*live);
  auto styled = Read(sources, entity);
  styled.sprite.color[0] = .5F;
  styled.sprite.flags |= 1U | 4U;
  Check(sources.set_sprite(sources.context, entity, &styled) == 0, "reload style");
  const NexoraTwoDCamera camera{{2, 3}, 22, .25F};
  Check(api.set_camera(api.context, &camera) == 0, "reload camera");
  NexoraTwoDSourceInstanceV1 pooled{};
  pooled.transform.rotation.w = 1;
  pooled.transform.scale = {-1, 1, 1};
  pooled.sprite = styled;
  NexoraTwoDInstanceHandle handle{};
  Check(sources.create_instances(sources.context, &pooled, 1, &handle) == 0, "reload pool");
  runtime::World empty{runtime::WorldKind::Play}, editor;
  Check(!live->ForkForReload(world) && !live->ForkForReload(empty) &&
            !live->ForkForReload(editor) && !live->RebindForReload(empty) &&
            !live->RebindForReload(editor) && &live->WorldOwner() == &world &&
            &live->FindEntity(entity)->WorldOwner() == &world,
        "invalid reload changed live owner");
  for (unsigned generation = 0; generation < 32; ++generation) {
    // A fresh Play after destroying the preceding owner also checks resolver ownership.
    Check(api.play_animation(api.context, entity, clip_id.high, clip_id.low, 1) == 0,
          "reload retained a resolver borrowing its destroyed parent");
    NexoraTwoDAnimationResult result{};
    std::array<NexoraTwoDEvent, 4> events{};
    Check(api.advance_animation(api.context, entity, .125, events.data(), 4, &result) == 0 &&
              Read(sources, entity).source_high == first_id.high,
          "reload half-frame clock");
    auto staged_world = world;
    bool reentrant_rejected{};
    live->Pool().ForEach([&](auto &) {
      reentrant_rejected = !live->ForkForReload(staged_world) && !live->RebindForReload(world);
    });
    Check(reentrant_rejected, "reload admitted pool iteration reentry");
    auto branch = live->ForkForReload(staged_world);
    Check(branch && branch->Revision() == live->Revision() && branch->Primary() == primary &&
              &branch->WorldOwner() == &staged_world &&
              &branch->FindEntity(entity)->WorldOwner() == &staged_world,
          "reload metadata did not branch onto the clone");
    NexoraTwoDApiV1 next{};
    next.struct_size = sizeof(next);
    NexoraTwoDSourceApiV1 next_sources{};
    next_sources.struct_size = sizeof(next_sources);
    Check(branch->Bridge().GetApi(&next) == 0 && next.context != api.context &&
              NexoraGetTwoDSourcesV1(&next, &next_sources) == 0 &&
              next.advance_animation(next.context, entity, .125, events.data(), 4, &result) == 0 &&
              Read(next_sources, entity).source_high == second_id.high &&
              Read(sources, entity).source_high == first_id.high,
          "reload lost its clock or mutated the active source");
    NexoraTwoDSourceInstanceV1 current{}, candidate{};
    Check(sources.read_instances(sources.context, &handle, 1, &current) == 0 &&
              next_sources.read_instances(next_sources.context, &handle, 1, &candidate) == 0,
          "reload changed pool handle identity");
    candidate.sprite.sprite.color[0] = float(generation + 1) / 64;
    Check(next_sources.write_instances(next_sources.context, &handle, 1, &candidate) == 0 &&
              sources.read_instances(sources.context, &handle, 1, &pooled) == 0 &&
              pooled.sprite.sprite.color[0] == current.sprite.sprite.color[0] &&
              world.SaveScene(primary) == world_bytes,
          "unpublished source/pool branch changed the live World");
    // Reject one fully mutated sibling before committing the prepared branch.
    auto discarded = live->ForkForReload(staged_world);
    Check(bool(discarded), "discarded reload branch");
    discarded.reset();
    runtime::World retired{runtime::WorldKind::Play};
    retired = std::move(world);
    world = std::move(staged_world);
    Check(live->RebindForReload(retired) && branch->RebindForReload(world),
          "identical World move rebind");
    live.reset();
    live = std::move(branch);
    Check(live->Bridge().GetApi(&next) == 0 && next.context != api.context,
          "reload changed the candidate API address during rebind");
    tables(*live);
    NexoraTwoDCamera retained{};
    const auto value = Read(sources, entity);
    const auto frame = live->Extract(640, 480);
    Check(api.get_camera(api.context, &retained) == 0 && retained.center.x == camera.center.x &&
              retained.center.y == camera.center.y &&
              retained.vertical_size == camera.vertical_size &&
              retained.rotation_radians == camera.rotation_radians &&
              value.source_high == second_id.high &&
              value.sprite.color[0] == styled.sprite.color[0] &&
              value.sprite.flags == styled.sprite.flags &&
              sources.read_instances(sources.context, &handle, 1, &pooled) == 0 &&
              pooled.sprite.sprite.color[0] == candidate.sprite.sprite.color[0] &&
              world.SaveScene(primary) == world_bytes && world.WorldMatrix(entity) == matrix &&
              frame && frame->statistics.input == 2,
          "reload failed to retain camera/style/World/pool extraction");
  }
}
void MissingAssets() {
  auto project = Project();
  game::GameWorld game{runtime::WorldKind::Play};
  runtime::GameSceneSession scenes(game, project);
  auto empty = twod::GameSpriteAssets::Load({});
  Check(bool(empty), "empty typed catalog");
  twod::GameSpriteSession sprites(game.InternalWorld(), std::move(*empty));
  std::uint64_t request{};
  std::string reason;
  Check(scenes.RequestLoad(project->StartupScene(), runtime::GameSceneLoadMode::Replace, false,
                           request) == runtime::GameSceneError::None &&
            !scenes.Process([&](const auto &view, std::string &error) {
              auto result = sprites.Prepare(view, &error);
              reason = error;
              return result;
            }) &&
            reason.find("source binding") != std::string::npos && sprites.Available() &&
            sprites.Revision() == 0 && scenes.View().instances.empty(),
        "missing frozen source admitted a startup World/metadata generation");
}
} // namespace
int main() {
  try {
    Run();
    ReloadBranches();
    MissingAssets();
    std::cout << "Main game mutable source/animation/pool, scene preparation and rollback passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
