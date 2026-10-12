#include "EditorImGuiTestAccess.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Action = editor::imgui::SceneTabAction;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
struct Fixture {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-scene-tabs-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *workspace = &writer;
  runtime::World world;
  runtime::Id first_id = world.LoadScene("First"), second_id = world.LoadScene("Second");
  editor::SceneDocument first{world, first_id}, second{world, second_id};
  std::unique_ptr<editor::SceneFileSession> first_files, second_files;
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::Id first_entity{}, second_entity{};
  bool active_second{}, reference{}, busy{}, frozen{};
  float scale;
  std::array<std::string, 2> originals;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Scene tabs") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(first_id) &&
                world.Activate(second_id),
            "Scene tab workspace fixture failed");
    first_entity = first.CreateCamera("First camera");
    second_entity = second.CreateLight("Second light");
    first_files = std::make_unique<editor::SceneFileSession>(writer, first);
    second_files = std::make_unique<editor::SceneFileSession>(writer, second);
    Require(first_entity && second_entity &&
                first_files->SaveAs(first_files->Token(), "Content/First.scene").Applied() &&
                second_files->SaveAs(second_files->Token(), "Content/Second.scene").Applied(),
            "Scene tab file fixture failed");
    originals = {Read(root / "Content/First.scene"), Read(root / "Content/Second.scene")};
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i != 4; ++i)
      Draw();
    Access::FocusScene(ui);
    Draw();
  }
  ~Fixture() {
    second_files.reset();
    first_files.reset();
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  auto Rows() const {
    return std::array{editor::imgui::SceneTabItem{first_id, first_files->Token(), "First",
                                                  first_files->CurrentPath(), true, first.Dirty(),
                                                  false, frozen},
                      editor::imgui::SceneTabItem{second_id, second_files->Token(), "Second",
                                                  second_files->CurrentPath(), !reference,
                                                  second.Dirty(), true, frozen}};
  }
  void Draw() {
    auto &document = active_second ? second : first;
    auto &files = active_second ? *second_files : *first_files;
    Require(ui.SetSceneTabs(Rows(), active_second ? second_id : first_id, busy),
            "Current scene tab context rejected");
    ui.SetSceneFileContext(files.Token(), files.CurrentPath(), files.SaveBlocked());
    ui.BeginFrame();
    ui.DrawProductShell(shell, &document, workspace, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void ClickAt(std::array<float, 2> point) {
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>(point[0] * scale);
    pointer.value1 = static_cast<int>(point[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    Draw();
  }
  void Click(std::size_t control) {
    const auto point = Access::SceneTabControl(ui, control);
    Require(point.has_value(), "Scene tab control not submitted");
    ClickAt(*point);
  }
  editor::imgui::SceneTabRequest Take(Action action) {
    const auto request = ui.TakeSceneTabRequest();
    Require(request && request->action == action && !ui.TakeSceneTabRequest(),
            "Scene tab action missing, misidentified or delivered twice");
    return *request;
  }
  void Unchanged() const {
    Require(Read(root / "Content/First.scene") == originals[0] &&
                Read(root / "Content/Second.scene") == originals[1],
            "Scene tab widgets performed source IO");
  }
};
void Controls(float scale) {
  Fixture f(scale);
  const auto second = Access::SceneTabPosition(f.ui, f.second_id);
  Require(second.has_value(), "Secondary scene tab absent");
  f.ClickAt(*second);
  const auto selected = f.Take(Action::Select);
  Require(selected.source == f.first_files->Token() && selected.target == f.second_id &&
              selected.target_token == f.second_files->Token() && !f.first.Dirty() &&
              !f.second.Dirty(),
          "Scene tab selection lost source/target scope or changed documents");
  f.active_second = true;
  f.Draw();
  f.Click(0);
  Require(f.Take(Action::New).source == f.second_files->Token(),
          "New additive request targeted the old active scene");
  f.Draw();
  f.Click(3);
  Require(f.Take(Action::SaveAll).source == f.second_files->Token(),
          "Save All request lost the current source token");
  f.Draw();
  for (const auto reference : {false, true}) {
    f.Click(reference ? 2 : 1);
    f.Draw();
    Access::SetSceneTabPath(f.ui, "Content/\xE4\xB8\x80.scene");
    f.Draw();
    f.Click(6);
    const auto opened = f.Take(reference ? Action::OpenReference : Action::OpenOwned);
    Require(opened.source == f.second_files->Token() &&
                opened.path == std::filesystem::path(u8"Content/一.scene"),
            "Additive chooser lost owning Unicode path or source scope");
    f.ui.SetSceneTabStatus("Opened", true);
    f.Draw();
  }
  Require(f.second.Rename(*f.second.Key(f.second_entity), "Dirty second"),
          "Dirty close fixture failed");
  f.Draw();
  f.Click(4);
  f.Draw();
  f.Click(10);
  Require(!f.ui.TakeSceneTabRequest() && f.second.Dirty(), "Cancel closed a dirty scene");
  f.Click(4);
  f.Draw();
  f.Click(9);
  const auto discarded = f.Take(Action::Close);
  Require(discarded.target == f.second_id && discarded.target_token == f.second_files->Token() &&
              discarded.discard_dirty && !discarded.save_before_close && f.second.Dirty(),
          "Discard close performed IO or lost the explicit target choice");
  f.ui.SetSceneTabStatus("Closed", true);
  f.Draw();
  f.Click(4);
  f.Draw();
  f.Click(8);
  const auto saved_close = f.Take(Action::Close);
  Require(saved_close.target == f.second_id &&
              saved_close.target_token == f.second_files->Token() &&
              saved_close.save_before_close && !saved_close.discard_dirty && f.second.Dirty(),
          "Save All and Close lost its scoped intent or wrote source files inside widgets");
  f.ui.SetSceneTabStatus("Closed", true);
  f.Draw();
  f.Unchanged();
}
void PermissionsAndScopes(float scale) {
  Fixture f(scale);
  f.active_second = true;
  f.reference = true;
  const auto key = *f.second.Key(f.second_entity);
  Require(f.second.Select(std::array{key}), "Reference selection fixture failed");
  f.Draw();
  const auto before = f.second.CaptureRuntimeScene();
  Access::QueueHierarchyRename(f.ui, key, "Forbidden reference edit");
  runtime::Transform changed;
  changed.x = 25;
  Access::QueueInspectorTransform(f.ui, key, changed);
  f.Draw();
  f.Draw();
  Require(f.second.CaptureRuntimeScene() == before && !f.second.Dirty(),
          "Reference view accepted source authoring");
  f.reference = false;
  f.Draw();
  Require(f.second.CaptureRuntimeScene() == before &&
              f.second.Name(f.second_entity) == "Second light",
          "Rejected reference edits revived after permissions changed");
  f.frozen = true;
  f.busy = true;
  f.Draw();
  Access::QueueHierarchyRename(f.ui, key, "Forbidden unresolved-set edit");
  Access::QueueInspectorTransform(f.ui, key, changed);
  f.Draw();
  f.Click(0);
  f.Click(3);
  Require(f.second.CaptureRuntimeScene() == before && !f.second.Dirty() &&
              !f.ui.TakeSceneTabRequest(),
          "Unresolved owned scene set accepted editing or a tab mutation request");
  f.frozen = false;
  f.busy = false;
  f.Draw();
  Require(f.second.CaptureRuntimeScene() == before && !f.second.Dirty(),
          "Rejected unresolved-set edits revived after admission recovered");
  f.workspace = &f.reader;
  f.Draw();
  f.Click(0);
  f.Click(3);
  Require(!f.ui.TakeSceneTabRequest(), "Read-only scene tabs admitted New or Save All");
  f.workspace = &f.writer;
  f.busy = true;
  f.Draw();
  f.Click(0);
  f.Click(1);
  f.Click(3);
  Require(!f.ui.TakeSceneTabRequest(), "Busy scene composition admitted a mutation request");
  f.busy = false;
  f.Draw();
  f.Click(0);
  f.active_second = false;
  f.Draw();
  Require(!f.ui.TakeSceneTabRequest(), "A source switch revived an unconsumed request");
  f.active_second = true;
  f.Draw();
  Require(f.second.Rename(key, "Dirty scoped close"), "Scoped close fixture failed");
  f.Draw();
  f.Click(4);
  auto changed_rows = f.Rows();
  ++changed_rows[1].token.document_generation;
  Require(f.ui.SetSceneTabs(changed_rows, f.second_id), "New target generation context rejected");
  Require(!f.ui.TakeSceneTabRequest(), "Changed target generation retained an old close request");
  f.Draw();
  f.Click(0);
  auto another_project = f.Rows();
  for (auto &row : another_project)
    ++row.token.project.high;
  Require(f.ui.SetSceneTabs(another_project, f.second_id) && !f.ui.TakeSceneTabRequest(),
          "A project switch revived a queued mutation");
  f.Draw();
  f.Unchanged();
}
void ContextAdmission() {
  Fixture f(1);
  const auto original = f.Rows();
  const auto target = Access::SceneTabPosition(f.ui, f.second_id);
  Require(target.has_value(), "Scoped target tab absent");
  f.ClickAt(*target);
  auto replaced_target = original;
  ++replaced_target[1].token.document_generation;
  Require(f.ui.SetSceneTabs(replaced_target, f.first_id) && !f.ui.TakeSceneTabRequest(),
          "A nonactive target generation change retained a queued selection request");
  f.Draw();
  const auto check_reject = [&](auto mutate) {
    auto proposed = std::vector(original.begin(), original.end());
    mutate(proposed);
    Require(!f.ui.SetSceneTabs(proposed, f.first_id), "Invalid scene tab metadata was admitted");
    const auto retained = Access::SceneTabs(f.ui);
    Require(retained.size() == original.size() && retained[0].label == original[0].label &&
                retained[1].token == original[1].token && retained[1].path == original[1].path,
            "Rejected scene tab context changed retained owning metadata");
  };
  check_reject([](auto &rows) { rows[1].id = rows[0].id; });
  check_reject(
      [](auto &rows) { rows[1].token.document_generation = rows[0].token.document_generation; });
  check_reject([](auto &rows) { rows[1].token.document_generation = 0; });
  check_reject([](auto &rows) { rows[1].token.project = {}; });
  check_reject([](auto &rows) { ++rows[1].token.project.high; });
  check_reject([](auto &rows) { rows[0].label = std::string(1, static_cast<char>(255)); });
  check_reject([](auto &rows) { rows[0].label.assign(257, 'a'); });
  check_reject([](auto &rows) { rows[0].label = std::string("a\0b", 3); });
  check_reject([](auto &rows) { rows[0].path = std::filesystem::path(std::string(1024, 'a')); });
  check_reject([](auto &rows) { rows.resize(17); });
  Require(!f.ui.SetSceneTabs(original, 9999), "Missing active document was admitted");
  auto copied = original;
  Require(f.ui.SetSceneTabs(copied, f.first_id), "Owning context admission failed");
  copied[0].label = "Changed caller storage";
  copied[1].path = "Changed.scene";
  Require(Access::SceneTabs(f.ui)[0].label == original[0].label &&
              Access::SceneTabs(f.ui)[1].path == original[1].path,
          "Scene tab context retained caller string/path borrows");
  f.Unchanged();
}
} // namespace
int main() {
  try {
    Controls(1);
    Controls(2);
    PermissionsAndScopes(1);
    PermissionsAndScopes(2);
    ContextAdmission();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
