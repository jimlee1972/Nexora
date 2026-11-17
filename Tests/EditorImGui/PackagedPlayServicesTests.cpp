#include "../Runtime/GameSceneFixture.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/GameSceneSession.h"
#include "PackagedPlayContext.h"
#include "PlayGameplayModule.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void CandidateConservation() {
  using Nexora::Presentation::SurfaceStatus;
  for (const bool invalid_camera : {false, true}) {
    const auto loaded = runtime::LoadGameProjectPackage(testscene::Package(invalid_camera));
    Require(loaded.has_value(), "candidate fixture package");
    editor::preview::PackagedPlayContext context(
        std::make_shared<const runtime::LoadedGameProject>(std::move(*loaded)));
    const auto accept = [](const Nexora::Presentation::SceneDrawData &) {
      return SurfaceStatus::Ready;
    };
    Require(context.Render({0, 0, 640, 480}, false, accept).committed, "candidate startup");
    const auto primary = context.Scenes().View().primary;
    const auto camera = context.Scenes().View().camera;
    const auto original = context.World().InternalWorld().SaveScene(primary);
    std::uint64_t request{};
    Require(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Replace,
                                         false, request) == runtime::GameSceneError::None,
            "candidate request");
    const auto reject = [](const Nexora::Presentation::SceneDrawData &) {
      return SurfaceStatus::Unsupported;
    };
    const auto frame =
        context.Render({0, 0, 640, 480}, false,
                       invalid_camera ? editor::preview::PackagedPlayContext::Draw(accept)
                                      : editor::preview::PackagedPlayContext::Draw(reject));
    Require(frame.rejected && !frame.committed && context.Commits() == 1 &&
                context.Scenes().View().primary == primary &&
                context.Scenes().View().camera == camera &&
                context.World().InternalWorld().SaveScene(primary) == original &&
                context.Render({0, 0, 640, 480}, false, accept).drawn,
            "candidate CPU/native-marker rejection changed live scene/camera/palette");
  }
}
void NativeRejectionKeepsPlaying() {
  using Nexora::Presentation::SurfaceStatus;
  for (const auto failure :
       {SurfaceStatus::Unsupported, SurfaceStatus::InvalidDescriptor, SurfaceStatus::DeviceLost}) {
    editor::preview::PackagedPlayContext context(testscene::Project());
    const auto accept = [](const Nexora::Presentation::SceneDrawData &) {
      return SurfaceStatus::Ready;
    };
    Require(context.Render({0, 0, 640, 480}, false, accept).committed, "retained startup");
    const auto primary = context.Scenes().View().primary;
    const auto original = context.World().InternalWorld().SaveScene(primary);
    std::uint64_t request{};
    Require(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Replace,
                                         false, request) == runtime::GameSceneError::None,
            "native rejection request");
    unsigned calls{};
    const auto reject_candidate = [&](const Nexora::Presentation::SceneDrawData &) {
      return ++calls == 1 ? failure : SurfaceStatus::Ready;
    };
    const auto frame = context.Render({0, 0, 640, 480}, false, reject_candidate);
    const bool recoverable = failure != SurfaceStatus::DeviceLost;
    Require(frame.rejected && !frame.committed && context.Commits() == 1 &&
                context.Scenes().View().primary == primary &&
                context.World().InternalWorld().SaveScene(primary) == original &&
                (recoverable ? (calls == 2 && frame.drawn && frame.status == SurfaceStatus::Ready)
                             : (calls == 1 && !frame.drawn && frame.status == failure)),
            "candidate native rejection lost the retained view or retried a lost device");
    if (recoverable)
      Require(context.Render({0, 0, 640, 480}, false, accept).drawn,
              "retained scene stopped after candidate admission rejection");
  }
}
void UiSubmissionConservation() {
  using Nexora::Presentation::SceneDrawData;
  using Nexora::Presentation::SurfaceStatus;
  auto package = testscene::Package();
  package.assets.clear();
  for (const auto id : {testscene::kLevel, testscene::kTitle}) {
    runtime::World world;
    const auto scene = world.LoadScene("Camera only");
    const auto camera = world.CreateEntity(scene).id;
    runtime::WorldCommandBuffer edits;
    edits.SetCamera(camera, runtime::CameraComponent{});
    Require(edits.Apply(world), "empty UI fixture camera");
    const auto bytes = runtime::EncodeCookedScene({*world.SaveScene(scene), {}, {}});
    Require(bool(bytes), "empty UI fixture encoding");
    package.assets.push_back(testscene::Cook(id, runtime::kCookedSceneType, bytes.Value()));
  }
  const auto loaded = runtime::LoadGameProjectPackage(std::move(package));
  Require(bool(loaded), "empty UI fixture package");
  auto project = std::make_shared<const runtime::LoadedGameProject>(std::move(*loaded));
  editor::preview::PackagedPlayContext context(project);
  unsigned submissions{};
  const auto reject = [&](const SceneDrawData &data) {
    Require(data.instances.empty(), "empty UI fixture unexpectedly rendered meshes");
    ++submissions;
    return SurfaceStatus::InvalidDescriptor;
  };
  const auto rejected = context.RenderWithUi({0, 0, 640, 480}, false, reject);
  Require(submissions == 1 && rejected.rejected && !rejected.committed && !context.Ready() &&
              context.Commits() == 0,
          "empty candidate bypassed GUI admission or published after GUI failure");
  std::uint64_t request{};
  Require(context.Scenes().RequestLoad(testscene::kLevel, runtime::GameSceneLoadMode::Replace,
                                       false, request) == runtime::GameSceneError::None,
          "empty UI startup retry");
  const auto accept = [&](const SceneDrawData &data) {
    Require(data.instances.empty(), "empty UI retained unexpected meshes");
    ++submissions;
    return SurfaceStatus::Ready;
  };
  Require(context.RenderWithUi({0, 0, 640, 480}, false, accept).committed && context.Ready() &&
              context.Commits() == 1 && submissions == 2,
          "empty candidate did not publish after GUI admission");
  const auto primary = context.Scenes().View().primary;
  const auto baseline = context.World().InternalWorld().SaveScene(primary);
  Require(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Replace,
                                       false, request) == runtime::GameSceneError::None,
          "empty UI replacement request");
  const auto lost_ui = [&](const SceneDrawData &) {
    ++submissions;
    return SurfaceStatus::DeviceLost;
  };
  const auto retained = context.RenderWithUi({0, 0, 640, 480}, false, lost_ui);
  Require(retained.rejected && !retained.committed && context.Commits() == 1 && submissions == 3 &&
              context.Scenes().View().primary == primary &&
              context.World().InternalWorld().SaveScene(primary) == baseline,
          "empty candidate UI failure replaced the live World or retried a lost device");
}
void Run(const std::filesystem::path &module_path, const std::filesystem::path &root) {
  std::filesystem::create_directory(root);
  const auto library = root / module_path.filename();
  std::filesystem::copy_file(module_path, library);
  runtime::World authoring;
  const auto authored_scene = authoring.LoadScene("Last edited Title");
  static_cast<void>(authoring.CreateEntity(authored_scene));
  Require(authoring.Activate(authored_scene), "authoring scene");
  const auto original = authoring.SaveScene(authored_scene);
  const auto project = testscene::Project();
  editor::preview::PackagedPlayContext context(project);
  auto &game = context.World();
  auto &scenes = context.Scenes();
  std::vector<float> cameras;
  const auto draw = [&](const Nexora::Presentation::SceneDrawData &data) {
    Require(data.pbr && data.instances.size() == 1 && data.vertices.size() == 3 &&
                data.indices.size() == 3 && data.batches.size() == 1 &&
                data.materials.size() == 2 && data.instances[0].model_transform.has_value(),
            "shared scene palette lost mesh/PBR/affine data");
    if (cameras.empty() || cameras.back() != data.cameraPosition[0])
      cameras.push_back(data.cameraPosition[0]);
    return Nexora::Presentation::SurfaceStatus::Ready;
  };
  const auto startup = context.Render({0, 0, 640, 480}, false, draw);
  Require(startup.committed && startup.drawn && context.Ready(), "configured data startup");
  std::string logs;
  editor::preview::PlayGameplayModule module(
      [&](std::uint32_t, std::string line) { logs += line + '\n'; });
  runtime::PlaySession play(authoring);
  Require(play.StartExternal(
              game.InternalWorld(), 1.0 / 60.0,
              [&](runtime::World &, double seconds) { return module.FixedUpdate(seconds); }),
          "borrow configured startup World");
  std::string error;
  const auto filename = library.filename().generic_u8string();
  const std::string relative(filename.begin(), filename.end());
  auto invalid = scenes.Api();
  invalid.get_status = nullptr;
  Require(!module.LoadRelative(game.InternalWorld(), root, relative, error, {}, {}, invalid),
          "incomplete scene table advertised");
  Require(module.LoadRelative(game.InternalWorld(), root, relative, error, {}, {}, scenes.Api()),
          error.c_str());
  std::vector<runtime::AssetUuid> sequence{project->StartupScene()};
  auto primary = scenes.View().primary;
  for (unsigned frame = 0; frame < 100; ++frame) {
    Require(play.Tick() && module.Update(1.0 / 60.0), "SDK callback in external Play");
    Require(scenes.View().primary == primary, "callback changed World before frame boundary");
    const auto frame_result = context.Render({0, 0, 640, 480}, false, draw);
    Require(frame_result.status == Nexora::Presentation::SurfaceStatus::Ready &&
                !frame_result.rejected && frame_result.drawn,
            "packaged frame preparation");
    if (scenes.View().primary != primary) {
      const auto view = scenes.View();
      primary = view.primary;
      const auto found =
          std::ranges::find(view.instances, primary, &runtime::GameSceneInstance::scene);
      Require(found != view.instances.end(), "primary instance missing");
      sequence.push_back(found->asset);
    }
    Require(play.PlayWorld() == &game.InternalWorld() &&
                authoring.SaveScene(authored_scene) == original &&
                play.PreviewTransformApplyBack().empty(),
            "isolated World/source conservation");
  }
  Require(sequence == std::vector{testscene::kLevel, testscene::kTitle, testscene::kLevel} &&
              scenes.Stats().instances == 1 && scenes.Stats().stored_entities == 3 &&
              cameras == std::vector<float>{10, 1, 10} && context.Commits() == 3,
          "Editor SDK did not drive configured A/B/A through common scene session");
  scenes.Stop();
  module.Unload();
  Require(logs.find("scene fixture stopped") != std::string::npos &&
              module.Allocations().live_bytes == 0 &&
              module.LastTeardownAllocations().live_bytes == 0 && play.Stop() &&
              !play.PlayWorld() && game.InternalWorld().FindScene(primary),
          "scene services/allocator/borrow lifetime teardown");
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
                    ("nexora-packaged-play-services-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    UiSubmissionConservation();
    CandidateConservation();
    NativeRejectionKeepsPlaying();
    const std::string_view value(argv[1]);
    Run(std::filesystem::path(std::u8string(value.begin(), value.end())), root);
    std::filesystem::remove_all(root);
    std::cout << "Actual SDK module, configured packaged Play A/B/A, source and service lifetimes "
                 "passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
