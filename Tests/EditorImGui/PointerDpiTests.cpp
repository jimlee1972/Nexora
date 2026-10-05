#include "EditorImGuiTestAccess.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
Nexora::Window::WindowEvent Pointer(int x, int y) {
  Nexora::Window::WindowEvent event;
  event.type = Nexora::Window::WindowEventType::Pointer;
  event.value0 = x;
  event.value1 = y;
  return event;
}
Nexora::Window::WindowEvent Dpi(float scale) {
  Nexora::Window::WindowEvent event;
  event.type = Nexora::Window::WindowEventType::DpiChanged;
  event.scale = scale;
  return event;
}
} // namespace
int main() {
  try {
    using namespace nexora::editor;
    imgui::EditorImGuiHost host;
    imgui::EditorImGuiTestAccess::ConfigureSyntheticInput(host);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    host.ProcessEvents(std::array{focus});
    ProductShell shell;
    const auto draw = [&] {
      host.BeginFrame();
      host.DrawProductShell(shell);
      static_cast<void>(host.EndFrame());
    };
    const auto expect = [&](float scale, int x, int y) {
      const auto position = imgui::EditorImGuiTestAccess::PointerPosition(host);
      // Dear ImGui floors positions during NewFrame after consuming the fractional conversion.
      Require(position[0] == std::floor(static_cast<float>(x) / scale) &&
                  position[1] == std::floor(static_cast<float>(y) / scale),
              "native client pointer was not converted once with the actual frame scale");
    };
    for (const float scale : {1.0F, 1.25F, 1.5F, 1.75F, 2.0F}) {
      host.SetDisplay(1280 / scale, 720 / scale, scale);
      host.ProcessEvents(std::array{Pointer(480, 360)});
      draw();
      expect(scale, 480, 360);
    }
    host.SetDisplay(1280 / 1.5F, 720 / 1.5F, 1.5F);
    host.ProcessEvents(std::array{Pointer(301, 151)});
    draw();
    expect(1.5F, 301, 151);
    host.SetDisplay(640, 360, 2);
    draw();
    expect(2, 301, 151);
    host.ProcessEvents(std::array{Dpi(1.25F)});
    draw();
    expect(1.25F, 301, 151);
    host.ProcessEvents(std::array{Pointer(500, 300), Dpi(1.5F), Pointer(600, 450), Dpi(2)});
    draw();
    expect(2, 600, 450);
    host.ProcessEvents(std::array{Pointer(-240, -120)});
    draw();
    expect(2, -240, -120);
    host.SetDisplay(1280, 720, std::numeric_limits<float>::quiet_NaN());
    host.ProcessEvents(std::array{Pointer(160, 80)});
    draw();
    expect(1, 160, 80);
    Require(imgui::EditorImGuiTestAccess::Inspect(host).framebuffer_scale == 1,
            "invalid DPI reached the renderer");
    host.SetDisplay(1280, 720, std::numeric_limits<float>::infinity());
    draw();
    expect(1, 160, 80);
    focus.value0 = 0;
    host.ProcessEvents(std::array{focus});
    draw();
    const auto blurred = imgui::EditorImGuiTestAccess::PointerPosition(host);
    host.SetDisplay(640, 360, 2);
    draw();
    Require(imgui::EditorImGuiTestAccess::PointerPosition(host) == blurred,
            "scale change resurrected a cached pointer after focus loss");
    nexora::runtime::World world;
    const auto scene_id = world.LoadScene("DPI gesture");
    Require(world.Activate(scene_id), "gesture scene failed");
    SceneDocument document(world, scene_id);
    const auto entity = document.Create("Entity");
    Require(document.Select(std::array{entity}), "gesture selection failed");
    imgui::EditorImGuiHost gesture;
    gesture.SetDisplay(1280, 720, 1);
    gesture.SetNativeScenePreview(true);
    imgui::EditorImGuiTestAccess::ConfigureSyntheticInput(gesture);
    focus.value0 = 1;
    gesture.ProcessEvents(std::array{focus});
    const auto draw_gesture = [&] {
      gesture.BeginFrame();
      gesture.DrawProductShell(shell, &document);
      static_cast<void>(gesture.EndFrame());
    };
    for (int frame = 0; frame < 3; ++frame)
      draw_gesture();
    const auto viewport = gesture.NativeScenePreviewViewport();
    Require(viewport.has_value(), "gesture canvas absent");
    const int x = static_cast<int>(viewport->x + viewport->width / 2);
    const int y = static_cast<int>(viewport->y + viewport->height / 2);
    Nexora::Window::WindowEvent button;
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    gesture.ProcessEvents(std::array{Pointer(x, y), button});
    draw_gesture();
    gesture.ProcessEvents(std::array{Pointer(x + 30, y + 20)});
    draw_gesture();
    Require(gesture.NativeSceneDragPreview().has_value(), "gesture preview did not start");
    gesture.SetDisplay(640, 360, 2);
    draw_gesture();
    button.value1 = 0;
    gesture.ProcessEvents(std::array{button});
    draw_gesture();
    Require(!gesture.NativeSceneDragPreview() && !gesture.NativeSceneDrag() &&
                document.Transform(entity)->x == 0,
            "DPI transition committed an interrupted gesture");
    std::cout << "Native pointer DPI conversion contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
