#include "Nexora/Runtime/Runtime.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora::runtime;
  World world;
  const auto first = world.LoadScene("Main");
  const auto second = world.LoadScene("Lighting");
  auto &entity = world.CreateEntity(first);
  entity.camera = entity.light = entity.mesh_renderer = true;
  Require(world.Activate(first) && world.Activate(second) && world.ActiveSceneCount() == 2,
          "M4 additive activation failed");
  Require(world.RequestUnload(second), "M4 safe unload was not queued");
  world.EndFrame();
  Require(world.FindScene(second)->state == SceneState::Unloaded,
          "M4 unload did not complete at frame boundary");

  AssetRegistry assets;
  Require(!assets.Stage({{1, "a", {2}, 1}, {2, "b", {1}, 1}}), "M5 accepted an asset cycle");
  Require(assets.Stage({{1, "a", {}, 1}}) && assets.ActivateStaged(),
          "M5 generation activation failed");
  assets.PinGeneration(1);
  Require(assets.Stage({{1, "b", {}, 2}}) && assets.ActivateStaged() && assets.IsPinned(1),
          "M5 pinning failed");
  Require(assets.Rollback() && assets.ActiveGeneration() == 1, "M5 rollback failed");

  ExtensionRegistry extensions{7};
  Require(!extensions.Load({"old", 6, {}}) && extensions.Load({"tools", 7, {"inspector"}}),
          "M6 ABI gate failed");
  Require(extensions.HasService("inspector"), "M6 service registration failed");
  int value = 0;
  UndoStack undo;
  undo.Execute([&] { value = 10; }, [&] { value = 0; });
  Require(value == 10 && undo.Undo() && value == 0, "M6 undo transaction failed");

  InputRouter input;
  Require(input.Route({InputKind::Touch, 42, false}) && !input.Route({InputKind::Touch, 42, false}),
          "M7 duplicated touch delivery");
  Require(VirtualList{10000, 50, 12}.ElementCount() == 12, "M7 list was not virtualized");

  LocalizationCatalog text{"en"};
  Require(text.Add({"menu.play", {{"en", "Play"}, {"zh-TW", "開始遊戲"}}}),
          "M7 localization entry was rejected");
  Require(text.Resolve("menu.play", "zh-TW") == "開始遊戲" &&
              text.Resolve("menu.play", "fr") == "Play" &&
              text.Resolve("menu.missing", "en") == "menu.missing",
          "M7 localization fallback failed");

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  const auto motion = CharacterMotor{}.Simulate({10.0, 0.0}, 3.0, true);
  Require(motion.actual_x == 3.0 && motion.grounded, "M8 motor did not resolve requested motion");
  NavigationGraph navigation;
  Require(navigation.AddNode({1, 0, 0, {2}}) && navigation.AddNode({2, 1, 0, {3}}) &&
              navigation.AddNode({3, 2, 0, {}}) &&
              navigation.FindPath(1, 3) == std::vector<Id>({1, 2, 3}) &&
              navigation.FindPath(3, 1).empty(),
          "M8 navigation path contract failed");
#endif

  ResidencySet residency;
  residency.Acquire(8);
  residency.Acquire(8);
  residency.Release(8);
  Require(residency.Resident(8), "M9 released an active media resource");
  residency.Release(8);
  Require(!residency.Resident(8), "M9 residency reference leaked");
  AnimationPlayer animation;
  Require(animation.Play({1, 1.0, true}), "M9 animation clip was rejected");
  animation.Advance(1.25);
  Require(animation.Playing() && animation.Time() == 0.25, "M9 animation looping failed");
  AudioMixer mixer{1};
  Require(mixer.Play({10, 0.5F, true}) && !mixer.Play({11, 1.0F, false}),
          "M9 audio voice limit failed");
  mixer.SetMasterGain(2.0F);
  Require(mixer.MasterGain() == 1.0F && mixer.Stop(10), "M9 audio control failed");
  MediaQueue media{2};
  Require(media.Push({1, 0.0}) && media.Push({2, 0.04}) && !media.Push({3, 0.08}) &&
              !media.PopReady(-1.0).has_value(),
          "M9 bounded media decode queue failed");
  const auto first_frame = media.PopReady(0.0);
  Require(first_frame.has_value() && first_frame->sequence == 1,
          "M9 ready media frame was not returned");
  media.Seek(2.0);
  Require(!media.Push({1, 1.0}) && media.Push({1, 2.0}), "M9 media seek failed");

  StreamingWorld streaming;
  Require(streaming.Add({10, 99, 100, 200, true, true, true}), "M10 cell add failed");
  Require(!streaming.UnloadFull(10) && streaming.Find(10)->bundle_id != streaming.Find(10)->cell_id,
          "M10 occupied pin failed");
  Require(streaming.Usage() == std::pair<std::size_t, std::size_t>{100, 200},
          "M10 budgets are incorrect");

  PlatformRuntime platform;
  platform.SetState(AppState::Background);
  const auto policy = platform.Pressure(true, true);
  Require(platform.State() == AppState::Background && policy.reduce_quality &&
              policy.release_caches,
          "M11 lifecycle policy failed");
  Require(platform.RouteWebViewPointer(true) && !platform.RouteWebViewPointer(false),
          "M11 native view routing failed");

  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
