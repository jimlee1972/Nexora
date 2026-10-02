#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SceneAuthoring.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
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

  std::string error;
  const std::vector<std::string> documents{"Content/Main.scene", "Content/Hero.prefab"};
  foundation::Uuid project_id;
  const auto recent_path = root / ".nexora/test-recent-projects";
  {
    editor::ProjectWorkspace project;
    Require(project.Create(root, "Preview", &error) && project.Writable() &&
                project.Project().schema_version == editor::ProjectDescriptor::kSchemaVersion &&
                !project.Project().id.IsNil(),
            "project creation did not establish the current writable descriptor");
    project_id = project.Project().id;
    Require(project.SaveWorkspace(documents, &error), "workspace save failed");

    editor::ProjectWorkspace contender;
    Require(!contender.Open(root, &error) &&
                error.starts_with("project is already open for writing"),
            "a second writer acquired the project lock");
    editor::ProjectWorkspace observer;
    Require(observer.Open(root, editor::ProjectAccess::ReadOnly, &error) && !observer.Writable() &&
                observer.Project().id == project_id && !observer.SaveWorkspace(documents, &error) &&
                !error.empty(),
            "read-only project access was not isolated from the writer");

    editor::RecentProjectStore recents;
    Require(recents.Open(recent_path, &error) && recents.Record(project, &error) &&
                recents.Record(observer, &error) && recents.Entries().size() == 1 &&
                recents.Entries().front().id == project_id,
            "recent-project persistence did not deduplicate the current project");
    editor::RecentProjectStore reopened_recents;
    Require(reopened_recents.Open(recent_path, &error) && reopened_recents.Entries().size() == 1 &&
                reopened_recents.Entries().front().root == project.Root(),
            "recent-project state did not survive reopen");
  }
  editor::ProjectWorkspace reopened;
  Require(reopened.Open(root, &error) && reopened.Project().name == "Preview" &&
              reopened.Project().id == project_id && reopened.OpenDocuments().size() == 2,
          "project open failed");

  const auto legacy_root = root / "LegacyProject";
  fs::create_directories(legacy_root / "Content");
  fs::create_directories(legacy_root / ".nexora");
  std::ofstream(legacy_root / "project.nexora") << "schema=1\nname=Legacy\n";
  std::ofstream(legacy_root / ".nexora/workspace") << "schema=1\n";
  foundation::Uuid legacy_id;
  {
    editor::ProjectWorkspace legacy_read_only;
    Require(legacy_read_only.Open(legacy_root, editor::ProjectAccess::ReadOnly, &error) &&
                legacy_read_only.UpgradeState() == editor::ProjectUpgradeState::Required &&
                legacy_read_only.Project().schema_version == 1 &&
                !legacy_read_only.Project().id.IsNil(),
            "legacy read-only project did not expose the required upgrade");
    legacy_id = legacy_read_only.Project().id;
    std::ifstream descriptor(legacy_root / "project.nexora");
    std::string schema;
    Require(std::getline(descriptor, schema) && schema == "schema=1",
            "read-only project open changed the legacy descriptor");
  }
  {
    editor::ProjectWorkspace legacy_writer;
    Require(legacy_writer.Open(legacy_root, &error) &&
                legacy_writer.UpgradeState() == editor::ProjectUpgradeState::Applied &&
                legacy_writer.Project().schema_version ==
                    editor::ProjectDescriptor::kSchemaVersion &&
                legacy_writer.Project().id == legacy_id,
            "legacy project was not atomically upgraded with stable identity");
    std::ifstream descriptor(legacy_root / "project.nexora");
    std::string schema;
    Require(std::getline(descriptor, schema) && schema == "schema=2",
            "upgraded project descriptor was not persisted");
  }
  {
    // A schema-1 project that predates the .nexora directory must still open and upgrade.
    const auto bare_root = root / "BareLegacyProject";
    fs::create_directories(bare_root / "Content");
    std::ofstream(bare_root / "project.nexora") << "schema=1\nname=Bare\n";
    editor::ProjectWorkspace bare_writer;
    Require(bare_writer.Open(bare_root, &error) &&
                bare_writer.UpgradeState() == editor::ProjectUpgradeState::Applied,
            "legacy project without a .nexora directory could not be opened for writing");
    // A directory with no project descriptor must be rejected without being modified.
    const auto empty_root = root / "NotAProject";
    fs::create_directories(empty_root);
    editor::ProjectWorkspace not_a_project;
    Require(!not_a_project.Open(empty_root, &error) && !fs::exists(empty_root / ".nexora"),
            "opening a non-project directory created project state");
    editor::ProjectWorkspace nul_name;
    Require(!nul_name.Create(root / "NulName", std::string("a\0b", 3), &error),
            "project name containing NUL was accepted");
  }
  const auto invalid_legacy_root = root / "InvalidLegacyProject";
  fs::create_directories(invalid_legacy_root / "Content");
  fs::create_directories(invalid_legacy_root / ".nexora");
  std::ofstream(invalid_legacy_root / "project.nexora") << "schema=1\nname=Invalid Legacy\n";
  std::ofstream(invalid_legacy_root / ".nexora/workspace") << "schema=999\n";
  editor::ProjectWorkspace invalid_legacy;
  Require(!invalid_legacy.Open(invalid_legacy_root, &error) && !error.empty(),
          "legacy project with an invalid workspace was opened");
  {
    std::ifstream descriptor(invalid_legacy_root / "project.nexora");
    std::string schema;
    Require(std::getline(descriptor, schema) && schema == "schema=1",
            "failed project preflight changed the legacy descriptor");
  }
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
  Require(assets.ImportTree(
              root / "Content", {}, [&](std::size_t current, std::size_t) { progress = current; },
              editor::AssetIdentityMode::PersistentReadWrite, &error) &&
              assets.Entries().size() == 2 && progress == 2,
          "asset import failed");
  Require(fs::is_regular_file(root / "Content/Hero.mesh.meta") &&
              fs::is_regular_file(root / "Content/Hero.material.meta"),
          "persistent asset identity sidecars were not created");
  Require(assets.Search("hero").size() == 2 && assets.Search({}, ".mesh").size() == 1 &&
              assets.Find(assets.Entries().front().id),
          "asset search failed");

  const auto mesh_entry = std::ranges::find(assets.Entries(), std::string("Hero.mesh"),
                                            &editor::AssetEntry::relative_path);
  Require(mesh_entry != assets.Entries().end(), "indexed mesh entry is missing");
  editor::ProjectContentSession content_session;
  Require(content_session.Open(reopened, assets, 11, true, &error) &&
              content_session.Browser().VisibleCount() == 2 && content_session.Writable(),
          "project content session did not bind the deterministic index");
  const auto indexed_mesh = mesh_entry->id;
  Require(content_session.Rename(indexed_mesh, "Player.mesh", &error) &&
              fs::is_regular_file(root / "Content/Player.mesh") &&
              fs::is_regular_file(root / "Content/Player.mesh.meta") &&
              !fs::exists(root / "Content/Hero.mesh") && content_session.CanUndo() &&
              !fs::exists(root / "Content/Hero.mesh.meta") && content_session.Undo(&error) &&
              fs::is_regular_file(root / "Content/Hero.mesh") &&
              fs::is_regular_file(root / "Content/Hero.mesh.meta"),
          "filesystem-backed content rename/undo failed");
  fs::create_directories(root / "Content/Characters");
  const std::array one_mesh{indexed_mesh};
  editor::AssetDragPayload content_drag{std::string(editor::AssetDragPayload::kType), 11,
                                        indexed_mesh};
  Require(content_session.Move(content_drag, "Content/Characters", &error) &&
              fs::is_regular_file(root / "Content/Characters/Hero.mesh") &&
              fs::is_regular_file(root / "Content/Characters/Hero.mesh.meta"),
          "generation-safe content drag did not move the source file");
  editor::AssetWorkspace moved_assets;
  Require(moved_assets.ImportTree(root / "Content", {}, {},
                                  editor::AssetIdentityMode::PersistentReadOnly, &error) &&
              moved_assets.Find(indexed_mesh) &&
              moved_assets.Find(indexed_mesh)->relative_path == "Characters/Hero.mesh",
          "moved asset UUID did not survive an immediate read-only reopen");
  const auto moved_artifact_before = content_session.Browser().Find(indexed_mesh)->artifact_hash;
  std::ofstream(root / "Content/Characters/Hero.mesh", std::ios::trunc) << "mesh-v2";
  Require(content_session.Reimport(indexed_mesh, &error) &&
              content_session.Browser().Find(indexed_mesh)->artifact_hash != moved_artifact_before,
          "reimport after a move did not publish the new artifact");
  const auto moved_artifact_after = content_session.Browser().Find(indexed_mesh)->artifact_hash;
  Require(content_session.Undo(&error) && fs::is_regular_file(root / "Content/Hero.mesh") &&
              fs::is_regular_file(root / "Content/Hero.mesh.meta") &&
              content_session.Browser().Find(indexed_mesh)->artifact_hash == moved_artifact_after,
          "filesystem-backed content move/reimport/undo lost current artifact metadata");
  content_drag.project_generation = 10;
  Require(!content_session.Move(content_drag, "Content/Characters", &error) &&
              fs::is_regular_file(root / "Content/Hero.mesh"),
          "stale graphical asset drag payload changed project content");
  std::ofstream(root / "Content/Characters/Hero.mesh") << "occupied";
  Require(!content_session.Move(one_mesh, "Content/Characters", &error) &&
              fs::is_regular_file(root / "Content/Hero.mesh") &&
              content_session.Browser().Find(indexed_mesh)->path == "Content/Hero.mesh",
          "failed content move did not preserve the source and model");
  fs::remove(root / "Content/Characters/Hero.mesh");
  const auto artifact_before = content_session.Browser().Find(indexed_mesh)->artifact_hash;
  std::ofstream(root / "Content/Hero.mesh", std::ios::trunc) << "mesh-v3";
  Require(content_session.Reimport(indexed_mesh, &error) &&
              content_session.Browser().Find(indexed_mesh)->artifact_hash != artifact_before,
          "content reimport did not publish the updated artifact hash");

  core::JobSystem import_jobs{1};
  import_jobs.Start();
  editor::AssetImportQueue imports{import_jobs, 2, 2};
  const auto wait_for_result = [](auto &&poll, const char *message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
      if (poll())
        return;
      std::this_thread::yield();
    }
    throw std::runtime_error(message);
  };

  const auto workspace_import =
      imports.Start({21, root / "Content", editor::AssetIdentityMode::PersistentReadOnly}, &error);
  Require(workspace_import != 0, "background workspace import did not start");
  std::optional<editor::ImportOperationResult> workspace_result;
  wait_for_result(
      [&] {
        workspace_result = imports.TakeResult(workspace_import);
        return workspace_result.has_value();
      },
      "background workspace import did not finish");
  Require(workspace_result->snapshot.state == editor::ImportOperationState::AwaitingPublish &&
              workspace_result->workspace && workspace_result->workspace->Entries().size() == 2 &&
              workspace_result->snapshot.progress.size() <= 2 &&
              workspace_result->snapshot.dropped_progress > 0,
          "background workspace import was not deterministic or progress history was unbounded");

  const auto failing_source = root / "failing.asset";
  std::ofstream(failing_source) << "staged";
  std::atomic_bool release_failure{false};
  const auto failure_blocker =
      import_jobs.Submit({[&release_failure](const core::CancellationToken &) {
                            while (!release_failure.load(std::memory_order_acquire))
                              std::this_thread::yield();
                          },
                          core::JobPriority::High,
                          {},
                          "Editor import failure barrier"});
  const runtime::AssetUuid failing_asset{81, 82};
  const auto failing_import =
      imports.Start({21, failing_asset, failing_source, "last-good", "default-v1", {}}, &error);
  Require(failing_import != 0 && fs::remove(failing_source),
          "background failure operation could not be staged");
  release_failure.store(true, std::memory_order_release);
  import_jobs.Wait(failure_blocker);
  std::optional<editor::ImportOperationResult> failure_result;
  wait_for_result(
      [&] {
        failure_result = imports.TakeResult(failing_import);
        return failure_result.has_value();
      },
      "background failure operation did not finish");
  Require(failure_result->snapshot.state == editor::ImportOperationState::Failed &&
              !failure_result->reimport && !failure_result->snapshot.diagnostics.empty() &&
              failure_result->snapshot.diagnostics.back().code == "reimport.read_failed" &&
              failure_result->snapshot.diagnostics.back().asset == failing_asset,
          "worker failure produced staging data or lacked an actionable structured diagnostic");

  const auto shutdown_source = root / "shutdown.asset";
  std::ofstream(shutdown_source) << std::string(1024 * 1024, 'x');
  editor::AssetImportQueue shutdown_imports{import_jobs};
  const auto shutdown_operation = shutdown_imports.Start(
      {22, runtime::AssetUuid{91, 92}, shutdown_source, "last-good", "default-v1", {}}, &error);
  Require(shutdown_operation != 0, "shutdown reimport operation did not start");
  shutdown_imports.Shutdown();
  Require(!shutdown_imports.Snapshot(shutdown_operation),
          "import queue shutdown retained a worker or staged completion");

  const auto asynchronous_before = content_session.Browser().Find(indexed_mesh)->artifact_hash;
  std::ofstream(root / "Content/Hero.mesh", std::ios::trunc) << "mesh-async";
  Require(content_session.BeginReimport(imports, indexed_mesh, &error),
          "background reimport did not start");
  wait_for_result([&] { return content_session.PollReimport(&error); },
                  "background reimport did not finish");
  const auto asynchronous_status = content_session.ReimportStatus();
  Require(asynchronous_status &&
              asynchronous_status->state == editor::ImportOperationState::Succeeded &&
              content_session.Browser().Find(indexed_mesh)->artifact_hash != asynchronous_before &&
              asynchronous_status->diagnostics.back().code == "reimport.succeeded",
          "authoring-thread reimport publication or structured success diagnostic failed");

  std::atomic_bool release_blocker{false};
  const auto blocker =
      import_jobs.Submit({[&release_blocker](const core::CancellationToken &) {
                            while (!release_blocker.load(std::memory_order_acquire))
                              std::this_thread::yield();
                          },
                          core::JobPriority::High,
                          {},
                          "Editor import cancellation barrier"});
  const auto cancellation_artifact = content_session.Browser().Find(indexed_mesh)->artifact_hash;
  std::ofstream(root / "Content/Hero.mesh", std::ios::trunc) << "mesh-cancelled";
  Require(content_session.BeginReimport(imports, indexed_mesh, &error) &&
              content_session.CancelReimport(),
          "queued background reimport was not cancellable");
  release_blocker.store(true, std::memory_order_release);
  import_jobs.Wait(blocker);
  wait_for_result([&] { return content_session.PollReimport(&error); },
                  "cancelled background reimport did not drain");
  const auto cancelled_status = content_session.ReimportStatus();
  Require(cancelled_status && cancelled_status->state == editor::ImportOperationState::Cancelled &&
              !cancelled_status->diagnostics.empty() &&
              cancelled_status->diagnostics.back().code == "import.cancelled" &&
              content_session.Browser().Find(indexed_mesh)->artifact_hash == cancellation_artifact,
          "cancelled reimport replaced the old artifact or lacked a structured diagnostic");

  std::ofstream(root / "Content/Hero.mesh", std::ios::trunc) << "mesh-staged";
  Require(content_session.BeginReimport(imports, indexed_mesh, &error),
          "stale-completion reimport did not start");
  std::ofstream(root / "Content/Hero.mesh", std::ios::trunc) << "mesh-newer";
  Require(content_session.Reimport(indexed_mesh, &error),
          "foreground revision change for stale-completion test failed");
  const auto newer_artifact = content_session.Browser().Find(indexed_mesh)->artifact_hash;
  wait_for_result([&] { return content_session.PollReimport(&error); },
                  "stale background reimport did not finish");
  const auto stale_status = content_session.ReimportStatus();
  Require(stale_status && stale_status->state == editor::ImportOperationState::Stale &&
              stale_status->diagnostics.back().code == "reimport.stale" &&
              content_session.Browser().Find(indexed_mesh)->artifact_hash == newer_artifact,
          "stale reimport completion replaced the current artifact");

  Require(content_session.Delete(one_mesh, &error) && !fs::exists(root / "Content/Hero.mesh") &&
              !fs::exists(root / "Content/Hero.mesh.meta") &&
              content_session.Browser().Find(indexed_mesh) == nullptr &&
              content_session.Undo(&error) && fs::is_regular_file(root / "Content/Hero.mesh") &&
              fs::is_regular_file(root / "Content/Hero.mesh.meta") &&
              content_session.Browser().Find(indexed_mesh),
          "recoverable content delete/undo failed");
  editor::AssetWorkspace reopened_assets;
  Require(reopened_assets.ImportTree(root / "Content", {}, {},
                                     editor::AssetIdentityMode::PersistentReadOnly, &error) &&
              reopened_assets.Find(indexed_mesh) &&
              reopened_assets.Find(indexed_mesh)->relative_path == "Hero.mesh" &&
              reopened_assets.Find(indexed_mesh)->artifact_hash ==
                  content_session.Browser().Find(indexed_mesh)->artifact_hash,
          "asset UUID or artifact identity did not survive move/reimport/undo/reopen");
  editor::ProjectWorkspace read_only_workspace;
  Require(read_only_workspace.Open(root, editor::ProjectAccess::ReadOnly, &error),
          "read-only observer could not open the writer-owned project");
  editor::ProjectContentSession invalid_writable_content;
  Require(!invalid_writable_content.Open(read_only_workspace, reopened_assets, 12, true, &error) &&
              !error.empty(),
          "read-only workspace opened writable project content");
  editor::ProjectContentSession read_only_content;
  Require(read_only_content.Open(read_only_workspace, reopened_assets, 12, false, &error) &&
              !read_only_content.Rename(indexed_mesh, "Blocked.mesh", &error) &&
              fs::is_regular_file(root / "Content/Hero.mesh") && !error.empty(),
          "read-only project content accepted a mutation");
  const auto identity_path = root / "Content/Hero.mesh.meta";
  std::ifstream identity_input(identity_path, std::ios::binary);
  const std::string valid_identity(std::istreambuf_iterator<char>(identity_input), {});
  std::ofstream(identity_path, std::ios::trunc) << "schema=999\n";
  Require(!reopened_assets.ImportTree(root / "Content", {}, {},
                                      editor::AssetIdentityMode::PersistentReadOnly, &error) &&
              reopened_assets.Find(indexed_mesh) && !error.empty(),
          "corrupt asset identity replaced the last good index or lacked a diagnostic");
  std::ofstream(identity_path, std::ios::trunc) << valid_identity;
  const auto material_identity_path = root / "Content/Hero.material.meta";
  std::ifstream material_identity_input(material_identity_path, std::ios::binary);
  const std::string valid_material_identity(std::istreambuf_iterator<char>(material_identity_input),
                                            {});
  std::ofstream(material_identity_path, std::ios::trunc)
      << "schema=1\nuuid=" << indexed_mesh.ToString() << "\ntype=.material\n";
  Require(!reopened_assets.ImportTree(root / "Content", {}, {},
                                      editor::AssetIdentityMode::PersistentReadOnly, &error) &&
              reopened_assets.Find(indexed_mesh) && !error.empty(),
          "duplicate asset UUID replaced the last good index or lacked a diagnostic");
  std::ofstream(material_identity_path, std::ios::trunc) << valid_material_identity;
  const auto missing_identity = root / "Content/MissingIdentity.mesh";
  std::ofstream(missing_identity) << "mesh";
  editor::AssetWorkspace missing_identity_assets;
  Require(!missing_identity_assets.ImportTree(
              root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadOnly, &error) &&
              !error.empty() && !fs::exists(root / "Content/MissingIdentity.mesh.meta"),
          "read-only persistent indexing created or accepted a missing identity sidecar");
  fs::remove(missing_identity);
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
              browser.Breadcrumbs().size() == 1 && browser.VisibleCount() == 2 &&
              browser.ChildFolders().size() == 1 &&
              browser.ChildFolders().front().path == "Content/Levels" &&
              browser.Visible(0, 1).size() == 1 && browser.Visible(1, 10).size() == 1,
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
  Require(browser.Select(material_id) && browser.Delete(delete_ids, &error) &&
              !browser.Find(material_id) && !browser.IsSelected(material_id) && browser.Undo() &&
              browser.Find(material_id) && browser.IsSelected(material_id),
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
  const auto parent_key = *document.Key(parent);
  const auto child_key = *document.Key(child);
  Require(parent_key == document.Nodes()[0].Key() && child_key == document.Nodes()[1].Key() &&
              parent_key.document_generation == document.Generation() &&
              parent_key.entity_generation != child_key.entity_generation,
          "Hierarchy keys must carry document and entity generations");
  Require(document.Rename(parent_key, "Renamed Parent") &&
              document.Name(parent) == "Renamed Parent" && document.Undo() &&
              document.Name(parent) == "Parent" && !document.Rename(parent_key, "Bad\nName") &&
              !document.Rename(parent_key, "Bad\rName"),
          "generation-safe Hierarchy rename/undo contract failed");
  Require(parent && child && document.Parent(child) == parent && !document.Reparent(parent, child),
          "hierarchy cycle policy failed");
  // A Hierarchy drag places a node among its new siblings; Nodes() lists the runtime order.
  const auto sibling = document.Create("Sibling", parent);
  Require(document.Move(sibling, parent, 0) && document.Nodes()[1].id == sibling &&
              document.Nodes()[2].id == child && document.Undo() &&
              document.Nodes()[1].id == child && document.Undo() && document.Nodes().size() == 2,
          "Move and Nodes() must follow the runtime sibling order");
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
  const auto placeholder_generation = loaded.Generation();
  Require(loaded.Reload(scene_path) && loaded.Generation() != placeholder_generation &&
              loaded.Name(child) == "Child" && loaded.Parent(child) == parent &&
              loaded_world.Parent(child) == parent,
          "scene reload failed");
  const auto loaded_child_key = *loaded.Key(child);
  const editor::SceneDocument::NodeKey stale_document_key{
      loaded_child_key.id, loaded_child_key.entity_generation, placeholder_generation};
  const editor::SceneDocument::NodeKey stale_entity_key{loaded_child_key.id,
                                                        loaded_child_key.entity_generation + 1,
                                                        loaded_child_key.document_generation};
  const std::array stale_selection{stale_document_key};
  Require(!loaded.Rename(stale_document_key, "Stale") &&
              !loaded.Rename(stale_entity_key, "Stale") && !loaded.Select(stale_selection) &&
              !loaded.Move(stale_document_key, std::nullopt, 0) && loaded.Name(child) == "Child",
          "stale Hierarchy keys were accepted after document/entity generation changed");

  // Before snapshot version 3 the hierarchy lived only in the node lines and every transform was
  // a world pose; migrating must parent the entities without moving them.
  const auto legacy_path = root / "Content/Legacy.scene";
  std::ofstream(legacy_path, std::ios::binary | std::ios::trunc)
      << "NEXORA_EDITOR_SCENE 1\nnode 5 0 Parent\nnode 6 5 Child\nworld\n"
         "NEXORA_SCENE 2 \"Legacy\" 0 2\n"
         "5 10 0 0 0 0.70710678118654752 0 0.70710678118654752 2 2 2 0 0 0 60 0.1 1000 1 0 0\n"
         "6 10 0 -2 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  runtime::World legacy_world;
  editor::SceneDocument legacy(legacy_world, legacy_world.LoadScene("Placeholder"));
  Require(legacy.Reload(legacy_path) && legacy.Parent(6) == runtime::Id{5} &&
              legacy_world.Parent(6) == runtime::Id{5},
          "a legacy editor scene must migrate its node-line hierarchy into the runtime");
  const auto migrated = *legacy_world.WorldTransform(6);
  Require(std::abs(migrated.x - 10.0) < 1e-9 && std::abs(migrated.z + 2.0) < 1e-9 &&
              std::abs(migrated.sx - 1.0) < 1e-9 &&
              std::abs(legacy_world.FindEntity(6)->transform.x - 1.0) < 1e-9,
          "migrating a legacy hierarchy must keep every world pose");
  // A migration that cannot apply (a parent id that is a node but not an entity of this world)
  // must fail without leaving a half-loaded scene behind.
  const auto broken_path = root / "Content/Broken.scene";
  std::ofstream(broken_path, std::ios::binary | std::ios::trunc)
      << "NEXORA_EDITOR_SCENE 1\nnode 7 0 Ghost\nnode 8 7 Child\nworld\n"
         "NEXORA_SCENE 2 \"Broken\" 0 1\n"
         "8 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  runtime::World broken_world;
  editor::SceneDocument broken(broken_world, broken_world.LoadScene("Placeholder"));
  Require(!broken.Reload(broken_path) && broken_world.FindEntity(8) == nullptr,
          "a failed legacy migration must not leave the scene loaded");

  // A node may have a runtime parent that is not a node; from snapshot version 3 on the file must
  // still reload, with the snapshot as the authority for the hierarchy.
  runtime::World mixed_world;
  const auto mixed_scene = mixed_world.LoadScene("Mixed");
  editor::SceneDocument mixed(mixed_world, mixed_scene);
  const auto anchor = mixed_world.CreateEntity(mixed_scene).id;
  const auto attached = mixed.Create("Attached");
  runtime::WorldCommandBuffer attach_to_anchor;
  attach_to_anchor.SetParent(attached, anchor, false);
  const auto mixed_path = root / "Content/Mixed.scene";
  Require(attach_to_anchor.Apply(mixed_world) && mixed.Save(mixed_path),
          "mixed scene setup failed");
  runtime::World mixed_reloaded_world;
  editor::SceneDocument mixed_reloaded(mixed_reloaded_world,
                                       mixed_reloaded_world.LoadScene("Placeholder"));
  Require(mixed_reloaded.Reload(mixed_path) && mixed_reloaded.Parent(attached) == anchor,
          "a saved scene whose node has a non-node parent must reload");

  // A node whose entity was undone cannot become a parent: creation must fail without leaving an
  // entity behind.
  const auto ghost = mixed.Create("Ghost");
  Require(ghost != 0 && mixed.Undo() && mixed_world.FindEntity(ghost) == nullptr,
          "undoing a node creation failed");
  const auto entity_count = mixed_world.FindScene(mixed_scene)->entities.size();
  Require(mixed.Create("Orphan", ghost) == 0 &&
              mixed_world.FindScene(mixed_scene)->entities.size() == entity_count,
          "creating a node under an undone parent must fail without leaving an entity");
  Require(document.Create("Bad\nName") == 0 && document.Create("Bad\rName") == 0,
          "a node name containing a newline must be rejected, since Save()/Reload() use a "
          "line-oriented format that a newline would silently corrupt");
  {
    // Names with leading or only whitespace are legal and must survive a save/reload round trip
    // instead of leaving a scene file that can never be loaded again.
    runtime::World space_world;
    const auto space_scene = space_world.LoadScene("Spaces");
    editor::SceneDocument spaced(space_world, space_scene);
    const auto blank = spaced.Create(" ");
    const auto padded = spaced.Create("  padded");
    const auto space_path = root / "Content/Spaces.scene";
    Require(blank != 0 && padded != 0 && spaced.Save(space_path),
            "whitespace-name scene save failed");
    runtime::World space_reloaded_world;
    const auto space_placeholder = space_reloaded_world.LoadScene("Placeholder");
    editor::SceneDocument space_reloaded(space_reloaded_world, space_placeholder);
    Require(space_reloaded.Reload(space_path) && space_reloaded.Name(blank) == " " &&
                space_reloaded.Name(padded) == "  padded",
            "whitespace-only or padded node names did not round-trip");
  }

  {
    runtime::World hinted_world;
    const auto hinted_scene = hinted_world.LoadScene("Euler authoring");
    editor::SceneDocument hinted(hinted_world, hinted_scene);
    const auto first = hinted.Create("First");
    const auto second = hinted.Create("Second");
    const std::array targets{*hinted.Key(first), *hinted.Key(second)};
    Require(hinted.SetEulerField(targets, 0, 450.0) && hinted.SetEulerField(targets, 1, -720.0),
            "authoring Euler angles failed");
    Require(hinted.SetEulerField(targets, 0, 810.0) && hinted.Undo() &&
                hinted.EulerAngles(first) == editor::EulerDegrees{450.0, -720.0, 0.0} &&
                hinted.EulerAngles(second) == editor::EulerDegrees{450.0, -720.0, 0.0},
            "one undo must restore hints even when only the authored revolution changed");
    const auto before_invalid = *hinted.Transform(first);
    auto stale = targets[1];
    ++stale.entity_generation;
    const std::array stale_targets{targets[0], stale};
    const std::array duplicate_targets{targets[0], targets[0]};
    Require(!hinted.SetEulerField(stale_targets, 2, 45.0) &&
                !hinted.SetEulerField(duplicate_targets, 2, 45.0) &&
                !hinted.SetEulerField(targets, 3, 45.0) &&
                !hinted.SetEulerField(targets, 2, std::numeric_limits<double>::infinity()) &&
                hinted.Transform(first) == before_invalid &&
                hinted.EulerAngles(first) == editor::EulerDegrees{450.0, -720.0, 0.0},
            "invalid Euler transactions must preserve both transform and hint");
    auto translated = before_invalid;
    translated.x = 12.0;
    Require(hinted.SetTransform(first, translated) && (*hinted.EulerAngles(first))[0] == 450.0 &&
                hinted.Undo() && hinted.Transform(first) == before_invalid,
            "position-only edits and undo must preserve the rotation hint");
    const auto hinted_path = root / "Content/Hinted.scene";
    Require(hinted.Save(hinted_path), "Euler hint save failed");
    std::ifstream saved(hinted_path, std::ios::binary);
    const std::string source{std::istreambuf_iterator<char>(saved), {}};
    Require(source.starts_with("NEXORA_EDITOR_SCENE 2\n") &&
                source.find("euler " + std::to_string(first) + " 450 -720 0\n") !=
                    std::string::npos,
            "scene v2 must serialize the authored angles rather than canonicalize them");
    runtime::World reopened_world;
    editor::SceneDocument reopened_hint_document(reopened_world,
                                                 reopened_world.LoadScene("Placeholder"));
    Require(
        reopened_hint_document.Reload(hinted_path) &&
            reopened_hint_document.EulerAngles(first) == editor::EulerDegrees{450.0, -720.0, 0.0} &&
            reopened_hint_document.EulerAngles(second) == editor::EulerDegrees{450.0, -720.0, 0.0},
        "Euler hints must survive reopening in a separate World and document");
    const auto generation = reopened_hint_document.Generation();
    const auto reopened_transform = *reopened_hint_document.Transform(first);
    const auto hint_start = source.find("euler ");
    const auto world_start = source.find("world\n");
    const std::string record = "euler " + std::to_string(first) + " 450 -720 0\n";
    const std::array corrupt_records{"euler " + std::to_string(first) + " 0 0 0\n", record + record,
                                     std::string{"euler 999999 450 -720 0\n"},
                                     "euler " + std::to_string(first) + " nan 0 0\n",
                                     "euler " + std::to_string(first) + " 450 -720 0 trailing\n"};
    const auto corrupt_hint_path = root / "Content/CorruptHint.scene";
    for (const auto &record_text : corrupt_records) {
      auto corrupt = source;
      corrupt.replace(hint_start, world_start - hint_start, record_text);
      std::ofstream(corrupt_hint_path, std::ios::binary | std::ios::trunc) << corrupt;
      Require(!reopened_hint_document.Reload(corrupt_hint_path) &&
                  reopened_hint_document.Generation() == generation &&
                  reopened_hint_document.Transform(first) == reopened_transform &&
                  reopened_hint_document.EulerAngles(first) ==
                      editor::EulerDegrees{450.0, -720.0, 0.0},
              "corrupt/orphan/duplicate/mismatched hints must not replace the live document");
    }
    auto legacy_hint_source = source;
    legacy_hint_source.erase(hint_start, world_start - hint_start);
    legacy_hint_source.replace(0, std::string("NEXORA_EDITOR_SCENE 2").size(),
                               "NEXORA_EDITOR_SCENE 1");
    const auto legacy_hint_path = root / "Content/LegacyHint.scene";
    std::ofstream(legacy_hint_path, std::ios::binary) << legacy_hint_source;
    Require(
        reopened_hint_document.Reload(legacy_hint_path) &&
            editor::SameRotation(*reopened_hint_document.Transform(first), reopened_transform) &&
            std::abs((*reopened_hint_document.EulerAngles(first))[0] - 90.0) < 1e-8 &&
            reopened_hint_document.Save(legacy_hint_path),
        "v1 scenes must load canonical angles and upgrade on a normal save");
    std::ifstream upgraded(legacy_hint_path);
    std::string header;
    Require(std::getline(upgraded, header) && header == "NEXORA_EDITOR_SCENE 2",
            "normal save must upgrade the Editor scene header to v2");
    runtime::WorldCommandBuffer external_rotation;
    external_rotation.SetTransform(first, {});
    Require(external_rotation.Apply(hinted_world) &&
                hinted.EulerAngles(first) == editor::EulerDegrees{0.0, 0.0, 0.0},
            "external quaternion changes must invalidate a stale hint");
    Require(hinted.Save(hinted_path), "saving after an external rotation failed");
    Require(reopened_hint_document.Reload(hinted_path) &&
                reopened_hint_document.EulerAngles(first) == editor::EulerDegrees{0.0, 0.0, 0.0} &&
                reopened_hint_document.EulerAngles(second) ==
                    editor::EulerDegrees{450.0, -720.0, 0.0},
            "saving must omit invalidated hints without losing unaffected ones");
  }

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

  editor::GizmoTransaction failed_gizmo;
  Require(failed_gizmo.Begin(inspect_entities,
                             [&](runtime::Id id, runtime::Transform &value) {
                               value = gizmo_transforms.at(id);
                               return true;
                             }),
          "failed gizmo transaction did not begin");
  std::size_t applied = 0;
  Require(!failed_gizmo.Update(dragged,
                               [&](runtime::Id id, runtime::Transform value) {
                                 if (applied++ == 1)
                                   return false;
                                 gizmo_transforms[id] = value;
                                 return true;
                               }) &&
              gizmo_transforms[parent] == runtime::Transform{1, 1, 1},
          "failed gizmo update left an earlier entity half-applied");

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
  auto invalid_camera = camera;
  invalid_camera.yaw = std::numeric_limits<double>::quiet_NaN();
  Require(!editor::CameraPersistence::Save(camera_path, invalid_camera, &error) &&
              editor::CameraPersistence::Load(camera_path, &error) == camera,
          "invalid camera state replaced the last readable camera file");
  const auto camera_directory = root / ".nexora/camera-directory";
  fs::create_directory(camera_directory);
  Require(!editor::CameraPersistence::Save(camera_directory, camera, &error) &&
              fs::is_directory(camera_directory),
          "camera persistence deleted an existing destination directory");

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
  const auto manifest_directory = root / "manifest-directory";
  fs::create_directory(manifest_directory);
  Require(!editor::BuildFrontend::Write(manifest, manifest_directory, &error) &&
              fs::is_directory(manifest_directory),
          "build manifest replacement deleted an existing destination directory");
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
  editor::AdditiveSceneGraph scene_graph;
  Require(scene_graph.Add({1, "Content/Base.scene", true, {}}) &&
              scene_graph.Add({2, "Content/Lighting.scene", false, {}}) &&
              scene_graph.SetDependencies(2, {1}) &&
              scene_graph.LoadOrder() == std::vector<editor::SceneDocumentId>{1, 2} &&
              !scene_graph.Remove(1),
          "additive scene graph failed");
  Require(!scene_graph.SetDependencies(1, {2}) && scene_graph.LoadOrder().size() == 2,
          "scene dependency cycle did not roll back");
  editor::DocumentMigration migration;
  Require(migration.Register(1,
                             [](std::string_view value) {
                               return std::optional{std::string(value) + "\nschema=2"};
                             }),
          "migration registration failed");
  std::string migratable = "scene";
  const auto dry_run = migration.Run(1, 2, "Content/Main.scene", migratable, true);
  Require(dry_run && dry_run->changes.size() == 1 && migratable == "scene",
          "migration dry-run mutated document");
  Require(migration.Run(1, 2, "Content/Main.scene", migratable, false) &&
              migratable == "scene\nschema=2",
          "migration commit failed");
  const auto journal_path = root / ".nexora/Main.autosave";
  std::uint64_t recovered_revision{};
  Require(editor::AutosaveJournal::Write(journal_path, 9, "recoverable scene", &error) &&
              editor::AutosaveJournal::Recover(journal_path, &recovered_revision, &error) ==
                  std::optional<std::string>{"recoverable scene"} &&
              recovered_revision == 9,
          "autosave journal round trip failed");
  const auto journal_directory = root / ".nexora/journal-directory";
  fs::create_directory(journal_directory);
  Require(!editor::AutosaveJournal::Write(journal_directory, 10, "payload", &error) &&
              fs::is_directory(journal_directory),
          "autosave replacement deleted an existing destination directory");
  std::ofstream(journal_path, std::ios::trunc) << "corrupt";
  Require(!editor::AutosaveJournal::Recover(journal_path, nullptr, &error),
          "corrupt autosave journal accepted");
  const std::vector<editor::MergeRecord> merge_input{
      {"entity/1/name", "Hero", "Hero", "Player", {}, editor::MergeChoice::Manual},
      {"entity/2/name", "Light", "Key", "Fill", {}, editor::MergeChoice::Manual}};
  const auto merge = editor::ThreeWayMerge(merge_input);
  Require(merge[0].choice == editor::MergeChoice::Remote && merge[0].resolution == "Player" &&
              merge[1].Conflicted() && merge[1].resolution.empty(),
          "three-way merge failed");
  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << "editor.preview_contract: " << error.what() << '\n';
    return 1;
  }
}
