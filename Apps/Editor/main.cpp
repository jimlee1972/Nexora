#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ProjectContent.h"
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/EditorSdk.h"
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {
struct ProjectState final {
  nexora::editor::ProjectWorkspace workspace;
  nexora::editor::AssetWorkspace assets;
};

bool OpenProjectWorkspace(const std::filesystem::path &root, nexora::editor::ProjectAccess access,
                          bool create, std::string name, ProjectState &destination,
                          std::string *error) {
  ProjectState candidate;
  if (create) {
    if (!candidate.workspace.Create(root, std::move(name), error))
      return false;
  } else if (!candidate.workspace.Open(root, access, error)) {
    return false;
  }
  destination = std::move(candidate);
  return true;
}

bool LoadProject(const std::filesystem::path &root, nexora::editor::ProjectAccess access,
                 bool create, std::string name, ProjectState &destination, std::string *error) {
  ProjectState candidate;
  if (!OpenProjectWorkspace(root, access, create, std::move(name), candidate, error))
    return false;
  if (!candidate.assets.ImportTree(candidate.workspace.Root() / "Content", {}, {},
                                   candidate.workspace.Writable()
                                       ? nexora::editor::AssetIdentityMode::PersistentReadWrite
                                       : nexora::editor::AssetIdentityMode::PersistentReadOnly,
                                   error))
    return false;
  destination = std::move(candidate);
  return true;
}

#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
int RunGraphical(std::optional<ProjectState> project,
                 nexora::editor::RecentProjectStore &recent_projects,
                 nexora::editor::ProjectAccess selector_access, std::uint32_t frame_limit) {
  auto created = Nexora::Presentation::CreateRenderSurface(
      {"Nexora Editor", 1280, 720, true, Nexora::Presentation::SurfaceBackend::Automatic});
  if (!created) {
    std::cerr << "graphical shell unavailable: " << created.reason << '\n';
    return 1;
  }
  nexora::editor::imgui::EditorImGuiHost ui;
  nexora::core::JobSystem import_jobs{1};
  import_jobs.Start();
  nexora::editor::AssetImportQueue imports{import_jobs};
  nexora::editor::ProjectContentSession content;
  struct PendingProject final {
    ProjectState candidate;
    nexora::editor::ImportOperationId import{};
    bool created{};
  };
  std::optional<PendingProject> pending_project;
  std::string layout_error;
  const auto load_layout = [&](nexora::editor::ProjectWorkspace &workspace) {
    layout_error.clear();
    if (const auto layout = workspace.LoadEditorLayout(&layout_error);
        layout && !ui.LoadLayout(*layout))
      std::cerr << "ignored invalid editor layout\n";
    else if (!layout_error.empty())
      std::cerr << layout_error << '\n';
  };
  std::uint64_t project_generation = 1;
  if (project) {
    if (!content.Open(project->workspace, project->assets, project_generation,
                      project->workspace.Writable(), &layout_error)) {
      std::cerr << layout_error << '\n';
      return 1;
    }
    load_layout(project->workspace);
  }
  nexora::editor::ProductShell shell;
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Main");
  if (!world.Activate(scene_id)) {
    std::cerr << "failed to activate the editor scene\n";
    return 1;
  }
  nexora::editor::SceneDocument scene(world, scene_id);
  nexora::runtime::RuntimeConsole console{1024};
  const auto log = [&](nexora::runtime::RuntimeLogSeverity severity, std::string category,
                       std::string message) {
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    static_cast<void>(
        console.Push({0, severity, std::move(category), static_cast<std::uint64_t>(timestamp),
                      "NexoraEditor", std::move(message)}));
  };
  log(nexora::runtime::RuntimeLogSeverity::Info, "Editor", "Graphical session started.");
  const auto scene_path = [](const nexora::editor::ProjectWorkspace &workspace) {
    return workspace.Root() / ".nexora" / "scenes" / "Main.scene";
  };
  const auto overview_path = [&](const nexora::editor::ProjectWorkspace &workspace) {
    auto path = scene_path(workspace);
    path.replace_extension(".overview.camera");
    return path;
  };
  bool scene_load_failed = false;
  bool overview_load_failed = false;
  const auto open_scene = [&] {
    const auto path = scene_path(project->workspace);
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error || (exists && !scene.Reload(path))) {
      scene_load_failed = true;
      ui.SetSceneSaveResult("Scene could not be loaded: " + path.string(), false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene",
          "Scene could not be loaded: " + path.string());
      return;
    }
    scene_load_failed = false;
    if (!exists && scene.Nodes().empty())
      scene.Create("Scene Root");
    log(nexora::runtime::RuntimeLogSeverity::Info, "Scene",
        exists ? "Opened saved scene." : "Created starter scene.");
    static_cast<void>(ui.SetSceneOverviewCamera({}));
    overview_load_failed = false;
    const auto camera_path = overview_path(project->workspace);
    std::error_code camera_error;
    const bool camera_exists = std::filesystem::exists(camera_path, camera_error);
    if (camera_error) {
      overview_load_failed = true;
      std::cerr << "scene overview camera could not be inspected: " << camera_error.message()
                << '\n';
    } else if (camera_exists) {
      std::string load_error;
      const auto camera = nexora::editor::CameraPersistence::Load(camera_path, &load_error);
      if (!camera || !camera->orthographic ||
          !ui.SetSceneOverviewCamera({camera->transform.x, camera->transform.z,
                                      std::clamp(320.0 / camera->orthographic_size, 4.0, 256.0)})) {
        overview_load_failed = true;
        std::cerr << "ignored invalid scene overview camera: " << camera_path << ' ' << load_error
                  << '\n';
      }
    }
  };
  if (project)
    open_scene();
  std::uint32_t frames = 0;
  int result = 0;
  bool exit_requested = false;
  auto recovery_choice = nexora::editor::imgui::RecoveryChoice::None;
  std::string selector_result = project ? "bypassed" : "none";
  const auto save_scene = [&] {
    if (scene_load_failed) {
      ui.SetSceneSaveResult("Scene load failed. Resolve the scene file before saving.", false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", "Save blocked by failed load.");
      return false;
    }
    if (!project || !project->workspace.Writable()) {
      ui.SetSceneSaveResult("Scene is read-only. Reopen the project for writing.", false);
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene",
          "Save blocked by read-only access.");
      return false;
    }
    if (!scene.Save(scene_path(project->workspace))) {
      ui.SetSceneSaveResult("Scene could not be saved. Check the project directory.", false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", "Scene save failed.");
      return false;
    }
    ui.SetSceneSaveResult("Scene saved.", true);
    log(nexora::runtime::RuntimeLogSeverity::Info, "Scene", "Scene saved.");
    return true;
  };
  while (!exit_requested && !created.surface->CloseRequested() &&
         (frame_limit == 0 || frames < frame_limit)) {
    const auto begin_frame_status = created.surface->BeginFrame();
    if (created.surface->CloseRequested()) {
      if (project && scene.Dirty() && created.surface->CancelCloseRequest()) {
        ui.RequestCloseConfirmation();
        continue;
      }
      break;
    }
    const auto action = Nexora::Presentation::RecoveryAction(begin_frame_status);
    if (action == Nexora::Presentation::SurfaceAction::Abort) {
      result = 1;
      break;
    }
    if (action != Nexora::Presentation::SurfaceAction::Render)
      continue;
    ui.ProcessEvents(created.surface->Events());
    const auto &frame = created.surface->FrameInfo();
    if (frame.width == 0 || frame.height == 0)
      continue;
    const auto dpi = std::max(frame.dpiScale, 0.25F);
    ui.SetDisplay(static_cast<float>(frame.width) / dpi, static_cast<float>(frame.height) / dpi,
                  dpi);
    ui.UpdateImeCandidate(*created.surface);
    ui.BeginFrame();
    if (!project && pending_project) {
      if (const auto snapshot = imports.Snapshot(pending_project->import)) {
        std::string status = "Importing project content";
        if (!snapshot->progress.empty()) {
          const auto &progress = snapshot->progress.back();
          if (progress.total != 0)
            status += " (" + std::to_string(progress.completed) + "/" +
                      std::to_string(progress.total) + ")";
        }
        ui.SetProjectSelectorStatus(std::move(status), true);
      }
      if (auto imported = imports.TakeResult(pending_project->import)) {
        if (imported->snapshot.state == nexora::editor::ImportOperationState::Cancelled) {
          ui.SetProjectSelectorError({});
          ui.SetProjectSelectorStatus({}, false);
          pending_project.reset();
        } else if (imported->snapshot.state !=
                       nexora::editor::ImportOperationState::AwaitingPublish ||
                   !imported->workspace) {
          const auto message = imported->snapshot.diagnostics.empty()
                                   ? "Project content import failed."
                                   : imported->snapshot.diagnostics.back().message;
          ui.SetProjectSelectorError(message);
          ui.SetProjectSelectorStatus({}, false);
          pending_project.reset();
        } else {
          pending_project->candidate.assets = std::move(*imported->workspace);
          nexora::editor::ProjectContentSession candidate_content;
          std::string selector_error;
          if (!candidate_content.Open(pending_project->candidate.workspace,
                                      pending_project->candidate.assets, project_generation,
                                      pending_project->candidate.workspace.Writable(),
                                      &selector_error)) {
            ui.SetProjectSelectorError(selector_error.empty() ? "Project content could not open."
                                                              : std::move(selector_error));
            ui.SetProjectSelectorStatus({}, false);
            pending_project.reset();
          } else {
            const bool was_created = pending_project->created;
            project = std::move(pending_project->candidate);
            content = std::move(candidate_content);
            pending_project.reset();
            selector_result = was_created ? "created" : "opened";
            if (!recent_projects.Record(project->workspace, &selector_error))
              std::cerr << "recent-project warning: " << selector_error << '\n';
            load_layout(project->workspace);
            open_scene();
            ui.SetProjectSelectorError({});
            ui.SetProjectSelectorStatus({}, false);
          }
        }
      }
    }
    if (project) {
      ui.DrawProductShell(shell, &scene, &project->workspace, &content, &recent_projects, &imports,
                          &console);
      if (ui.TakeSceneSaveRequest())
        static_cast<void>(save_scene());
      const auto close_choice = ui.TakeCloseChoice();
      if (close_choice == nexora::editor::imgui::CloseChoice::SaveAndExit)
        exit_requested = save_scene();
      else if (close_choice == nexora::editor::imgui::CloseChoice::DiscardAndExit)
        exit_requested = true;
      if (const auto choice = ui.TakeRecoveryChoice();
          choice != nexora::editor::imgui::RecoveryChoice::None)
        recovery_choice = choice;
    } else {
      ui.DrawProjectSelector(&recent_projects, selector_access);
      if (ui.TakeProjectSelectorCancel() && pending_project) {
        static_cast<void>(imports.Cancel(pending_project->import));
        ui.SetProjectSelectorStatus("Cancelling project import", true);
      }
      if (auto request = ui.TakeProjectSelectorRequest()) {
        if (pending_project) {
          ui.SetProjectSelectorError("A project import is already running.");
        } else {
          ProjectState candidate;
          std::string selector_error;
          const bool create_project =
              request->action == nexora::editor::imgui::ProjectSelectorAction::Create;
          if (!OpenProjectWorkspace(request->root, request->access, create_project,
                                    std::move(request->name), candidate, &selector_error)) {
            ui.SetProjectSelectorError(selector_error.empty() ? "Project activation failed."
                                                              : std::move(selector_error));
          } else {
            const auto import =
                imports.Start({project_generation, candidate.workspace.Root() / "Content",
                               candidate.workspace.Writable()
                                   ? nexora::editor::AssetIdentityMode::PersistentReadWrite
                                   : nexora::editor::AssetIdentityMode::PersistentReadOnly},
                              &selector_error);
            if (import == 0) {
              ui.SetProjectSelectorError(selector_error.empty() ? "Project import could not start."
                                                                : std::move(selector_error));
            } else {
              ui.SetProjectSelectorError({});
              ui.SetProjectSelectorStatus("Importing project content", true);
              pending_project = PendingProject{std::move(candidate), import, create_project};
            }
          }
        }
      }
    }
    static_cast<void>(ui.EndFrame());
    // A surface-level loss (the window vanished, the swapchain went out of date) is recoverable and
    // is resolved by the next BeginFrame, which also pumps a pending close request. Anything else,
    // device loss included, is a real failure.
    const auto surface_recoverable = [](Nexora::Presentation::SurfaceStatus status) {
      const auto recovery = Nexora::Presentation::RecoveryAction(status);
      return recovery == Nexora::Presentation::SurfaceAction::RecreateSurface ||
             recovery == Nexora::Presentation::SurfaceAction::Suspend;
    };
    if (const auto render_status = ui.Render(*created.surface, frame.width, frame.height);
        render_status != Nexora::Presentation::SurfaceStatus::Ready) {
      if (surface_recoverable(render_status))
        continue;
      result = created.surface->CloseRequested() ? 0 : 1;
      break;
    }
    if (const auto end_status = created.surface->EndFrame();
        end_status != Nexora::Presentation::SurfaceStatus::Ready) {
      if (surface_recoverable(end_status))
        continue;
      result = created.surface->CloseRequested() ? 0 : 1;
      break;
    }
    ++frames;
  }
  if (project && project->workspace.Writable() &&
      !project->workspace.SaveEditorLayout(ui.SaveLayout(), &layout_error))
    std::cerr << layout_error << '\n';
  if (project && project->workspace.Writable() && !scene_load_failed && !overview_load_failed) {
    const auto camera_path = overview_path(project->workspace);
    std::error_code directory_error;
    std::filesystem::create_directories(camera_path.parent_path(), directory_error);
    if (directory_error) {
      std::cerr << "scene overview camera directory could not be created: "
                << directory_error.message() << '\n';
    } else {
      const auto view = ui.GetSceneOverviewCamera();
      nexora::editor::SceneCameraState camera;
      camera.transform.x = view.x;
      camera.transform.z = view.z;
      camera.orthographic = true;
      camera.orthographic_size = 320.0 / view.pixels_per_unit;
      std::string camera_error;
      if (!nexora::editor::CameraPersistence::Save(camera_path, camera, &camera_error))
        std::cerr << "scene overview camera could not be saved: " << camera_error << '\n';
    }
  }
  const auto diagnostics = created.surface->Diagnostics();
  std::cerr << "graphical evidence: acquired=" << diagnostics.acquiredFrames
            << " presented=" << diagnostics.presentedFrames
            << " ui_draws=" << diagnostics.nativeUiDrawCalls
            << " ui_uploads=" << diagnostics.nativeUiTextureUploads
            << " ui_rejected=" << diagnostics.nativeUiRejectedTextures << " recovery="
            << (recovery_choice == nexora::editor::imgui::RecoveryChoice::Recover   ? "recover"
                : recovery_choice == nexora::editor::imgui::RecoveryChoice::Discard ? "discard"
                                                                                    : "none")
            << " selector=" << selector_result << " access="
            << (project ? (project->workspace.Writable() ? "read-write" : "read-only") : "none")
            << " schema=" << (project ? project->workspace.Project().schema_version : 0)
            << " upgrade="
            << (!project ? "none"
                : project->workspace.UpgradeState() == nexora::editor::ProjectUpgradeState::Applied
                    ? "applied"
                : project->workspace.UpgradeState() == nexora::editor::ProjectUpgradeState::Required
                    ? "required"
                    : "current")
            << " recents=" << recent_projects.Entries().size()
            << " scene_nodes=" << scene.Nodes().size() << '\n';
  if (created.surface->DrainAndDestroy() != Nexora::Presentation::SurfaceStatus::Ready)
    result = 1;
  return result;
}
#endif

int Run(int argc, char **argv) {
  std::filesystem::path project;
  std::filesystem::path report;
  std::filesystem::path recent_projects_path;
  bool graphical = false;
  bool read_only = false;
  std::uint32_t frame_limit = 0;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument.starts_with("--project="))
      project = argument.substr(10);
    else if (argument.starts_with("--report="))
      report = argument.substr(9);
    else if (argument.starts_with("--recent-projects="))
      recent_projects_path = argument.substr(18);
    else if (argument == "--graphical")
      graphical = true;
    else if (argument == "--read-only")
      read_only = true;
    else if (argument.starts_with("--frames=")) {
      const auto digits = argument.substr(9);
      const auto [end, status] =
          std::from_chars(digits.data(), digits.data() + digits.size(), frame_limit);
      if (digits.empty() || status != std::errc{} || end != digits.data() + digits.size()) {
        std::cerr << "--frames expects a non-negative 32-bit integer\n";
        return 2;
      }
    } else if (argument == "--help") {
      std::cout << "NexoraEditor [--project=PATH] [--read-only] [--report=PATH] [--graphical] "
                   "[--frames=N] [--recent-projects=PATH]\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << argument << '\n';
      return 2;
    }
  }
  if (project.empty() && !graphical) {
    std::cerr << "--project is required unless --graphical opens the project selector\n";
    return 2;
  }
  std::string error;
  nexora::editor::RecentProjectStore recent_projects;
  if (recent_projects_path.empty())
    recent_projects_path = nexora::editor::RecentProjectStore::DefaultPath();
  if (!recent_projects.Open(recent_projects_path, &error))
    std::cerr << "recent-project warning: " << error << '\n';
  std::optional<ProjectState> project_state;
  if (!project.empty()) {
    ProjectState opened;
    if (!LoadProject(project,
                     read_only ? nexora::editor::ProjectAccess::ReadOnly
                               : nexora::editor::ProjectAccess::ReadWrite,
                     false, {}, opened, &error)) {
      std::cerr << "content indexing or project open failed: " << error << '\n';
      return 1;
    }
    project_state = std::move(opened);
    if (!recent_projects.Record(project_state->workspace, &error))
      std::cerr << "recent-project warning: " << error << '\n';
  }
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
  if (graphical)
    return RunGraphical(std::move(project_state), recent_projects,
                        read_only ? nexora::editor::ProjectAccess::ReadOnly
                                  : nexora::editor::ProjectAccess::ReadWrite,
                        frame_limit);
#else
  static_cast<void>(frame_limit);
  if (graphical) {
    std::cerr << "graphical shell was not enabled at build time\n";
    return 2;
  }
#endif
  if (!project_state) {
    std::cerr << "--project is required\n";
    return 2;
  }
  const std::string json =
      "{\n  \"application\": \"NexoraEditor\",\n  \"project\": \"" +
      project_state->workspace.Project().name +
      "\",\n  \"panels\": " + std::to_string(nexora::editor::ProductShell::Panels().size()) +
      ",\n  \"assets\": " + std::to_string(project_state->assets.Entries().size()) +
      ",\n  \"access\": \"" + (project_state->workspace.Writable() ? "read-write" : "read-only") +
      "\",\n  \"schema\": " + std::to_string(project_state->workspace.Project().schema_version) +
      "\n}\n";
  if (report.empty())
    std::cout << json;
  else {
    std::ofstream output(report);
    if (!output || !(output << json))
      return 1;
  }
  return 0;
}
} // namespace

int main(int argc, char **argv) { return Run(argc, argv); }
