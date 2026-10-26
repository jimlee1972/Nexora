#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nexora::editor::imgui::detail {

// Editor-owned vector artwork: no font glyphs, textures or external asset lifetime.
enum class EditorIcon {
  Add,
  AddChild,
  Delete,
  Copy,
  Cut,
  Paste,
  Duplicate,
  Rename,
  Up,
  Down,
  Clear,
  FrameSelected,
  FrameAll,
  SelectAll,
  Open,
  Save,
  SaveAll,
  Close,
  Cancel,
  Confirm,
  Reset,
  Undo,
  Redo,
  Play,
  Pause,
  Stop,
  Step,
  Move,
  Rotate,
  Scale,
  Select,
  Refresh,
  Import,
  Export,
  Build,
  Compare,
  Inspect,
  Settings,
  Link,
  Unlink,
  Apply,
  Reload,
  Search,
  Folder,
  File,
  Lock,
  Unlock,
  Info,
  Warning,
  Camera,
  Light,
  Mesh,
  Publish,
  Git,
  Download,
  Upload,
  Earlier,
  Paint,
  Erase,
  New,
  Back,
  Forward,
  Collapse,
  Expand,
  Revert,
  Merge,
  Restore,
  Reimport,
  Record,
  Browse,
  Startup,
  Preview,
  Pin,
  Unpin,
  Filter,
  Sort,
  Next,
  Previous,
  Eye,
  EyeOff,
  Atlas,
  Animation,
  Tilemap
};

inline void DrawEditorIcon(ImDrawList &draw, EditorIcon icon, ImVec2 origin, float size,
                           ImU32 color) {
  const float unit = size / 24.0F;
  const float stroke = std::max(1.0F, unit * 1.65F);
  const auto point = [&](float x, float y) {
    return ImVec2(origin.x + x * unit, origin.y + y * unit);
  };
  const auto line = [&](float x, float y, float xx, float yy) {
    draw.AddLine(point(x, y), point(xx, yy), color, stroke);
  };
  const auto rect = [&](float x, float y, float xx, float yy) {
    draw.AddRect(point(x, y), point(xx, yy), color, unit, 0, stroke);
  };
  const auto circle = [&](float x, float y, float r) {
    draw.AddCircle(point(x, y), r * unit, color, 20, stroke);
  };
  const auto dot = [&](float x, float y, float r = 1.5F) {
    draw.AddCircleFilled(point(x, y), r * unit, color, 12);
  };
  const auto triangle = [&](float x, float y, float xx, float yy, float xxx, float yyy) {
    draw.AddTriangleFilled(point(x, y), point(xx, yy), point(xxx, yyy), color);
  };
  const auto plus = [&](float x, float y) {
    line(x - 4, y, x + 4, y);
    line(x, y - 4, x, y + 4);
  };
  const auto check = [&] {
    line(5, 12, 10, 17);
    line(10, 17, 20, 6);
  };
  const auto arrow = [&](bool up) {
    const float top = up ? 4.0F : 20.0F;
    const float end = up ? 19.0F : 5.0F;
    line(12, end, 12, top);
    line(12, top, 7, up ? 9.0F : 15.0F);
    line(12, top, 17, up ? 9.0F : 15.0F);
  };
  const auto file = [&] {
    line(5, 3, 14, 3);
    line(14, 3, 19, 8);
    line(19, 8, 19, 21);
    line(19, 21, 5, 21);
    line(5, 21, 5, 3);
    line(14, 3, 14, 8);
    line(14, 8, 19, 8);
  };
  const auto refresh = [&] {
    draw.PathArcTo(point(12, 12), 7 * unit, 0.55F, 5.6F, 22);
    draw.PathStroke(color, 0, stroke);
    triangle(18, 4, 21, 10, 14, 9);
  };
  const auto folder = [&] {
    line(3, 19, 3, 5);
    line(3, 5, 10, 5);
    line(10, 5, 13, 8);
    line(13, 8, 21, 8);
    line(21, 8, 21, 19);
    line(21, 19, 3, 19);
  };
  switch (icon) {
  case EditorIcon::Add:
  case EditorIcon::New:
    plus(12, 12);
    break;
  case EditorIcon::AddChild:
    line(4, 3, 4, 13);
    line(4, 13, 10, 13);
    rect(2, 2, 7, 7);
    plus(16, 15);
    break;
  case EditorIcon::Delete:
    line(4, 6, 20, 6);
    rect(7, 3, 17, 6);
    line(6, 7, 7, 21);
    line(7, 21, 17, 21);
    line(17, 21, 18, 7);
    line(10, 10, 10, 17);
    line(14, 10, 14, 17);
    break;
  case EditorIcon::Copy:
  case EditorIcon::Duplicate:
    rect(8, 7, 21, 21);
    line(16, 4, 16, 2);
    line(16, 2, 3, 2);
    line(3, 2, 3, 16);
    line(3, 16, 5, 16);
    if (icon == EditorIcon::Duplicate)
      plus(14, 14);
    break;
  case EditorIcon::Cut:
    circle(6, 17, 3);
    circle(18, 17, 3);
    line(7, 14, 19, 3);
    line(17, 14, 5, 3);
    break;
  case EditorIcon::Paste:
    rect(5, 5, 19, 21);
    rect(9, 2, 15, 7);
    line(9, 12, 15, 12);
    line(9, 16, 15, 16);
    break;
  case EditorIcon::Rename:
    line(5, 17, 16, 6);
    line(16, 6, 19, 9);
    line(19, 9, 8, 20);
    line(8, 20, 4, 21);
    line(4, 21, 5, 17);
    line(4, 3, 12, 3);
    break;
  case EditorIcon::Up:
  case EditorIcon::Upload:
    arrow(true);
    if (icon == EditorIcon::Upload)
      line(4, 22, 20, 22);
    break;
  case EditorIcon::Down:
  case EditorIcon::Download:
    arrow(false);
    if (icon == EditorIcon::Download)
      line(4, 22, 20, 22);
    break;
  case EditorIcon::Close:
  case EditorIcon::Cancel:
  case EditorIcon::Clear:
    line(6, 6, 18, 18);
    line(6, 18, 18, 6);
    if (icon == EditorIcon::Clear)
      circle(12, 12, 10);
    break;
  case EditorIcon::Confirm:
  case EditorIcon::Apply:
    check();
    if (icon == EditorIcon::Apply)
      line(4, 21, 20, 21);
    break;
  case EditorIcon::FrameSelected:
  case EditorIcon::FrameAll:
  case EditorIcon::SelectAll:
    line(3, 9, 3, 3);
    line(3, 3, 9, 3);
    line(15, 3, 21, 3);
    line(21, 3, 21, 9);
    line(21, 15, 21, 21);
    line(21, 21, 15, 21);
    line(9, 21, 3, 21);
    line(3, 21, 3, 15);
    if (icon == EditorIcon::FrameSelected)
      rect(9, 9, 15, 15);
    else if (icon == EditorIcon::FrameAll) {
      rect(7, 7, 12, 12);
      rect(13, 13, 18, 18);
    } else
      check();
    break;
  case EditorIcon::Folder:
  case EditorIcon::Open:
  case EditorIcon::Browse:
    folder();
    if (icon == EditorIcon::Open) {
      line(4, 18, 8, 11);
      line(8, 11, 22, 11);
      line(22, 11, 19, 18);
    } else if (icon == EditorIcon::Browse) {
      dot(8, 14, 1);
      dot(12, 14, 1);
      dot(16, 14, 1);
    }
    break;
  case EditorIcon::File:
    file();
    break;
  case EditorIcon::Save:
  case EditorIcon::SaveAll:
    rect(4, 3, 20, 21);
    rect(8, 3, 16, 9);
    rect(8, 14, 16, 21);
    if (icon == EditorIcon::SaveAll) {
      line(1, 6, 1, 23);
      line(1, 23, 17, 23);
    }
    break;
  case EditorIcon::Undo:
  case EditorIcon::Revert:
  case EditorIcon::Restore:
    line(4, 9, 14, 9);
    line(14, 9, 19, 13);
    line(19, 13, 19, 18);
    line(4, 9, 9, 4);
    line(4, 9, 9, 14);
    if (icon == EditorIcon::Restore)
      line(6, 21, 16, 21);
    if (icon == EditorIcon::Revert)
      line(15, 3, 21, 3);
    break;
  case EditorIcon::Redo:
    line(20, 9, 10, 9);
    line(10, 9, 5, 13);
    line(5, 13, 5, 18);
    line(20, 9, 15, 4);
    line(20, 9, 15, 14);
    break;
  case EditorIcon::Play:
  case EditorIcon::Preview:
    triangle(7, 3, 7, 21, 21, 12);
    if (icon == EditorIcon::Preview)
      line(2, 4, 2, 20);
    break;
  case EditorIcon::Pause:
    rect(5, 4, 9, 20);
    rect(15, 4, 19, 20);
    break;
  case EditorIcon::Stop:
    draw.AddRectFilled(point(5, 5), point(19, 19), color, unit);
    break;
  case EditorIcon::Step:
  case EditorIcon::Next:
  case EditorIcon::Forward:
    triangle(5, 5, 5, 19, 16, 12);
    if (icon != EditorIcon::Forward)
      line(19, 5, 19, 19);
    break;
  case EditorIcon::Earlier:
  case EditorIcon::Previous:
  case EditorIcon::Back:
    triangle(19, 5, 19, 19, 8, 12);
    if (icon != EditorIcon::Back)
      line(5, 5, 5, 19);
    break;
  case EditorIcon::Move:
    line(3, 12, 21, 12);
    line(12, 3, 12, 21);
    triangle(2, 12, 7, 9, 7, 15);
    triangle(22, 12, 17, 9, 17, 15);
    triangle(12, 2, 9, 7, 15, 7);
    triangle(12, 22, 9, 17, 15, 17);
    break;
  case EditorIcon::Rotate:
  case EditorIcon::Refresh:
  case EditorIcon::Reload:
  case EditorIcon::Reset:
    refresh();
    if (icon == EditorIcon::Reset)
      dot(12, 12);
    else if (icon == EditorIcon::Reload)
      line(9, 12, 15, 12);
    else if (icon == EditorIcon::Rotate) {
      line(12, 9, 12, 15);
      line(9, 12, 15, 12);
    }
    break;
  case EditorIcon::Scale:
    rect(3, 14, 10, 21);
    line(10, 14, 21, 3);
    line(14, 3, 21, 3);
    line(21, 3, 21, 10);
    break;
  case EditorIcon::Select:
    line(5, 3, 6, 20);
    line(6, 20, 11, 15);
    line(11, 15, 18, 16);
    line(18, 16, 5, 3);
    line(12, 16, 16, 22);
    break;
  case EditorIcon::Import:
  case EditorIcon::Reimport:
    line(3, 4, 3, 20);
    line(3, 20, 18, 20);
    line(3, 4, 11, 4);
    line(22, 10, 9, 10);
    line(9, 10, 14, 5);
    line(9, 10, 14, 15);
    if (icon == EditorIcon::Reimport)
      line(17, 17, 22, 17);
    break;
  case EditorIcon::Export:
  case EditorIcon::Publish:
    rect(3, 10, 15, 21);
    line(10, 14, 21, 3);
    line(14, 3, 21, 3);
    line(21, 3, 21, 10);
    if (icon == EditorIcon::Publish)
      line(3, 5, 9, 5);
    break;
  case EditorIcon::Build:
    line(5, 3, 20, 18);
    line(3, 5, 18, 20);
    line(5, 3, 3, 5);
    line(20, 18, 18, 20);
    rect(11, 3, 20, 7);
    line(15, 7, 5, 20);
    break;
  case EditorIcon::Compare:
    rect(2, 4, 9, 20);
    rect(15, 4, 22, 20);
    line(10, 9, 14, 9);
    line(10, 15, 14, 15);
    break;
  case EditorIcon::Inspect:
  case EditorIcon::Search:
    circle(10, 10, 7);
    line(15, 15, 21, 21);
    if (icon == EditorIcon::Inspect)
      plus(10, 10);
    break;
  case EditorIcon::Settings:
    circle(12, 12, 7);
    circle(12, 12, 3);
    for (int i = 0; i < 8; ++i) {
      const float angle = static_cast<float>(i) * 0.785398163F;
      line(12 + 7 * std::cos(angle), 12 + 7 * std::sin(angle), 12 + 10 * std::cos(angle),
           12 + 10 * std::sin(angle));
    }
    break;
  case EditorIcon::Link:
  case EditorIcon::Unlink:
    circle(8, 15, 5);
    circle(16, 7, 5);
    line(8, 15, 16, 7);
    if (icon == EditorIcon::Unlink)
      line(3, 3, 21, 21);
    break;
  case EditorIcon::Lock:
  case EditorIcon::Unlock:
    rect(5, 11, 19, 21);
    draw.PathArcTo(point(icon == EditorIcon::Lock ? 12.0F : 16.0F, 10), 5 * unit, 3.14159265F,
                   6.2831853F, 12);
    draw.PathStroke(color, 0, stroke);
    dot(12, 16, 1);
    break;
  case EditorIcon::Info:
    circle(12, 12, 10);
    dot(12, 7, 1);
    line(12, 11, 12, 18);
    break;
  case EditorIcon::Warning:
    draw.AddTriangle(point(12, 2), point(23, 21), point(1, 21), color, stroke);
    line(12, 8, 12, 14);
    dot(12, 18, 1);
    break;
  case EditorIcon::Camera:
    rect(3, 7, 21, 20);
    rect(7, 4, 14, 7);
    circle(12, 13, 4);
    break;
  case EditorIcon::Light:
    circle(12, 9, 5);
    line(9, 14, 9, 19);
    line(15, 14, 15, 19);
    line(9, 19, 15, 19);
    line(10, 22, 14, 22);
    line(1, 8, 3, 8);
    line(21, 8, 23, 8);
    line(12, 0, 12, 2);
    break;
  case EditorIcon::Mesh:
    line(12, 2, 22, 7);
    line(22, 7, 22, 17);
    line(22, 17, 12, 22);
    line(12, 22, 2, 17);
    line(2, 17, 2, 7);
    line(2, 7, 12, 2);
    line(2, 7, 12, 12);
    line(12, 12, 22, 7);
    line(12, 12, 12, 22);
    break;
  case EditorIcon::Git:
  case EditorIcon::Merge:
    circle(6, 4, 2);
    circle(6, 20, 2);
    circle(18, 6, 2);
    line(6, 6, 6, 18);
    line(18, 8, 18, 12);
    line(18, 12, 6, 17);
    if (icon == EditorIcon::Merge)
      triangle(7, 15, 12, 15, 9, 20);
    break;
  case EditorIcon::Paint:
    line(7, 13, 18, 2);
    line(18, 2, 22, 6);
    line(22, 6, 11, 17);
    draw.AddBezierCubic(point(7, 13), point(0, 14), point(8, 19), point(2, 22), color, stroke);
    line(2, 22, 11, 17);
    break;
  case EditorIcon::Erase:
    line(3, 15, 14, 3);
    line(14, 3, 22, 11);
    line(22, 11, 12, 21);
    line(12, 21, 8, 21);
    line(8, 21, 3, 15);
    line(7, 11, 16, 18);
    break;
  case EditorIcon::Collapse:
    line(5, 15, 12, 8);
    line(12, 8, 19, 15);
    break;
  case EditorIcon::Expand:
    line(5, 8, 12, 15);
    line(12, 15, 19, 8);
    break;
  case EditorIcon::Record:
    draw.AddCircleFilled(point(12, 12), 8 * unit, color, 24);
    break;
  case EditorIcon::Startup:
    triangle(12, 2, 3, 12, 21, 12);
    rect(6, 12, 18, 22);
    rect(10, 16, 14, 22);
    break;
  case EditorIcon::Pin:
  case EditorIcon::Unpin:
    line(8, 3, 18, 3);
    line(9, 3, 9, 10);
    line(17, 3, 17, 10);
    line(9, 10, 5, 15);
    line(5, 15, 21, 15);
    line(21, 15, 17, 10);
    line(13, 15, 13, 23);
    if (icon == EditorIcon::Unpin)
      line(2, 2, 22, 22);
    break;
  case EditorIcon::Filter:
    line(3, 4, 21, 4);
    line(21, 4, 14, 12);
    line(14, 12, 14, 20);
    line(14, 20, 10, 22);
    line(10, 22, 10, 12);
    line(10, 12, 3, 4);
    break;
  case EditorIcon::Sort:
    line(3, 5, 17, 5);
    line(3, 11, 13, 11);
    line(3, 17, 9, 17);
    line(19, 9, 19, 22);
    line(15, 18, 19, 22);
    line(23, 18, 19, 22);
    break;
  case EditorIcon::Eye:
  case EditorIcon::EyeOff:
    draw.AddBezierCubic(point(2, 12), point(7, 2), point(17, 2), point(22, 12), color, stroke);
    draw.AddBezierCubic(point(2, 12), point(7, 22), point(17, 22), point(22, 12), color, stroke);
    circle(12, 12, 3);
    if (icon == EditorIcon::EyeOff)
      line(2, 2, 22, 22);
    break;
  case EditorIcon::Atlas:
  case EditorIcon::Tilemap:
    rect(3, 3, 21, 21);
    line(3, 12, 21, 12);
    line(12, 3, 12, 21);
    if (icon == EditorIcon::Atlas)
      rect(6, 6, 9, 9);
    else {
      line(3, 7, 21, 7);
      line(7, 3, 7, 21);
    }
    break;
  case EditorIcon::Animation:
    rect(2, 4, 22, 20);
    line(6, 4, 6, 20);
    line(18, 4, 18, 20);
    line(2, 8, 6, 8);
    line(2, 16, 6, 16);
    line(18, 8, 22, 8);
    line(18, 16, 22, 16);
    triangle(9, 8, 9, 16, 15, 12);
    break;
  }
}

inline float IconButtonExtent() {
  return std::max(ImGui::GetFontSize() * 1.75F,
                  ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0F);
}

inline bool DrawIconButton(const char *label, EditorIcon icon, bool selected, bool small,
                           const char *shortcut) {
  ImGuiContext &context = *GImGui;
  const ImVec4 foreground = ImGui::GetStyleColorVec4(ImGuiCol_Text);
  const float edge = IconButtonExtent();
  // Retain the complete original label for ImGui's identity and keyboard/navigation behavior.
  // A transparent text color suppresses the stock caption before drawing our vector artwork.
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(foreground.x, foreground.y, foreground.z, 0));
  if (selected)
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  const bool pressed =
      ImGui::ButtonEx(label, ImVec2(edge, edge), small ? ImGuiButtonFlags_AlignTextBaseLine : 0);
  if (selected)
    ImGui::PopStyleColor();
  ImGui::PopStyleColor();
  const ImGuiLastItemData button_item = context.LastItemData;
  if (ImGui::IsItemVisible()) {
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const float artwork = edge * 0.76F;
    const ImVec2 origin(minimum.x + (maximum.x - minimum.x - artwork) * 0.5F,
                        minimum.y + (maximum.y - minimum.y - artwork) * 0.5F);
    DrawEditorIcon(*ImGui::GetWindowDrawList(), icon, origin, artwork,
                   ImGui::GetColorU32(foreground));
    if (selected) {
      const float marker = std::max(1.0F, edge / 16.0F);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(minimum.x + marker * 2, maximum.y - marker),
                                          ImVec2(maximum.x - marker * 2, maximum.y - marker),
                                          ImGui::GetColorU32(ImGuiCol_Text), marker);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    const char *end = std::strstr(label, "##");
    if (end == nullptr)
      end = label + std::strlen(label);
    if (end != label && ImGui::BeginTooltip()) {
      ImGui::TextUnformatted(label, end);
      if (shortcut != nullptr)
        ImGui::TextDisabled("%s", shortcut);
      ImGui::EndTooltip();
    }
  }
  // Tooltips submit their own items: callers must still observe the actual button.
  context.LastItemData = button_item;
  return pressed;
}

inline bool IconButton(const char *label, EditorIcon icon, bool selected = false,
                       const char *shortcut = nullptr) {
  return DrawIconButton(label, icon, selected, false, shortcut);
}

inline bool SmallIconButton(const char *label, EditorIcon icon, bool selected = false,
                            const char *shortcut = nullptr) {
  return DrawIconButton(label, icon, selected, true, shortcut);
}

inline bool IconRadioButton(const char *label, EditorIcon icon, bool selected) {
  return DrawIconButton(label, icon, selected, false, nullptr);
}

} // namespace nexora::editor::imgui::detail
