#include "GameSceneFixture.h"
#include "Nexora/Runtime/GameSceneSession.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/GameplayServices.h"

#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Error = runtime::GameSceneError;
using Mode = runtime::GameSceneLoadMode;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Marker final {
  int &live;
  explicit Marker(int &count) : live(count) { ++live; }
  ~Marker() { --live; }
};
struct Prepared final : runtime::GameScenePresentation {
  std::shared_ptr<Marker> candidate;
  std::shared_ptr<Marker> &active;
  runtime::Id id;
  runtime::Id &visible;
  Prepared(int &live, std::shared_ptr<Marker> &owner, runtime::Id scene, runtime::Id &current)
      : candidate(std::make_shared<Marker>(live)), active(owner), id(scene), visible(current) {}
  void Commit() noexcept override {
    active.swap(candidate);
    visible = id;
  }
};
void Core() {
  const auto project = testscene::Project();
  game::GameWorld game(runtime::WorldKind::Play);
  const auto persistent = game.LoadScene("Persistent runtime", true);
  Require(game.ActivateScene(persistent), "persistent scene activation");
  const auto keeper = game.SpawnEntity(persistent);
  Require(game.SetAudio(keeper, runtime::AudioVoice{50}) && game.PlayAudio(keeper),
          "persistent binding");
  runtime::GameSceneSession session(game, project);
  int resources = 0;
  std::shared_ptr<Marker> presentation;
  runtime::Id visible = 0;
  bool reject = false, throws = false, cancel = false;
  auto prepare = [&](const runtime::GameSceneView &view,
                     std::string &error) -> std::unique_ptr<runtime::GameScenePresentation> {
    Require(view.primary && view.camera && runtime::CaptureGameSceneRenderItems(view).has_value(),
            "invalid prepared world/render view");
    std::uint64_t unchanged = 91;
    Require(session.RequestLoad(testscene::kTitle, Mode::Replace, false, unchanged) ==
                    Error::Busy &&
                unchanged == 91,
            "prepare reentrancy was admitted");
    auto staged = std::make_unique<Prepared>(resources, presentation, view.primary, visible);
    if (cancel)
      session.Stop();
    if (throws)
      throw std::runtime_error("injected prepare exception");
    if (reject) {
      error = "injected native admission failure";
      return {};
    }
    return staged;
  };
  std::uint64_t request = 0;
  Require(session.RequestLoad(project->StartupScene(), Mode::Replace, false, request) ==
                  Error::None &&
              game.InternalWorld().ActiveSceneCount() == 1 && session.View().instances.empty(),
          "load mutated the live world during a callback");
  Require(session.Process(prepare), "non-Main startup preparation");
  const auto first = session.View().primary;
  Require(visible == first && resources == 1 && game.GetEntity(keeper) &&
              game.InternalWorld().FindScene(first)->name == "Level01",
          "startup/persistent/commit ownership");
  const auto old_completed = request;
  const auto source_mesh = project->ResolveScene(testscene::kLevel)->RenderItems()[0].entity;
  const auto &initial = session.View().instances[0];
  const auto mesh_entity = initial.entities.at(source_mesh);
  const auto source_parent =
      *project->ResolveScene(testscene::kLevel)->WorldView().Parent(source_mesh);
  const auto parent = initial.entities.at(source_parent);
  Require(game.InternalWorld().Parent(mesh_entity) == parent && parent != source_parent,
          "authored hierarchy IDs collided with persistent runtime IDs");
  auto frame = runtime::CaptureGameSceneRenderItems(session.View());
  Require(frame && frame->size() == 1 && (*frame)[0].world_transform[12] == 9,
          "remapped parent transform");
  Require(game.SetAudio(mesh_entity, runtime::AudioVoice{51}) && game.PlayAudio(mesh_entity),
          "transient binding");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  Require(
      game.SetPhysics(
          parent, runtime::PhysicsBody{0, {-0.5, 0., -0.5}, {0.5, 2., 0.5}, false, false, {}}) &&
          game.SetCharacter(parent, runtime::CharacterControllerConfig{}),
      "transient simulation bindings");
#endif
  const auto old_snapshot = game.InternalWorld().SaveScene(first);
  reject = true;
  Require(session.RequestLoad(testscene::kTitle, Mode::Replace, false, request) == Error::None,
          "request replacement");
  NexoraSceneTransitionV1 status{sizeof(status)};
  Require(session.Status(old_completed, status) == Error::None && status.state == 2,
          "queued request lost last completed status");
  Require(!session.Process(prepare) && visible == first && resources == 1 &&
              game.InternalWorld().SaveScene(first) == old_snapshot &&
              session.Stats().audio_bindings == 2,
          "failed prepare changed old scene/native owners/bindings");
  Require(session.Status(request, status) == Error::None && status.state == 3 &&
              status.result == -3,
          "failed request status");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  Require(session.Stats().physics_bindings == 1 && session.Stats().character_bindings == 1 &&
              game.GetCharacter(parent).has_value(),
          "failure lost simulation bindings");
#endif
  reject = false;
  throws = true;
  Require(session.RequestLoad(testscene::kTitle, Mode::Replace, false, request) == Error::None &&
              !session.Process(prepare) && resources == 1 && visible == first,
          "prepare exception escaped conservation");
  throws = false;
  Require(session.RequestLoad(testscene::kTitle, Mode::Replace, false, request) == Error::None &&
              session.Process(prepare),
          "successful replacement");
  Require(!game.GetEntity(mesh_entity) && !game.InternalWorld().FindScene(first) &&
              game.GetEntity(keeper) && session.Stats().audio_bindings == 1 &&
              session.Stats().playing_audio == 1,
          "retired scene bindings/records leaked or persistent state lost");
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  Require(session.Stats().physics_bindings == 0 && session.Stats().character_bindings == 0 &&
              !game.GetCharacter(parent),
          "retired simulation bindings leaked");
#endif
  auto api = session.Api();
  std::uint64_t sentinel_request = 733;
  NexoraSceneTransitionV1 short_status{0, 99, 98, 97, 96, 95};
  NexoraSceneInstanceV1 short_info{0, 99, 98, 97, 96, 95};
  Require(api.request_load(api.context, 1, 1, 2, 0, &sentinel_request) == -1 &&
              api.request_load(api.context, 1, 1, 0, 2, &sentinel_request) == -1 &&
              api.request_unload(api.context, session.View().primary, &sentinel_request) == -1 &&
              sentinel_request == 733 &&
              api.get_status(api.context, request, &short_status) == -2 &&
              short_status.state == 99 && short_status.request == 98 &&
              api.get_scene_info(api.context, session.View().primary, &short_info) == -2 &&
              short_info.flags == 99 && short_info.scene == 98,
          "invalid/short C requests changed output or unloaded the last scene");
  NexoraSceneInstanceV1 primary_info{sizeof(primary_info)};
  Require(api.get_scene_info(api.context, session.View().primary, &primary_info) == 0 &&
              primary_info.flags == NEXORA_SCENE_PRIMARY && primary_info.camera &&
              primary_info.asset_high == 1 && primary_info.asset_low == 1,
          "primary scene C info");
  std::uint64_t high = 99, low = 98;
  Require(api.resolve_scene(api.context, "Level01.scene", 13, &high, &low) == 0 &&
              runtime::AssetUuid{high, low} == testscene::kLevel,
          "C catalog resolution");
  const auto title = session.View().primary;
  Require(session.RequestLoad(testscene::kLevel, Mode::Additive, false, request) == Error::None &&
              session.Process(prepare),
          "additive load failed");
  Require(session.View().primary == title && session.View().instances.size() == 2 &&
              runtime::CaptureGameSceneRenderItems(session.View())->size() == 2,
          "additive stole camera or omitted scene");
  std::uint64_t untouched = 123;
  Require(session.RequestLoad(testscene::kLevel, Mode::Additive, false, untouched) ==
                  Error::InvalidArgument &&
              untouched == 123,
          "duplicate additive scene admitted");
  const auto extra = session.View().instances.back().scene;
  Require(session.RequestUnload(extra, request) == Error::None && session.Process(prepare) &&
              session.RequestUnload(extra, untouched) == Error::NotFound && untouched == 123,
          "unload/stale instance policy");
  for (unsigned i = 0; i < 120; ++i) {
    Require(session.RequestLoad(i % 2 ? testscene::kTitle : testscene::kLevel, Mode::Replace, false,
                                request) == Error::None &&
                session.Process(prepare),
            "repeated A/B transition");
    const auto stats = session.Stats();
    Require(stats.scene_records == 2 && stats.instances == 1 && stats.stored_entities == 4 &&
                stats.audio_bindings == 1 && resources == 1 && game.GetEntity(keeper),
            "repeated transitions grew scene/binding/presentation ownership");
  }
  high = 91;
  low = 92;
  Require(api.resolve_scene(api.context, "Missing.scene", 13, &high, &low) == -5 && high == 91 &&
              low == 92,
          "missing C scene changed outputs");
  bool thread_rejected = false;
  std::thread wrong_thread([&] {
    std::uint64_t value = 42;
    thread_rejected = api.get_active_scene(api.context, &value) == -3 && value == 42;
  });
  wrong_thread.join();
  Require(thread_rejected, "wrong-thread scene service");
  runtime::GameplayServices services;
  services.scene = api;
  NexoraGameplayServicesApiV1 negotiated{sizeof(negotiated), 1};
  Require(services.GetApi(&negotiated) == 0 && negotiated.get_scene_api,
          "scene service negotiation");
  NexoraGameplaySceneApiV1 copied{sizeof(copied), 1};
  Require(negotiated.get_scene_api(negotiated.context, &copied) == 0 &&
              copied.context == api.context,
          "copied scene table");
  negotiated.struct_size = offsetof(NexoraGameplayServicesApiV1, get_scene_api);
  const auto sentinel = negotiated.get_scene_api;
  Require(services.GetApi(&negotiated) == 0 &&
              negotiated.struct_size == offsetof(NexoraGameplayServicesApiV1, get_scene_api) &&
              negotiated.get_scene_api == sentinel,
          "old services prefix overwritten by suffix");
  Require(session.RequestLoad(testscene::kLevel, Mode::Additive, true, request) == Error::None &&
              session.Process(prepare),
          "persistent authored scene load");
  const auto retained = session.View().instances.back().scene;
  Require(session.RequestUnload(retained, untouched) == Error::InvalidArgument &&
              session.RequestLoad(testscene::kTitle, Mode::Replace, false, request) ==
                  Error::None &&
              session.Process(prepare) && game.InternalWorld().FindScene(retained),
          "persistent scene was unloaded by Replace");
  const auto primary_to_unload = session.View().primary;
  Require(session.RequestUnload(primary_to_unload, request) == Error::None &&
              session.Process(prepare) && session.View().primary == retained &&
              session.View().camera && visible == retained,
          "unloading primary did not select retained camera");
  Require(session.RequestLoad(testscene::kLevel, Mode::Replace, false, request) == Error::None,
          "stop fixture request");
  cancel = true;
  const auto last_visible = visible;
  Require(!session.Process(prepare) && visible == last_visible && resources == 1,
          "Stop during prepare published candidate resources");
  session.Stop();
  Require(session.Status(request, status) == Error::None && status.state == 4 &&
              session.RequestLoad(testscene::kTitle, Mode::Replace, false, untouched) ==
                  Error::Lifecycle,
          "Stop did not cancel/block pending work");
  presentation.reset();
  Require(resources == 0 && frame->front().mesh->vertices.size() == 3,
          "final presentation/source resource lifetime");
}

struct ScriptHost final {
  runtime::GameplayServices services;
  std::size_t allocations{}, applied{}, ready{}, stopped{};
  NexoraGameplayHostV3 Host() {
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = 3;
    host.capabilities = 1ULL << 6;
    host.context = this;
    host.allocate = [](void *p, std::uint64_t, std::uint64_t size,
                       std::uint64_t alignment) -> void * {
      if (!size || size > 1024 * 1024 || alignment < 8 || alignment > 64 ||
          (alignment & (alignment - 1)))
        return nullptr;
      auto *value =
          ::operator new(static_cast<std::size_t>(size), std::align_val_t(alignment), std::nothrow);
      if (value)
        ++static_cast<ScriptHost *>(p)->allocations;
      return value;
    };
    host.deallocate = [](void *p, std::uint64_t, void *allocation, std::uint64_t,
                         std::uint64_t alignment) {
      ::operator delete(allocation, std::align_val_t(alignment));
      --static_cast<ScriptHost *>(p)->allocations;
    };
    host.log = [](void *p, std::uint32_t, const char *text, std::uint32_t length) {
      auto &self = *static_cast<ScriptHost *>(p);
      const std::string_view message(text, length);
      self.ready += message == "scene fixture ready";
      self.applied += message == "scene fixture applied";
      self.stopped += message == "scene fixture stopped";
    };
    host.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *output) {
      return static_cast<ScriptHost *>(p)->services.GetApi(output);
    };
    return host;
  }
};
void Script(const std::filesystem::path &library) {
  const auto project = testscene::Project();
  game::GameWorld world(runtime::WorldKind::Play);
  runtime::GameSceneSession session(world, project);
  std::uint64_t request{};
  Require(session.RequestLoad(project->StartupScene(), Mode::Replace, false, request) ==
                  Error::None &&
              session.Process(),
          "script startup");
  const auto initial = session.View().primary;
  ScriptHost provider;
  provider.services.scene = session.Api();
  runtime::GameplayModuleHost module(provider.Host());
  Require(std::filesystem::is_regular_file(library), "SDK Zig module output path is missing");
  Require(module.Load(library) && provider.allocations == 1 && provider.ready == 1,
          "SDK Zig module did not negotiate scene service/startup");
  std::size_t transitions{};
  auto previous = initial;
  for (unsigned frame = 0; frame < 120; ++frame) {
    Require(module.Update(1.0 / 60.0), "actual Zig module callback failed");
    Require(session.View().primary == previous, "script request changed scene inside callback");
    world.InternalWorld().EndFrame();
    Require(session.Process(), "script scene request failed at frame boundary");
    if (session.View().primary != previous) {
      previous = session.View().primary;
      ++transitions;
    }
  }
  Require(transitions == 2 && provider.applied == 2 &&
              session.View().instances.front().asset == testscene::kLevel,
          "actual Zig module did not complete Level01 to Title to Level01");
  session.Stop();
  module.Unload();
  Require(provider.allocations == 0 && provider.stopped == 1,
          "Zig module Stop/service/allocator lifetime failed");
  std::cout << "Actual SDK Zig module: non-Main startup, two deferred transitions and Stop/Destroy "
               "passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    Core();
    if (argc == 2)
      Script(argv[1]);
    std::cout << "Queued scene transitions, 120 A/B cycles, failure conservation, C service and "
                 "prefix passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
