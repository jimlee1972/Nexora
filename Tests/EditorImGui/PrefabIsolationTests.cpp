#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/PrefabDocumentSession.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Action = editor::imgui::PrefabIsolationAction;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-prefab-controls-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root / "Project", "Prefab controls", &error), "Project fixture failed");
  runtime::World world;
  editor::SceneDocument source(world, world.LoadScene("Primary"));
  const auto node = source.Create("Original"), child = source.Create("Child", node);
  Require(source.SetOpaqueComponent(*source.Key(child), {99, "Absent.Provider", {0, 255, 27}}),
          "Opaque fixture failed");
  const std::string source_bytes = source.PrepareSave()->Bytes();
  editor::SceneFileSession files(workspace, source);
  editor::PrefabDocumentSession session(workspace);
  const foundation::Uuid id{701, 1};
  std::uint64_t identity = 100;
  const auto factory = [&] { return foundation::Uuid{702, ++identity}; };
  Require(session.Create(id, source, false, &error), "Isolation fixture failed");
  editor::imgui::EditorImGuiHost ui;
  editor::ProductShell shell;
  ui.SetDisplay(1400, 1000, scale);
  Access::ConfigureSyntheticInput(ui);
  ui.OpenPrefabIsolation();
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  bool allowed = true;
  std::uint64_t scope = 3;
  const auto draw = [&] {
    const auto *document = session.Document();
    editor::imgui::PrefabIsolationObservation view;
    view.project = workspace.Project().id;
    view.project_scope = scope;
    view.owner_generation = session.Generation();
    view.source = files.Token();
    view.asset = session.AssetId();
    view.open = document != nullptr;
    view.document_generation = document ? document->Generation() : 0;
    view.dirty = view.open && session.Dirty();
    view.writable = workspace.Writable();
    if (const auto previous = session.SourceBaseline(); previous && previous->id == view.asset)
      view.revision = previous->revision;
    Require(ui.SetPrefabIsolation(view), "Valid observation rejected");
    ui.BeginFrame();
    ui.DrawProductShell(shell, &source, &workspace);
    ui.DrawPrefabIsolation(document, session.EditableDocument(), workspace, allowed);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  const auto click = [&](std::size_t control) {
    const auto point = Access::PrefabControlPosition(ui, control);
    Require(point.has_value(), "Actual prefab control absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    ui.ProcessEvents(std::array{pointer});
    draw();
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{button});
    draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    draw();
    draw();
  };
  const auto key = [&](Nexora::Window::Key code, Nexora::Window::KeyModifiers modifiers = {}) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(code);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    draw();
    event.value1 = 0;
    event.modifiers = {};
    ui.ProcessEvents(std::array{event});
    draw();
  };
  const auto type = [&](std::string_view text) {
    key(Nexora::Window::Key::A, Nexora::Window::KeyModifiers::Control);
    for (const unsigned char character : text) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = character;
      ui.ProcessEvents(std::array{event});
      draw();
    }
  };
  click(10);
  click(11);
  type("Isolated edit");
  Require(session.Document()->Name(node) == "Original", "Name draft changed source before Enter");
  key(Nexora::Window::Key::Enter);
  Require(session.Document()->Name(node) == "Isolated edit" &&
              source.PrepareSave()->Bytes() == source_bytes,
          "Actual isolated rename failed or changed primary source");
  click(8);
  Require(session.Document()->Name(node) == "Original", "Actual prefab Undo failed");
  click(9);
  Require(session.Document()->Name(node) == "Isolated edit", "Actual prefab Redo failed");
  click(12);
  type("3.5");
  key(Nexora::Window::Key::Enter);
  Require(session.Document()->Transform(node)->x == 3.5 && source.Transform(node)->x == 0,
          "Actual isolated position field failed or changed original");
  click(4);
  const auto saved_request = ui.TakePrefabIsolationRequest();
  Require(saved_request && saved_request->action == Action::Save &&
              saved_request->scope.asset == id &&
              saved_request->scope.owner_generation == session.Generation() &&
              !saved_request->discard_dirty && session.Save(factory, &error),
          "Actual save request lost scope or failed wrapped publication");
  draw();
  const auto saved = session.SourceBaseline();
  Require(saved && !session.Dirty() && editor::PrefabAssets::Load(workspace, id) == saved,
          "Actual save baseline failed");
  click(8);
  Require(session.Dirty() && session.Document()->Transform(node)->x == 0, "Save erased field Undo");
  click(15);
  const auto review_request = ui.TakePrefabIsolationRequest();
  auto review = session.Review(&error);
  Require(review_request && review_request->action == Action::Review && review &&
              review->CanRevert(),
          "Actual Review control did not emit an owning source comparison");
  ui.SetPrefabReview(review->Changes(), review->CanRevert());
  draw();
  click(16);
  Require(!ui.TakePrefabIsolationRequest() && Access::PrefabControlPosition(ui, 17),
          "Actual property revert skipped explicit confirmation");
  click(18);
  Require(!ui.TakePrefabIsolationRequest() && session.Document()->Transform(node)->x == 0,
          "Cancel revert changed document properties");
  click(16);
  ++scope;
  draw();
  draw();
  Require(!Access::PrefabControlPosition(ui, 17) && !ui.TakePrefabIsolationRequest(),
          "Scope replacement revived property-revert confirmation");
  ui.SetPrefabReview(review->Changes(), review->CanRevert());
  draw();
  click(16);
  click(17);
  const auto revert_request = ui.TakePrefabIsolationRequest();
  Require(revert_request && revert_request->action == Action::Revert &&
              !revert_request->discard_dirty && session.Revert(*review, true, &error) &&
              session.Document()->Transform(node)->x == 3.5 && !session.Dirty(),
          "Actual confirmed revert failed or bypassed the original document baseline");
  ui.SetPrefabReview(std::nullopt);
  draw();
  click(8);
  Require(session.Dirty() && session.Document()->Transform(node)->x == 0 &&
              editor::PrefabAssets::Load(workspace, id) == saved,
          "One Undo did not restore reverted edits or revert wrote its source");
  click(5);
  Require(!ui.TakePrefabIsolationRequest() && Access::PrefabControlPosition(ui, 6),
          "Dirty close skipped confirmation");
  click(7);
  Require(session.Dirty() && !ui.TakePrefabIsolationRequest(), "Keep editing discarded source");
  click(5);
  ++scope;
  draw();
  draw();
  Require(!Access::PrefabControlPosition(ui, 6) && !ui.TakePrefabIsolationRequest(),
          "Scope change revived discard confirmation");
  allowed = false;
  draw();
  click(4);
  Require(!ui.TakePrefabIsolationRequest() && session.Dirty(), "Blocked caller emitted Save");
  allowed = true;
  draw();
  ui.RequestCloseConfirmation();
  draw();
  click(4);
  Require(!ui.TakePrefabIsolationRequest(), "Save bypassed close confirmation");
  Require(source.PrepareSave()->Bytes() == source_bytes && session.SourceBaseline() == saved,
          "Prefab controls changed original scene or saved source");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly, &error),
            "Read-only observer failed");
    editor::PrefabDocumentSession inspection(observer);
    Require(inspection.Open(id, false, &error), "Read-only prefab open failed");
    const auto before = inspection.Document()->PrepareSave()->Bytes();
    editor::imgui::EditorImGuiHost readonly_ui;
    readonly_ui.SetDisplay(1400, 1000, scale);
    Access::ConfigureSyntheticInput(readonly_ui);
    readonly_ui.OpenPrefabIsolation();
    readonly_ui.ProcessEvents(std::array{focus});
    editor::imgui::PrefabIsolationObservation view;
    view.project = observer.Project().id;
    view.project_scope = 99;
    view.owner_generation = inspection.Generation();
    view.document_generation = inspection.Document()->Generation();
    view.asset = id;
    view.open = true;
    view.revision = saved->revision;
    Require(readonly_ui.SetPrefabIsolation(view), "Read-only observation rejected");
    const auto render = [&] {
      readonly_ui.BeginFrame();
      readonly_ui.DrawProductShell(shell, &source, &observer);
      readonly_ui.DrawPrefabIsolation(inspection.Document(), inspection.EditableDocument(),
                                      observer, true);
      static_cast<void>(readonly_ui.EndFrame());
    };
    for (int i = 0; i < 4; ++i)
      render();
    readonly_ui.SetPrefabReview(review->Changes(), true);
    render();
    const auto readonly_click = [&](std::size_t control) {
      const auto point = Access::PrefabControlPosition(readonly_ui, control);
      Require(point.has_value(), "Read-only Save control absent");
      Nexora::Window::WindowEvent pointer, button;
      pointer.type = Nexora::Window::WindowEventType::Pointer;
      pointer.value0 = static_cast<int>((*point)[0] * scale);
      pointer.value1 = static_cast<int>((*point)[1] * scale);
      readonly_ui.ProcessEvents(std::array{pointer});
      render();
      button.type = Nexora::Window::WindowEventType::PointerButton;
      button.value0 = 0;
      button.value1 = 1;
      readonly_ui.ProcessEvents(std::array{button});
      render();
      button.value1 = 0;
      readonly_ui.ProcessEvents(std::array{button});
      render();
      render();
    };
    readonly_click(4);
    Require(!readonly_ui.TakePrefabIsolationRequest(), "Read-only Save emitted a request");
    readonly_click(16);
    Require(!Access::PrefabControlPosition(readonly_ui, 17) &&
                !readonly_ui.TakePrefabIsolationRequest(),
            "Read-only Revert displayed write confirmation");
    readonly_click(15);
    const auto inspect_request = readonly_ui.TakePrefabIsolationRequest();
    Require(inspect_request && inspect_request->action == Action::Review,
            "Read-only controls blocked owning source inspection");
    Require(!readonly_ui.TakePrefabIsolationRequest() &&
                inspection.Document()->PrepareSave()->Bytes() == before &&
                editor::PrefabAssets::Load(observer, id) == saved,
            "Read-only real controls emitted publication or changed source");
  }
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
