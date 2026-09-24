#include "Nexora/EditorImGui/EditorImGui.h"

#include <array>
#include <cassert>

int main() {
  nexora::editor::imgui::EditorImGuiHost host;
  host.SetDisplay(1280.0F, 720.0F, 1.5F);
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, 320, 240},
      Nexora::Window::WindowEvent{{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, 'N', 0},
  };
  host.ProcessEvents(events);
  host.BeginFrame();
  nexora::editor::ProductShell shell;
  host.DrawProductShell(shell);
  const auto metrics = host.EndFrame();
  assert(metrics.command_lists > 0);
  assert(metrics.vertices > 0);
  assert(metrics.indices > 0);
}
