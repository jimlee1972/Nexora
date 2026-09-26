#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/SceneAuthoring.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

int Run() {
  using namespace nexora;
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() /
                    ("nexora-editor-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    fs::path root;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(root, ec);
    }
  } cleanup{root};

  editor::ProductShell shell;
  Require(editor::ProductShell::Panels().size() == 8 &&
              editor::ProductShell::IsStablePanelId("nexora.scene") &&
              !editor::ProductShell::IsStablePanelId("Scene"),
          "stable panel contract failed");
  Require(shell.RouteCommand("editor.scene.save") && shell.LastCommand() == "editor.scene.save" &&
              !shell.RouteCommand("game.save"),
          "command routing failed");

  editor::ProjectWorkspace project;
  std::string error;
  Require(project.Create(root, "Preview", &error), "project creation failed");
  const std::vector<std::string> documents{"Content/Main.scene", "Content/Hero.prefab"};
  Require(project.SaveWorkspace(documents, &error), "workspace save failed");
  editor::ProjectWorkspace reopened;
  Require(reopened.Open(root, &error) && reopened.Project().name == "Preview" &&
              reopened.OpenDocuments().size() == 2,
          "project open failed");
  {
    std::ofstream recovery(root / ".nexora/workspace.recovery", std::ios::trunc);
    recovery << "schema=1\ndocument=Content/Recovered.scene\n";
  }
  Require(reopened.HasRecoveryJournal() && reopened.RecoverWorkspace(&error) &&
              reopened.OpenDocuments().front() == "Content/Recovered.scene",
          "workspace recovery failed");
  Require(!reopened.HasRecoveryJournal(), "successful recovery must remove the journal");
  {
    std::ofstream recovery(root / ".nexora/workspace.recovery", std::ios::trunc);
    recovery << "schema=1\n";
  }
  Require(reopened.DiscardRecovery(&error) && !reopened.HasRecoveryJournal(),
          "workspace recovery discard failed");
  error.clear();
  Require(!reopened.DiscardRecovery(&error) && !error.empty(),
          "missing recovery journal did not report an actionable error");
  {
    std::ofstream recovery(root / ".nexora/workspace.recovery", std::ios::trunc);
    recovery << "schema=1\ncorrupt-entry\n";
  }
  error.clear();
  Require(!reopened.RecoverWorkspace(&error) && reopened.HasRecoveryJournal() && !error.empty(),
          "corrupt recovery journal was not preserved with an actionable error");
  Require(reopened.DiscardRecovery(&error), "corrupt recovery journal could not be discarded");
  const std::string layout = "[Window][Hierarchy###nexora.hierarchy]\nPos=0,0\n";
  Require(reopened.SaveEditorLayout(layout, &error) && reopened.LoadEditorLayout(&error) == layout,
          "editor layout round-trip failed");
  std::ofstream(root / ".nexora/editor-layout.ini", std::ios::trunc) << "schema=0\n" << layout;
  Require(reopened.LoadEditorLayout(&error) == layout,
          "legacy editor layout did not migrate through the current reader");
  Require(reopened.SaveEditorLayout(layout, &error), "migrated editor layout was not rewritten");
  {
    std::ifstream migrated(root / ".nexora/editor-layout.ini");
    std::string schema;
    Require(std::getline(migrated, schema) && schema == "schema=1",
            "migrated editor layout did not use the current schema");
  }
  std::ofstream(root / ".nexora/editor-layout.ini", std::ios::trunc) << "schema=999\n";
  Require(!reopened.LoadEditorLayout(&error) && !error.empty(),
          "unsupported editor layout schema was accepted");

  {
    std::ofstream(root / "Content/Hero.mesh") << "mesh";
    std::ofstream(root / "Content/Hero.material") << "material";
  }
  editor::AssetWorkspace assets;
  std::size_t progress{};
  Require(assets.ImportTree(root / "Content", {},
                            [&](std::size_t current, std::size_t) { progress = current; }) &&
              assets.Entries().size() == 2 && progress == 2,
          "asset import failed");
  Require(assets.Search("hero").size() == 2 && assets.Search({}, ".mesh").size() == 1 &&
              assets.Find(assets.Entries().front().id),
          "asset search failed");
  editor::AssetWorkspace cancelled;
  Require(cancelled.ImportTree(root / "Content", [] { return true; }) &&
              cancelled.Entries().front().state == editor::ImportState::Cancelled,
          "asset cancellation failed");

  const runtime::AssetUuid mesh_id{1, 1}, material_id{1, 2}, scene_id{1, 3};
  editor::ContentBrowserModel browser(7);
  const std::vector<editor::ContentItem> content{
      {mesh_id, "Content/Hero.mesh", "mesh", "mesh-v1", editor::ThumbnailState::Ready},
      {material_id, "Content/Hero.material", "material", "material-v1",
       editor::ThumbnailState::Loading},
      {scene_id, "Content/Levels/Main.scene", "scene", "scene-v1", editor::ThumbnailState::Failed}};
  Require(browser.Reset(content, 7) && browser.SetFolder("Content") &&
              browser.Breadcrumbs().size() == 1 && browser.Visible(0, 1).size() == 1 &&
              browser.Visible(1, 10).size() == 1,
          "virtualized content browser or breadcrumb state failed");
  browser.SetFilter("hero", "mesh");
  Require(browser.Visible(0, 10).size() == 1 && browser.Select(mesh_id) &&
              browser.Select(material_id, true) && browser.Selection().size() == 2 &&
              browser.Toggle(mesh_id) && !browser.IsSelected(mesh_id),
          "content filter or stable selection model failed");
  Require(browser.Rename(mesh_id, "Player.mesh", &error) &&
              browser.Find(mesh_id)->path == "Content/Player.mesh" && browser.Undo() &&
              browser.Find(mesh_id)->path == "Content/Hero.mesh",
          "transactional rename/undo failed");
  const std::vector<runtime::AssetUuid> move_ids{mesh_id, material_id};
  Require(browser.Move(move_ids, "Content/Characters", &error) &&
              browser.Find(mesh_id)->path == "Content/Characters/Hero.mesh",
          "transactional multi-asset move failed");
  const std::vector<runtime::AssetUuid> invalid_delete{mesh_id, {99, 99}};
  Require(!browser.Delete(invalid_delete, &error) && browser.Find(mesh_id),
          "failed delete did not roll back atomically");
  const std::vector<runtime::AssetUuid> delete_ids{material_id};
  Require(browser.Delete(delete_ids, &error) && !browser.Find(material_id) && browser.Undo() &&
              browser.Find(material_id),
          "transactional delete/undo failed");

  editor::AssetDragPayload drag{std::string(editor::AssetDragPayload::kType), 7, mesh_id};
  Require(editor::ValidateDrag(drag, browser, "Content/Props", true) ==
              editor::DragValidation::Valid,
          "valid typed drag payload was rejected");
  drag.project_generation = 6;
  Require(editor::ValidateDrag(drag, browser, "Content/Props", true) ==
              editor::DragValidation::StaleProject,
          "stale typed drag payload was accepted");
  drag.project_generation = 7;
  drag.type = "untyped";
  Require(editor::ValidateDrag(drag, browser, "Content/Props", true) ==
              editor::DragValidation::WrongType,
          "wrong drag payload type was accepted");

  editor::AssetDependencyGraph dependencies;
  Require(dependencies.Set(mesh_id, std::vector<runtime::AssetUuid>{material_id}) &&
              dependencies.Set(material_id, std::vector<runtime::AssetUuid>{scene_id}) &&
              dependencies.Forward(mesh_id) == std::vector<runtime::AssetUuid>{material_id} &&
              dependencies.Reverse(scene_id) == std::vector<runtime::AssetUuid>{material_id} &&
              dependencies.FindCycle().empty() &&
              dependencies.Set(scene_id, std::vector<runtime::AssetUuid>{mesh_id}) &&
              !dependencies.FindCycle().empty(),
          "dependency graph inspection or cycle detection failed");
  Require(dependencies.Set(scene_id, {}), "dependency cycle could not be removed");

  editor::ReimportTransaction reimport(7, mesh_id, "mesh-v1");
  Require(reimport.Stage(
              {7, mesh_id, "source-v2", "settings-v1", "mesh-v2", {material_id}, {}, false}) &&
              !reimport.Commit(8, dependencies) && reimport.Artifact() == "mesh-v1" &&
              reimport.Commit(7, dependencies) && reimport.Artifact() == "mesh-v2",
          "reimport staleness validation or atomic publish failed");
  editor::ReimportTransaction cyclic_reimport(7, scene_id, "scene-v1");
  Require(cyclic_reimport.Stage(
              {7, scene_id, "source-v2", "settings-v1", "scene-v2", {mesh_id}, {}, false}) &&
              !cyclic_reimport.Commit(7, dependencies) && cyclic_reimport.Artifact() == "scene-v1",
          "failed reimport did not preserve the previous artifact");

  using namespace std::chrono_literals;
  editor::WatcherDebouncer watcher(50ms);
  const auto now = std::chrono::steady_clock::now();
  watcher.Push({"Content/Hero.mesh", now, false});
  watcher.Push({"Content/Hero.mesh", now + 10ms, false});
  watcher.Push({"Content/Self.mesh", now, true});
  Require(watcher.Flush(now + 40ms).empty() &&
              watcher.Flush(now + 70ms) == std::vector<fs::path>{fs::path("Content/Hero.mesh")},
          "watcher debounce/coalescing or self-write suppression failed");

  editor::DirtyConflictModel conflicts;
  Require(!conflicts.Detect(mesh_id, "same", "same", true) &&
              conflicts.Detect(mesh_id, "editor", "disk", true) &&
              conflicts.Find(mesh_id)->choice == editor::DirtyConflictChoice::Pending &&
              conflicts.Resolve(mesh_id, editor::DirtyConflictChoice::Compare) &&
              conflicts.Find(mesh_id)->choice == editor::DirtyConflictChoice::Compare,
          "dirty external-change conflict resolution failed");

  runtime::World world;
  const auto scene = world.LoadScene("Main");
  Require(world.Activate(scene), "scene activation failed");
  editor::SceneDocument document(world, scene);
  const auto parent = document.Create("Parent");
  const auto child = document.Create("Child", parent);
  Require(document.Nodes().size() == 2 && document.Nodes()[1].parent == parent,
          "hierarchy view contract failed");
  Require(parent && child && document.Parent(child) == parent && !document.Reparent(parent, child),
          "hierarchy cycle policy failed");
  const std::vector<runtime::Id> selected{child};
  Require(document.Select(selected) && document.SetTransform(child, {1, 2, 3}) &&
              document.CopySelection() && document.Paste(),
          "scene editing failed");
  Require(document.Selection().size() == 1 &&
              document.Name(document.Selection().front()) == "Child Copy",
          "clipboard did not create a stable selection");
  Require(document.Undo(), "scene undo failed");
  const auto scene_path = root / "Content/Main.scene";
  Require(document.Save(scene_path), "scene atomic save failed");
  runtime::World loaded_world;
  const auto placeholder = loaded_world.LoadScene("Placeholder");
  editor::SceneDocument loaded(loaded_world, placeholder);
  Require(loaded.Reload(scene_path) && loaded.Name(child) == "Child", "scene reload failed");
  Require(document.Create("Bad\nName") == 0 && document.Create("Bad\rName") == 0,
          "a node name containing a newline must be rejected, since Save()/Reload() use a "
          "line-oriented format that a newline would silently corrupt");

  runtime::ReflectionRegistry reflection;
  const auto transform_type = runtime::HashTypeName("Transform");
  Require(reflection.Register({"Transform", transform_type, {{"x", 1, 0, sizeof(double)}}}),
          "reflection registration failed");
  std::unordered_map<runtime::Id, editor::InspectorValue> inspector_values{{parent, 1.0},
                                                                           {child, 2.0}};
  editor::InspectorPropertyAdapter inspector(reflection);
  const std::vector<runtime::Id> inspect_entities{parent, child};
  const std::vector<runtime::TypeId> inspect_components{transform_type};
  auto properties = inspector.Inspect(inspect_entities, inspect_components,
                                      [&](runtime::Id entity, runtime::TypeId, std::string_view) {
                                        return std::optional{inspector_values.at(entity)};
                                      });
  Require(properties.size() == 1 && properties.front().mixed && !properties.front().value,
          "mixed-value inspector state was not represented explicitly");
  Require(inspector.Apply(inspect_entities, properties.front(), editor::InspectorValue{3.0},
                          [&](runtime::Id entity, runtime::TypeId, std::string_view,
                              const editor::InspectorValue &value) {
                            inspector_values[entity] = value;
                            return true;
                          }) &&
              inspector_values[parent] == editor::InspectorValue{3.0} &&
              inspector_values[child] == editor::InspectorValue{3.0},
          "multi-selection inspector edit failed");

  editor::UnknownComponentStore unknown;
  Require(unknown.Set(child, {77, "Plugin.Component", {0, 1, 127, 255}}),
          "unknown component staging failed");
  Require(unknown.Set(parent, {78, "Plugin.Empty", {}}), "empty unknown component staging failed");
  editor::UnknownComponentStore unknown_reloaded;
  Require(unknown_reloaded.Deserialize(unknown.Serialize()) &&
              unknown_reloaded.Find(child).size() == 1 &&
              unknown_reloaded.Find(child).front().data ==
                  std::vector<std::uint8_t>({0, 1, 127, 255}) &&
              unknown_reloaded.Find(parent).front().data.empty(),
          "unknown component opaque data was not preserved byte-for-byte");
  const auto preserved_unknown = unknown_reloaded.Serialize();
  Require(!unknown_reloaded.Deserialize("corrupt") &&
              unknown_reloaded.Serialize() == preserved_unknown,
          "corrupt opaque component input replaced valid authoring state");

  std::unordered_map<runtime::Id, runtime::Transform> gizmo_transforms{{parent, {1, 1, 1}},
                                                                       {child, {2, 2, 2}}};
  editor::GizmoTransaction gizmo;
  Require(gizmo.Begin(inspect_entities,
                      [&](runtime::Id id, runtime::Transform &value) {
                        value = gizmo_transforms.at(id);
                        return true;
                      }),
          "gizmo transaction did not begin");
  const std::vector<runtime::Transform> dragged{{10, 10, 10}, {20, 20, 20}};
  Require(gizmo.Update(dragged,
                       [&](runtime::Id id, runtime::Transform value) {
                         gizmo_transforms[id] = value;
                         return true;
                       }) &&
              gizmo.Cancel([&](runtime::Id id, runtime::Transform value) {
                gizmo_transforms[id] = value;
                return true;
              }) &&
              gizmo_transforms[parent] == runtime::Transform{1, 1, 1} &&
              gizmo.State() == editor::GizmoState::Idle,
          "gizmo cancellation did not restore the initial transform snapshot");

  editor::AsyncPickingValidator picking;
  picking.Reset(4, 8);
  const auto stale_pick = picking.Request();
  const auto current_pick = picking.Request();
  Require(!picking.Accept(stale_pick) && picking.Accept(current_pick),
          "picking request ordering validation failed");
  picking.Reset(5, 8);
  Require(!picking.Accept(current_pick), "stale scene-generation pick was accepted");

  const editor::SceneCameraState camera{{1, 2, 3}, 0.25, 0.5, 12.0, true, 42.0};
  const auto camera_path = root / ".nexora/scene-camera.state";
  Require(editor::CameraPersistence::Save(camera_path, camera, &error) &&
              editor::CameraPersistence::Load(camera_path, &error) == camera,
          "scene camera persistence failed");

  editor::UndoRedoHistory history;
  int replay_value = 1000;
  for (int index = 0; index < 1000; ++index)
    Require(history.Push({[&] {
                            --replay_value;
                            return true;
                          },
                          [&] {
                            ++replay_value;
                            return true;
                          }}),
            "undo history push failed");
  for (int index = 0; index < 1000; ++index)
    Require(history.Undo(), "1,000-step undo replay failed");
  Require(replay_value == 0 && history.UndoDepth() == 0 && history.RedoDepth() == 1000,
          "undo replay produced incorrect state");
  for (int index = 0; index < 1000; ++index)
    Require(history.Redo(), "1,000-step redo replay failed");
  Require(replay_value == 1000 && history.UndoDepth() == 1000,
          "redo replay produced incorrect state");

  const auto nodes_before_corruption = loaded.Nodes().size();
  std::ofstream(scene_path, std::ios::trunc)
      << "NEXORA_EDITOR_SCENE 1\nnode 1 2 First\nnode 2 1 Second\nworld\ncorrupt";
  Require(!loaded.Reload(scene_path) && loaded.Nodes().size() == nodes_before_corruption &&
              loaded.Name(child) == "Child",
          "corrupt scene recovery did not preserve the loaded document state");

  runtime::PlaySession play(world);
  Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }) && play.Pause() &&
              play.Step() && play.Stop(),
          "PIE controls failed");

  editor::SpecializedToolRegistry tools;
  Require(
      tools.Register({"material", "Material Graph", editor::CapabilityState::ReadOnly,
                      "renderer graph editing is unavailable"}) &&
          tools.Register({"physics", "Physics Debug", editor::CapabilityState::Implemented, {}}) &&
          !tools.Register({"physics", "Duplicate", editor::CapabilityState::Implemented, {}}) &&
          tools.Find("material")->state == editor::CapabilityState::ReadOnly,
      "specialized tool capability policy failed");

  editor::BuildManifest manifest{
      1,
      {"linux-dev", "linux-x64", "Development", "cmake --build --preset linux-development"},
      {{"bin/game", "sha256:game", 42}}};
  const auto manifest_path = root / "build-manifest.json";
  Require(editor::BuildFrontend::Write(manifest, manifest_path, &error) &&
              fs::file_size(manifest_path) > 0,
          "build manifest failed");
  manifest.artifacts.push_back({"../escape", "bad", 1});
  Require(!editor::BuildFrontend::Validate(manifest, &error), "unsafe build artifact accepted");
#if defined(_WIN32)
  // std::filesystem::path only parses a drive letter as a root-name on Windows,
  // so this rejection is inherently platform-specific and cannot be exercised
  // by the Linux gate.
  manifest.artifacts.back() = {"C:/Windows/System32/evil.dll", "bad", 1};
  Require(!editor::BuildFrontend::Validate(manifest, &error),
          "a Windows drive-letter-rooted artifact path must be rejected even though it starts "
          "with neither '/' nor '\\\\', or it can escape the sandbox root it gets joined onto");
#endif

  editor::ProfileSession profile;
  Require(profile.Add({1, 2.0, 3.0, 100}) && profile.Add({2, 8.0, 4.0, 200}) &&
              !profile.Add({2, 1.0, 1.0, 1}) && profile.Peak()->frame == 2,
          "profile session failed");
  editor::VirtualHierarchy hierarchy(100000);
  Require(hierarchy.Visible(99990, 50) == std::pair<std::size_t, std::size_t>{99990, 10},
          "virtual hierarchy bounds failed");
  editor::ExtensionPolicy policy{true, {"Nexora"}};
  Require(policy.Allows("Nexora", true) && !policy.Allows("Nexora", false) &&
              !policy.Allows("Unknown", true),
          "extension signature policy failed");
  editor::TelemetryConsent telemetry;
  Require(!telemetry.Record("startup") && telemetry.Events().empty(), "telemetry was not opt-in");
  telemetry.Set(true);
  Require(telemetry.Record("startup") && telemetry.Events().size() == 1, "opt-in telemetry failed");
  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    return error.what()[0] == '\0' ? 0 : 1;
  }
}
