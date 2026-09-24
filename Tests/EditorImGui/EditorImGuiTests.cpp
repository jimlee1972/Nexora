#include "Nexora/EditorImGui/EditorImGui.h"

#include <array>
#include <cassert>

int main() {
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Editor ImGui contract");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto root = scene.Create("Scene Root");
  assert(root != 0 && scene.Nodes().size() == 1);
  host.SetDisplay(1280.0F, 720.0F, 1.5F);
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, 320, 240},
      Nexora::Window::WindowEvent{{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, 'N', 0},
  };
  host.ProcessEvents(events);
  host.BeginFrame();
  nexora::editor::ProductShell shell;
  host.DrawProductShell(shell, &scene);
  const auto metrics = host.EndFrame();
  assert(metrics.command_lists > 0);
  assert(metrics.vertices > 0);
  assert(metrics.indices > 0);
  auto device = nexora::rhi::CreateValidationDevice();
  const auto target =
      device->CreateTexture({1280, 720, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::Undefined, "Editor ImGui offscreen"});
  const auto draws =
      host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::Undefined, false);
  const auto diagnostics = device->Diagnostics();
  assert(draws > 0 && diagnostics.draw_calls == draws && diagnostics.validation_errors == 0);
  device->DestroyTexture(target);
}
