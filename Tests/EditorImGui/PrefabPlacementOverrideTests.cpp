#include "EditorImGuiTestAccess.h"
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
using Overrides = editor::PrefabPlacementOverrides;
using Action = editor::imgui::PrefabPlacementOverrideAction;
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
  Require(workspace.Create(root, "Prefab instance overrides"), "Project fixture failed");
  runtime::World source_world, target_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  editor::SceneDocument scene(target_world, target_world.LoadScene("Main"));
  const auto node = source.Create("Retained source");
  Require(source.SetOpaqueComponent(*source.Key(node), {781, "Unavailable.Provider", {3, 255, 17}}),
          "Unknown source fixture failed");
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{8820, ++serial}; };
  const auto asset = editor::PrefabAssets::Capture({8821, 1}, source, factory);
  Require(asset && editor::PrefabAssets::Publish(workspace, *asset), "Source publication failed");
  const auto prepared = editor::ProjectPrefabPlacement::Prepare(workspace, asset->id, scene);
  const auto placed = prepared ? editor::ProjectPrefabPlacement::Instantiate(
                                     workspace, scene, *prepared, {8822, 1}, true)
                               : std::nullopt;
  Require(placed && placed->size() == 1, "Bound fixture failed");
  const auto retained = scene.PrepareSave()->Bytes();
  const auto key = placed->front().target;
  auto transform = *scene.Transform(key.id);
  transform.x = 4.5;
  Require(scene.Rename(key, "Local instance") &&
              scene.SetOpaqueComponent(key, {781, "Unavailable.Provider", {3, 255, 19}}) &&
              scene.SetTransform(key.id, transform) && scene.SetEulerField(std::array{key}, 1, 720),
          "Local override fixture failed");
  editor::SceneFileSession files(workspace, scene);
  std::filesystem::create_directories(root / ".nexora/scenes");
  Require(files.SaveAs(files.Token(), ".nexora/scenes/Main.scene").Applied(), "Scene save failed");
  Require(source.Rename(*source.Key(node), "Published newer"), "Source advance failed");
  const auto advanced = editor::PrefabAssets::Capture(asset->id, source, factory, &*asset);
  Require(advanced && editor::PrefabAssets::Publish(workspace, *advanced, &*asset),
          "Source archive publication failed");
  return retained;
}
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-placement-override-ui-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  const auto retained = WriteFixture(root);
  editor::ProjectWorkspace writer, reader;
  Require(writer.Open(root) && reader.Open(root, editor::ProjectAccess::ReadOnly),
          "Workspaces failed");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  Require(scene.Reload(root / ".nexora/scenes/Main.scene"), "Reopen failed");
  const auto key = *scene.Key(scene.Nodes().front().id);
  Require(scene.Select(std::array{key}) && scene.CopySelection(), "Selection fixture failed");
  const auto local = *scene.PrepareSave();
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
    Require(point.has_value(), "Actual override control absent");
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
    click_point(Access::PrefabOverridePosition(ui, control));
  };
  click(0);
  auto request = ui.TakePrefabPlacementOverrideRequest();
  Require(request && request->action == Action::Review && request->scope.source == files.Token() &&
              request->scope.node == key && !ui.TakePrefabPlacementOverrideRequest(),
          "Review did not transfer one owning actual scope");
  const auto review = Overrides::Prepare(writer, scene, key);
  Require(review && review->Rows().size() >= 3 && scene.MatchesPreparedSave(local) &&
              Files(root) == original_files,
          "Core review mutated live state");
  editor::imgui::PrefabPlacementOverrideReport report{
      request->scope,
      1,
      review->PublishedRevision(),
      {review->Rows().begin(), review->Rows().end()}};
  Require(ui.SetPrefabPlacementOverrideReport(report), "Valid copied report rejected");
  auto invalid = report;
  invalid.review = 0;
  Require(!ui.SetPrefabPlacementOverrideReport(invalid) && Access::PrefabOverrideReport(ui),
          "Invalid report erased valid captured state");
  draw();
  const auto named = std::ranges::find(review->Rows(), std::string("name"),
                                       &editor::PrefabPlacementOverrideRow::field);
  Require(named != review->Rows().end(), "Selected Name fixture absent");
  const auto name_index = static_cast<std::size_t>(named - review->Rows().begin());
  click(4);
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Empty selection emitted targeted consent");
  click_point(Access::PrefabOverrideRowPosition(ui, name_index));
  click(4);
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Selected revert bypassed confirmation");
  click(3);
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.MatchesPreparedSave(local),
          "Selected Cancel mutated live properties");
  click(4);
  click(2);
  request = ui.TakePrefabPlacementOverrideRequest();
  Require(request && request->action == Action::RevertSelected && request->review == 1 &&
              request->rows == std::vector<std::size_t>{name_index} &&
              Overrides::RevertSelected(writer, scene, *review, request->rows, true) &&
              scene.Name(key.id) == "Retained source" && scene.Transform(key.id)->x == 4.5 &&
              scene.EulerAngles(key.id)->at(1) == 720 &&
              scene.OpaqueComponents(key)->front().data ==
                  std::vector<std::uint8_t>({3, 255, 19}) &&
              scene.Key(key.id) == key && scene.Selection().front() == key.id &&
              Files(root) == original_files,
          "Selected Name revert changed unselected values, identity or source files");
  const auto partial = *scene.PrepareSave();
  Require(scene.Undo() && scene.MatchesPreparedSave(local) && scene.Redo() &&
              scene.MatchesPreparedSave(partial) && scene.Undo() &&
              ui.SetPrefabPlacementOverrideReport(report),
          "Selected revert lost one Undo/Redo or refreshed report");
  draw();
  const auto rotation = std::ranges::find(review->Rows(), std::string("authoring/euler/y"),
                                          &editor::PrefabPlacementOverrideRow::field);
  const auto opaque = std::ranges::find(review->Rows(), std::string("opaque/781/payload_hex"),
                                        &editor::PrefabPlacementOverrideRow::field);
  Require(rotation != review->Rows().end() && opaque != review->Rows().end(),
          "Selected rotation/opaque fixture rows absent");
  std::vector<std::size_t> property_rows{
      static_cast<std::size_t>(rotation - review->Rows().begin()),
      static_cast<std::size_t>(opaque - review->Rows().begin())};
  std::ranges::sort(property_rows);
  for (const auto row : property_rows)
    click_point(Access::PrefabOverrideRowPosition(ui, row));
  click(4);
  click(2);
  request = ui.TakePrefabPlacementOverrideRequest();
  Require(request && request->action == Action::RevertSelected && request->rows == property_rows &&
              Overrides::RevertSelected(writer, scene, *review, request->rows, true) &&
              scene.Name(key.id) == "Local instance" && scene.Transform(key.id)->x == 4.5 &&
              scene.EulerAngles(key.id)->at(1) == 0 &&
              scene.OpaqueComponents(key)->front().data ==
                  std::vector<std::uint8_t>({3, 255, 17}) &&
              Files(root) == original_files,
          "Selected actual rotation/opaque controls changed unselected Name/position or files");
  const auto properties = *scene.PrepareSave();
  Require(scene.Undo() && scene.MatchesPreparedSave(local) && scene.Redo() &&
              scene.MatchesPreparedSave(properties) && scene.Undo() &&
              ui.SetPrefabPlacementOverrideReport(report),
          "Selected rotation/opaque did not preserve exact one-step history");
  draw();
  click(4);
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Refreshed report retained row selection");
  click_point(Access::PrefabOverrideRowPosition(ui, name_index));
  click(4);
  auto refreshed = report;
  refreshed.review = 3;
  Require(ui.SetPrefabPlacementOverrideReport(refreshed), "Refreshed serial rejected");
  draw();
  draw();
  Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest() &&
              scene.MatchesPreparedSave(local) && ui.SetPrefabPlacementOverrideReport(report),
          "Review refresh retained prior targeted consent");
  draw();
  click_point(Access::PrefabOverrideRowPosition(ui, name_index));
  click(4);
  focus.value0 = 0;
  ui.ProcessEvents(std::array{focus});
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  draw();
  draw();
  Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest(),
          "Blur/regain without a frame revived targeted consent");
  click(4);
  ui.SetPrefabPlacementSourceContext(root, false);
  ui.SetPrefabPlacementSourceContext(root, true);
  draw();
  draw();
  Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest() &&
              scene.MatchesPreparedSave(local),
          "Read permission loss/regain revived targeted consent");
  click(4);
  Require(scene.Select(std::span<const editor::SceneDocument::NodeKey>{}),
          "Selected consent scope-clear fixture failed");
  draw();
  draw();
  Require(!Access::PrefabOverrideReport(ui) && !ui.TakePrefabPlacementOverrideRequest() &&
              scene.Select(std::array{key}),
          "Selection replacement retained old targeted consent");
  draw();
  Require(ui.SetPrefabPlacementOverrideReport(report), "New selected scope report rejected");
  draw();
  click(0);
  request = ui.TakePrefabPlacementOverrideRequest();
  Require(request && request->action == Action::Review && scene.MatchesPreparedSave(local) &&
              ui.SetPrefabPlacementOverrideReport(report),
          "Cancelled selected-scope modal blocked new actual inspection");
  draw();
  click(1);
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.MatchesPreparedSave(local),
          "Revert bypassed explicit confirmation");
  click(3);
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Cancel authorized revert");
  draw();
  click(1);
  click(2);
  request = ui.TakePrefabPlacementOverrideRequest();
  Require(request && request->action == Action::Revert && request->review == 1 &&
              Overrides::Revert(writer, scene, *review, true) &&
              scene.PrepareSave()->Bytes() == retained && scene.Key(key.id) == key &&
              scene.Selection().front() == key.id && Files(root) == original_files &&
              scene.Undo() && scene.MatchesPreparedSave(local) && scene.Redo() &&
              scene.PrepareSave()->Bytes() == retained && scene.Undo(),
          "Confirmed revert lost exact retained bytes, one Undo/Redo or source files");
  Require(ui.SetPrefabPlacementOverrideReport(report), "Restored report rejected");
  workspace = &reader;
  authoring = false;
  draw();
  click_point(Access::PrefabOverrideRowPosition(ui, name_index));
  click(4);
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.MatchesPreparedSave(local),
          "Read-only selected rows emitted authoring consent");
  click(1);
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.MatchesPreparedSave(local),
          "Read-only control emitted authoring intent");
  click(0);
  request = ui.TakePrefabPlacementOverrideRequest();
  Require(request && request->action == Action::Review && Overrides::Prepare(reader, scene, key),
          "Read-only inspection was disabled");
  read_allowed = false;
  draw();
  click(0);
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Play/modal gate emitted review");
  read_allowed = authoring = true;
  workspace = &writer;
  Require(ui.SetPrefabPlacementOverrideReport(report), "Restored copied report failed");
  draw();
  click(1);
  authoring = false;
  draw();
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.MatchesPreparedSave(local),
          "Revoked authorization retained modal consent");
  authoring = true;
  Require(ui.SetPrefabPlacementOverrideReport(report) && scene.Rename(key, "Stale local"),
          "Stale target fixture failed");
  const auto stale = *scene.PrepareSave();
  Require(!Overrides::Revert(writer, scene, *review, true) && scene.MatchesPreparedSave(stale) &&
              scene.Undo(),
          "Stale owning review published state");
  const auto parent = scene.Create("Structural parent");
  Require(scene.Reparent(key.id, parent) && scene.Select(std::array{key}),
          "Structural override fixture failed");
  const auto structural = Overrides::Prepare(writer, scene, key);
  Require(structural &&
              std::ranges::any_of(structural->Rows(),
                                  &editor::PrefabPlacementOverrideRow::structural) &&
              ui.SetPrefabPlacementOverrideReport(
                  {report.scope,
                   2,
                   structural->PublishedRevision(),
                   {structural->Rows().begin(), structural->Rows().end()}}),
          "Structural review was not visible");
  draw();
  click(1);
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.Undo() && scene.Undo() &&
              scene.MatchesPreparedSave(local) && ui.SetPrefabPlacementOverrideReport(report),
          "Structural UI authorized a partial property revert");
  draw();
  click(0);
  ui.BeginFrame();
  static_cast<void>(ui.EndFrame());
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Hidden frame retained a queued review");
  draw();
  click(0);
  focus.value0 = 0;
  ui.ProcessEvents(std::array{focus});
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Focus revocation retained a queued review");
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  Require(scene.Select(std::span<const editor::SceneDocument::NodeKey>{}),
          "Clear selection failed");
  draw();
  Require(!Access::PrefabOverrideReport(ui) && !ui.TakePrefabPlacementOverrideRequest() &&
              scene.MatchesPreparedSave(local) && Files(root) == original_files && scene.Paste() &&
              scene.Undo() && scene.MatchesPreparedSave(local),
          "Scope replacement retained consent/report or lost history/clipboard/files");
  Require(scene.Select(std::array{key}), "Close fixture selection failed");
  draw();
  click(0);
  ui.RequestCloseConfirmation();
  Require(!ui.TakePrefabPlacementOverrideRequest(), "Close modal consumed a queued review");
  draw();
  click(0);
  Require(!ui.TakePrefabPlacementOverrideRequest() && scene.MatchesPreparedSave(local) &&
              Files(root) == original_files,
          "Actual close modal admitted override review or changed files");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 4 && (std::string_view(argv[1]) == "--write-native-fixture" ||
                      std::string_view(argv[1]) == "--write-native-selected-fixture")) {
      auto bytes = WriteFixture(argv[2]);
      if (std::string_view(argv[1]) == "--write-native-selected-fixture") {
        runtime::World world;
        editor::SceneDocument scene(world, world.LoadScene("Main"));
        Require(scene.Reload(std::filesystem::path(argv[2]) / ".nexora/scenes/Main.scene"),
                "Selected native expected fixture reload failed");
        const auto key = *scene.Key(scene.Nodes().front().id);
        editor::ProjectWorkspace workspace;
        Require(workspace.Open(argv[2]), "Selected native source observation failed");
        const auto review = Overrides::Prepare(workspace, scene, key);
        Require(review.has_value(), "Selected native logical review failed");
        std::ofstream rows(std::string(argv[3]) + ".rows");
        for (const auto field : {"name", "authoring/euler/y", "opaque/781/payload_hex"}) {
          const auto row = std::ranges::find(review->Rows(), std::string(field),
                                             &editor::PrefabPlacementOverrideRow::field);
          Require(row != review->Rows().end(), "Selected native expected field absent");
          rows << (row - review->Rows().begin()) << '\n';
        }
        Require(static_cast<bool>(rows), "Selected native external row oracle failed");
        const auto retained_snapshot = bytes;
        Require(scene.Rename(key, "Retained source"), "Selected expected Name failed");
        bytes = scene.PrepareSave()->Bytes();
        Require(scene.Undo(), "Selected independent property oracle reset failed");
        runtime::World expected_world;
        editor::SceneDocument expected(expected_world, expected_world.LoadScene("Main"));
        Require(expected.ReloadBytes(retained_snapshot), "Retained property oracle reload failed");
        const auto expected_key = *expected.Key(key.id);
        auto expected_transform = *expected.Transform(key.id);
        const auto local_transform = *scene.Transform(key.id);
        expected_transform.x = local_transform.x;
        expected_transform.y = local_transform.y;
        expected_transform.z = local_transform.z;
        Require(expected.Rename(expected_key, std::string(scene.Name(key.id))) &&
                    expected.SetTransform(key.id, expected_transform),
                "Selected independent unselected-property oracle failed");
        // Retained source has no authored Euler hint. Start from its exact metadata rather
        // than authoring a zero hint, which would add a record absent from the source.
        const auto properties = expected.PrepareSave()->Bytes();
        std::ofstream property_output(std::string(argv[3]) + ".properties", std::ios::binary);
        property_output.write(properties.data(), static_cast<std::streamsize>(properties.size()));
        Require(static_cast<bool>(property_output), "Selected native property oracle write failed");
      }
      std::ofstream output(argv[3], std::ios::binary);
      output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
      Require(static_cast<bool>(output), "Expected retained scene write failed");
    } else {
      Run(1);
      Run(2);
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
