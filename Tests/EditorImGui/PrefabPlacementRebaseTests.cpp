#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include "TemporaryDirectoryCleanup.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Rebase = editor::PrefabPlacementRebase;
using Action = editor::imgui::PrefabPlacementRebaseAction;
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
std::string WriteFixture(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Prefab source rebase"), "Project failed");
  runtime::World source_world, target_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  editor::SceneDocument scene(target_world, target_world.LoadScene("Main"));
  const auto node = source.Create("Retained name");
  Require(source.SetOpaqueComponent(*source.Key(node), {791, "Unavailable.Provider", {0, 255, 17}}),
          "Opaque source failed");
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{8930, ++serial}; };
  const auto asset = editor::PrefabAssets::Capture({8931, 1}, source, factory);
  Require(asset && editor::PrefabAssets::Publish(workspace, *asset), "Publication failed");
  const auto prepared = editor::ProjectPrefabPlacement::Prepare(workspace, asset->id, scene);
  const auto placed = prepared ? editor::ProjectPrefabPlacement::Instantiate(
                                     workspace, scene, *prepared, {8932, 1}, true)
                               : std::nullopt;
  Require(placed && placed->size() == 1, "Placement failed");
  const auto key = placed->front().target;
  auto local = *scene.Transform(key.id);
  local.x = 4.5;
  Require(scene.Rename(key, "Local instance") && scene.SetTransform(key.id, local) &&
              scene.SetEulerField(std::array{key}, 1, 720) &&
              scene.SetOpaqueComponent(key, {791, "Unavailable.Provider", {0, 255, 19}}),
          "Local overrides failed");
  editor::SceneFileSession files(workspace, scene);
  std::filesystem::create_directories(root / ".nexora/scenes");
  Require(files.SaveAs(files.Token(), ".nexora/scenes/Main.scene").Applied(), "Scene save failed");
  auto published = *source.Transform(node);
  published.sx = 2.5;
  Require(source.Rename(*source.Key(node), "Published name") &&
              source.SetTransform(node, published),
          "Source change failed");
  const auto advanced = editor::PrefabAssets::Capture(asset->id, source, factory, &*asset);
  Require(advanced && editor::PrefabAssets::Publish(workspace, *advanced, &*asset),
          "Source advance failed");
  // Independent oracle: Keep Local Name; source-only Scale; preserve all other local groups.
  auto expected_transform = *scene.Transform(key.id);
  expected_transform.sx = 2.5;
  Require(scene.SetTransform(key.id, expected_transform), "Expected scale failed");
  auto expected = scene.PrepareSave()->Bytes();
  const auto prefix =
      "prefab-placement " + foundation::Uuid{8932, 1}.ToString() + ' ' + asset->id.ToString() + ' ';
  const auto old = prefix + "1\n";
  const auto position = expected.find(old);
  Require(position != expected.npos, "Expected placement revision absent");
  expected.replace(position, old.size(), prefix + "2\n");
  return expected;
}
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-placement-rebase-ui-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  auto expected = WriteFixture(root);
  editor::ProjectWorkspace writer, reader;
  Require(writer.Open(root) && reader.Open(root, editor::ProjectAccess::ReadOnly), "Open failed");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  Require(scene.Reload(root / ".nexora/scenes/Main.scene"), "Scene reload failed");
  const auto key = *scene.Key(scene.Nodes().front().id);
  // In-process latent turn: canonical bytes omit the hint while the pose differs.
  // Build the expected pose with independent public setters, without rebase code.
  runtime::World expected_world;
  editor::SceneDocument expected_scene(expected_world, expected_world.LoadScene("Expected"));
  Require(expected_scene.ReloadBytes(expected), "Independent expected reload failed");
  auto away = *scene.Transform(key.id);
  away.qx = away.qy = away.qw = 0;
  away.qz = 1;
  auto expected_away = *expected_scene.Transform(key.id);
  expected_away.qx = expected_away.qy = expected_away.qw = 0;
  expected_away.qz = 1;
  Require(scene.SetTransform(key.id, away) && expected_scene.SetTransform(key.id, expected_away),
          "Latent rotation fixture failed");
  expected = expected_scene.PrepareSave()->Bytes();
  Require(scene.Select(std::array{key}) && scene.CopySelection(), "Selection failed");
  const auto before = *scene.PrepareSave();
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
  bool read_allowed = true, authoring = true;
  const auto draw = [&] {
    ui.SetSceneFileContext(files.Token(), ".nexora/scenes/Main.scene");
    ui.SetPrefabPlacementSourceContext(root, read_allowed);
    ui.SetPrefabPlacementOverrideContext(authoring);
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  const auto click_point = [&](std::optional<std::array<float, 2>> point) {
    Require(point.has_value(), "Actual rebase control absent");
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
  const auto click = [&](std::size_t control) {
    click_point(Access::PrefabRebasePosition(ui, control));
  };
  click(0);
  auto request = ui.TakePrefabPlacementRebaseRequest();
  Require(request && request->action == Action::Review && request->scope.node == key &&
              request->scope.source == files.Token() && !ui.TakePrefabPlacementRebaseRequest(),
          "Scoped review request failed");
  const auto review = Rebase::Prepare(reader, scene, key);
  Require(review && review->Unresolved() == 1, "Actual Name conflict absent");
  auto report = editor::imgui::PrefabPlacementRebaseReport{
      request->scope, 1, 2, {review->Rows().begin(), review->Rows().end()}};
  Require(ui.SetPrefabPlacementRebaseReport(report), "Owning report rejected");
  draw();
  draw();
  const auto named =
      std::ranges::find_if(report.rows, [](const auto &row) { return row.group == "name"; });
  Require(named != report.rows.end() && named->conflict, "Scoped Name row absent");
  const auto row = static_cast<std::size_t>(named - report.rows.begin());
  click(1);
  Require(!ui.TakePrefabPlacementRebaseRequest(), "Unresolved rebase acquired consent");
  click_point(Access::PrefabRebaseChoicePosition(ui, row, false));
  click(1);
  click(3);
  Require(!ui.TakePrefabPlacementRebaseRequest() && scene.MatchesPreparedSave(before),
          "Canceled rebase wrote state");
  click(1);
  authoring = false;
  ui.SetPrefabPlacementOverrideContext(false);
  authoring = true;
  ui.SetPrefabPlacementOverrideContext(true);
  draw();
  Require(!ui.TakePrefabPlacementRebaseRequest(), "Revoked/regained authority retained consent");
  click(1);
  focus.value0 = 0;
  ui.ProcessEvents(std::array{focus});
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  draw();
  Require(!ui.TakePrefabPlacementRebaseRequest(), "Blur retained rebase consent");
  click(1);
  Require(scene.Rename(key, "Changed after review"), "Stale content fixture failed");
  const auto stale_content = *scene.PrepareSave();
  draw();
  click(2);
  const auto stale_request = ui.TakePrefabPlacementRebaseRequest();
  Require(stale_request && stale_request->action == Action::Apply,
          "Actual stale confirmation request absent");
  const auto stale_resolved = Rebase::Resolve(*review, stale_request->choices);
  Require(stale_resolved && !Rebase::Apply(writer, scene, *stale_resolved, true) &&
              scene.MatchesPreparedSave(stale_content) && scene.Undo() &&
              scene.MatchesPreparedSave(before),
          "Stale confirmed review overwrote later authoring/history");
  ui.SetPrefabPlacementRebaseError("Review changed; review again.");
  Require(!Access::PrefabRebaseReport(ui) && !ui.TakePrefabPlacementRebaseRequest() &&
              ui.SetPrefabPlacementRebaseReport(report),
          "Rejected action retained report/consent");
  draw();
  click_point(Access::PrefabRebaseChoicePosition(ui, row, false));
  click(1);
  click(2);
  request = ui.TakePrefabPlacementRebaseRequest();
  Require(request && request->action == Action::Apply && request->review == 1 &&
              request->choices.size() == 1 && request->choices.front().row == row &&
              request->choices.front().decision == editor::PrefabPlacementRebaseDecision::KeepLocal,
          "Captured choices changed at consent");
  const auto resolved = Rebase::Resolve(*review, request->choices);
  Require(resolved && Rebase::Apply(writer, scene, *resolved, true) &&
              scene.PrepareSave()->Bytes() == expected,
          "Confirmed rebase candidate failed");
  auto revived = *scene.Transform(key.id);
  revived.qx = revived.qy = revived.qz = 0;
  revived.qw = 1;
  Require(scene.SetTransform(key.id, revived) && scene.EulerAngles(key.id)->at(1) == 720 &&
              scene.Undo() && scene.PrepareSave()->Bytes() == expected,
          "Confirmed graphical rebase lost latent authored turns");
  Require(scene.PrepareSave()->Bytes() == expected &&
              scene.PrefabPlacements().front().revision == 2 && scene.Key(key.id) == key &&
              Files(root) == original_files && scene.Undo() && scene.MatchesPreparedSave(before) &&
              scene.Redo() && scene.PrepareSave()->Bytes() == expected && scene.Paste() &&
              scene.Undo() && scene.PrepareSave()->Bytes() == expected && scene.Undo() &&
              scene.MatchesPreparedSave(before),
          "Actual confirmed rebase lost exact groups/revision/history/clipboard/files");
  draw();
  report.review = 2;
  Require(ui.SetPrefabPlacementRebaseReport(report), "Refresh failed");
  draw();
  click_point(Access::PrefabRebaseChoicePosition(ui, row, true));
  click(1);
  report.review = 3;
  Require(ui.SetPrefabPlacementRebaseReport(report), "Consent refresh rejected");
  draw();
  Require(!ui.TakePrefabPlacementRebaseRequest(), "Review refresh retained prior consent");
  workspace = &reader;
  authoring = false;
  draw();
  click_point(Access::PrefabRebaseChoicePosition(ui, row, true));
  click(1);
  Require(!ui.TakePrefabPlacementRebaseRequest() && Files(root) == original_files &&
              scene.MatchesPreparedSave(before),
          "Readonly choices acquired write authority");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 4 && std::string_view(argv[1]) == "--write-native-fixture") {
      const auto bytes = WriteFixture(argv[2]);
      std::ofstream output(argv[3], std::ios::binary);
      output << bytes;
      Require(static_cast<bool>(output), "Expected native bytes write failed");
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
