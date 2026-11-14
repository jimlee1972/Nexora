#include "EditorImGuiTestAccess.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::editor;
using namespace nexora::editor::imgui;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
ImGuiWindow *Window(const char *name) {
  auto *window = ImGui::FindWindowByName(name);
  Require(window != nullptr, "panel missing");
  return window;
}
void Draw(EditorImGuiHost &host, ProductShell &shell, int count = 6) {
  ProjectContentSession content;
  for (int i = 0; i < count; ++i) {
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, nullptr, &content);
    static_cast<void>(host.EndFrame());
  }
}
void Check(float dpi, const char *selected, const char *other) {
  std::string saved;
  ProductShell shell;
  {
    EditorImGuiHost host;
    EditorImGuiTestAccess::ConfigureSyntheticInput(host);
    host.SetDisplay(1600, 1000, dpi);
    Draw(host, shell);
    auto *content = Window("Content###nexora.content");
    ImGui::SetWindowDock(content, 0, ImGuiCond_Always);
    ImGui::SetWindowPos(content, {590, 620});
    ImGui::SetWindowSize(content, {716, 180});
    ImGui::SetWindowFocus(selected);
    Draw(host, shell);
    Require(Window(selected)->DockNode && Window(selected)->DockTabIsVisible,
            "selected tab fixture failed");
    saved = host.SaveLayout();
  }
  // A fresh context is essential: loading into the old context does not exercise appearing panels.
  for (int restart = 0; restart < 3; ++restart) {
    EditorImGuiHost host;
    EditorImGuiTestAccess::ConfigureSyntheticInput(host);
    host.SetDisplay(1600, 1000, dpi);
    Require(host.LoadLayout(saved), "saved layout rejected");
    Draw(host, shell);
    auto *selected_window = Window(selected);
    Require(selected_window->DockNode && selected_window->DockTabIsVisible &&
                selected_window->DockNode->SelectedTabId == selected_window->TabId,
            "restart changed the saved active dock tab");
    const auto *content = Window("Content###nexora.content");
    Require(!content->DockNode && content->Pos.x == 590 && content->Pos.y == 620 &&
                content->Size.x == 716 && content->Size.y == 180,
            "restart changed floating Content geometry");
    const auto restored = host.SaveLayout();
    Require(restored == saved, "restart changed serialized layout");
    // Explicit user focus must still select another dock tab after restoration.
    ImGui::SetWindowFocus(other);
    Draw(host, shell);
    Require(Window(other)->DockTabIsVisible, "explicit tab focus stopped working");
  }
}
} // namespace

int main() {
  try {
    for (const float dpi : {1.0F, 1.25F, 1.5F, 2.0F}) {
      Check(dpi, "Hierarchy###nexora.hierarchy", "Project###nexora.project");
      Check(dpi, "Project###nexora.project", "Hierarchy###nexora.hierarchy");
      Check(dpi, "Scene###nexora.scene", "Game###nexora.game");
      Check(dpi, "Game###nexora.game", "Scene###nexora.scene");
    }
    std::cout << "Saved dock selection and floating geometry survive fresh UI contexts.\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
