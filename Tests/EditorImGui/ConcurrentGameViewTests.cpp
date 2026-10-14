#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Runtime/EditorSdk.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Layout(bool overlap) {
  std::string ini;
  for (const auto &panel : nexora::editor::ProductShell::Panels()) {
    ini += "[Window][" + std::string(panel.title) + "###" + std::string(panel.id) + "]\n";
    if (panel.id == "nexora.scene")
      ini += "Pos=0,40\nSize=620,640\nCollapsed=0\n";
    else if (panel.id == "nexora.game")
      ini += std::string("Pos=") + (overlap ? "0,120" : "640,40") + "\nSize=620,640\nCollapsed=0\n";
    else
      ini += "Pos=0,700\nSize=100,20\nCollapsed=1\n";
    ini += "\n";
  }
  return ini;
}
void Check(float dpi) {
  using namespace nexora;
  runtime::World source;
  const auto scene = source.LoadScene("Concurrent views");
  Require(source.Activate(scene), "source scene activation failed");
  editor::SceneDocument document(source, scene);
  const auto camera = document.CreateCamera("Camera");
  Require(camera && document.SetTransform(camera, {0, 0, 5}), "camera fixture failed");
  const auto baseline = source.SaveScene(scene);
  const auto dirty = document.Dirty();
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost host;
  host.SetDisplay(1280, 720, dpi);
  host.SetNativeScenePreview(true);
  Require(host.LoadLayout(Layout(false)), "saved concurrent layout rejected");
  runtime::PlaySession play(source);
  const auto draw = [&](int count = 3) {
    for (int i = 0; i < count; ++i) {
      host.BeginFrame();
      Require(!host.NativeGameViewport(), "Game viewport survived frame boundary");
      host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
      static_cast<void>(host.EndFrame());
    }
  };
  const auto pair = [&] {
    const auto left = host.NativeScenePreviewViewport(), right = host.NativeGameViewport();
    Require(left && right, "concurrent Scene/Game viewport was not published");
    Require(!Nexora::Presentation::SceneViewportsOverlap(*left, *right),
            "published native views overlap");
    for (const auto view : {*left, *right})
      Require(view.width && view.height && view.x + view.width <= 1280 * dpi &&
                  view.y + view.height <= 720 * dpi,
              "concurrent native rectangle escaped framebuffer");
  };
  for (int cycle = 0; cycle < 32; ++cycle) {
    Require(play.Start(0.25,
                       [&](runtime::World &world, double) {
                         auto pose = world.FindEntity(camera)->transform;
                         pose.x += 0.125;
                         runtime::WorldCommandBuffer commands;
                         commands.SetTransform(camera, pose);
                         return commands.Apply(world);
                       }),
            "Play start failed");
    host.SetConcurrentNativeViews(false);
    draw();
    Require(host.NativeScenePreviewViewport() && !host.NativeGameViewport(),
            "unsupported pair did not preserve Scene/map fallback");
    host.SetConcurrentNativeViews(true);
    draw();
    pair();
    Require(host.LoadLayout(Layout(true)), "overlap layout rejected");
    draw();
    Require(host.SceneCanvasViewport() && !host.NativeScenePreviewViewport() &&
                !host.NativeGameViewport(),
            "overlapping native views did not fall back together");
    Require(host.LoadLayout(Layout(false)), "restored disjoint layout rejected");
    draw();
    pair();
    Require(play.Pause() && play.Step() &&
                play.PlayWorld()->FindEntity(camera)->transform.x == 0.125,
            "paired views changed paused fixed-step isolation");
    draw(1);
    pair();
    Require(source.SaveScene(scene) == baseline && document.Dirty() == dirty && play.Stop(),
            "concurrent views changed source/history or failed Stop");
    draw(1);
    Require(!host.NativeGameViewport() && host.NativeScenePreviewViewport(),
            "Stop retained Game canvas or lost Scene canvas");
  }
  Require(document.Undo() && source.FindEntity(camera)->transform.z == 0 && document.Redo() &&
              source.SaveScene(scene) == baseline,
          "concurrent view lifecycle changed authoring Undo cursor");
}
} // namespace
int main() {
  try {
    Check(1);
    Check(2);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
