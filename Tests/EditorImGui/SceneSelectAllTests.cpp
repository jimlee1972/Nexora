#include "EditorImGuiTestAccess.h"
#include "ScenePreviewCandidates.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Event = Nexora::Window::WindowEvent;
using EventType = Nexora::Window::WindowEventType;
using Key = Nexora::Window::Key;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-scene-select-all-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  editor::ProductShell shell;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Select all");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id first{}, child{}, other{}, redo{};
  editor::imgui::EditorImGuiHost ui;
  float dpi;
  bool consume = true;
  std::optional<std::string> baseline;
  explicit Fixture(float scale) : dpi(scale) {
    Require(writer.Create(root, "Scene selection") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "select-all workspace failed");
    first = scene.Create("First");
    child = scene.Create("Child", first);
    other = scene.Create("Other");
    Require(scene.SetTransform(other, {4, 1, 2}) &&
                scene.SetOpaqueComponent(*scene.Key(first), {31, "Absent plugin", {1, 0, 255}}) &&
                scene.Select(std::array{first}) && scene.CopySelection() &&
                scene.Save(root / "Content/All.scene"),
            "selection fixture failed");
    redo = scene.Create("Retained Redo");
    Require(scene.Undo(), "selection history fixture failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1600, 1000, dpi);
    ui.SetNativeScenePreview(true);
    Access::ConfigureSyntheticInput(ui);
    Focus(true);
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusScene(ui);
    Draw();
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  editor::SceneFileToken Token() const { return {active->Project().id, scene.Generation()}; }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
    if (consume) {
      if (const auto request = ui.TakeNativeSceneSelectAllRequest()) {
        const auto candidates = editor::preview::NativeSceneProxyCandidates(scene);
        Require(editor::preview::SelectNativeSceneCandidates(scene, Token(), *request, candidates),
                "current native selection request was rejected");
      }
    }
  }
  void Focus(bool focused) {
    Event event{};
    event.type = EventType::FocusChanged;
    event.value0 = focused ? 1 : 0;
    ui.ProcessEvents(std::array{event});
  }
  static Event KeyEvent(Key key, bool down, bool ctrl = false) {
    Event event{};
    event.type = EventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = down ? 1 : 0;
    event.modifiers =
        ctrl ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
    return event;
  }
  void Press(Key key, bool ctrl = false) {
    ui.ProcessEvents(std::array{KeyEvent(key, true, ctrl)});
    Draw();
    ui.ProcessEvents(std::array{KeyEvent(key, false)});
    Draw();
  }
  void SelectAll() {
    Access::FocusScene(ui);
    Draw();
    Press(Key::A, true);
  }
  void Click() {
    const auto position = Access::SceneSelectAllPosition(ui);
    Require(position.has_value(), "Scene Select all button missing");
    Event pointer{}, button{};
    pointer.type = EventType::Pointer;
    pointer.value0 = static_cast<int>((*position)[0] * dpi);
    pointer.value1 = static_cast<int>((*position)[1] * dpi);
    button.type = EventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  std::optional<editor::SceneFileToken> Request() {
    Access::FocusScene(ui);
    Draw();
    ui.ProcessEvents(std::array{KeyEvent(Key::A, true, true)});
    Draw();
    const auto request = ui.TakeNativeSceneSelectAllRequest();
    ui.ProcessEvents(std::array{KeyEvent(Key::A, false)});
    return request;
  }
  void VerifyAll() const {
    Require(std::ranges::equal(scene.Selection(), std::array{first, child, other}),
            "Scene Select all did not use deterministic full selection order");
    VerifyWorld();
  }
  void VerifyWorld() const {
    Require(world.SaveScene(scene_id) == baseline && !scene.Dirty() &&
                scene.InspectOpaqueComponents(*scene.Key(first))->front().preview ==
                    std::vector<std::uint8_t>({1, 0, 255}),
            "selection changed World, dirty state or unknown component bytes");
  }
};
void Run(float dpi, bool native) {
  Fixture f(dpi);
  f.ui.SetNativeScenePreview(native);
  f.Draw();
  f.SelectAll();
  f.VerifyAll();
  Require(f.scene.Select(std::array{f.first}), "button baseline failed");
  f.Draw();
  f.Click();
  f.VerifyAll();
  f.active = &f.reader;
  Require(f.scene.Select(std::span<const runtime::Id>{}), "readonly baseline failed");
  f.SelectAll();
  f.VerifyAll();
  Access::FocusHierarchy(f.ui);
  f.Draw();
  Require(f.scene.Select(std::array{f.other}), "other-panel baseline failed");
  Access::FocusProfiler(f.ui);
  f.Draw();
  f.Press(Key::A, true);
  Require(std::ranges::equal(f.scene.Selection(), std::array{f.other}),
          "Scene Ctrl+A stole another panel's input");
  Access::FocusScene(f.ui);
  f.Draw();
  f.Focus(false);
  f.Draw();
  f.Press(Key::A, true);
  Require(std::ranges::equal(f.scene.Selection(), std::array{f.other}),
          "unfocused Scene Ctrl+A changed selection");
  f.Focus(true);
  f.Draw();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Press(Key::A, true);
  f.Click();
  Require(std::ranges::equal(f.scene.Selection(), std::array{f.other}),
          "Select all bypassed the close modal");
  f.Press(Key::Escape);
  Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel, "close fixture failed");
  f.active = &f.writer;
  f.Draw();
  Access::FocusInspector(f.ui);
  Access::FocusInspectorTransformField(f.ui, 0);
  f.Draw();
  f.Press(Key::A, true);
  Require(std::ranges::equal(f.scene.Selection(), std::array{f.other}),
          "Scene Ctrl+A replaced text-field selection");
  f.VerifyWorld();
  Require(f.scene.Redo() && f.scene.Name(f.redo) == "Retained Redo" && f.scene.Undo(),
          "select-all consumed authoring Redo");
  f.SelectAll();
  Require(f.scene.Paste() && f.scene.Selection().size() == 1 && f.scene.Nodes().size() == 5 &&
              f.scene.Undo(),
          "select-all replaced the original owning clipboard forest");
  Require(f.scene.NewScene(), "empty scene fixture failed");
  f.baseline = f.world.SaveScene(f.scene_id);
  const bool empty_dirty = f.scene.Dirty();
  f.SelectAll();
  f.Click();
  Require(f.scene.Selection().empty() && f.scene.Dirty() == empty_dirty,
          "empty Select all changed authoring state");
}
void RunNativePackets(float dpi) {
  Fixture f(dpi);
  f.consume = false;
  auto request = f.Request();
  Require(request.has_value(), "Ctrl+A request missing");
  Require(!f.ui.TakeNativeSceneSelectAllRequest(), "native selection request replayed");
  const auto candidates = editor::preview::NativeSceneProxyCandidates(f.scene);
  auto stale = *request;
  ++stale.document_generation;
  Require(!editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), stale, candidates) &&
              std::ranges::equal(f.scene.Selection(), std::array{f.first}),
          "stale scope changed selection");
  auto wrong = *request;
  wrong.project = {};
  Require(!editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), wrong, candidates),
          "foreign project selection was accepted");
  auto malformed = candidates;
  malformed.back().entity = 0;
  Require(!editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), *request, malformed) &&
              std::ranges::equal(f.scene.Selection(), std::array{f.first}),
          "invalid packet partially selected");
  malformed = candidates;
  malformed.back().min.x = malformed.back().max.x + 1;
  Require(!editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), *request, malformed),
          "malformed bounds were selected");
  malformed = candidates;
  malformed.push_back(candidates.front());
  Require(!editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), *request, malformed),
          "duplicate candidate IDs were accepted");
  auto filtered = candidates;
  filtered[0].visible = false;
  filtered[1].locked = true;
  Require(editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), *request, filtered) &&
              std::ranges::equal(f.scene.Selection(), std::array{f.other}),
          "hidden/locked candidates were selected");
  f.Draw();
  request = f.Request();
  Require(request.has_value(), "fresh selection request missing");
  f.Draw();
  Require(!f.ui.TakeNativeSceneSelectAllRequest(), "request survived BeginFrame");
  f.ui.SetNativeScenePreviewAvailable(false);
  f.SelectAll();
  Require(!f.ui.TakeNativeSceneSelectAllRequest(), "unavailable preview emitted selection");
  f.ui.SetNativeScenePreviewAvailable(true);
  f.Draw();
  f.ui.ProcessEvents(std::array{Fixture::KeyEvent(Key::A, true, true)});
  f.Draw();
  f.ui.RequestCloseConfirmation();
  Require(!f.ui.TakeNativeSceneSelectAllRequest(), "close retained an unapplied selection request");
  f.Draw();
  f.ui.ProcessEvents(std::array{Fixture::KeyEvent(Key::A, false)});
  f.Draw();
  f.Press(Key::Escape);
  Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
          "late close did not cancel");
  Access::FocusScene(f.ui);
  f.Draw();
  f.ui.ProcessEvents(
      std::array{Fixture::KeyEvent(Key::A, true, true), Fixture::KeyEvent(Key::F, true, true)});
  const auto camera = f.ui.GetNativeSceneOrbit();
  f.Draw();
  Require(f.ui.TakeNativeSceneSelectAllRequest() &&
              f.ui.GetNativeSceneOrbit().distance == camera.distance &&
              f.ui.GetNativeSceneOrbit().target_y == camera.target_y,
          "same-frame framing used pre-selection bounds");
  f.ui.ProcessEvents(
      std::array{Fixture::KeyEvent(Key::A, false), Fixture::KeyEvent(Key::F, false)});
  f.Draw();
  Access::FocusScene(f.ui);
  f.Draw();
  Event pointer{}, button{};
  const auto view = f.ui.NativeScenePreviewViewport();
  pointer.type = EventType::Pointer;
  pointer.value0 = view->x + view->width / 2;
  pointer.value1 = view->y + view->height / 2;
  button.type = EventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer, button});
  f.Draw();
  f.Press(Key::A, true);
  Require(!f.ui.TakeNativeSceneSelectAllRequest(), "Ctrl+A interrupted a held drag");
  pointer.value0 += static_cast<int>(40 * dpi);
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{pointer, button, Fixture::KeyEvent(Key::A, true, true)});
  f.Draw();
  Require(f.ui.NativeSceneDrag() && !f.ui.TakeNativeSceneSelectAllRequest(),
          "Ctrl+A changed selection before the released drag committed");
  f.ui.ProcessEvents(std::array{Fixture::KeyEvent(Key::A, false)});
  f.Draw();
  Access::FocusScene(f.ui);
  f.Draw();
  f.ui.ProcessEvents(std::array{Fixture::KeyEvent(Key::A, true, true)});
  f.Draw();
  f.Focus(false);
  Require(!f.ui.TakeNativeSceneSelectAllRequest(),
          "focus loss retained an unapplied selection request");
  f.VerifyWorld();
}
void RunBudget() {
  Fixture f(1);
  const auto path = f.root / "Content/Budget.scene";
  constexpr std::size_t count = editor::imgui::kMaximumNativeSceneFrameCandidates + 2;
  {
    std::ofstream source(path, std::ios::binary);
    source << "NEXORA_EDITOR_SCENE 2\n";
    for (std::size_t i = 0; i < count; ++i)
      source << "node " << i + 100 << " 0 Budget " << i << '\n';
    source << "world\nNEXORA_SCENE 3 \"Budget\" 0 " << count << '\n';
    for (std::size_t i = 0; i < count; ++i)
      source << i + 100 << " 0 " << (i == 0 ? 1000000 : 0)
             << " 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  }
  Require(f.scene.Reload(path), "budget scene failed");
  f.baseline = f.world.SaveScene(f.scene_id);
  f.SelectAll();
  Require(f.scene.Selection().size() == editor::imgui::kMaximumNativeSceneFrameCandidates &&
              f.scene.Selection().front() == 101 && f.scene.Selection().back() == 4099 &&
              f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty(),
          "native Ctrl+A included invalid transform or hidden budget tail");
  auto oversized = editor::preview::NativeSceneProxyCandidates(f.scene);
  oversized.push_back(oversized.front());
  const auto selected =
      std::vector<runtime::Id>(f.scene.Selection().begin(), f.scene.Selection().end());
  Require(!editor::preview::SelectNativeSceneCandidates(f.scene, f.Token(), f.Token(), oversized) &&
              std::ranges::equal(f.scene.Selection(), selected),
          "oversized packet changed selection");
}
} // namespace
int main() {
  try {
    Run(1, true);
    Run(2, true);
    Run(1, false);
    Run(2, false);
    RunNativePackets(1);
    RunNativePackets(2);
    RunBudget();
    std::cout << "Scene Select all contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
