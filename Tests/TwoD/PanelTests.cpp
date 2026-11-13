#include "EditorIcons.h"
#include "Nexora/EditorImGui/TwoDPanel.h"
#include <cmath>
#include <imgui.h>
#include <iostream>
#include <limits>
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
  const auto spriteId = doc.AddSprite(s);
  Check(bool(spriteId), "sprite fixture");
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
  const float buttonWidth = editor::imgui::detail::IconButtonExtent();
  const float y = ImGui::GetFontSize() + style.FramePadding.y * 2 + style.WindowPadding.y +
                  ImGui::GetFontSize() + style.ItemSpacing.y + buttonWidth / 2;
  const float undoX = style.WindowPadding.x + buttonWidth / 2;
  const auto click = [&](float x, bool allowed) {
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(0, true);
    frame(allowed);
    io.AddMouseButtonEvent(0, false);
    frame(allowed);
  };
  click(undoX, true);
  Check(doc.Save() == initial, "graphical Undo button");
  click(undoX + buttonWidth + style.ItemSpacing.x, true);
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
  Check(doc.SetCollider(*spriteId, {}), "preview collider");
  const auto authored = doc.Save();
  panel.camera.verticalSize = 0;
  panel.camera.center = {std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity()};
  panel.requestedTab = editor::imgui::TwoDPanelTab::Scene;
  frame(true);
  frame(true);
  Check(std::isfinite(panel.camera.verticalSize) && panel.camera.verticalSize > 0 &&
            std::isfinite(panel.camera.center.x) && std::isfinite(panel.camera.center.y) &&
            doc.Save() == authored,
        "invalid preview camera or authoring mutation");

  // Source import emits an owner request, including the same access checks as other imports.
  const auto sourceImport = [&](bool allowed) {
    const float x = undoX + 2 * (buttonWidth + style.ItemSpacing.x);
    const float importY = y + buttonWidth + style.ItemSpacing.y;
    io.AddMousePosEvent(x, importY);
    io.AddMouseButtonEvent(0, true);
    frame(allowed);
    io.AddMouseButtonEvent(0, false);
    frame(allowed);
  };
  sourceImport(true);
  Check(panel.importSpriteRequested, "graphical source import request");
  panel.importSpriteRequested = false;
  sourceImport(false);
  Check(!panel.importSpriteRequested, "unauthorized source import request");
  doc.SetReadOnly(true);
  sourceImport(true);
  Check(!panel.importSpriteRequested, "read-only source import request");
  doc.SetReadOnly(false);
  twod::SpriteAsset source;
  source.id = {14, 15};
  source.atlas = atlas->id;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = source.trimWidth = source.trimHeight = 1;
  source.pivotX = source.pivotY = 2;
  source.pixelsPerUnit = 1;
  auto bound = *doc.Scene().Find(*spriteId);
  bound.source = source.id;
  Check(doc.ReplaceSpriteSource(source) && doc.EditSprite(*spriteId, bound, {}),
        "panel source binding");
  panel.selected = *spriteId;
  panel.camera.verticalSize = 10;
  panel.camera.center = {};
  const auto previewX = [&] {
    frame(true);
    const auto *data = ImGui::GetDrawData();
    for (int i = 0; i < data->CmdListsCount; ++i) {
      const auto *list = data->CmdLists[i];
      for (const auto &command : list->CmdBuffer)
        if (command.GetTexID() == 1 && command.ElemCount >= 6)
          return list->VtxBuffer[list->IdxBuffer[command.IdxOffset] + command.VtxOffset].pos.x;
    }
    throw std::runtime_error("source preview missing");
  };
  const auto beforePivot = previewX();
  source.pivotX = 3;
  Check(doc.ReplaceSpriteSource(source) && previewX() < beforePivot,
        "panel preview ignored retained source geometry");

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
