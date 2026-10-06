#include "EditorImGuiTestAccess.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-transform-input-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    using namespace nexora;
    using Access = editor::imgui::EditorImGuiTestAccess;
    runtime::World world;
    const auto scene_id = world.LoadScene("Transform Input");
    Require(world.Activate(scene_id), "activation failed");
    editor::SceneDocument scene(world, scene_id);
    const auto first = scene.Create("First"), second = scene.Create("Second");
    const std::array keys{*scene.Key(first), *scene.Key(second)};
    auto a = *editor::WithEulerDegrees({}, {10, 20, 30});
    auto b = *editor::WithEulerDegrees({}, {-15, 40, 60});
    a.x = 1;
    a.y = 2;
    a.z = 7;
    a.sx = 2;
    a.sy = 3;
    a.sz = 4;
    b.x = 10;
    b.y = 20;
    b.z = 7;
    b.sx = -2;
    b.sy = -3;
    b.sz = -4;
    const std::array original{a, b};
    Require(scene.SetTransforms(keys, original) && scene.Select(keys), "fixture failed");
    editor::ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(root, "Transform Input", &error), "project creation failed");
    const auto path = root / "Content/scene";
    Require(scene.Save(path), "baseline save failed");
    const auto baseline = world.SaveScene(scene_id);
    editor::ProductShell shell;
    editor::imgui::EditorImGuiHost ui;
    ui.SetDisplay(1280, 900, 1);
    Access::ConfigureSyntheticInput(ui);
    editor::ProjectWorkspace *active = &workspace;
    runtime::PlaySession play(world);
    const auto draw = [&] {
      ui.BeginFrame();
      ui.DrawProductShell(shell, &scene, active, nullptr, nullptr, nullptr, nullptr, &play);
      static_cast<void>(ui.EndFrame());
    };
    const auto focus = [&](bool focused) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::FocusChanged;
      event.value0 = focused;
      ui.ProcessEvents(std::array{event});
      draw();
    };
    focus(true);
    draw();
    draw();
    Access::FocusInspector(ui);
    draw();
    draw();
    Require(Access::InspectorTransformMixed(ui) == std::array{true, true, false, true, true, true},
            "mixed Position/Scale states are wrong");
    const auto key = [&](Nexora::Window::Key code, bool down, bool control = false) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Key;
      event.value0 = static_cast<int>(code);
      event.value1 = down;
      event.modifiers =
          control ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
      ui.ProcessEvents(std::array{event});
      draw();
    };
    const auto tap = [&](Nexora::Window::Key code) {
      key(code, true);
      key(code, false);
    };
    const auto draft = [&](std::size_t axis, std::string_view text) {
      Access::FocusInspectorTransformField(ui, axis);
      draw();
      draw();
      key(Nexora::Window::Key::A, true, true);
      key(Nexora::Window::Key::A, false);
      const auto before = world.SaveScene(scene_id);
      for (const char character : text) {
        Nexora::Window::WindowEvent event;
        event.type = Nexora::Window::WindowEventType::Text;
        event.value0 = character;
        ui.ProcessEvents(std::array{event});
        draw();
        Require(world.SaveScene(scene_id) == before, "typing mutated a transform before Enter");
      }
      Require(Access::InspectorTransformText(ui, axis) == text, "partial draft was lost");
    };
    draft(0, "-12.5");
    tap(Nexora::Window::Key::Enter);
    a.x = b.x = -12.5;
    Require(scene.Transform(first) == a && scene.Transform(second) == b,
            "Enter changed unrelated multi-selection fields");
    Require(scene.Undo() && world.SaveScene(scene_id) == baseline && !scene.Dirty(),
            "multi-character edit created more than one Undo step");
    draft(2, "7");
    tap(Nexora::Window::Key::Enter);
    Require(scene.Redo() && scene.Transform(first) == a && scene.Transform(second) == b,
            "equal-value Enter discarded Redo");
    Require(scene.Undo() && !scene.Dirty(), "Redo reset failed");

    draft(4, "-2.5");
    auto external = *scene.Transform(second);
    external.y = 123;
    Require(scene.SetTransforms(keys, std::array{original.front(), external}),
            "external edit failed");
    tap(Nexora::Window::Key::Enter);
    auto expected_a = original.front(), expected_b = external;
    expected_a.sy = expected_b.sy = -2.5;
    Require(scene.Transform(first) == expected_a && scene.Transform(second) == expected_b,
            "field commit overwrote a newer unrelated field or rotation");
    Require(scene.Undo() && scene.Transform(second) == external && scene.Undo() && !scene.Dirty(),
            "scale edit did not retain distinct original scales");
    for (const auto &[axis, text] : std::array<std::pair<std::size_t, std::string_view>, 3>{
             {{3, "0"}, {0, "1e999"}, {1, "-"}}}) {
      draft(axis, text);
      tap(Nexora::Window::Key::Enter);
      Require(world.SaveScene(scene_id) == baseline && !scene.Dirty(),
              "invalid input mutated the batch");
    }
    draft(0, "99");
    tap(Nexora::Window::Key::Escape);
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty() && world.SaveScene(scene_id) == baseline,
            "Escape committed abandoned typing");
    draft(0, "99");
    focus(false);
    focus(true);
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "focus recovery committed abandoned typing");
    draft(0, "99");
    Require(scene.Select(std::array{first}), "selection switch failed");
    draw();
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "selection switch reused another selection's draft");
    Require(scene.Select(keys), "selection restore failed");
    draft(0, "99");
    Require(scene.Reload(path) && scene.Select(std::array{first, second}), "reload failed");
    draw();
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "document reload submitted a stale draft");
    const std::array live_keys{*scene.Key(first), *scene.Key(second)};

    editor::ProjectWorkspace observer;
    Require(observer.Open(root, editor::ProjectAccess::ReadOnly, &error), "read-only open failed");
    active = &observer;
    auto forbidden = *scene.Transform(first);
    forbidden.x = 88;
    Access::QueueInspectorTransforms(ui, live_keys, std::array{forbidden, forbidden});
    Access::QueueInspectorEulerField(ui, live_keys, 0, 88);
    draw();
    Require(!scene.Dirty(), "read-only queued Transform/Euler requests mutated the document");
    Access::FocusInspectorTransformField(ui, 0);
    draw();
    draw();
    Nexora::Window::WindowEvent typed;
    typed.type = Nexora::Window::WindowEventType::Text;
    typed.value0 = '9';
    ui.ProcessEvents(std::array{typed});
    draw();
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "read-only field accepted typing");
    active = &workspace;
    draft(0, "99");
    std::ofstream(root / ".nexora/workspace.recovery") << "schema=1\n";
    Access::QueueInspectorTransforms(ui, live_keys, std::array{forbidden, forbidden});
    Access::QueueInspectorEulerField(ui, live_keys, 0, 88);
    draw();
    Require(!scene.Dirty() && workspace.DiscardRecovery(&error),
            "recovery allowed a transform request");
    draw();
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "recovery exit revived a draft");

    // Inspecting Play cancels Editor drafts without changing either world.
    draft(0, "99");
    Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
            "Play start failed");
    Access::SelectPlayEntity(ui, first);
    draw();
    Require(play.Stop(), "Play stop failed");
    draw();
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "leaving Play Inspector revived an Editor draft");
    Access::FocusInspector(ui);
    draw();
    draw();
    draft(0, "99");
    Access::FocusHierarchy(ui);
    draw();
    draw();
    Access::FocusInspector(ui);
    draw();
    draw();
    tap(Nexora::Window::Key::Enter);
    Require(!scene.Dirty(), "leaving Inspector revived abandoned typing");

    draft(0, "+1.2345678901234567e2");
    tap(Nexora::Window::Key::Enter);
    Require(scene.Transform(first)->x == 123.45678901234567 &&
                scene.Transform(second)->x == 123.45678901234567 && scene.Save(path),
            "scientific/full-precision position commit failed");
    runtime::World reopened_world;
    editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
    Require(reopened.Reload(path) && reopened.Transform(first)->x == scene.Transform(first)->x &&
                reopened.Transform(second)->sy == original.back().sy,
            "typed field or unrelated values were lost on reopen");
    // Close confirmation blocks both its first frame and later open-modal frames.
    ui.RequestCloseConfirmation();
    Access::QueueInspectorTransforms(ui, live_keys, std::array{forbidden, forbidden});
    draw();
    Require(!scene.Dirty(), "close confirmation's first frame admitted a transform request");
    Access::QueueInspectorEulerField(ui, live_keys, 0, 88);
    draw();
    Require(!scene.Dirty(), "close confirmation admitted queued transform changes");
    workspace = {};
    observer = {};
    std::filesystem::remove_all(root);
    std::cout << "Transform keyboard input contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove_all(root);
    return 1;
  }
}
