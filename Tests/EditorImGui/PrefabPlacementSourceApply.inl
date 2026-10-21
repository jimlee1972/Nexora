// Actual widgets transfer owning scope/row choices; independent file/value checks follow SDK IO.
using Assets = editor::PrefabAssets;
const auto source_id = review->Source().asset;
const auto original = *Assets::Load(writer, source_id);
const auto scene_path = root / ".nexora/scenes/Main.scene";
const auto asset_path = root / ".nexora/prefabs" / (source_id.ToString() + ".nxprefab");
const auto source_bytes = original_files.at(asset_path.lexically_relative(root));
const auto read = [](const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(input), {}};
};
const auto unchanged = [&] {
  Require(scene.MatchesPreparedSave(local) && !scene.Dirty() && scene.Key(key.id) == key &&
              scene.PrefabPlacements().front().revision == 1 && Files(root) == original_files,
          "UI consent/rejection changed live history, binding, baseline or files");
};
const auto choose_name = [&] { click_point(Access::PrefabOverrideRowPosition(ui, name_index)); };
const auto restore = [&] {
  Require(ui.SetPrefabPlacementOverrideReport(report), "Current source report restore failed");
  draw();
  choose_name();
};
const auto open_source = [&] {
  click(5);
  Require(Access::PrefabOverridePosition(ui, 2).has_value() &&
              !ui.TakePrefabPlacementOverrideRequest(),
          "Actual source button skipped confirmation or failed to open it");
};
choose_name();
auto incompatible = report;
incompatible.can_apply_to_source = false;
Require(ui.SetPrefabPlacementOverrideReport(incompatible), "Default-off report rejected");
draw();
choose_name();
click(5);
Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest(),
        "Default-off source compatibility emitted consent");
unchanged();
// The producer hint cannot override incompatible owning row/source metadata.
const auto reject_report = [&](auto altered) {
  Require(ui.SetPrefabPlacementOverrideReport(std::move(altered)),
          "Incompatible source report rejected before display");
  draw();
  choose_name();
  click(5);
  Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest(),
          "Incompatible source metadata enabled publication consent");
  unchanged();
};
auto ahead = report;
ahead.published_revision = report.scope.retained.revision + 1;
reject_report(std::move(ahead));
auto nested = report;
nested.rows[name_index].scope.push_back(source_id);
reject_report(std::move(nested));
auto structural_report = report;
structural_report.rows[name_index].structural = true;
reject_report(std::move(structural_report));
restore();
open_source();
const auto confirmation = draw(true);
Require(confirmation.find("new source revision") != std::string::npos &&
            confirmation.find("Scene Undo") != std::string::npos &&
            confirmation.find(source_id.ToString()) != std::string::npos,
        "Source confirmation omitted destination or separate revision/history semantics");
click(3);
Require(!ui.TakePrefabPlacementOverrideRequest(), "Source Cancel emitted a write request");
unchanged();
// Replacing the report while a checkbox press is held must not select the new row generation.
restore();
choose_name(); // Clear it before the held press.
const auto held = Access::PrefabOverrideRowPosition(ui, name_index);
Require(held.has_value(), "Held source row absent");
Nexora::Window::WindowEvent pointer, button;
pointer.type = Nexora::Window::WindowEventType::Pointer;
pointer.value0 = static_cast<int>((*held)[0] * scale);
pointer.value1 = static_cast<int>((*held)[1] * scale);
button.type = Nexora::Window::WindowEventType::PointerButton;
button.value0 = 0;
button.value1 = 1;
ui.ProcessEvents(std::array{pointer, button});
draw();
++report.review;
Require(ui.SetPrefabPlacementOverrideReport(report), "Replacement source review rejected");
draw();
button.value1 = 0;
ui.ProcessEvents(std::array{button});
draw();
click(5);
Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest(),
        "Held checkbox release retargeted a replacement source review");
restore();
open_source();
focus.value0 = 0;
ui.ProcessEvents(std::array{focus});
draw();
Require(!ui.TakePrefabPlacementOverrideRequest(), "Focus loss retained source consent");
focus.value0 = 1;
ui.ProcessEvents(std::array{focus});
draw();
restore();
open_source();
authoring = false;
draw();
Require(!ui.TakePrefabPlacementOverrideRequest(), "Revoked authoring retained source consent");
authoring = true;
restore();
open_source();
read_allowed = false;
draw();
Require(!ui.TakePrefabPlacementOverrideRequest(), "Play/modal read gate retained source consent");
read_allowed = true;
workspace = &reader;
authoring = false;
restore();
click(5);
Require(!Access::PrefabOverridePosition(ui, 2) && !ui.TakePrefabPlacementOverrideRequest(),
        "Read-only source view emitted publication consent");
workspace = &writer;
authoring = true;
unchanged();
restore();
open_source();
// A fresh explicit source observation revokes previously captured review/consent.
Require(ui.SetPrefabPlacementSourceReport({report.scope, true, 1, report.scope.retained, 1}),
        "Explicit source observation rejected");
draw();
Require(!Access::PrefabOverrideReport(ui) && !ui.TakePrefabPlacementOverrideRequest(),
        "Refreshed source observation retained old publication consent");
restore();
open_source();
click(2);
auto source_request = ui.TakePrefabPlacementOverrideRequest();
Require(source_request && source_request->action == Action::ApplySelectedToSource &&
            source_request->scope == report.scope && source_request->review == report.review &&
            source_request->rows == std::vector{name_index} &&
            !ui.TakePrefabPlacementOverrideRequest(),
        "Source confirmation lost exact owning scope/rows or repeated its request");
{
  std::ofstream output(asset_path, std::ios::binary | std::ios::trunc);
  output << "corrupt source";
}
Require(!Overrides::ApplySelectedToSource(writer, scene, *review, source_request->rows, true) &&
            read(asset_path) == "corrupt source" && scene.MatchesPreparedSave(local),
        "Stale/corrupt source consumed confirmed UI authorization");
{
  std::ofstream output(asset_path, std::ios::binary | std::ios::trunc);
  output.write(source_bytes.data(), static_cast<std::streamsize>(source_bytes.size()));
}
restore();
open_source();
click(2);
source_request = ui.TakePrefabPlacementOverrideRequest();
Require(source_request.has_value(), "Allocation source request absent");
editor::PrefabPlacementSourceTestAccess::FailNextBeforePublish();
bool allocation{};
try {
  static_cast<void>(
      Overrides::ApplySelectedToSource(writer, scene, *review, source_request->rows, true));
} catch (const std::bad_alloc &) {
  allocation = true;
}
Require(allocation, "Source candidate allocation seam did not execute");
ui.SetPrefabPlacementOverrideError("Inspect the current source and archive before retrying.");
draw();
Require(!Access::PrefabOverrideReport(ui) && !ui.TakePrefabPlacementOverrideRequest(),
        "Unconfirmed source attempt retained blind retry consent");
unchanged();
restore();
open_source();
click(2);
source_request = ui.TakePrefabPlacementOverrideRequest();
Require(source_request.has_value(), "Final source request absent");
const auto published =
    Overrides::ApplySelectedToSource(writer, scene, *review, source_request->rows, true);
Require(published && published->revision == 2 && published->nodes == original.nodes &&
            Assets::LoadRevision(writer, {source_id, 1}) == original &&
            scene.MatchesPreparedSave(local) && !scene.Dirty() && !scene.Undo() && !scene.Redo() &&
            scene.PrefabPlacements().front().revision == 1 &&
            read(scene_path) == original_files.at(scene_path.lexically_relative(root)),
        "Confirmed source publication lost archive or changed Scene/history/binding");
Require(ui.SetPrefabPlacementSourceReport({report.scope, true, 2, report.scope.retained, 1}),
        "Confirmed source observation failed");
draw();
Require(!Access::PrefabOverrideReport(ui), "Confirmed source publication retained stale rows");
runtime::World source_world;
editor::SceneDocument source(source_world, source_world.LoadScene("Source reopen"));
Require(source.ReloadBytes(published->scene_bytes) && source.Name(key.id) == "Local instance" &&
            source.Transform(key.id)->x == 0 &&
            source.EulerAngles(key.id) == editor::EulerDegrees{0, 0, 0} &&
            source.OpaqueComponents(*source.Key(key.id))->front().data ==
                std::vector<std::uint8_t>({3, 255, 17}),
        "UI selected Name publication changed unselected source pose/Euler/unknown bytes");
const auto rebase = editor::PrefabPlacementRebase::Prepare(writer, scene, key);
const auto resolved = rebase ? editor::PrefabPlacementRebase::Resolve(*rebase, {}) : std::nullopt;
Require(resolved &&editor::PrefabPlacementRebase::Apply(writer, scene, *resolved, true) &&
            scene.PrefabPlacements().front().revision == 2 && scene.Dirty() && scene.Undo() &&
            scene.MatchesPreparedSave(local) && !scene.Dirty() && scene.Redo(),
        "Explicit rebase after GUI publication was not one separate Undo/Redo");
Require(files.BindCurrent(".nexora/scenes/Main.scene") && files.Save(files.Token()).Applied(),
        "Explicit post-rebase Scene Save failed");
runtime::World reopened_world;
editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Fresh scene"));
Require(reopened.Reload(scene_path) && !reopened.Dirty() &&
            reopened.PrefabPlacements().front().revision == 2 &&
            reopened.Name(key.id) == "Local instance" && reopened.Transform(key.id)->x == 4.5 &&
            reopened.EulerAngles(key.id) == editor::EulerDegrees{0, 720, 0} &&
            reopened.OpaqueComponents(*reopened.Key(key.id))->front().data ==
                std::vector<std::uint8_t>({3, 255, 19}) &&
            Assets::Load(writer, source_id) == published,
        "GUI publication/rebase/Save/reopen lost live values, unknown data or binding");
