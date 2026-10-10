#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/PrefabPlacementInspection.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::map<std::filesystem::path, std::string> Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file()) {
      std::ifstream input(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string{std::istreambuf_iterator<char>(input), {}});
    }
  return result;
}
void WriteFixture(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Prefab source inspection"), "Project fixture failed");
  runtime::World source_world, target_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  editor::SceneDocument scene(target_world, target_world.LoadScene("Main"));
  const auto node = source.Create("Retained source");
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{8810, ++serial}; };
  const auto asset = editor::PrefabAssets::Capture({8811, 1}, source, factory);
  Require(asset && editor::PrefabAssets::Publish(workspace, *asset), "Source publication failed");
  const auto prepared = editor::ProjectPrefabPlacement::Prepare(workspace, asset->id, scene);
  Require(prepared && editor::ProjectPrefabPlacement::Instantiate(workspace, scene, *prepared,
                                                                  {8812, 1}, true),
          "Bound scene fixture failed");
  editor::SceneFileSession files(workspace, scene);
  std::filesystem::create_directories(root / ".nexora/scenes");
  Require(files.SaveAs(files.Token(), ".nexora/scenes/Main.scene").Applied(),
          "Bound scene save failed");
  Require(source.Rename(*source.Key(node), "Published newer"), "Source advance fixture failed");
  const auto advanced = editor::PrefabAssets::Capture(asset->id, source, factory, &*asset);
  Require(advanced && editor::PrefabAssets::Publish(workspace, *advanced, &*asset),
          "Source advance publication failed");
}
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-placement-source-ui-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  WriteFixture(root);
  editor::ProjectWorkspace writer, reader;
  Require(writer.Open(root) && reader.Open(root, editor::ProjectAccess::ReadOnly),
          "Source inspection workspaces failed");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  Require(scene.Reload(root / ".nexora/scenes/Main.scene"), "Bound scene reopen failed");
  const auto key = *scene.Key(scene.Nodes().front().id);
  Require(scene.Select(std::array{key}) && scene.CopySelection(), "Selection fixture failed");
  const auto expected = *scene.PrepareSave();
  const auto original_files = Files(root);
  editor::SceneFileSession files(writer, scene);
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  ui.SetDisplay(1600 * scale, 1100 * scale, scale);
  Access::ConfigureSyntheticInput(ui);
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  auto *workspace = &writer;
  bool allowed = true;
  const auto draw = [&] {
    ui.SetSceneFileContext(files.Token(), ".nexora/scenes/Main.scene");
    ui.SetPrefabPlacementSourceContext(root, allowed);
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  const auto click = [&] {
    const auto point = Access::PrefabSourceInspectionPosition(ui);
    Require(point.has_value(), "Actual bound-node source control absent");
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
  allowed = false;
  draw();
  click();
  Require(!ui.TakePrefabPlacementSourceRequest(), "Blocked inspection emitted a request");
  allowed = true;
  draw();
  click();
  const auto request = ui.TakePrefabPlacementSourceRequest();
  Require(request && request->root == root && request->source == files.Token() &&
              request->node == key && request->instance == foundation::Uuid{8812, 1} &&
              request->retained == editor::PrefabRevisionReference{{8811, 1}, 1} &&
              request->scope.empty() && !ui.TakePrefabPlacementSourceRequest(),
          "Actual source control lost owning identity or was not consumed once");
  const auto inspected = editor::PrefabPlacementInspector::Inspect(writer, scene, key);
  Require(inspected && inspected->Resolved() && inspected->PublishedRevision() == 2,
          "Retained/current source inspection failed");
  editor::imgui::PrefabPlacementSourceReport report{
      *request, inspected->Resolved(), inspected->PublishedRevision(), inspected->ScopedSource(),
      inspected->MappedNodes()};
  Require(ui.SetPrefabPlacementSourceReport(report), "Captured source display rejected");
  auto invalid = report;
  invalid.mapped_nodes = 4097;
  Require(!ui.SetPrefabPlacementSourceReport(invalid), "Oversized source display admitted");
  invalid = report;
  ++invalid.scope.node.document_generation;
  Require(!ui.SetPrefabPlacementSourceReport(invalid), "Foreign source display admitted");
  draw();
  Require(Access::PrefabSourceInspectionReport(ui) &&
              Access::PrefabSourceInspectionReport(ui)->resolved,
          "Rejected source display replaced the captured result");
  workspace = &reader;
  draw();
  click();
  Require(ui.TakePrefabPlacementSourceRequest() && scene.MatchesPreparedSave(expected),
          "Read-only inspection was blocked or changed the scene");
  click();
  allowed = false;
  draw();
  Require(!ui.TakePrefabPlacementSourceRequest(),
          "Revoked interaction retained pending inspection");
  allowed = true;
  draw();
  click();
  Require(scene.Select(std::span<const runtime::Id>{}), "Selection clear failed");
  draw();
  Require(!ui.TakePrefabPlacementSourceRequest() && !Access::PrefabSourceInspectionReport(ui) &&
              !Access::PrefabSourceInspectionPosition(ui),
          "Selection replacement retained old inspection, result or control");
  Require(scene.Select(std::array{key}), "Selection restore failed");
  draw();
  click();
  ui.BeginFrame();
  static_cast<void>(ui.EndFrame());
  Require(!ui.TakePrefabPlacementSourceRequest(), "Hidden Inspector consumed an old click");
  Require(scene.MatchesPreparedSave(expected) && Files(root) == original_files && scene.Paste() &&
              scene.Name(scene.Selection().front()) == "Retained source Copy" && scene.Undo() &&
              scene.MatchesPreparedSave(expected),
          "Inspection changed files, content, history or clipboard");
  draw();
  click();
  ui.RequestCloseConfirmation();
  Require(!ui.TakePrefabPlacementSourceRequest(),
          "Close decision consumed a previously queued source inspection");
  draw();
  click();
  Require(!ui.TakePrefabPlacementSourceRequest() && scene.MatchesPreparedSave(expected),
          "Actual close modal admitted source inspection or mutated the scene");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-native-fixture") {
      WriteFixture(argv[2]);
      return 0;
    }
    Run(1);
    Run(2);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
