#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <chrono>
#include <cmath>
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
  try {
    using namespace nexora;
    using Access = editor::imgui::EditorImGuiTestAccess;
    using Key = Nexora::Window::Key;
    using Mods = Nexora::Window::KeyModifiers;
    const float scale = argc > 1 ? static_cast<float>(std::atoi(argv[1])) : 1.0F;
    const auto root = std::filesystem::temp_directory_path() /
                      ("nexora-transform-layout-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    editor::test::TemporaryDirectoryCleanup cleanup(root);
    editor::ProjectWorkspace writer, reader;
    Require(writer.Create(root, "Transform Layout"), "project fixture failed");
    runtime::World world;
    const auto scene_id = world.LoadScene("Transform Layout");
    Require(world.Activate(scene_id), "scene activation failed");
    editor::SceneDocument scene(world, scene_id);
    const auto first = scene.Create("A"), second = scene.Create("B");
    const std::array keys{*scene.Key(first), *scene.Key(second)};
    runtime::Transform a, b;
    a.x = 1;
    a.y = 2;
    a.z = 3;
    b.x = 9;
    b.y = 8;
    b.z = 3;
    b.sx = -2;
    Require(scene.SetTransforms(keys, std::array{a, b}) && scene.Select(keys),
            "transform fixture failed");
    Require(scene.Save(root / "Content/layout.scene"), "scene fixture save failed");
    const auto baseline = world.SaveScene(scene_id);
    editor::ProductShell shell;
    editor::imgui::EditorImGuiHost ui;
    ui.SetDisplay(1600 * scale, 1200 * scale, scale);
    Access::ConfigureSyntheticInput(ui);
    editor::ProjectWorkspace *workspace = &writer;
    float inspector_width{};
    const auto draw = [&] {
      ui.BeginFrame();
      if (inspector_width > 0) {
        auto *window = ImGui::FindWindowByName("###nexora.inspector");
        Require(window != nullptr, "actual Inspector window is missing");
        ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
        ImGui::SetWindowPos(window, ImVec2(100, 80), ImGuiCond_Always);
        ImGui::SetWindowSize(window, ImVec2(inspector_width, 900), ImGuiCond_Always);
      }
      ui.DrawProductShell(shell, &scene, workspace);
      static_cast<void>(ui.EndFrame());
    };
    Nexora::Window::WindowEvent focused;
    focused.type = Nexora::Window::WindowEventType::FocusChanged;
    focused.value0 = 1;
    ui.ProcessEvents(std::array{focused});
    for (int frame = 0; frame < 4; ++frame)
      draw();
    Access::FocusInspector(ui);
    draw();
    inspector_width = 520;
    draw();
    draw();
    const auto geometry = [&] {
      std::array<std::array<std::array<float, 4>, 3>, 3> rows;
      for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
          const auto rect = Access::InspectorTransformFieldRect(ui, row, axis);
          Require(rect.has_value(), "a visible editable XYZ field is absent");
          rows[row][axis] = *rect;
          Require((*rect)[2] - (*rect)[0] >= 24 && (*rect)[3] > (*rect)[1],
                  "XYZ field is too narrow or has no height");
          Require((*rect)[0] >= 100 && (*rect)[2] <= 100 + inspector_width,
                  "XYZ field extends outside its Inspector window");
          if (axis != 0) {
            const auto &previous = rows[row][axis - 1];
            Require(std::abs(previous[1] - (*rect)[1]) < 0.5F &&
                        std::abs(previous[3] - (*rect)[3]) < 0.5F,
                    "XYZ inputs are not on one row");
            Require(previous[2] <= (*rect)[0], "XYZ inputs overlap or are out of order");
          }
        }
        if (row != 0)
          Require(rows[row - 1][0][3] <= rows[row][0][1],
                  "Position, Rotation and Scale are not in reading order");
      }
      return rows;
    };
    static_cast<void>(geometry());
    const auto event = [&](Key key, bool down, Mods modifiers = Mods::None) {
      Nexora::Window::WindowEvent input;
      input.type = Nexora::Window::WindowEventType::Key;
      input.value0 = static_cast<int>(key);
      input.value1 = down;
      input.modifiers = modifiers;
      ui.ProcessEvents(std::array{input});
      draw();
    };
    const auto tap = [&](Key key, Mods modifiers = Mods::None) {
      if (modifiers == Mods::Control)
        event(Key::LeftControl, true, modifiers);
      event(key, true, modifiers);
      event(key, false, modifiers);
      if (modifiers == Mods::Control)
        event(Key::LeftControl, false);
    };
    const auto click = [&](std::size_t row, std::size_t axis) {
      const auto rect = Access::InspectorTransformFieldRect(ui, row, axis);
      Require(rect.has_value(), "requested actual XYZ input is clipped");
      Nexora::Window::WindowEvent position, button;
      position.type = Nexora::Window::WindowEventType::Pointer;
      position.value0 = static_cast<int>(((*rect)[0] + (*rect)[2]) * 0.5F * scale);
      position.value1 = static_cast<int>(((*rect)[1] + (*rect)[3]) * 0.5F * scale);
      ui.ProcessEvents(std::array{position});
      draw();
      button.type = Nexora::Window::WindowEventType::PointerButton;
      button.value0 = 0;
      button.value1 = 1;
      ui.ProcessEvents(std::array{button});
      draw();
      button.value1 = 0;
      ui.ProcessEvents(std::array{button});
      draw();
    };
    const auto type = [&](std::string_view text) {
      tap(Key::A, Mods::Control);
      for (const unsigned char character : text) {
        Nexora::Window::WindowEvent input;
        input.type = Nexora::Window::WindowEventType::Text;
        input.value0 = character;
        ui.ProcessEvents(std::array{input});
        draw();
      }
    };
    click(0, 0);
    type("-12.5");
    Require(world.SaveScene(scene_id) == baseline, "typing committed before Enter");
    tap(Key::Enter);
    auto changed_a = a, changed_b = b;
    changed_a.x = changed_b.x = -12.5;
    Require(scene.Transform(first) == changed_a && scene.Transform(second) == changed_b,
            "pointer edit changed unrelated transform axes");
    tap(Key::Z, Mods::Control);
    Require(world.SaveScene(scene_id) == baseline, "keyboard Undo did not restore one edit");
    click(0, 2);
    tap(Key::Tab);
    type("42");
    Require(world.SaveScene(scene_id) == baseline, "Tab navigation committed a draft");
    tap(Key::Enter);
    const auto angles_a = scene.EulerAngles(first), angles_b = scene.EulerAngles(second);
    Require(angles_a && angles_b && std::abs((*angles_a)[0] - 42) < 1e-8 &&
                std::abs((*angles_b)[0] - 42) < 1e-8 && scene.Transform(first)->z == a.z &&
                scene.Transform(second)->z == b.z && scene.Transform(second)->sx == b.sx,
            "Tab from Position Z did not edit Rotation X while preserving other fields");
    tap(Key::Z, Mods::Control);
    Require(world.SaveScene(scene_id) == baseline, "rotation keyboard Undo failed");
    inspector_width = 220;
    draw();
    draw();
    static_cast<void>(geometry());
    Require(reader.Open(root, editor::ProjectAccess::ReadOnly), "read-only fixture failed");
    workspace = &reader;
    draw();
    for (std::size_t row = 0; row < 3; ++row)
      for (std::size_t axis = 0; axis < 3; ++axis)
        Require(!Access::InspectorTransformFieldRect(ui, row, axis),
                "disabled read-only XYZ field was exposed as actionable");
    std::cout << "Transform layout and actual input passed at " << scale << "x\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
