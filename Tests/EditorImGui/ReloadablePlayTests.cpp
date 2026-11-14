#include "GameSceneFixture.h"
#include "PackagedPlayContext.h"
#include "PlayInputForwarding.h"
#include "ReloadablePlayGameplayModule.h"
#if defined(NEXORA_HAS_2D)
#include "Nexora/Editor/TwoDAssetImport.h"
#endif
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Owner = editor::preview::ReloadablePlayGameplayModule;
enum class Fault {
  None,
  SaveWrite,
  SaveSceneRequest,
  Restore,
  Start,
  CachedOldWrite,
  StorageWrite,
  OpenUiFrame
};
Fault fault{};
runtime::Id initial_scene{}, observed_entity{};
std::uint64_t observed_counter{}, creates{}, destroys{}, stops{};
unsigned completed_cycles{}, expected_rejections{};
NexoraGameplayHostV3 saved_host{};
struct Payload final {
  std::uint64_t version{1}, counter{}, scene{}, entity{};
  NexoraTwoDInstanceHandle pooled{};
};
struct State final {
  NexoraGameplayHostV3 host{};
  NexoraGameplaySceneApiV1 scenes{};
  NexoraTwoDApiV1 two_d{};
  NexoraGameplayInputApiV1 input{};
  NexoraGameplayUiApiV1 ui{};
  NexoraStorageApiV1 storage{};
  Payload payload;
};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int32_t Write(State &state, double value) {
  const game::GameplayTransformWire transform{value, 0, 0};
  return state.host.write_component(state.host.context, state.payload.entity,
                                    game::TransformComponentType(), &transform, sizeof(transform));
}
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  auto *memory = host->allocate(host->context, 17, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto *state = new (memory) State;
  state->host = *host;
  if (host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API) {
    NexoraGameplayServicesApiV1 services{};
    services.struct_size = sizeof(services);
    services.abi_version = 1;
    state->scenes.struct_size = sizeof(state->scenes);
    state->scenes.abi_version = 1;
    if (NexoraGetGameplayServicesV1(host, &services)) {
      state->~State();
      host->deallocate(host->context, 17, memory, sizeof(State), alignof(State));
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
    if (services.get_scene_api(services.context, &state->scenes) != 0)
      state->scenes = {};
    if (host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) {
      state->two_d.struct_size = sizeof(state->two_d);
      state->input.struct_size = sizeof(state->input);
      state->input.abi_version = 1;
      state->ui.struct_size = sizeof(state->ui);
      state->ui.abi_version = 1;
      state->storage.struct_size = sizeof(state->storage);
      state->storage.abi_version = 1;
      if (host->get_twod_api(host->context, &state->two_d) ||
          services.get_input_api(services.context, &state->input) ||
          services.get_ui_api(services.context, &state->ui) ||
          services.get_storage_api(services.context, &state->storage)) {
        state->~State();
        host->deallocate(host->context, 17, memory, sizeof(State), alignof(State));
        return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
      }
    }
  }
  ++creates;
  *output = state;
  return 0;
}
int32_t Start(void *context) {
  auto &state = *static_cast<State *>(context);
  if (state.host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE) {
    if (fault == Fault::OpenUiFrame && state.ui.context)
      static_cast<void>(state.ui.begin_frame(state.ui.context, 320, 240));
    if (fault == Fault::StorageWrite && state.storage.context) {
      constexpr std::uint8_t value = 17;
      static_cast<void>(
          state.storage.save(state.storage.context, "reload-forbidden", 16, 1, &value, 1));
    }
    if (fault == Fault::CachedOldWrite) {
      const game::GameplayTransformWire transform{123456, 0, 0};
      // Deliberately ignore the old proxy's error: final admission must still reject.
      static_cast<void>(saved_host.write_component(saved_host.context, state.payload.entity,
                                                   game::TransformComponentType(), &transform,
                                                   sizeof(transform)));
    }
    if (fault == Fault::Start) {
      static_cast<void>(Write(state, 999));
      if (state.two_d.context) {
        NexoraTwoDEvent event{};
        NexoraTwoDAnimationResult result{};
        static_cast<void>(state.two_d.advance_animation(state.two_d.context, state.payload.entity,
                                                        0.03, &event, 1, &result));
        static_cast<void>(state.input.clear_bindings(state.input.context, 0, 42));
      }
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
    if (fault == Fault::None && state.ui.context &&
        (state.ui.begin_frame(state.ui.context, 320, 240) ||
         state.ui.panel(state.ui.context, {0, 0, 320, 240}, 0x330000ffU) ||
         state.ui.panel(state.ui.context, {0, 0, 32, 32}, 0xff0000ffU) ||
         state.ui.end_frame(state.ui.context)))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    return 0;
  }
  state.payload.scene = initial_scene;
  NexoraEntitySpawnDescriptor descriptor{};
  descriptor.struct_size = sizeof(descriptor);
  const auto result = state.host.spawn_entity(state.host.context, initial_scene, &descriptor,
                                              &state.payload.entity);
  if (!result) {
    auto *scratch = state.host.allocate(state.host.context, 17, 128, 16);
    if (!scratch)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    state.host.deallocate(state.host.context, 17, scratch, 128, 16);
  }
  if (result || !state.two_d.context)
    return result;
  const NexoraTwoDSprite sprite{17, 23, 1, 8, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0};
  const NexoraTwoDInstance instance{{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, sprite};
  const NexoraGameplayInputBindingV1 key{
      sizeof(key), 1, NEXORA_INPUT_DEVICE_KEYBOARD, 0, NEXORA_INPUT_KEY_A, 1, 0, 0};
  return state.two_d.set_sprite(state.two_d.context, state.payload.entity, &sprite) ||
                 state.two_d.create_instances(state.two_d.context, &instance, 1,
                                              &state.payload.pooled) ||
                 state.two_d.play_animation(state.two_d.context, state.payload.entity, 19, 29, 1) ||
                 state.input.bind(state.input.context, 0, 42, &key)
             ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE
             : 0;
}
int32_t Update(void *context, double) {
  auto &state = *static_cast<State *>(context);
  ++state.payload.counter;
  observed_counter = state.payload.counter;
  observed_entity = state.payload.entity;
  if (state.two_d.context) {
    std::uint32_t exists = 99;
    if (state.storage.exists(state.storage.context, "reload-forbidden", 16, &exists) || exists)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    NexoraTwoDSprite sprite{};
    NexoraTwoDInstance instance{};
    NexoraTwoDEvent event{};
    NexoraTwoDAnimationResult animation{};
    NexoraGameplayActionStateV1 input{};
    input.struct_size = sizeof(input);
    input.abi_version = 1;
    if (state.two_d.get_sprite(state.two_d.context, state.payload.entity, &sprite) ||
        state.two_d.read_instances(state.two_d.context, &state.payload.pooled, 1, &instance) ||
        sprite.region != 1 || instance.sprite.region != 1 ||
        state.two_d.advance_animation(state.two_d.context, state.payload.entity,
                                      state.payload.counter % 2 ? 0.04 : 0.06, &event, 1,
                                      &animation) ||
        animation.event_count != (state.payload.counter % 2 ? 0U : 1U) ||
        state.input.read_action(state.input.context, 0, 42, &input) || !input.held ||
        state.ui.begin_frame(state.ui.context, 320, 240) ||
        state.ui.panel(state.ui.context, {0, 0, 320, 240}, 0x330000ffU) ||
        state.ui.end_frame(state.ui.context))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  return Write(state, static_cast<double>(state.payload.counter));
}
std::uint32_t Save(void *context, void *output, std::uint32_t capacity) {
  auto &state = *static_cast<State *>(context);
  saved_host = state.host;
  if (fault == Fault::SaveWrite)
    static_cast<void>(Write(state, 999));
  if (fault == Fault::SaveSceneRequest && state.scenes.context) {
    std::uint64_t request{};
    static_cast<void>(state.scenes.request_load(state.scenes.context, testscene::kTitle.high,
                                                testscene::kTitle.low, 0, 0, &request));
  }
  if (!output)
    return sizeof(Payload);
  if (capacity < sizeof(Payload))
    return 0;
  std::memcpy(output, &state.payload, sizeof(Payload));
  return sizeof(Payload);
}
int32_t Restore(void *context, const void *data, std::uint32_t size) {
  auto &state = *static_cast<State *>(context);
  if (size != sizeof(Payload))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  std::memcpy(&state.payload, data, size);
  if (state.payload.version != 1)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  if (fault == Fault::Restore) {
    static_cast<void>(Write(state, 888));
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  return 0;
}
void Stop(void *context) {
  auto &state = *static_cast<State *>(context);
  ++stops;
  static_cast<void>(state.host.despawn_entity(state.host.context, state.payload.entity));
  if (state.scenes.context) {
    std::uint64_t request{};
    static_cast<void>(state.scenes.request_load(state.scenes.context, testscene::kTitle.high,
                                                testscene::kTitle.low, 0, 0, &request));
  }
}
void Destroy(void *context) {
  auto &state = *static_cast<State *>(context);
  const auto host = state.host;
  ++destroys;
  if (state.two_d.context && state.payload.pooled.owner)
    static_cast<void>(state.two_d.destroy_instances(state.two_d.context, &state.payload.pooled, 1));
  static_cast<void>(host.despawn_entity(host.context, state.payload.entity));
  state.~State();
  host.deallocate(host.context, 17, context, sizeof(State), alignof(State));
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->capabilities =
      NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION | NEXORA_GAMEPLAY_CAPABILITY_RELOAD_LIFECYCLE;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  module->save_state = Save;
  module->load_state = Restore;
  return 0;
}
int32_t WrongAbi(std::uint32_t version, NexoraGameModuleV3 *module) {
  Loader(version, module);
  module->abi_version = version + 1;
  return 0;
}
std::uint64_t Cycles(Owner &owner, runtime::World &world, bool packaged, bool two_d = false) {
  if (two_d) {
    Nexora::Window::WindowEvent event{};
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(Nexora::Window::Key::A);
    event.value1 = 1;
    owner.SetInputFocus(true);
    owner.ProcessInput(std::span(&event, 1)); // Initial capture is deliberately discarded.
    owner.ProcessInput(std::span(&event, 1));
  }
  Check(owner.Update(0) && observed_counter == 1, "initial dispatch");
  std::uint64_t texture{};
#if defined(NEXORA_HAS_2D)
  if (two_d) {
    const auto frame = owner.TwoDFrame({40, 30, 320, 240});
    Check(frame && !frame->sprites.retainedAtlases.empty() && !frame->data.commands.empty(),
          "initial transaction Sprite/UI frame");
    texture = frame->sprites.retainedAtlases.front().texture;
  }
#endif
  const auto entity = observed_entity;
  auto expected = observed_counter;
  const auto baseline_entities = world.ActiveEntityCount();
  const auto teardown_serial = owner.AllocationTeardownSequence();
  const auto initial_peak = owner.Allocations().peak_bytes;
  for (unsigned cycle = 1; cycle <= 32; ++cycle) {
    const auto generation = owner.Generation();
    for (const auto failure :
         {Fault::SaveWrite, Fault::Restore, Fault::Start, Fault::CachedOldWrite,
          Fault::SaveSceneRequest, Fault::StorageWrite, Fault::OpenUiFrame}) {
      if (failure == Fault::SaveSceneRequest && !packaged)
        continue;
      if (failure == Fault::StorageWrite && !two_d)
        continue;
      if (failure == Fault::OpenUiFrame && !two_d)
        continue;
      fault = failure;
      const auto snapshot = world.SaveScene(initial_scene);
      Check(!owner.Reload(Loader) && owner.IsLoaded() && owner.Generation() == generation &&
                world.SaveScene(initial_scene) == snapshot &&
                world.ActiveEntityCount() == baseline_entities &&
                owner.AllocationTeardownSequence() == teardown_serial &&
                owner.Allocations().live_allocations == 1,
            "rejected candidate changed the live World/module/allocation ledger");
      ++expected_rejections;
      fault = Fault::None;
      Check(owner.Update(0) && observed_entity == entity && observed_counter == ++expected,
            "old module dispatch/state lost after rejection");
    }
    const auto before_create = creates;
    Check(!owner.Reload(WrongAbi) && creates == before_create && owner.Generation() == generation,
          "ABI mismatch invoked candidate create");
    ++expected_rejections;
    std::atomic<bool> work_done{};
    auto lease = owner.AcquireWork(generation);
    Check(bool(lease), "active generation work admission");
    std::thread worker([held = std::move(lease), &work_done]() mutable {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      work_done = true;
      held = {};
    });
    const bool reloaded = owner.Reload(Loader);
    const bool drained = work_done.load();
    worker.join();
#if defined(NEXORA_HAS_2D)
    if (reloaded && two_d) {
      const auto ready = owner.TwoDFrame({40, 30, 320, 240}, false);
      Check(ready && ready->data.vertices.size() == 8,
            "reloaded-hook UI snapshot waits for a gameplay tick or retains an unfinished frame");
    }
#endif
    Check(reloaded && drained && owner.Generation() == generation + 1 && owner.IsBoundTo(&world) &&
              world.FindEntity(entity) && world.ActiveEntityCount() == baseline_entities &&
              owner.Allocations().live_allocations == 1 &&
              owner.AllocationTeardownSequence() == teardown_serial &&
              owner.Allocations().peak_bytes == initial_peak &&
              owner.LastTeardownAllocations().reclaimed_allocations == 0 && owner.Update(0) &&
              observed_entity == entity && observed_counter == ++expected,
          "successful reload reset state/handles or old cleanup crossed into the live World");
#if defined(NEXORA_HAS_2D)
    if (two_d) {
      const auto frame = owner.TwoDFrame({40, 30, 320, 240});
      Check(frame && !frame->sprites.retainedAtlases.empty() && !frame->data.commands.empty() &&
                frame->sprites.retainedAtlases.front().texture == texture &&
                owner.TakeRetiredTwoDTextures().empty(),
            "main transaction reset texture/UI ownership or retired an admitted texture");
    }
#endif
    ++completed_cycles;
  }
  return texture;
}
void Inspection() {
  runtime::World source;
  const auto scene = source.LoadScene("Inspection");
  Check(source.Activate(scene), "inspection scene activation");
  runtime::PlaySession play(source);
  Owner owner;
  Check(play.Start(1.0 / 60.0, [&](runtime::World &, double dt) { return owner.FixedUpdate(dt); }),
        "inspection-only Play");
  constexpr auto user = editor::PlayInputBindings::kMaximumUsers - 1;
  editor::preview::ForwardPlayInput(play, owner, true, {}, {}, user);
  Check(play.AcceptsInput() && owner.InputUser() == user && play.Tick() && !owner.IsLoaded(),
        "main facade disabled ordinary inspection input without a script module");
  editor::preview::ForwardPlayInput(play, owner, true, {}, {}, user + 1);
  Check(!play.AcceptsInput() && owner.InputUser() == user,
        "invalid inspection user captured input");
  owner.Unload();
  Check(owner.AllocationTeardownSequence() == 0 && play.Stop(),
        "inspection-only Stop invented a bound module teardown");
}
void Plain() {
  runtime::World authoring;
  initial_scene = authoring.LoadScene("Main");
  Check(authoring.Activate(initial_scene), "authoring activation");
  const auto original = authoring.SaveScene(initial_scene);
  auto world = authoring.CloneForPlay();
  Owner owner;
  Check(!owner.Load(authoring, Loader) && owner.Load(world, Loader), "plain Play preparation");
  Cycles(owner, world, false);
  owner.Unload();
  Check(!owner.IsLoaded() && !owner.IsBoundTo(&world) &&
            owner.LastTeardownAllocations().reclaimed_allocations == 0 &&
            authoring.SaveScene(initial_scene) == original,
        "final plain cleanup or authoring conservation");
}
void Packaged() {
  editor::preview::PackagedPlayContext context(testscene::Project());
  const auto draw = [](const Nexora::Presentation::SceneDrawData &) {
    return Nexora::Presentation::SurfaceStatus::Ready;
  };
  const auto frame = context.Render({0, 0, 320, 240}, false, draw);
  Check(frame.committed && context.Ready(), "CPU packaged scene preparation");
  initial_scene = context.Scenes().View().primary;
  const auto keeper = context.World().SpawnEntity(initial_scene);
  Check(context.World().SetAudio(keeper, runtime::AudioVoice{77}) &&
            context.World().PlayAudio(keeper),
        "packaged audio binding fixture");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  runtime::PhysicsBody body;
  body.minimum = {-1, -1, -1};
  body.maximum = {1, 1, 1};
  Check(context.World().SetPhysics(keeper, body) &&
            context.World().SetCharacter(keeper, runtime::CharacterControllerConfig{}) &&
            context.World().TickCharacter(keeper, {}, 0.02, false).has_value(),
        "packaged physics/character binding fixture");
  const auto character = context.World().GetCharacter(keeper);
#endif
  Owner owner;
  Check(owner.Load(context.World().InternalWorld(), Loader, {}, {}, context.Scenes().Api(),
                   context.ReloadHooks()),
        "packaged Play preparation");
  std::uint64_t pending{};
  Check(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Additive, false,
                                     pending) == runtime::GameSceneError::None,
        "queued packaged request fixture");
  auto *const world_address = &context.World().InternalWorld();
  Cycles(owner, *world_address, true);
  NexoraSceneTransitionV1 status{};
  status.struct_size = sizeof(status);
  Check(world_address == &context.World().InternalWorld() &&
            context.World().GetEntity(keeper)->has_audio &&
            context.World().ActiveAudioVoices() == 1 &&
            context.Scenes().Status(pending, status) == runtime::GameSceneError::None &&
            status.state == 1 && context.Scenes().View().primary == initial_scene,
        "packaged handoff lost World address/bindings/queued request/session IDs");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  const auto retained_character = context.World().GetCharacter(keeper);
  const auto hit = context.World().RaycastEntity({{0, 0, 3}, {0, 0, -1}, 10});
  Check(context.World().GetEntity(keeper)->has_physics && character && retained_character &&
            retained_character->position == character->position &&
            retained_character->velocity == character->velocity &&
            retained_character->ground == character->ground &&
            retained_character->crouched == character->crouched &&
            retained_character->ground_body == character->ground_body && hit && *hit == keeper,
        "packaged handoff reset character state or physics bindings");
#endif
  const auto applied = context.Render({0, 0, 320, 240}, false, draw);
  Check(applied.committed && context.Scenes().View().instances.size() == 2 &&
            context.Scenes().Status(pending, status) == runtime::GameSceneError::None &&
            status.state == 2 && context.World().InternalWorld().FindEntity(observed_entity),
        "transferred packaged request did not apply at its ordinary frame boundary");
  context.Scenes().Stop();
  owner.Unload();
  Check(owner.LastTeardownAllocations().reclaimed_allocations == 0,
        "packaged generation allocation cleanup");
}
#if defined(NEXORA_HAS_2D)
void TwoD() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-reloadable-play-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{root};
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Reloadable 2D"), "2D transaction project fixture");
  const auto write = [&](const auto &name, const auto &bytes) {
    std::ofstream file(root / "Content" / name, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    Check(bool(file), "2D transaction asset fixture");
  };
  constexpr runtime::AssetUuid atlas_id{17, 23}, clip_id{19, 29};
  const twod::Image image{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlas_id, std::span(&image, 1), 4);
  Check(bool(atlas), "2D transaction atlas fixture");
  write("Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
  twod::Clip clip;
  clip.id = clip_id;
  clip.atlas = atlas_id;
  auto &animation_frame = clip.frames.emplace_back();
  animation_frame.region = 1;
  animation_frame.seconds = 0.1;
  animation_frame.event = "tick";
  write("Clip.nx2clip", twod::EncodeClip(clip));
  editor::AssetWorkspace index;
  editor::ProjectContentSession content;
  Check(
      index.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite) &&
          content.Open(workspace, index, 13),
      "2D transaction content fixture");
  runtime::World authoring;
  initial_scene = authoring.LoadScene("Main");
  Check(authoring.Activate(initial_scene), "2D transaction source scene");
  editor::SceneDocument document(authoring, initial_scene);
  std::string error;
  auto binding = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
  Check(binding && binding->assets && error.empty(), "2D transaction immutable assets");
  const auto original = authoring.SaveScene(initial_scene);
  auto world = authoring.CloneForPlay();
  Owner owner;
  Check(owner.Load(world, Loader, {}, *binding), "2D main transaction binding");
  const auto texture = Cycles(owner, world, false, true);
  owner.Unload();
  const auto retired = owner.TakeRetiredTwoDTextures();
  Check(texture && std::ranges::count(retired, texture) == 1 &&
            authoring.SaveScene(initial_scene) == original &&
            owner.LastTeardownAllocations().reclaimed_allocations == 0,
        "2D final retirement or source conservation");
}
#endif
} // namespace
int main() {
  try {
    Inspection();
    Plain();
    Packaged();
#if defined(NEXORA_HAS_2D)
    TwoD();
#endif
    Check(creates == destroys && stops >= 66, "generation lifecycle balance");
    std::cout << "Editor main transaction: " << completed_cycles << " successful reloads, "
              << expected_rejections << " expected rejections; " << creates << " creates/"
              << destroys << " destroys, " << stops
              << " stops. Plain/packaged"
#if defined(NEXORA_HAS_2D)
                 "/2D"
#endif
                 " continuity, rollback, cached guards, actual work drain and private retirement "
                 "passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
