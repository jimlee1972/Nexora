#include "Nexora/EditorImGui/TwoDPanel.h"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
using namespace nexora;
namespace {
void Check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
void Run() {
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.DisplaySize = {1000, 900};
  io.DeltaTime = 1.0F / 60;
  io.IniFilename = nullptr;
  io.ConfigInputTrickleEventQueue = false;
  unsigned char *pixels = nullptr;
  int w{}, h{};
  io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  editor::TwoDDocument doc;
  twod::Image image{1, 1, 1, twod::Bytes(4, std::byte{255})};
  auto atlas = twod::PackAtlas({10, 11}, std::span(&image, 1), 8);
  Check(atlas && doc.ReplaceAtlas(*atlas), "panel fixture");
  Check(doc.ReplaceClip({{12, 13}, atlas->id, true, {{1, 0.1, "tick"}}}), "panel clip fixture");
  const auto initial = doc.Save();
  twod::Sprite s;
  s.atlas = atlas->id;
  s.region = 1;
  Check(bool(doc.AddSprite(s)), "sprite fixture");
  auto withSprite = doc.Save();
  editor::imgui::TwoDPanelState panel;
  const auto frame = [&](bool allowed) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({950, 850});
    editor::imgui::DrawTwoDPanel(doc, panel, allowed, 1, 1.0F / 60);
    ImGui::Render();
  };
  frame(true);
  frame(true);
  const auto &style = ImGui::GetStyle();
  const float y = ImGui::GetFontSize() + style.FramePadding.y * 2 + style.WindowPadding.y +
                  ImGui::GetFontSize() + style.ItemSpacing.y + ImGui::GetFrameHeight() / 2;
  const float undoX =
      style.WindowPadding.x + ImGui::CalcTextSize("Undo").x / 2 + style.FramePadding.x;
  const auto click = [&](float x, bool allowed) {
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(0, true);
    frame(allowed);
    io.AddMouseButtonEvent(0, false);
    frame(allowed);
  };
  click(undoX, true);
  Check(doc.Save() == initial, "graphical Undo button");
  const auto undoWidth = ImGui::CalcTextSize("Undo").x + style.FramePadding.x * 2;
  click(undoX + undoWidth + style.ItemSpacing.x, true);
  Check(doc.Save() == withSprite, "graphical Redo button");
  click(undoX, false);
  Check(doc.Save() == withSprite, "blocked graphical mutation");
  Check(!panel.saveRequested && !panel.importAtlasRequested, "spurious IO request");
  for (auto tab : {editor::imgui::TwoDPanelTab::Scene, editor::imgui::TwoDPanelTab::Atlas,
                   editor::imgui::TwoDPanelTab::Animation, editor::imgui::TwoDPanelTab::Tilemap}) {
    panel.requestedTab = tab;
    frame(true);
    frame(true);
  }
  Check(panel.preview.RegionId() == 1U, "graphical animation preview");
  ImGui::DestroyContext();
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "2D panel edit history and permissions passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
