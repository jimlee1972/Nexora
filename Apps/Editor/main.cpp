#include "Nexora/Editor/EditorWorkspace.h"
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
int RunGraphical(nexora::editor::ProjectWorkspace &workspace, std::uint32_t frame_limit) {
  auto created = Nexora::Presentation::CreateRenderSurface(
      {"Nexora Editor", 1280, 720, true, Nexora::Presentation::SurfaceBackend::Automatic});
  if (!created) {
    std::cerr << "graphical shell unavailable: " << created.reason << '\n';
    return 1;
  }
  nexora::editor::imgui::EditorImGuiHost ui;
  std::string layout_error;
  if (const auto layout = workspace.LoadEditorLayout(&layout_error);
      layout && !ui.LoadLayout(*layout))
    std::cerr << "ignored invalid editor layout\n";
  else if (!layout_error.empty())
    std::cerr << layout_error << '\n';
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
  while (!created.surface->CloseRequested() && (frame_limit == 0 || frames < frame_limit)) {
    const auto status = created.surface->BeginFrame();
    const auto action = Nexora::Presentation::RecoveryAction(status);
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
    ui.DrawProductShell(shell, &scene, &workspace);
    if (const auto choice = ui.TakeRecoveryChoice();
        choice != nexora::editor::imgui::RecoveryChoice::None)
      recovery_choice = choice;
    static_cast<void>(ui.EndFrame());
    if (ui.Render(*created.surface, frame.width, frame.height) !=
        Nexora::Presentation::SurfaceStatus::Ready) {
      result = 1;
      break;
    }
    if (created.surface->EndFrame() != Nexora::Presentation::SurfaceStatus::Ready) {
      result = 1;
      break;
    }
    ++frames;
  }
  if (!workspace.SaveEditorLayout(ui.SaveLayout(), &layout_error))
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
            << '\n';
  if (created.surface->DrainAndDestroy() != Nexora::Presentation::SurfaceStatus::Ready)
    result = 1;
  return result;
}
#endif

int Run(int argc, char **argv) {
  std::filesystem::path project;
  std::filesystem::path report;
  bool graphical = false;
  std::uint32_t frame_limit = 0;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument.starts_with("--project="))
      project = argument.substr(10);
    else if (argument.starts_with("--report="))
      report = argument.substr(9);
    else if (argument == "--graphical")
      graphical = true;
    else if (argument.starts_with("--frames="))
      frame_limit = static_cast<std::uint32_t>(std::stoul(std::string(argument.substr(9))));
    else if (argument == "--help") {
      std::cout << "NexoraEditor --project=PATH [--report=PATH] [--graphical] [--frames=N]\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << argument << '\n';
      return 2;
    }
  }
  if (project.empty()) {
    std::cerr << "--project is required\n";
    return 2;
  }
  nexora::editor::ProjectWorkspace workspace;
  std::string error;
  if (!workspace.Open(project, &error)) {
    std::cerr << error << '\n';
    return 1;
  }
  nexora::editor::AssetWorkspace assets;
  if (!assets.ImportTree(project / "Content")) {
    std::cerr << "content indexing failed\n";
    return 1;
  }
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
  if (graphical)
    return RunGraphical(workspace, frame_limit);
#else
  static_cast<void>(frame_limit);
  if (graphical) {
    std::cerr << "graphical shell was not enabled at build time\n";
    return 2;
  }
#endif
  const std::string json =
      "{\n  \"application\": \"NexoraEditor\",\n  \"project\": \"" + workspace.Project().name +
      "\",\n  \"panels\": " + std::to_string(nexora::editor::ProductShell::Panels().size()) +
      ",\n  \"assets\": " + std::to_string(assets.Entries().size()) + "\n}\n";
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
