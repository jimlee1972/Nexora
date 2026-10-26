#include "EditorIcons.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

int main(int argc, char **argv) {
  const float scale = argc > 1 ? static_cast<float>(std::atoi(argv[1])) : 1.0F;
  ImGui::CreateContext();
  try {
    using namespace nexora::editor::imgui::detail;
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800 * scale, 600 * scale);
    io.DeltaTime = 1.0F / 60.0F;
    io.ConfigInputTrickleEventQueue = false;
    io.FontGlobalScale = scale;
    ImGui::GetStyle().ScaleAllSizes(scale);
    unsigned char *pixels{};
    int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImVec2 center;
    ImGuiID expected{};
    int vertices{};
    const auto draw = [&](const char *label, bool disabled = false, bool selected = false) {
      ImGui::NewFrame();
      ImGui::SetNextWindowPos(ImVec2(20 * scale, 20 * scale));
      ImGui::SetNextWindowSize(ImVec2(500 * scale, 300 * scale));
      ImGui::Begin("Icon test", nullptr, ImGuiWindowFlags_NoSavedSettings);
      expected = ImGui::GetID(label);
      ImGui::BeginDisabled(disabled);
      const int before = ImGui::GetWindowDrawList()->VtxBuffer.Size;
      const bool clicked = IconButton(label, EditorIcon::Save, selected);
      vertices = ImGui::GetWindowDrawList()->VtxBuffer.Size - before;
      Require(GImGui->LastItemData.ID == expected, "icon/tooltip changed the original widget ID");
      const ImVec2 minimum = ImGui::GetItemRectMin();
      const ImVec2 maximum = ImGui::GetItemRectMax();
      Require(std::abs((maximum.x - minimum.x) - (maximum.y - minimum.y)) < 0.01F,
              "icon button is not square");
      Require(maximum.x - minimum.x >= 19 * scale, "icon hit target is too small");
      center = ImVec2((minimum.x + maximum.x) / 2, (minimum.y + maximum.y) / 2);
      ImGui::EndDisabled();
      ImGui::End();
      ImGui::Render();
      return clicked;
    };
    io.AddMousePosEvent(-100, -100);
    draw("Save###stable");
    draw("Save###stable");
    const int short_vertices = vertices;
    const ImGuiID original = expected;
    draw("A much longer save action caption that must never be painted###stable");
    Require(expected == original && vertices == short_vertices,
            "visible label text affected icon identity or drawing geometry");
    io.AddMousePosEvent(center.x, center.y);
    draw("Save###stable");
    draw("Save###stable");
    Require(ImGui::FindWindowByName("##Tooltip_00") != nullptr,
            "hover did not submit the function tooltip");
    io.AddMouseButtonEvent(0, true);
    Require(!draw("Save###stable"), "button fired before release");
    io.AddMouseButtonEvent(0, false);
    Require(draw("Save###stable"), "real pointer release did not activate icon button");
    draw("Save###stable", true);
    io.AddMouseButtonEvent(0, true);
    Require(!draw("Save###stable", true), "disabled icon fired on press");
    io.AddMouseButtonEvent(0, false);
    Require(!draw("Save###stable", true), "disabled icon fired on release");
    draw("Save###stable", false, true);
    Require(vertices > short_vertices, "selected icon is missing its state marker");

    // Exercise every artwork at the current DPI, including the optional 2D tools.
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20 * scale, 20 * scale));
    ImGui::SetNextWindowSize(ImVec2(700 * scale, 500 * scale));
    ImGui::Begin("Artwork");
    for (int value = static_cast<int>(EditorIcon::Add);
         value <= static_cast<int>(EditorIcon::Tilemap); ++value) {
      ImGui::PushID(value);
      const auto icon = static_cast<EditorIcon>(value);
      SmallIconButton("Tool", icon);
      Require(GImGui->LastItemData.ID == ImGui::GetID("Tool"),
              "small icon button changed identity");
      ImGui::SameLine();
      IconRadioButton("Mode", icon, true);
      ImGui::PopID();
      if ((value + 1) % 8 != 0)
        ImGui::SameLine();
    }
    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
    std::cout << "Icon button contracts passed at " << scale << "x\n";
    return 0;
  } catch (const std::exception &error) {
    ImGui::DestroyContext();
    std::cerr << error.what() << '\n';
    return 1;
  }
}
