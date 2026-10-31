#pragma once

#include "Nexora/Editor/EditorWorkspace.h"
#include <cmath>
#include <cstddef>
#include <imgui.h>
#include <initializer_list>

namespace nexora::editor::imgui::detail {

struct ThemeOutput final {
  bool targetEncodesSrgb{true};
};

// The Editor's mixed scene/UI surface may use an sRGB or legacy UNORM attachment. Decode
// the display-referred palette only when the actual native target performs the output encode.
inline ImVec4 LinearColor(ImVec4 color) {
  const auto decode = [](float v) {
    return v <= .04045F ? v / 12.92F : std::pow((v + .055F) / 1.055F, 2.4F);
  };
  return {decode(color.x), decode(color.y), decode(color.z), color.w};
}

inline ImVec4 PaletteColor(ImVec4 color) {
  const auto *output = static_cast<const ThemeOutput *>(ImGui::GetIO().UserData);
  return !output || output->targetEncodesSrgb ? LinearColor(color) : color;
}

inline bool LightTheme() { return ImGui::GetStyleColorVec4(ImGuiCol_WindowBg).x > 0.3F; }
inline ImVec4 ErrorColor() {
  return PaletteColor(LightTheme() ? ImVec4(.66F, .08F, .08F, 1) : ImVec4(1, .45F, .45F, 1));
}
inline ImVec4 WarningColor() {
  return PaletteColor(LightTheme() ? ImVec4(.48F, .28F, .02F, 1) : ImVec4(1, .75F, .35F, 1));
}
inline ImVec4 SuccessColor() {
  return PaletteColor(LightTheme() ? ImVec4(.08F, .38F, .12F, 1) : ImVec4(.45F, .85F, .45F, 1));
}
inline ImVec4 AxisColor(std::size_t axis) {
  const ImVec4 dark[] = {{.95F, .35F, .35F, 1}, {.4F, .85F, .4F, 1}, {.4F, .65F, 1, 1}};
  const ImVec4 light[] = {{.65F, .08F, .08F, 1}, {.08F, .38F, .12F, 1}, {.08F, .29F, .65F, 1}};
  return PaletteColor(LightTheme() ? light[axis] : dark[axis]);
}

// Editor-owned neutral palettes inspired by Unity's Light/Professional surfaces. Start from a
// complete ImGui palette for future color slots; changing colors never rescales layout or fonts.
inline void ApplyTheme(EditorColorTheme theme) {
  const bool light = theme == EditorColorTheme::Light;
  if (light)
    ImGui::StyleColorsLight();
  else
    ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 4.0F;
  style.FrameRounding = 3.0F;
  auto &c = style.Colors;
  const auto gray = [](float v, float alpha = 1.0F) { return ImVec4(v, v, v, alpha); };
  const auto pair = [&](ImGuiCol slot, float dark, float bright) {
    c[slot] = gray(light ? bright : dark);
  };
  pair(ImGuiCol_Text, .87F, .10F);
  // Secondary paths/hints are meaningful text, including the initial Project Browser.
  pair(ImGuiCol_TextDisabled, .70F, .30F);
  pair(ImGuiCol_WindowBg, .22F, .76F);
  c[ImGuiCol_ChildBg] = gray(0, 0);
  pair(ImGuiCol_PopupBg, .18F, .88F);
  pair(ImGuiCol_Border, .12F, .50F);
  c[ImGuiCol_BorderShadow] = gray(0, 0);
  pair(ImGuiCol_FrameBg, .16F, .91F);
  pair(ImGuiCol_FrameBgHovered, .28F, .97F);
  pair(ImGuiCol_FrameBgActive, .12F, 1);
  pair(ImGuiCol_TitleBg, .16F, .67F);
  pair(ImGuiCol_TitleBgActive, .18F, .72F);
  pair(ImGuiCol_TitleBgCollapsed, .16F, .67F);
  pair(ImGuiCol_MenuBarBg, .18F, .80F);
  pair(ImGuiCol_ScrollbarBg, .19F, .73F);
  pair(ImGuiCol_ScrollbarGrab, .36F, .55F);
  pair(ImGuiCol_ScrollbarGrabHovered, .45F, .47F);
  pair(ImGuiCol_ScrollbarGrabActive, .52F, .40F);
  pair(ImGuiCol_Button, .30F, .86F);
  pair(ImGuiCol_ButtonHovered, .38F, .95F);
  pair(ImGuiCol_ButtonActive, .18F, .66F);
  const ImVec4 accent = light ? ImVec4(.42F, .62F, .83F, 1) : ImVec4(.17F, .36F, .53F, 1);
  c[ImGuiCol_Header] = accent;
  c[ImGuiCol_HeaderActive] = accent;
  c[ImGuiCol_HeaderHovered] = light ? ImVec4(.40F, .61F, .81F, 1) : ImVec4(.22F, .44F, .63F, 1);
  pair(ImGuiCol_Separator, .12F, .55F);
  c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_SeparatorActive] = accent;
  const ImVec4 indicator = light ? ImVec4(.04F, .24F, .46F, 1) : ImVec4(.39F, .68F, .94F, 1);
  for (const auto slot :
       {ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive,
        ImGuiCol_ResizeGripHovered, ImGuiCol_ResizeGripActive, ImGuiCol_TabSelectedOverline,
        ImGuiCol_TabDimmedSelectedOverline, ImGuiCol_NavCursor, ImGuiCol_TextLink})
    c[slot] = indicator;
  pair(ImGuiCol_Tab, .17F, .68F);
  pair(ImGuiCol_TabSelected, .27F, .84F);
  pair(ImGuiCol_TabDimmed, .17F, .68F);
  pair(ImGuiCol_TabDimmedSelected, .24F, .79F);
  pair(ImGuiCol_TabHovered, .35F, .92F);
  pair(ImGuiCol_DockingEmptyBg, .13F, .62F);
  c[ImGuiCol_DockingPreview] = ImVec4(indicator.x, indicator.y, indicator.z, .65F);
  pair(ImGuiCol_TableHeaderBg, .18F, .70F);
  pair(ImGuiCol_TableBorderStrong, .12F, .50F);
  pair(ImGuiCol_TableBorderLight, .28F, .66F);
  c[ImGuiCol_TableRowBg] = gray(0, 0);
  c[ImGuiCol_TableRowBgAlt] = gray(light ? 0.0F : 1.0F, .035F);
  c[ImGuiCol_PlotLines] = indicator;
  c[ImGuiCol_PlotHistogram] = light ? ImVec4(.48F, .28F, .02F, 1) : ImVec4(1, .75F, .35F, 1);
  c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, .45F);
  c[ImGuiCol_ModalWindowDimBg] = gray(0, .40F);
  for (auto &color : c)
    color = PaletteColor(color);
}
} // namespace nexora::editor::imgui::detail
