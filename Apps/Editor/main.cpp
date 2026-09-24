#include "Nexora/Editor/EditorWorkspace.h"
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#endif

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
  while (!created.surface->CloseRequested() && (frame_limit == 0 || frames < frame_limit)) {
    const auto status = created.surface->BeginFrame();
    const auto action = Nexora::Presentation::RecoveryAction(status);
    if (action == Nexora::Presentation::SurfaceAction::Abort)
      break;
    if (action != Nexora::Presentation::SurfaceAction::Render)
      continue;
    ui.ProcessEvents(created.surface->Events());
    const auto &frame = created.surface->FrameInfo();
    if (frame.width == 0 || frame.height == 0)
      continue;
    ui.SetDisplay(static_cast<float>(frame.width), static_cast<float>(frame.height),
                  frame.dpiScale);
    ui.UpdateImeCandidate(*created.surface);
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, &workspace);
    static_cast<void>(ui.EndFrame());
    if (ui.Render(*created.surface, frame.width, frame.height) !=
        Nexora::Presentation::SurfaceStatus::Ready)
      break;
    if (created.surface->EndFrame() != Nexora::Presentation::SurfaceStatus::Ready)
      break;
    ++frames;
  }
  return created.surface->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready ? 0 : 1;
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
