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
    const auto entity = scene.Create("Base");
    auto bound = scene.PrepareSave()->Bytes();
    const std::string plain_header = "NEXORA_EDITOR_SCENE 2\n";
    const std::string bound_header = "NEXORA_EDITOR_SCENE 4\n";
    Require(bound.starts_with(plain_header), "Initial scene schema changed");
    bound.replace(0, plain_header.size(), bound_header);
    bound.insert(bound.find("world\n"), "prefab-placement 10000000-0000-0000-0000-000000000001 "
                                        "20000000-0000-0000-0000-000000000001 7\n"
                                        "prefab-node 10000000-0000-0000-0000-000000000001 0 "
                                        "30000000-0000-0000-0000-000000000001 " +
                                            std::to_string(entity) + '\n');
    Require(scene.ReloadBytes(bound), "Persistent binding fixture failed");
    current = std::make_unique<editor::SceneFileSession>(writer, scene);
    Require(entity && current->SaveAs(current->Token(), "Content/Main.scene").Applied(),
            "Base save failed");
    const auto base = scene.PrepareSave()->Bytes();
    Require(scene.Rename(*scene.Key(entity), "Local"), "Local fixture failed");
    const auto local = scene.PrepareSave()->Bytes();
    runtime::World remote_world;
    editor::SceneDocument remote(remote_world, remote_world.LoadScene("Remote"));
    Require(remote.ReloadBytes(base) && remote.Rename(*remote.Key(entity), "Disk"),
            "Remote fixture failed");
    auto disk = remote.PrepareSave()->Bytes();
    const auto binding = disk.find("prefab-placement ");
    Require(binding != disk.npos, "Remote binding was lost before the independent deletion");
    disk.erase(binding, disk.find("world\n", binding) - binding);
    disk.replace(0, bound_header.size(), plain_header);
    std::ofstream(root / "Content/Main.scene", std::ios::binary | std::ios::trunc) << disk;
    const auto comparison = editor::CompareSceneRevisions(base, local, disk);
    Require(comparison && comparison->conflicts == 1 && comparison->rows.size() == 7 &&
                std::ranges::count_if(comparison->rows,
                                      [](const auto &row) {
                                        return row.stable_path.starts_with("prefabs/") &&
                                               row.base && row.local && !row.remote &&
                                               row.choice == editor::SceneComparisonChoice::Remote;
                                      }) == 6,
            "Actual comparison fixture lost persistent binding deletion rows");
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
  Require(Access::SceneComparisonRenderedRows(f.ui) == 7 &&
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
