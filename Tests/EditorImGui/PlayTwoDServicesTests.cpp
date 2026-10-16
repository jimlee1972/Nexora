#include "Nexora/Editor/TwoDAssetImport.h"
#include "PlayGameplayModule.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
constexpr runtime::AssetUuid atlasId{17, 23}, clipId{19, 29};
NexoraGameplayHostV3 host{};
NexoraTwoDApiV1 api{};
NexoraTwoDInstanceHandle handle{}, previous{};
runtime::Id parent{}, child{};
bool fail_create{}, fail_start{}, stopped{}, destroyed{};
unsigned stop_count{}, destroy_count{};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  Check(bool(output), "write fixture");
}
NexoraTwoDSprite Sprite() {
  return {atlasId.high, atlasId.low, 1, 8, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0};
}
int32_t Create(void **state, const NexoraGameplayHostV3 *wire) {
  host = *wire;
  stopped = destroyed = false;
  api = {};
  api.struct_size = sizeof(api);
  if (!(host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) || !host.get_twod_api ||
      host.get_twod_api(host.context, &api))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraTwoDSprite missing{};
  NexoraTwoDInstance instance{};
  if (api.get_sprite(api.context, parent, &missing) != NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT ||
      (previous.owner && api.read_instances(api.context, &previous, 1, &instance) !=
                             NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const auto sprite = Sprite();
  const NexoraTwoDCollider collider{0, {0, 0}, {0.5F, 0.5F}, 0.5F, 1, UINT32_MAX};
  instance = {{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, sprite};
  if (api.set_sprite(api.context, parent, &sprite) || api.set_sprite(api.context, child, &sprite) ||
      api.set_collider(api.context, child, &collider) ||
      api.create_instances(api.context, &instance, 1, &handle))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  previous = handle;
  *state = &api;
  return fail_create ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) {
  if (api.play_animation(api.context, parent, clipId.high, clipId.low, 1))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  return fail_start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double seconds) {
  NexoraTwoDEvent event{};
  NexoraTwoDAnimationResult result{};
  const auto status = api.advance_animation(api.context, parent, seconds, &event, 1, &result);
  return status || result.event_count != 1 || event.length != 6 ||
                 std::string_view(event.name, event.length) != "frozen"
             ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE
             : NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {
  ++stop_count;
  NexoraTwoDSprite sprite{};
  stopped = api.get_sprite(api.context, parent, &sprite) == NEXORA_GAMEPLAY_OK;
}
void Destroy(void *) {
  ++destroy_count;
  NexoraTwoDInstance instance{};
  game::GameplayTransformWire transform{};
  destroyed = api.read_instances(api.context, &handle, 1, &instance) == NEXORA_GAMEPLAY_OK &&
              host.read_component(host.context, parent, game::TransformComponentType(), &transform,
                                  sizeof(transform)) == NEXORA_GAMEPLAY_OK;
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
int32_t PlainCreate(void **state, const NexoraGameplayHostV3 *wire) {
  *state = nullptr;
  return !(wire->capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) && !wire->get_twod_api
             ? NEXORA_GAMEPLAY_OK
             : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
int32_t PlainLoader(std::uint32_t version, NexoraGameModuleV3 *module) {
  Loader(version, module);
  module->create = PlainCreate;
  module->on_start = [](void *) -> int32_t { return NEXORA_GAMEPLAY_OK; };
  module->update = [](void *, double) -> int32_t { return NEXORA_GAMEPLAY_OK; };
  module->on_stop = [](void *) {};
  module->destroy = [](void *) {};
  return NEXORA_GAMEPLAY_OK;
}
void Run(const std::filesystem::path &root) {
  runtime::World authoring;
  const auto scene = authoring.LoadScene("Main");
  Check(authoring.Activate(scene), "activate authoring");
  editor::SceneDocument document(authoring, scene);
  parent = document.Create("Parent");
  child = document.Create("Child", parent);
  const auto baseline = document.CaptureRuntimeScene();
  Check(bool(baseline), "source baseline");
  editor::preview::PlayTwoDBinding binding;
  {
    editor::ProjectWorkspace workspace;
    Check(workspace.Create(root, "Editor Play 2D"), "project fixture");
    editor::AssetWorkspace index;
    editor::ProjectContentSession content;
    std::string error;
    Check(index.ImportTree(root / "Content") && content.Open(workspace, index, 13), "empty index");
    const auto empty = editor::preview::CapturePlayTwoD(workspace, content, document, false, error);
    Check(empty && !empty->assets && !empty->scene && error.empty(), "empty 3D project path");
    const twod::Image image{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
    const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
    Check(bool(atlas), "atlas fixture");
    const auto bytes = twod::EncodeAtlas(*atlas);
    Write(root / "Content/Atlas.nx2atlas", bytes);
    Write(root / "Content/Clip.nx2clip",
          twod::EncodeClip({clipId, atlasId, true, {{1, 0.1, "frozen"}}}));
    Check(index.ImportTree(root / "Content", {}, {},
                           editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(workspace, index, 13),
          "typed index");
    Check(!editor::preview::CapturePlayTwoD(workspace, content, document, false, error) &&
              !error.empty(),
          "unauthorized capture");
    auto captured = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
    Check(captured && captured->scene == scene && captured->assets && error.empty() &&
              captured->assets->Matches(workspace, content, true),
          "project preparation");
    binding = std::move(*captured);
    Write(root / "Content/Atlas.nx2atlas", {});
    Check(!binding.assets->Matches(workspace, content, true), "activation accepted changed bytes");
    Write(root / "Content/Atlas.nx2atlas", bytes);
    Check(binding.assets->Matches(workspace, content, true), "restored source scope");
  }
  Check(!editor::preview::PlayTwoDServices::Bind(authoring, binding), "authoring World admitted");
  runtime::PlaySession play(authoring);
  editor::preview::PlayGameplayModule module;
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    Check(play.Start(0.1, [](runtime::World &, double) { return true; }), "start Play clone");
    auto &world = *play.PlayWorld();
    auto invalid = binding;
    invalid.scene = 0;
    Check(!module.Load(world, Loader, {}, invalid) && !module.IsBoundTo(&world), "invalid binding");
    fail_create = true;
    Check(!module.Load(world, Loader, {}, binding) && !module.IsLoaded() && !destroyed,
          "failed Create cleanup");
    fail_create = false;
    fail_start = true;
    Check(!module.Load(world, Loader, {}, binding) && !module.IsLoaded() && destroyed && !stopped,
          "failed Start destroy lifetime");
    fail_start = false;
    Check(module.Load(world, Loader, {}, binding) && module.IsBoundTo(&world), "bind actual clone");
    const auto stops = stop_count, destroys = destroy_count;
    Check(module.Load(world, Loader, {}, binding) && stop_count == stops + 1 &&
              destroy_count == destroys + 1,
          "replacement did not unload the previous owner");
    NexoraTwoDApiV1 short_api{};
    short_api.struct_size = 1;
    Check(host.get_twod_api(host.context, &short_api) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
              short_api.struct_size == 1 && !short_api.context,
          "short API output changed");
    int32_t cross_thread{};
    std::thread worker([&] { cross_thread = api.set_camera(api.context, nullptr); });
    worker.join();
    Check(cross_thread == NEXORA_GAMEPLAY_ERROR_LIFECYCLE, "wrong-thread API admitted");
    Check(module.Update(0.1), "frozen clip after project owners destroyed");
    const auto render = [&] {
      return module.ExtractTwoDFrame(400, 300, [](const auto &) { return 7; });
    };
    const auto frame = render();
    Check(frame && frame->statistics.input == 3 && frame->statistics.visible == 3 &&
              frame->vertices.size() == 12 && frame->indices.size() == 18,
          "main Play scene/pool frame extraction");
    if (cycle == 0) {
      bool rejected{};
      std::thread renderer([&] { rejected = !render(); });
      renderer.join();
      Check(rejected, "cross-thread frame extraction admitted");
      std::vector<NexoraTwoDInstance> instances(4093,
                                                {{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, Sprite()});
      std::vector<NexoraTwoDInstanceHandle> handles(instances.size());
      Check(api.create_instances(api.context, instances.data(), 4093, handles.data()) == 0,
            "preview capacity fixture");
      const auto at_capacity = render();
      Check(at_capacity && at_capacity->statistics.input == 4096, "exact preview instance budget");
      NexoraTwoDInstanceHandle excess{};
      Check(api.create_instances(api.context, instances.data(), 1, &excess) == 0 && !render(),
            "preview overflow did not reject atomically");
      Check(api.destroy_instances(api.context, &excess, 1) == 0 &&
                api.destroy_instances(api.context, handles.data(), 4093) == 0 && render(),
            "preview budget rejection changed pool");
    }

    Check(api.rebuild_queries(api.context, 0) == NEXORA_GAMEPLAY_OK &&
              host.despawn_entity(host.context, parent) == NEXORA_GAMEPLAY_OK &&
              !world.FindEntity(parent) && !world.FindEntity(child) &&
              api.rebuild_queries(api.context, 0) == NEXORA_GAMEPLAY_OK,
          "descendant metadata not forgotten");
    // Restore the clone for teardown/reload callbacks; source state and history stay untouched.
    module.Unload();
    Check(play.Stop() && play.Start(0.1, [](runtime::World &, double) { return true; }),
          "fresh session after subtree deletion");
    Check(module.Load(*play.PlayWorld(), Loader, {}, binding), "fresh scene metadata");
    module.Unload();
    Check(stopped && destroyed, "Stop/Destroy lost services or World");
    NexoraTwoDApiV1 unavailable{};
    unavailable.struct_size = sizeof(unavailable);
    Check(host.get_twod_api(host.context, &unavailable) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              !unavailable.context,
          "unbound getter returned a bridge");
    Check(module.Load(*play.PlayWorld(), PlainLoader), "3D-only module advertised 2D");
    module.Unload();
    Check(play.Stop() && document.CaptureRuntimeScene() == baseline && authoring.FindEntity(child),
          "authoring scene changed");
  }
  Check(document.Undo() && !authoring.FindEntity(child) && document.Redo() &&
            document.CaptureRuntimeScene() == baseline,
        "source Undo/Redo history changed");
  binding.assets.reset();
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-editor-play-2d-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root);
    std::filesystem::remove_all(root);
    std::cout << "Editor Play 2D: 32 real clone lifetimes, project freeze, descendant cleanup and "
                 "3D isolation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
