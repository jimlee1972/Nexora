#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ProjectContent.h"
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#endif

#include <algorithm>
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

bool LoadProject(const std::filesystem::path &root, nexora::editor::ProjectAccess access,
                 bool create, std::string name, ProjectState &destination, std::string *error) {
  ProjectState candidate;
  if (create) {
    if (!candidate.workspace.Create(root, std::move(name), error))
      return false;
  } else if (!candidate.workspace.Open(root, access, error)) {
    return false;
  }
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
  nexora::editor::ProjectContentSession content;
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
  scene.Create("Scene Root");
  std::uint32_t frames = 0;
  int result = 0;
  auto recovery_choice = nexora::editor::imgui::RecoveryChoice::None;
  std::string selector_result = project ? "bypassed" : "none";
  while (!created.surface->CloseRequested() && (frame_limit == 0 || frames < frame_limit)) {
    const auto begin_frame_status = created.surface->BeginFrame();
    const auto action = Nexora::Presentation::RecoveryAction(begin_frame_status);
    if (action == Nexora::Presentation::SurfaceAction::Abort) {
      // A window closed during startup can no longer back a swapchain, so BeginFrame reports a
      // failure caused by the close itself. When the user already asked to quit, that is a clean
      // exit, not an error.
      result = created.surface->CloseRequested() ? 0 : 1;
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
    if (project) {
      ui.DrawProductShell(shell, &scene, &project->workspace, &content, &recent_projects);
      if (const auto choice = ui.TakeRecoveryChoice();
          choice != nexora::editor::imgui::RecoveryChoice::None)
        recovery_choice = choice;
    } else {
      ui.DrawProjectSelector(&recent_projects, selector_access);
      if (auto request = ui.TakeProjectSelectorRequest()) {
        ProjectState candidate;
        std::string selector_error;
        const bool create_project =
            request->action == nexora::editor::imgui::ProjectSelectorAction::Create;
        if (!LoadProject(request->root, request->access, create_project, std::move(request->name),
                         candidate, &selector_error)) {
          ui.SetProjectSelectorError(selector_error.empty() ? "Project activation failed."
                                                            : std::move(selector_error));
        } else {
          nexora::editor::ProjectContentSession candidate_content;
          if (!candidate_content.Open(candidate.workspace, candidate.assets, project_generation,
                                      candidate.workspace.Writable(), &selector_error)) {
            ui.SetProjectSelectorError(selector_error.empty() ? "Project content could not open."
                                                              : std::move(selector_error));
          } else {
            project = std::move(candidate);
            content = std::move(candidate_content);
            selector_result = create_project ? "created" : "opened";
            if (!recent_projects.Record(project->workspace, &selector_error))
              std::cerr << "recent-project warning: " << selector_error << '\n';
            load_layout(project->workspace);
            ui.SetProjectSelectorError({});
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
    if (const auto status = ui.Render(*created.surface, frame.width, frame.height);
        status != Nexora::Presentation::SurfaceStatus::Ready) {
      if (surface_recoverable(status))
        continue;
      result = created.surface->CloseRequested() ? 0 : 1;
      break;
    }
    if (const auto status = created.surface->EndFrame();
        status != Nexora::Presentation::SurfaceStatus::Ready) {
      if (surface_recoverable(status))
        continue;
      result = created.surface->CloseRequested() ? 0 : 1;
      break;
    }
    ++frames;
  }
  if (project && project->workspace.Writable() &&
      !project->workspace.SaveEditorLayout(ui.SaveLayout(), &layout_error))
    std::cerr << layout_error << '\n';
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
            << " recents=" << recent_projects.Entries().size() << '\n';
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
    else if (argument.starts_with("--frames="))
      frame_limit = static_cast<std::uint32_t>(std::stoul(std::string(argument.substr(9))));
    else if (argument == "--help") {
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
