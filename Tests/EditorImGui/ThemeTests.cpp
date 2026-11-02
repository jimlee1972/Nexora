#include "EditorImGuiTestAccess.h"
#include "EditorTheme.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
float Luminance(ImVec4 c) {
  // The theme's RGB channels already represent linear light for RenderUi.
  return .2126F * c.x + .7152F * c.y + .0722F * c.z;
}
float Contrast(ImVec4 a, ImVec4 b) {
  const float x = Luminance(a), y = Luminance(b);
  return (std::max(x, y) + .05F) / (std::min(x, y) + .05F);
}
} // namespace

int main(int argc, char **argv) {
  try {
    using namespace nexora::editor;
    using namespace nexora::editor::imgui;
    using Access = EditorImGuiTestAccess;
    const float scale = argc > 1 ? static_cast<float>(std::atoi(argv[1])) : 1.0F;
    const auto root = std::filesystem::temp_directory_path() /
                      ("nexora-appearance-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    test::TemporaryDirectoryCleanup cleanup(root);
    const auto path = root / "user/appearance";
    std::string error;
    Require(LoadEditorColorTheme(path, &error) == EditorColorTheme::Dark && error.empty(),
            "missing preference must default to Dark");
    Require(!std::filesystem::exists(path), "reading missing preferences wrote a file");
    for (const auto theme : {EditorColorTheme::Light, EditorColorTheme::Dark}) {
      Require(SaveEditorColorTheme(path, theme, &error) && error.empty(), "theme save failed");
      Require(LoadEditorColorTheme(path, &error) == theme, "saved theme did not round trip");
    }
    {
      std::ofstream out(path, std::ios::binary);
      out << "schema=2\ntheme=light\n";
    }
    Require(!LoadEditorColorTheme(path, &error) && !error.empty(),
            "unsupported preferences were accepted");
    Require(std::filesystem::file_size(path) == 21, "rejected read rewrote preferences");
    {
      std::ofstream out(path, std::ios::binary);
      out << std::string(4096, 'x');
    }
    Require(!LoadEditorColorTheme(path, &error), "oversized preferences accepted");
    Require(SaveEditorColorTheme(path, EditorColorTheme::Light),
            "explicit preference repair failed");
    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    {
      std::ofstream out(temporary);
      out << "occupied";
    }
    Require(!SaveEditorColorTheme(path, EditorColorTheme::Dark, &error) && !error.empty(),
            "occupied staging file must reject save");
    Require(LoadEditorColorTheme(path) == EditorColorTheme::Light,
            "failed save replaced the good preference");
    std::filesystem::remove(temporary);
    {
      std::ofstream out(path, std::ios::binary);
      out << "schema=1\r\ntheme=light\r\n";
    }
    Require(LoadEditorColorTheme(path) == EditorColorTheme::Light, "CRLF preference rejected");
    Require(!LoadEditorColorTheme({}) && !SaveEditorColorTheme({}, EditorColorTheme::Dark),
            "empty preference path accepted");
    Require(!SaveEditorColorTheme(path, static_cast<EditorColorTheme>(99)),
            "invalid theme accepted");

    EditorImGuiHost ui;
    ProductShell shell;
    Access::ConfigureSyntheticInput(ui);
    ui.SetDisplay(1200 * scale, 900 * scale, scale);
    const auto draw = [&] {
      ui.BeginFrame();
      ui.DrawProductShell(shell);
      static_cast<void>(ui.EndFrame());
    };
    draw();
    draw();
    draw();
    const float font_scale = ImGui::GetIO().FontGlobalScale;
    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    for (const auto theme : {EditorColorTheme::Light, EditorColorTheme::Dark}) {
      ui.SetColorTheme(theme);
      ui.SetDisplay(1200 * scale, 900 * scale, scale);
      draw();
      Require(ui.GetColorTheme() == theme, "SetDisplay reset selected theme");
      const auto &c = ImGui::GetStyle().Colors;
      for (const auto bg : {ImGuiCol_WindowBg, ImGuiCol_FrameBg, ImGuiCol_Button, ImGuiCol_Header,
                            ImGuiCol_PopupBg})
        Require(Contrast(c[ImGuiCol_Text], c[bg]) >= 4.5F, "text/icon contrast below 4.5:1");
      for (const auto color : {detail::ErrorColor(), detail::WarningColor(), detail::SuccessColor(),
                               detail::AxisColor(0), detail::AxisColor(1), detail::AxisColor(2)})
        Require(Contrast(color, c[ImGuiCol_WindowBg]) >= 3.0F, "semantic color is unreadable");
      Require(ImGui::GetStyle().FramePadding.x == padding.x &&
                  ImGui::GetIO().FontGlobalScale == font_scale,
              "theme switching changed DPI geometry");
    }
    for (const bool srgb_target : {true, false}) {
      ui.SetUiOutputSrgb(srgb_target);
      for (const auto theme : {EditorColorTheme::Dark, EditorColorTheme::Light}) {
        ui.SetColorTheme(theme);
        draw();
        const auto &c = ImGui::GetStyle().Colors;
        const float display_gray = theme == EditorColorTheme::Light ? .76F : .22F;
        const auto expected =
            srgb_target ? detail::LinearColor({display_gray, display_gray, display_gray, 1})
                        : ImVec4(display_gray, display_gray, display_gray, 1);
        Require(std::abs(c[ImGuiCol_WindowBg].x - expected.x) < .00001F,
                "palette does not match actual attachment encoding");
        const auto linear = [&](ImVec4 value) {
          return srgb_target ? value : detail::LinearColor(value);
        };
        Require(Contrast(linear(c[ImGuiCol_Text]), linear(c[ImGuiCol_WindowBg])) >= 4.5F &&
                    Contrast(linear(c[ImGuiCol_TextDisabled]), linear(c[ImGuiCol_WindowBg])) >=
                        4.5F,
                "normal/secondary text contrast below 4.5:1");
        Require(ImGui::GetStyle().FramePadding.x == padding.x &&
                    ImGui::GetIO().FontGlobalScale == font_scale,
                "output encoding changed DPI geometry");
      }
    }
    ui.SetUiOutputSrgb(true);
    ui.SetColorTheme(EditorColorTheme::Dark);
    const auto point = [&](std::size_t index) {
      const auto p = Access::AppearancePosition(ui, index);
      Require(p.has_value(), "actual appearance menu item missing");
      return *p;
    };
    auto &io = ImGui::GetIO();
    const auto hover = [&](std::size_t index) {
      const auto p = point(index);
      io.AddMousePosEvent(p[0], p[1]);
      for (int i = 0; i < 30; ++i)
        draw();
    };
    const auto click = [&](std::size_t index) {
      hover(index);
      io.AddMouseButtonEvent(0, true);
      draw();
      io.AddMouseButtonEvent(0, false);
      draw();
      draw();
    };
    click(0);
    hover(1);
    draw();
    click(3);
    Require(ui.TakeColorThemeRequest() == EditorColorTheme::Light,
            "real menu click did not request Light");
    Require(!ui.TakeColorThemeRequest(), "appearance request was not consumed once");
    Require(ui.GetColorTheme() == EditorColorTheme::Dark,
            "UI changed theme before the owner's frame boundary");
    ui.SetColorTheme(EditorColorTheme::Light);
    Require(SaveEditorColorTheme(path, ui.GetColorTheme()), "menu theme save failed");
    {
      EditorImGuiHost reopened;
      reopened.SetColorTheme(*LoadEditorColorTheme(path));
      reopened.SetDisplay(1200 * scale, 900 * scale, scale);
      Require(reopened.GetColorTheme() == EditorColorTheme::Light && detail::LightTheme(),
              "reopened host lost saved Light theme");
    }
    ui.SetDisplay(1200 * scale, 900 * scale, scale);
    draw();
    const auto layout = ui.SaveLayout();
    ui.SetColorTheme(EditorColorTheme::Dark);
    Require(ui.LoadLayout(layout) && ui.GetColorTheme() == EditorColorTheme::Dark,
            "project layout overrode user theme");
    draw();
    click(0);
    hover(1);
    draw();
    click(2);
    Require(ui.TakeColorThemeRequest() == EditorColorTheme::Dark,
            "real menu click did not request Dark");
    std::cout << "Appearance preference and real-menu contracts passed at " << scale << "x\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
