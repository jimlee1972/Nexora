#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/PlayApply.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct TemporaryProject final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-play-apply-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TemporaryProject() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};
} // namespace
int main() {
  try {
    using namespace nexora;
    using namespace editor;
    TemporaryProject temporary;
    ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(temporary.root, "Apply", &error), "project failed");
    runtime::World world;
    const auto scene = world.LoadScene("Main");
    Require(world.Activate(scene), "activate failed");
    SceneDocument document(world, scene);
    const auto first = document.Create("First");
    const auto second = document.Create("Second");
    const auto selected = std::array{first};
    Require(document.Select(selected), "selection failed");
    runtime::PlaySession play(world);
    const auto simulate = [&](runtime::World &clone, double) {
      runtime::WorldCommandBuffer move;
      auto one = clone.FindEntity(first)->transform;
      auto two = clone.FindEntity(second)->transform;
      one.x += 1;
      two.y += 2;
      move.SetTransform(first, one);
      move.SetTransform(second, two);
      move.SetCamera(first, runtime::CameraComponent{});
      static_cast<void>(clone.CreateEntity(scene));
      return move.Apply(clone);
    };
    Require(play.Generation() == 0 && play.Start(0.25, simulate) && play.Generation() == 1 &&
                play.Tick() && play.Pause(),
            "Play fixture failed");
    const auto reviewed = CapturePlayTransformReview(document, play);
    Require(reviewed.diffs.size() == 2 &&
                ApplyReviewedPlayTransforms(document, play, reviewed, &error) ==
                    PlayTransformApplyStatus::Applied &&
                document.Transform(first)->x == 1 && document.Transform(second)->y == 2 &&
                document.Selection().front() == first &&
                world.FindScene(scene)->entities.size() == 2 &&
                !document.Camera(*document.Key(first)),
            "apply copied unsupported components or lost selection");
    Require(document.Undo() && document.Transform(first)->x == 0 &&
                document.Transform(second)->y == 0 && document.Redo() &&
                document.Transform(first)->x == 1 && document.Transform(second)->y == 2,
            "apply was not one reversible transaction");
    Require(play.Stop() && document.SetTransform(first, {}) && document.SetTransform(second, {}),
            "reset failed");
    Require(play.Start(0.25, simulate) && play.Generation() == 2 && play.Tick() && play.Pause() &&
                ApplyReviewedPlayTransforms(document, play, reviewed, &error) ==
                    PlayTransformApplyStatus::Conflict &&
                document.Transform(first)->x == 0 && document.Transform(second)->y == 0,
            "old session review applied to a fresh clone");
    const auto concurrent = CapturePlayTransformReview(document, play);
    Require(document.SetTransform(first, {8, 0, 0}) &&
                ApplyReviewedPlayTransforms(document, play, concurrent, &error) ==
                    PlayTransformApplyStatus::Conflict &&
                document.Transform(first)->x == 8 && document.Transform(second)->y == 0,
            "conflict partially changed the Editor");
    Require(document.Undo(), "conflict fixture undo failed");
    auto valid = CapturePlayTransformReview(document, play);
    Require(document.Save(temporary.root / "Main.scene") &&
                document.Reload(temporary.root / "Main.scene") &&
                ApplyReviewedPlayTransforms(document, play, valid, &error) ==
                    PlayTransformApplyStatus::Conflict,
            "document reload accepted stale review keys");
    valid = CapturePlayTransformReview(document, play);
    Require(document.Select(selected) && document.DeleteSelection() &&
                ApplyReviewedPlayTransforms(document, play, valid, &error) ==
                    PlayTransformApplyStatus::Conflict &&
                document.Undo(),
            "missing entity review was accepted");
    auto stale_key = valid;
    ++stale_key.keys.front().entity_generation;
    Require(ApplyReviewedPlayTransforms(document, play, stale_key, &error) ==
                PlayTransformApplyStatus::Conflict,
            "stale entity generation was accepted");
    valid = CapturePlayTransformReview(document, play);
    runtime::WorldCommandBuffer parent;
    parent.SetParent(first, second, false);
    Require(parent.Apply(*play.PlayWorld()) &&
                ApplyReviewedPlayTransforms(document, play, valid, &error) ==
                    PlayTransformApplyStatus::Conflict &&
                play.State() == runtime::PlayState::Paused && play.Stop(),
            "reparent conflict escaped isolation");
    Require(play.Start(0.25, simulate) && play.Tick(), "UI fixture failed");
    imgui::EditorImGuiHost host;
    host.SetDisplay(1280, 900, 1);
    imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    host.ProcessEvents(std::array{focus});
    ProductShell shell;
    ProjectWorkspace *active = &workspace;
    const auto draw = [&] {
      host.BeginFrame();
      host.DrawProductShell(shell, &document, active, nullptr, nullptr, nullptr, nullptr, &play);
      static_cast<void>(host.EndFrame());
    };
    const auto click = [&](bool confirm) {
      const auto point = imgui::EditorImGuiTestAccess::PlayApplyPosition(host, confirm);
      Require(point.has_value(), "Apply button absent");
      Nexora::Window::WindowEvent pointer;
      pointer.type = Nexora::Window::WindowEventType::Pointer;
      pointer.value0 = static_cast<int>((*point)[0]);
      pointer.value1 = static_cast<int>((*point)[1]);
      Nexora::Window::WindowEvent button;
      button.type = Nexora::Window::WindowEventType::PointerButton;
      button.value0 = 0;
      button.value1 = 1;
      host.ProcessEvents(std::array{pointer, button});
      draw();
      button.value1 = 0;
      host.ProcessEvents(std::array{button});
      draw();
    };
    for (int frame = 0; frame < 4; ++frame)
      draw();
    click(false);
    Require(imgui::EditorImGuiTestAccess::PlayApplyOpen(host) &&
                host.TakePlayCommand() == imgui::PlayCommand::Pause && play.Pause(),
            "Apply review did not request Pause");
    draw();
    Nexora::Window::WindowEvent step;
    step.type = Nexora::Window::WindowEventType::Key;
    step.value0 = static_cast<int>(Nexora::Window::Key::F10);
    step.value1 = 1;
    host.ProcessEvents(std::array{step});
    draw();
    Require(host.TakePlayCommand() == imgui::PlayCommand::None, "modal admitted a Play shortcut");
    step.value1 = 0;
    host.ProcessEvents(std::array{step});
    draw();
    click(true);
    const auto request = host.TakePlayApplyRequest();
    Require(request && !host.TakePlayApplyRequest() &&
                ApplyReviewedPlayTransforms(document, play, *request, &error) ==
                    PlayTransformApplyStatus::Applied &&
                play.Stop() && document.Undo(),
            "confirmed owning request failed");
    draw();
    Require(play.Start(0.25, simulate) && play.Tick(), "cancel fixture failed");
    draw();
    draw();
    click(false);
    Require(host.TakePlayCommand() == imgui::PlayCommand::Pause && play.Pause(),
            "cancel pause failed");
    draw();
    const auto unchanged = document.Transform(first);
    Nexora::Window::WindowEvent escape;
    escape.type = Nexora::Window::WindowEventType::Key;
    escape.value0 = static_cast<int>(Nexora::Window::Key::Escape);
    escape.value1 = 1;
    host.ProcessEvents(std::array{escape});
    draw();
    Require(!imgui::EditorImGuiTestAccess::PlayApplyOpen(host) && !host.TakePlayApplyRequest() &&
                play.State() == runtime::PlayState::Paused &&
                document.Transform(first) == unchanged,
            "Escape applied changes or resumed the clone");
    escape.value1 = 0;
    host.ProcessEvents(std::array{escape});
    Require(play.Stop(), "cancel stop failed");
    draw();
    ProjectWorkspace observer;
    Require(observer.Open(temporary.root, ProjectAccess::ReadOnly, &error) &&
                play.Start(0.25, simulate),
            "read-only fixture failed");
    active = &observer;
    draw();
    draw();
    click(false);
    Require(!imgui::EditorImGuiTestAccess::PlayApplyOpen(host) &&
                host.TakePlayCommand() == imgui::PlayCommand::None && play.Stop(),
            "read-only Apply button emitted a review");
    active = &workspace;
    draw();
    host.SetDisplay(640, 360, 2);
    Require(play.Start(0.25, simulate) && play.Tick(), "DPI review fixture failed");
    for (int frame = 0; frame < 4; ++frame)
      draw();
    click(false);
    Require(host.TakePlayCommand() == imgui::PlayCommand::Pause && play.Pause(),
            "DPI pause failed");
    draw();
    draw();
    const auto confirm = imgui::EditorImGuiTestAccess::PlayApplyPosition(host, true);
    Require(confirm && (*confirm)[0] >= 0 && (*confirm)[0] < 640 && (*confirm)[1] >= 0 &&
                (*confirm)[1] < 360,
            "DPI modal hid its confirm button");
    click(true);
    Require(host.TakePlayApplyRequest().has_value() && play.Stop(),
            "DPI confirm was not reachable");
    const auto other_scene = world.LoadScene("Other");
    const auto outside = world.CreateEntity(other_scene).id;
    const auto before = document.Transform(first);
    Require(play.Start(0.25,
                       [&](runtime::World &clone, double seconds) {
                         if (!simulate(clone, seconds))
                           return false;
                         runtime::WorldCommandBuffer move;
                         move.SetTransform(outside, {7, 0, 0});
                         return move.Apply(clone);
                       }) &&
                play.Tick() && play.Pause(),
            "other-scene fixture failed");
    const auto unsupported = CapturePlayTransformReview(document, play);
    Require(unsupported.diffs.size() == 3 &&
                ApplyReviewedPlayTransforms(document, play, unsupported, &error) ==
                    PlayTransformApplyStatus::Conflict &&
                document.Transform(first) == before &&
                world.FindEntity(outside)->transform.x == 0 && play.Stop(),
            "unsupported-scene apply partially changed the Editor");
    std::cout << "Play apply review isolation and Undo contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
