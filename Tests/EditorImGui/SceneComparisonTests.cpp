#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-semantic-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  runtime::World world;
  runtime::Id id = world.LoadScene("Comparison");
  editor::SceneDocument scene{world, id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::SceneComparisonSnapshot observation;
  float scale;
  bool busy{};
  // File session must be constructed after the workspace opens.
  std::unique_ptr<editor::SceneFileSession> current;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Semantic UI"), "Project creation failed");
    current = std::make_unique<editor::SceneFileSession>(writer, scene);
    const auto entity = scene.Create("Base");
    Require(entity && current->SaveAs(current->Token(), "Content/Main.scene").Applied(),
            "Base save failed");
    const auto base = scene.PrepareSave()->Bytes();
    Require(scene.Rename(*scene.Key(entity), "Local"), "Local fixture failed");
    const auto local = scene.PrepareSave()->Bytes();
    runtime::World remote_world;
    editor::SceneDocument remote(remote_world, remote_world.LoadScene("Remote"));
    Require(remote.ReloadBytes(base) && remote.Rename(*remote.Key(entity), "Disk"),
            "Remote fixture failed");
    const auto disk = remote.PrepareSave()->Bytes();
    std::ofstream(root / "Content/Main.scene", std::ios::binary | std::ios::trunc) << disk;
    const auto comparison = editor::CompareSceneRevisions(base, local, disk);
    Require(comparison && comparison->conflicts == 1, "Actual comparison fixture failed");
    observation = {1,
                   editor::SceneComparisonPhase::Ready,
                   current->Token(),
                   "Content/Main.scene",
                   "Captured comparison",
                   std::make_shared<const editor::SceneComparison>(*comparison)};
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
  }
  ~Fixture() {
    current.reset();
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  void Draw() {
    ui.SetSceneFileContext(current->Token(), "Content/Main.scene");
    ui.SetSceneComparisonStatus(observation, busy);
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Click(const std::optional<std::array<float, 2>> &point) {
    Require(point.has_value(), "Comparison control missing");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
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
};
void Run(float dpi) {
  Fixture f(dpi);
  const auto original = f.scene.PrepareSave();
  f.Click(Access::SceneFilePosition(f.ui, 0));
  f.Click(Access::SceneComparisonPosition(f.ui, 0));
  Require(f.ui.TakeSceneComparisonRequest() == f.current->Token() &&
              !f.ui.TakeSceneComparisonRequest(),
          "File menu comparison request lost scope/one-shot semantics");
  f.Draw();
  Require(Access::SceneComparisonRenderedRows(f.ui) > 0 &&
              Access::SceneComparisonStatus(f.ui).result->conflicts == 1 &&
              f.scene.MatchesPreparedSave(*original),
          "Actual semantic rows were not displayed read-only");
  const auto conflict = f.current->Save(f.current->Token());
  Require(conflict.status == editor::SceneFileStatus::NeedsOverwrite,
          "External-source conflict missing");
  editor::imgui::SceneFileRequest request{editor::imgui::SceneFileAction::SaveAs,
                                          f.current->Token(), "Content/Main.scene"};
  request.overwrite_token = conflict.overwrite_token;
  f.ui.RequestSceneOverwrite(request);
  f.Draw();
  f.Draw();
  f.Click(Access::SceneComparisonPosition(f.ui, 1));
  Require(f.ui.TakeSceneComparisonRequest() == f.current->Token(),
          "Overwrite compare action lost scope");
  f.Draw();
  Require(Access::SceneComparisonRenderedRows(f.ui) > 0 && !f.ui.TakeSceneFileRequest(),
          "Overwrite comparison emitted a destructive decision");
  f.busy = true;
  f.Draw();
  f.Click(Access::SceneComparisonPosition(f.ui, 2));
  Require(f.ui.TakeSceneComparisonCancelRequest() && !f.ui.TakeSceneComparisonCancelRequest(),
          "Comparison cancellation was not one-shot");
  f.busy = false;
  f.Draw();
  f.Click(Access::SceneComparisonPosition(f.ui, 3));
  f.Click(Access::SceneFilePosition(f.ui, 6));
  Require(!f.ui.TakeSceneFileRequest() && f.scene.MatchesPreparedSave(*original),
          "Comparison/back/cancel changed source intent or authoring content");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "Read-only project failed");
  f.active = &f.reader;
  f.Draw();
  f.Click(Access::SceneFilePosition(f.ui, 0));
  f.Click(Access::SceneComparisonPosition(f.ui, 0));
  Require(f.ui.TakeSceneComparisonRequest() == f.current->Token(),
          "Read-only comparison was blocked");
  f.Draw();
  auto unsafe = std::make_shared<editor::SceneComparison>();
  unsafe->rows.push_back(
      {"entities/1/name", {}, std::string(65537, 'x'), {}, editor::SceneComparisonChoice::Local});
  auto malformed = f.observation;
  malformed.result = unsafe;
  f.ui.SetSceneComparisonStatus(malformed, false);
  Require(Access::SceneComparisonStatus(f.ui).phase == editor::SceneComparisonPhase::Failed &&
              !Access::SceneComparisonStatus(f.ui).result,
          "Over-budget widget observation was retained");
  f.ui.SetSceneFileContext({f.writer.Project().id, f.scene.Generation() + 1}, "Content/Main.scene");
  f.ui.SetSceneComparisonStatus(f.observation, false);
  Require(!Access::SceneComparisonStatus(f.ui).result && !f.ui.TakeSceneComparisonRequest(),
          "Old document comparison resurrected after scope replacement");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Graphical semantic scene comparison passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
