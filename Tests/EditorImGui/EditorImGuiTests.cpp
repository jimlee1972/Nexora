#include "Nexora/EditorImGui/EditorImGui.h"

#include "imgui.h"

#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

int main() {
  nexora::editor::imgui::EditorImGuiHost host;
  assert((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0);
  assert((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0);
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Editor ImGui contract");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto root = scene.Create("Scene Root");
  assert(root != 0 && scene.Nodes().size() == 1);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, 320, 240},
      Nexora::Window::WindowEvent{{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, 'N', 0},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::DpiChanged, 0, 0, 0, 1.5F, 0, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::S),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
  };
  host.ProcessEvents(events);
  host.SetDisplay(1600.0F, 900.0F, 1.5F);
  assert(ImGui::GetIO().DisplaySize.x == 1600.0F);
  assert(ImGui::GetIO().DisplaySize.y == 900.0F);
  assert(ImGui::GetIO().DisplayFramebufferScale.x == 1.5F);
  assert(ImGui::GetIO().FontGlobalScale > 0.66F && ImGui::GetIO().FontGlobalScale < 0.67F);
  host.BeginFrame();
  nexora::editor::ProductShell shell;
  host.DrawProductShell(shell, &scene);
  assert(shell.LastCommand() == "editor.scene.save");
  const auto metrics = host.EndFrame();
  assert(metrics.command_lists > 0);
  assert(metrics.vertices > 0);
  assert(metrics.indices > 0);
  auto device = nexora::rhi::CreateValidationDevice();
  const auto target =
      device->CreateTexture({1280, 720, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::Undefined, "Editor ImGui offscreen"});
  const auto user_texture =
      device->CreateTexture({1, 1, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::ShaderRead, "Editor user texture"});
  const auto texture_id = host.RegisterTexture(*device, user_texture);
  assert(texture_id != 0);
  for (int list = 0; list < ImGui::GetDrawData()->CmdListsCount; ++list)
    for (auto &command : ImGui::GetDrawData()->CmdLists[list]->CmdBuffer)
      command.TextureId = static_cast<ImTextureID>(texture_id);
  const auto draws =
      host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::Undefined, false);
  auto repeated_draws = draws;
  for (int frame = 0; frame < 3; ++frame)
    repeated_draws =
        host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false);
  const auto retained = host.GetRendererMetrics();
  assert(repeated_draws == draws && retained.frames == 4 && retained.buffer_reallocations == 6 &&
         retained.font_rebuilds == 1 && retained.completion_waits == 0 &&
         retained.rejected_textures == 0);
  const auto diagnostics = device->Diagnostics();
  assert(draws > 0 && diagnostics.draw_calls == draws * 4 && diagnostics.validation_errors == 0);
  assert(host.UnregisterTexture(texture_id));
  assert(!host.UnregisterTexture(texture_id));
  for (int list = 0; list < ImGui::GetDrawData()->CmdListsCount; ++list)
    for (auto &command : ImGui::GetDrawData()->CmdLists[list]->CmdBuffer)
      command.TextureId = static_cast<ImTextureID>(texture_id);
  assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) ==
         draws);
  assert(host.GetRendererMetrics().rejected_textures > 0);
  const auto next_texture_id = host.RegisterTexture(*device, user_texture);
  assert(next_texture_id != texture_id && host.UnregisterTexture(next_texture_id));

  // Exercise every DPI bucket and a bounded long-running layout/render loop. Each bucket switch
  // rebuilds the atlas once; subsequent stable frames reuse allocations and reject no callbacks.
  constexpr std::array dpi_scales{1.25F, 1.5F, 2.0F, 1.0F};
  for (const float dpi : dpi_scales) {
    host.SetDisplay(1280.0F / dpi, 720.0F / dpi, dpi);
    host.BeginFrame();
    host.DrawProductShell(shell, &scene);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  const auto reallocations_before_soak = host.GetRendererMetrics().buffer_reallocations;
  for (int frame = 0; frame < 512; ++frame) {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  const auto soaked = host.GetRendererMetrics();
  assert(soaked.font_rebuilds == 5 && soaked.buffer_reallocations == reallocations_before_soak);
  const auto layout = host.SaveLayout();
  assert(!layout.empty() && host.LoadLayout(layout));
  assert(!host.LoadLayout("not an ImGui layout"));

  const auto recovery_root =
      std::filesystem::temp_directory_path() /
      ("nexora-imgui-recovery-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  nexora::editor::ProjectWorkspace workspace;
  std::string error;
  assert(workspace.Create(recovery_root, "Recovery", &error));
  std::ofstream(recovery_root / ".nexora/workspace.recovery") << "schema=1\ninvalid\n";
  assert(!host.ApplyRecoveryChoice(workspace, nexora::editor::imgui::RecoveryChoice::Recover));
  assert(workspace.HasRecoveryJournal() && !host.RecoveryError().empty());
  assert(host.ApplyRecoveryChoice(workspace, nexora::editor::imgui::RecoveryChoice::Discard));
  assert(host.TakeRecoveryChoice() == nexora::editor::imgui::RecoveryChoice::Discard);
  assert(host.TakeRecoveryChoice() == nexora::editor::imgui::RecoveryChoice::None);
  std::filesystem::remove_all(recovery_root);
  host.ReleaseRenderer(*device);
  device->DestroyTexture(user_texture);
  device->DestroyTexture(target);
}
