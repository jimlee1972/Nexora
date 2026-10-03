#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/InspectorRotation.h"
#include "Nexora/EditorImGui/EditorImGui.h"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

namespace {
void TestEulerRotation() {
  using namespace nexora::editor;
  nexora::runtime::Transform original{3.0, 4.0, 5.0};
  original.sx = -2.0;
  for (const double x : {-180.0, -90.0, -89.999, -30.0, 0.0, 89.999, 90.0, 180.0, 450.0}) {
    for (const double y : {-175.0, 0.0, 37.0, 180.0}) {
      for (const double z : {-120.0, 0.0, 75.0}) {
        const auto rotation = WithEulerDegrees(original, {x, y, z});
        assert(rotation && rotation->x == 3.0 && rotation->y == 4.0 && rotation->sx == -2.0);
        const auto angles = ToEulerDegrees(*rotation);
        assert(angles);
        const auto round_trip = WithEulerDegrees(original, *angles);
        assert(round_trip && SameRotation(*rotation, *round_trip));
      }
    }
  }
  // Check the convention against independently composed Runtime axis rotations, not only round
  // trips.
  const auto x = WithEulerDegrees({}, {30.0, 0.0, 0.0});
  const auto y = WithEulerDegrees({}, {0.0, 40.0, 0.0});
  const auto z = WithEulerDegrees({}, {0.0, 0.0, 50.0});
  const auto composed =
      nexora::runtime::ComposeTransforms(*y, nexora::runtime::ComposeTransforms(*x, *z));
  assert(SameRotation(composed, *WithEulerDegrees({}, {30.0, 40.0, 50.0})));
  const auto quarter_turn = nexora::runtime::ToMatrix(*WithEulerDegrees({}, {0.0, 0.0, 90.0}));
  assert(std::abs(quarter_turn[0]) < 1e-12 && std::abs(quarter_turn[1] - 1.0) < 1e-12);
  assert(!WithEulerDegrees(original, {0.0, std::numeric_limits<double>::infinity(), 0.0}));
  assert(!WithEulerDegrees(original, {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}));
  original.qw = 0.0;
  assert(!ToEulerDegrees(original));
}

void TestSceneUndoShortcut() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Undo shortcut");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto root = scene.Create("Root");
  const auto added = scene.Create("Added");
  const std::array selected{added};
  assert(root && added && scene.Select(selected));
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Z),
                                  1,
                                  Nexora::Window::KeyModifiers::Control}};
  host.ProcessEvents(events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(shell.LastCommand() == "editor.scene.undo");
  assert(scene.Nodes().size() == 1 && scene.Nodes().front().id == root);
  assert(scene.Selection().empty());
  const std::array redo_events{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Z),
                                  0,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Y),
                                  1,
                                  Nexora::Window::KeyModifiers::Control}};
  host.ProcessEvents(redo_events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(shell.LastCommand() == "editor.scene.redo" && scene.Nodes().size() == 2 &&
         scene.Nodes()[1].id == added);
  assert(scene.Undo());
  const auto control_shift = static_cast<Nexora::Window::KeyModifiers>(3);
  const std::array alternate_redo{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Y),
                                  0,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftShift),
                                  1,
                                  control_shift},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Z),
                                  1,
                                  control_shift}};
  host.ProcessEvents(alternate_redo);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(shell.LastCommand() == "editor.scene.redo" && scene.Nodes().size() == 2);
}

void TestSceneClipboardShortcuts() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Clipboard shortcuts");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto source = scene.Create("Source");
  const std::array selected{source};
  assert(source && scene.SetTransform(source, {1.0, 2.0, 3.0}) && scene.Select(selected));
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  const auto draw = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene);
    static_cast<void>(host.EndFrame());
  };
  const auto key = [](Nexora::Window::Key code, int down) {
    return Nexora::Window::WindowEvent{
        {},   Nexora::Window::WindowEventType::Key, 0, 0, 0, 1.0F, static_cast<std::int32_t>(code),
        down, Nexora::Window::KeyModifiers::Control};
  };
  draw();
  const std::array copy_events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      key(Nexora::Window::Key::LeftControl, 1), key(Nexora::Window::Key::C, 1)};
  host.ProcessEvents(copy_events);
  draw();
  assert(shell.LastCommand() == "editor.scene.copy");
  assert(scene.SetTransform(source, {12.0, 0.0, 0.0}));
  const std::array paste_events{key(Nexora::Window::Key::C, 0), key(Nexora::Window::Key::V, 1)};
  host.ProcessEvents(paste_events);
  draw();
  assert(shell.LastCommand() == "editor.scene.paste");
  assert(scene.Nodes().size() == 2 && scene.Selection().size() == 1);
  const auto copy = scene.Selection().front();
  const auto pose = scene.Transform(copy);
  assert(copy != source && pose && pose->x == 1.0 && pose->y == 2.0 && pose->z == 3.0);
  assert(scene.Undo() && scene.Nodes().size() == 1 && scene.Selection().empty());
}

void TestHierarchyDeleteShortcut() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Delete shortcut");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto root = scene.Create("Root");
  const auto child = scene.Create("Child", root);
  const std::array selected{root};
  assert(root && child && scene.Select(selected));
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Delete),
                                  1}};
  host.ProcessEvents(events);
  host.BeginFrame();
  nexora::editor::imgui::EditorImGuiTestAccess::FocusHierarchy(host);
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(shell.LastCommand() == "editor.scene.delete" && scene.Nodes().empty());
  assert(scene.Undo() && scene.Nodes().size() == 2 && scene.Parent(child) == root);
}

void TestHierarchyCreateShortcut() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Create shortcut");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftShift),
                                  1,
                                  static_cast<Nexora::Window::KeyModifiers>(3)},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::N),
                                  1,
                                  static_cast<Nexora::Window::KeyModifiers>(3)}};
  host.ProcessEvents(events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(shell.LastCommand() == "editor.scene.create" && scene.Nodes().size() == 1 &&
         scene.Nodes().front().name == "Entity");
  assert(scene.Undo() && scene.Nodes().empty());
}

void TestHierarchyDuplicateShortcut() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Duplicate shortcut");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto source = scene.Create("Source");
  const std::array selected{source};
  assert(source && scene.Select(selected));
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::D),
                                  1,
                                  Nexora::Window::KeyModifiers::Control}};
  host.ProcessEvents(events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(shell.LastCommand() == "editor.scene.duplicate" && scene.Nodes().size() == 2 &&
         scene.Selection().size() == 1 && scene.Name(scene.Selection().front()) == "Source Copy");
  assert(scene.Undo() && scene.Nodes().size() == 1);
}

void TestSceneOverviewSelection() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Scene overview");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto overview_parent = scene.Create("Parent");
  const auto overview_child = scene.Create("Child", overview_parent);
  assert(overview_parent && overview_child &&
         scene.SetTransform(overview_parent, {5.0, 0.0, 0.0}) &&
         scene.SetTransform(overview_child, {3.0, 0.0, 2.0}));
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const auto parent_position = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(overview_parent));
  const auto child_position = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(overview_child));
  const auto canvas = host.SceneCanvasViewport();
  assert(canvas && canvas->width > 0 && canvas->height > 0 && canvas->x + canvas->width <= 1280 &&
         canvas->y + canvas->height <= 720 && parent_position &&
         (*parent_position)[0] >= canvas->x && (*parent_position)[0] < canvas->x + canvas->width &&
         (*parent_position)[1] >= canvas->y && (*parent_position)[1] < canvas->y + canvas->height);
  assert(parent_position && child_position &&
         std::abs((*child_position)[0] - (*parent_position)[0] - 96.0F) < 1.0F &&
         std::abs((*child_position)[1] - (*parent_position)[1] - 64.0F) < 1.0F);
  nexora::editor::imgui::EditorImGuiHost scaled_host;
  scaled_host.SetDisplay(640.0F, 360.0F, 2.0F);
  for (int frame = 0; frame < 2; ++frame) {
    scaled_host.BeginFrame();
    scaled_host.DrawProductShell(shell, &scene);
    static_cast<void>(scaled_host.EndFrame());
  }
  const auto scaled_canvas = scaled_host.SceneCanvasViewport();
  const auto scaled_parent = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      scaled_host, *scene.Key(overview_parent));
  assert(scaled_canvas && scaled_parent && scaled_canvas->width > 0 &&
         scaled_canvas->x + scaled_canvas->width <= 1280 &&
         scaled_canvas->y + scaled_canvas->height <= 720 &&
         (*scaled_parent)[0] * 2.0F >= scaled_canvas->x &&
         (*scaled_parent)[0] * 2.0F < scaled_canvas->x + scaled_canvas->width);
  scaled_host.SetNativeScenePreview(true);
  scaled_host.BeginFrame();
  scaled_host.DrawProductShell(shell, &scene);
  static_cast<void>(scaled_host.EndFrame());
  assert(scaled_host.NativeScenePreviewViewport() &&
         !nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
             scaled_host, *scene.Key(overview_parent)));
  scaled_host.SetNativeScenePreview(false);
  assert(!scaled_host.NativeScenePreviewViewport());
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Pointer,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>((*child_position)[0]),
                                  static_cast<std::int32_t>((*child_position)[1])},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
  host.ProcessEvents(events);
  host.BeginFrame();
  assert(!host.SceneCanvasViewport());
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(scene.Selection().size() == 1 && scene.Selection().front() == overview_child);
  const std::array zoom_events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 0},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Wheel, 0, 0, 0, 1.0F, 0, 120}};
  host.ProcessEvents(zoom_events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const auto zoomed_parent = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(overview_parent));
  const auto zoomed_child = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(overview_child));
  assert(zoomed_parent && zoomed_child &&
         (*zoomed_child)[0] - (*zoomed_parent)[0] > (*child_position)[0] - (*parent_position)[0]);
  const std::array additive_events{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Pointer,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>((*zoomed_parent)[0]),
                                  static_cast<std::int32_t>((*zoomed_parent)[1])},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
  host.ProcessEvents(additive_events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  assert(scene.Selection().size() == 2);
  const std::array frame_events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::F),
                                  1}};
  host.ProcessEvents(frame_events);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene);
  static_cast<void>(host.EndFrame());
  const auto framed = nexora::editor::imgui::EditorImGuiTestAccess::SceneOverviewCenter(host);
  assert(std::abs(framed[0] - 6.5F) < 0.01F && std::abs(framed[1] - 1.0F) < 0.01F);
}

void TestSceneOverviewDrag() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Scene drag");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto drag_entity = scene.Create("Drag entity");
  assert(drag_entity);
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  const auto draw = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene);
    static_cast<void>(host.EndFrame());
  };
  draw();
  draw();
  const auto marker = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(drag_entity));
  assert(marker);
  const auto px = static_cast<std::int32_t>((*marker)[0]);
  const auto py = static_cast<std::int32_t>((*marker)[1]);
  const std::array press{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, px, py},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
  host.ProcessEvents(press);
  draw();
  const std::array move{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, px + 64, py + 32}};
  host.ProcessEvents(move);
  draw();
  assert(scene.WorldTransform(drag_entity)->x == 0.0);
  const std::array release{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 0}};
  host.ProcessEvents(release);
  draw();
  const auto moved = scene.WorldTransform(drag_entity);
  assert(moved && moved->x == 2.0 && moved->z == 1.0 && scene.Undo() &&
         scene.WorldTransform(drag_entity)->x == 0.0 &&
         scene.WorldTransform(drag_entity)->z == 0.0);
  draw();
  const auto reset_marker = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(drag_entity));
  assert(reset_marker);
  const auto reset_x = static_cast<std::int32_t>((*reset_marker)[0]);
  const auto reset_y = static_cast<std::int32_t>((*reset_marker)[1]);
  const std::array cancel_press{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, reset_x, reset_y},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
  host.ProcessEvents(cancel_press);
  draw();
  const std::array cancel_move{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, reset_x + 64, reset_y}};
  host.ProcessEvents(cancel_move);
  draw();
  const std::array escape{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::Escape),
                                  1}};
  host.ProcessEvents(escape);
  draw();
  host.ProcessEvents(release);
  draw();
  assert(scene.WorldTransform(drag_entity)->x == 0.0 &&
         scene.WorldTransform(drag_entity)->z == 0.0);

  const auto drag_axis = [&](std::int32_t start_x, std::int32_t start_y) {
    const std::array axis_press{
        Nexora::Window::WindowEvent{
            {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, start_x, start_y},
        Nexora::Window::WindowEvent{
            {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
    host.ProcessEvents(axis_press);
    draw();
    const std::array axis_move{Nexora::Window::WindowEvent{
        {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, start_x + 64, start_y + 32}};
    host.ProcessEvents(axis_move);
    draw();
    host.ProcessEvents(release);
    draw();
  };
  drag_axis(reset_x + 28, reset_y);
  assert(scene.WorldTransform(drag_entity)->x == 2.0 &&
         scene.WorldTransform(drag_entity)->z == 0.0 && scene.Undo());
  draw();
  drag_axis(reset_x, reset_y + 28);
  assert(scene.WorldTransform(drag_entity)->x == 0.0 &&
         scene.WorldTransform(drag_entity)->z == 1.0 && scene.Undo());

  const auto child = scene.Create("Snap child", drag_entity);
  assert(child && scene.SetTransform(child, {1.0, 0.0, 2.0}));
  assert(host.SetSceneOverviewCamera({0.0, 0.0, 64.0}));
  nexora::editor::imgui::EditorImGuiTestAccess::SetSceneSnap(host, true, 1);
  draw();
  const auto snap_marker = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(drag_entity));
  assert(snap_marker);
  const auto snap_x = static_cast<std::int32_t>((*snap_marker)[0]);
  const auto snap_y = static_cast<std::int32_t>((*snap_marker)[1]);
  const std::array snap_press{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, snap_x, snap_y},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
  host.ProcessEvents(snap_press);
  draw();
  const std::array snap_move{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, snap_x + 80, snap_y + 50}};
  host.ProcessEvents(snap_move);
  draw();
  const auto snap_preview = nexora::editor::imgui::EditorImGuiTestAccess::SceneMarkerPosition(
      host, *scene.Key(drag_entity));
  assert(snap_preview && std::abs((*snap_preview)[0] - (*snap_marker)[0] - 96.0F) < 1.0F &&
         std::abs((*snap_preview)[1] - (*snap_marker)[1] - 64.0F) < 1.0F);
  host.ProcessEvents(release);
  draw();
  assert(scene.WorldTransform(drag_entity)->x == 1.5 &&
         scene.WorldTransform(drag_entity)->z == 1.0 && scene.WorldTransform(child)->x == 2.5 &&
         scene.WorldTransform(child)->z == 3.0 && scene.Undo() &&
         scene.WorldTransform(drag_entity)->x == 0.0 && scene.WorldTransform(child)->x == 1.0);
}

void TestSceneOverviewCameraState() {
  nexora::editor::imgui::EditorImGuiHost host;
  assert(host.GetSceneOverviewCamera().pixels_per_unit == 32.0);
  assert(host.SetSceneOverviewCamera({12.5, -3.25, 64.0}));
  const auto camera = host.GetSceneOverviewCamera();
  assert(camera.x == 12.5 && camera.z == -3.25 && camera.pixels_per_unit == 64.0);
  assert(!host.SetSceneOverviewCamera({std::numeric_limits<double>::infinity(), 0.0, 32.0}) &&
         !host.SetSceneOverviewCamera({0.0, 0.0, 0.0}));
  assert(host.GetSceneOverviewCamera().x == 12.5);
  const auto initial_orbit = host.GetNativeSceneOrbit();
  assert(initial_orbit.distance > 0.0);
  assert(host.SetNativeSceneOrbit({1.25, 0.7, 25.0, 4.5}));
  const auto orbit = host.GetNativeSceneOrbit();
  assert(orbit.yaw == 1.25 && orbit.pitch == 0.7 && orbit.distance == 25.0 &&
         orbit.target_y == 4.5);
  assert(!host.SetNativeSceneOrbit({0.0, 0.0, 25.0}) &&
         !host.SetNativeSceneOrbit({0.0, 0.7, std::numeric_limits<double>::infinity()}) &&
         !host.SetNativeSceneOrbit({0.0, 0.7, 25.0, 100001.0}));
  assert(host.GetNativeSceneOrbit().distance == 25.0);
}

void TestNativeSceneCameraControls() {
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Native camera controls");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto target = scene.Create("Camera target");
  assert(target && scene.SetTransform(target, {0.0, 7.0, 0.0}));
  nexora::editor::ProductShell shell;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.SetNativeScenePreview(true);
  const auto draw = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene);
    static_cast<void>(host.EndFrame());
  };
  draw();
  draw();
  const auto canvas = host.NativeScenePreviewViewport();
  assert(canvas && canvas->width > 100 && canvas->height > 100);
  const auto px = static_cast<std::int32_t>(canvas->x + canvas->width / 2);
  const auto py = static_cast<std::int32_t>(canvas->y + canvas->height / 2);
  const std::array hover{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, px, py}};
  host.ProcessEvents(hover);
  draw();
  const std::array pick_press{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 1}};
  host.ProcessEvents(pick_press);
  draw();
  const auto pick = host.NativeScenePick();
  assert(pick && pick->x == static_cast<std::uint32_t>(px) &&
         pick->y == static_cast<std::uint32_t>(py) && !pick->additive);
  const std::array pick_release{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, 0, 0}};
  host.ProcessEvents(pick_release);
  draw();
  assert(!host.NativeScenePick());
  host.ProcessEvents(pick_press);
  draw();
  const std::array pick_move{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, px + 48, py + 24}};
  host.ProcessEvents(pick_move);
  draw();
  const auto preview_request = host.NativeSceneDragPreview();
  assert(preview_request && preview_request->start_x == px && preview_request->start_y == py &&
         preview_request->end_x == px + 48 && preview_request->end_y == py + 24);
  host.ProcessEvents(pick_release);
  draw();
  assert(!host.NativeSceneDragPreview());
  const auto move_request = host.NativeSceneDrag();
  assert(move_request && move_request->start_x == px && move_request->start_y == py &&
         move_request->end_x == px + 48 && move_request->end_y == py + 24);
  host.ProcessEvents(hover);
  draw();
  assert(!host.NativeSceneDrag() && !host.NativeSceneDragPreview());
  const auto distance = host.GetNativeSceneOrbit().distance;
  const std::array wheel{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::Wheel, 0, 0, 0, 1.0F, 0, 120}};
  host.ProcessEvents(wheel);
  draw();
  assert(host.GetNativeSceneOrbit().distance < distance);

  const auto drag = [&](std::int32_t button) {
    const std::array press{Nexora::Window::WindowEvent{
        {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, button, 1}};
    host.ProcessEvents(press);
    draw();
    const std::array move{Nexora::Window::WindowEvent{
        {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, px + 40, py + 30}};
    host.ProcessEvents(move);
    draw();
    const std::array release{Nexora::Window::WindowEvent{
        {}, Nexora::Window::WindowEventType::PointerButton, 0, 0, 0, 1.0F, button, 0}};
    host.ProcessEvents(release);
    draw();
    host.ProcessEvents(hover);
    draw();
  };
  const auto orbit = host.GetNativeSceneOrbit();
  drag(1);
  assert(host.GetNativeSceneOrbit().yaw != orbit.yaw &&
         host.GetNativeSceneOrbit().pitch != orbit.pitch);
  const auto camera = host.GetSceneOverviewCamera();
  drag(2);
  assert(host.GetSceneOverviewCamera().x != camera.x &&
         host.GetSceneOverviewCamera().z != camera.z);
  assert(scene.Select(std::array{target}));
  const std::array frame_key{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::F),
                                  1}};
  host.ProcessEvents(frame_key);
  draw();
  assert(host.GetNativeSceneOrbit().target_y == 7.0 && host.GetSceneOverviewCamera().x == 0.0 &&
         host.GetSceneOverviewCamera().z == 0.0);
}
} // namespace

int main() {
  TestEulerRotation();
  TestSceneUndoShortcut();
  TestSceneClipboardShortcuts();
  TestHierarchyDeleteShortcut();
  TestHierarchyCreateShortcut();
  TestHierarchyDuplicateShortcut();
  TestSceneOverviewSelection();
  TestSceneOverviewDrag();
  TestSceneOverviewCameraState();
  TestNativeSceneCameraControls();
  using nexora::editor::imgui::EditorImGuiTestAccess;
  nexora::editor::imgui::EditorImGuiHost host;
  const auto initial_state = EditorImGuiTestAccess::Inspect(host);
  assert(!initial_state.platform_viewports_enabled);
  assert(initial_state.keyboard_navigation_enabled);
  assert(initial_state.input_trickle_enabled);
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Editor ImGui contract");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto root = scene.Create("Scene Root");
  const auto child = scene.Create("Child", root);
  const auto sibling = scene.Create("Sibling");
  assert(root != 0 && child != 0 && sibling != 0 && scene.Nodes().size() == 3);
  const auto root_key = scene.Key(root);
  const auto child_key = scene.Key(child);
  const auto sibling_key = scene.Key(sibling);
  assert(root_key && child_key && sibling_key);
  const auto content_root =
      std::filesystem::temp_directory_path() /
      ("nexora-imgui-content-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  nexora::editor::ProjectWorkspace content_workspace;
  std::string content_error;
  assert(content_workspace.Create(content_root, "Content", &content_error));
  std::ofstream(content_root / "Content/Hero.mesh") << "mesh";
  std::ofstream(content_root / "Content/Hero.material") << "material";
  nexora::editor::AssetWorkspace content_assets;
  assert(content_assets.ImportTree(content_root / "Content", {}, {},
                                   nexora::editor::AssetIdentityMode::PersistentReadWrite,
                                   &content_error));
  nexora::core::JobSystem import_jobs{1};
  import_jobs.Start();
  nexora::editor::AssetImportQueue imports{import_jobs};
  nexora::editor::ProjectContentSession content;
  assert(content.Open(content_workspace, content_assets, 3, true, &content_error));
  nexora::editor::RecentProjectStore recent_projects;
  assert(recent_projects.Open(content_root / ".nexora/test-ui-recents", &content_error));
  assert(recent_projects.Record(content_workspace, &content_error));
  const auto items = content.Browser().Items();
  const auto mesh = std::ranges::find(items, std::filesystem::path("Content/Hero.mesh"),
                                      &nexora::editor::ContentItem::path);
  const auto material = std::ranges::find(items, std::filesystem::path("Content/Hero.material"),
                                          &nexora::editor::ContentItem::path);
  assert(mesh != items.end() && material != items.end());
  const std::array material_dependency{material->id};
  assert(content.Dependencies().Set(mesh->id, material_dependency));
  assert(content.Browser().Select(mesh->id));
  // Dear ImGui's input trickling (ConfigInputTrickleEventQueue, on by default) deliberately applies
  // only one input-type transition per NewFrame() so fast real interleaved events (e.g. a mouse
  // move followed by a click) keep correct sub-frame chronology; a batch mixing pointer/text/key
  // events queued in one ProcessEvents() call below would then need several frames to fully drain.
  // This test replays a synthetic event batch as a single deterministic unit rather than live
  // input, so disable trickling to make ProcessEvents() -> one NewFrame() a reliable, complete
  // apply.
  EditorImGuiTestAccess::SetInputTrickle(host, false);
  assert(!EditorImGuiTestAccess::Inspect(host).input_trickle_enabled);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  const auto selector_metrics = host.EndFrame();
  const auto selector_state = EditorImGuiTestAccess::Inspect(host);
  assert(selector_metrics.vertices > 0 && selector_metrics.indices > 0);
  assert(selector_state.project_selector_visible && selector_state.selector_recent_projects == 1);
  assert(!selector_state.app_focused && selector_state.selector_root_focus_pending);
  const std::array focus_event{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0}};
  host.ProcessEvents(focus_event);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  const auto focused_selector_state = EditorImGuiTestAccess::Inspect(host);
  assert(focused_selector_state.app_focused && !focused_selector_state.selector_root_focus_pending);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  assert(EditorImGuiTestAccess::Inspect(host).selector_root_active);
  std::vector<Nexora::Window::WindowEvent> selector_text;
  for (const char character : std::string_view("selected"))
    selector_text.push_back(
        {{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, character, 0});
  host.ProcessEvents(selector_text);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  assert(EditorImGuiTestAccess::ProjectSelectorRoot(host) == "selected");
  const std::array selector_shortcut{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::O),
                                  1,
                                  Nexora::Window::KeyModifiers::Control}};
  host.ProcessEvents(selector_shortcut);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  const auto keyboard_selector_request = host.TakeProjectSelectorRequest();
  assert(keyboard_selector_request);
  assert(keyboard_selector_request->action == nexora::editor::imgui::ProjectSelectorAction::Open);
  assert(keyboard_selector_request->root == std::filesystem::path("selected"));
  assert(keyboard_selector_request->access == nexora::editor::ProjectAccess::ReadWrite);
  const std::array selector_key_release{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::O),
                                  0,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  0}};
  host.ProcessEvents(selector_key_release);
  host.SetProjectSelectorError("project could not be opened");
  assert(host.ProjectSelectorError() == "project could not be opened");
  const auto selector_root = content_root / "selected";
  EditorImGuiTestAccess::QueueProjectSelection(
      host, {nexora::editor::imgui::ProjectSelectorAction::Create, selector_root, "Selected",
             nexora::editor::ProjectAccess::ReadWrite});
  const auto selector_request = host.TakeProjectSelectorRequest();
  assert(selector_request &&
         selector_request->action == nexora::editor::imgui::ProjectSelectorAction::Create &&
         selector_request->root == selector_root && selector_request->name == "Selected" &&
         selector_request->access == nexora::editor::ProjectAccess::ReadWrite);
  assert(!host.TakeProjectSelectorRequest());
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  host.SetProjectSelectorStatus("Importing project content", true);
  // Shortcuts must not queue a second request while an import is already running.
  host.ProcessEvents(selector_shortcut);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  assert(!host.TakeProjectSelectorRequest());
  host.ProcessEvents(selector_key_release);
  EditorImGuiTestAccess::QueueProjectImportCancellation(host);
  assert(host.TakeProjectSelectorCancel());
  assert(!host.TakeProjectSelectorCancel());
  host.SetProjectSelectorStatus({}, false);
  nexora::editor::ProductShell shell;
  std::atomic_bool release_import{false};
  const auto import_blocker =
      import_jobs.Submit({[&release_import](const nexora::core::CancellationToken &) {
                            while (!release_import.load(std::memory_order_acquire))
                              std::this_thread::yield();
                          },
                          nexora::core::JobPriority::High,
                          {},
                          "Editor ImGui import barrier"});
  assert(content.BeginReimport(imports, mesh->id, &content_error));
  // Dear ImGui's Shortcut()/SetShortcutRouting() arbitrate routing one frame ahead: a route
  // registered during a frame only "wins" starting the *next* frame (see RoutingNext/RoutingCurr
  // in imgui.cpp's UpdateKeyRoutingTable()/SetShortcutRouting()). Draw one frame with no key event
  // queued so the Ctrl+S route is primed before the simulated keypress below; DrawProductShell()
  // registers the shortcut unconditionally regardless of key state, and this warm-up frame never
  // calls Render(), so it does not perturb the renderer-metrics assertions further down.
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  assert(shell.LastCommand().empty());
  static_cast<void>(host.EndFrame());
  const auto importing_state = EditorImGuiTestAccess::Inspect(host);
  assert(importing_state.content_import_active);
  assert(importing_state.content_import_state == nexora::editor::ImportOperationState::Running ||
         importing_state.content_import_state == nexora::editor::ImportOperationState::Cancelling);
  assert(content.CancelReimport());
  release_import.store(true, std::memory_order_release);
  import_jobs.Wait(import_blocker);
  const auto import_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!content.PollReimport(&content_error) &&
         std::chrono::steady_clock::now() < import_deadline)
    std::this_thread::yield();
  assert(content.ReimportStatus() &&
         content.ReimportStatus()->state == nexora::editor::ImportOperationState::Cancelled);
  assert(content.Conflicts().Detect(mesh->id, "editor-mesh-v2", "disk-mesh-v3", true));
  const std::array mesh_dependency{mesh->id};
  assert(content.Dependencies().Set(material->id, mesh_dependency));
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, 320, 240},
      Nexora::Window::WindowEvent{{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, 'N', 0},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::DpiChanged, 0, 0, 0, 1.5F, 0, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::S),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
  };
  host.ProcessEvents(events);
  host.SetDisplay(1600.0F, 900.0F, 1.5F);
  const auto display_state = EditorImGuiTestAccess::Inspect(host);
  assert(display_state.display_width == 1600.0F);
  assert(display_state.display_height == 900.0F);
  assert(display_state.framebuffer_scale == 1.5F);
  assert(display_state.font_global_scale > 0.66F && display_state.font_global_scale < 0.67F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  assert(shell.LastCommand() == "editor.scene.save");
  assert(host.TakeSceneSaveRequest());
  assert(!host.TakeSceneSaveRequest());
  host.SetSceneSaveResult("Scene saved.", true);
  const auto metrics = host.EndFrame();
  assert(metrics.command_lists > 0);
  assert(metrics.vertices > 0);
  assert(metrics.indices > 0);
  const auto content_state = EditorImGuiTestAccess::Inspect(host);
  assert(content_state.content_visible_items == 2);
  assert(content_state.content_visible_folders == 0);
  assert(content_state.content_selection == 1);
  assert(content_state.content_forward_dependencies == 1);
  assert(content_state.content_reverse_dependencies == 1);
  assert(content_state.content_dependency_cycle == 3);
  assert(!content_state.content_import_active);
  assert(content_state.content_import_state == nexora::editor::ImportOperationState::Cancelled);
  assert(content_state.content_import_diagnostics > 0);
  assert(content_state.content_conflicts == 1);
  assert(content_state.content_conflict_visible);
  assert(!content_state.content_conflict_compare_visible);
  assert(content_state.content_conflict_choice == nexora::editor::DirtyConflictChoice::Pending);
  assert(content_state.project_writable);
  assert(!content_state.project_upgrade_required);
  assert(content_state.recent_projects == 1);

  const std::array selected_root{*root_key};
  assert(scene.Select(selected_root));
  auto edited_transform = *scene.Transform(root);
  edited_transform.x = 12.5;
  edited_transform.sy = 2.0;
  EditorImGuiTestAccess::QueueInspectorTransform(host, *root_key, edited_transform);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  const auto inspector_state = EditorImGuiTestAccess::Inspect(host);
  assert(inspector_state.inspector_selection == 1);
  assert(inspector_state.inspector_transform_visible);
  assert(scene.Transform(root) == edited_transform);
  assert(scene.Undo());
  assert(scene.Transform(root)->x == 0.0 && scene.Transform(root)->sy == 1.0);

  EditorImGuiTestAccess::QueueInspectorCamera(host, *root_key,
                                              nexora::runtime::CameraComponent{70.0, 0.25, 800.0});
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Camera(*root_key) && scene.Camera(*root_key)->vertical_field_of_view == 70.0);
  assert(scene.Undo() && !scene.Camera(*root_key));
  EditorImGuiTestAccess::QueueInspectorLight(host, *root_key,
                                             nexora::runtime::LightComponent{3.0F});
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Light(*root_key) && scene.Light(*root_key)->intensity == 3.0F);
  assert(scene.Undo() && !scene.Light(*root_key));

  const std::array multi_selection{*root_key, *sibling_key};
  assert(scene.Select(multi_selection));
  auto root_transform = *scene.Transform(root);
  auto sibling_transform = *scene.Transform(sibling);
  root_transform.z = 7.0;
  sibling_transform.z = 7.0;
  const std::array multi_transforms{root_transform, sibling_transform};
  EditorImGuiTestAccess::QueueInspectorTransforms(host, multi_selection, multi_transforms);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(EditorImGuiTestAccess::Inspect(host).inspector_selection == 2);
  assert(scene.Transform(root)->z == 7.0 && scene.Transform(sibling)->z == 7.0);
  assert(scene.Undo());
  assert(scene.Transform(root)->z == 0.0 && scene.Transform(sibling)->z == 0.0);

  // Editing one Euler field keeps each target's other axes and all position/scale values.
  const auto initial_root =
      *nexora::editor::WithEulerDegrees(*scene.Transform(root), {10.0, 20.0, 30.0});
  const auto initial_sibling =
      *nexora::editor::WithEulerDegrees(*scene.Transform(sibling), {-15.0, 40.0, 60.0});
  const std::array initial_rotations{initial_root, initial_sibling};
  assert(scene.SetTransforms(multi_selection, initial_rotations));
  EditorImGuiTestAccess::QueueInspectorEulerField(host, multi_selection, 0, 450.0);
  const auto draw_inspector = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
    static_cast<void>(host.EndFrame());
  };
  draw_inspector();
  draw_inspector();
  const auto root_angles = EditorImGuiTestAccess::InspectorEulerAngles(host, *root_key);
  const auto sibling_angles = EditorImGuiTestAccess::InspectorEulerAngles(host, *sibling_key);
  assert(root_angles && sibling_angles && (*root_angles)[0] == 450.0 &&
         (*sibling_angles)[0] == 450.0);
  assert(std::abs((*root_angles)[1] - 20.0) < 1e-9 && std::abs((*root_angles)[2] - 30.0) < 1e-9);
  assert(std::abs((*sibling_angles)[1] - 40.0) < 1e-9 &&
         std::abs((*sibling_angles)[2] - 60.0) < 1e-9);
  assert(nexora::editor::SameRotation(
      *scene.Transform(root),
      *nexora::editor::WithEulerDegrees(initial_root, {450.0, 20.0, 30.0})));
  assert(scene.Transform(root)->x == initial_root.x &&
         scene.Transform(root)->sx == initial_root.sx);
  const std::array after_rotation{*scene.Transform(root), *scene.Transform(sibling)};
  auto stale_key = *sibling_key;
  ++stale_key.document_generation;
  const std::array stale_selection{*root_key, stale_key};
  EditorImGuiTestAccess::QueueInspectorEulerField(host, stale_selection, 1, 22.0);
  draw_inspector();
  assert(scene.Transform(root) == after_rotation[0] &&
         scene.Transform(sibling) == after_rotation[1]);
  EditorImGuiTestAccess::QueueInspectorEulerField(host, multi_selection, 2,
                                                  std::numeric_limits<double>::quiet_NaN());
  draw_inspector();
  assert(scene.Transform(root) == after_rotation[0] &&
         scene.Transform(sibling) == after_rotation[1]);
  assert(scene.Undo());
  assert(scene.Transform(root) == initial_root && scene.Transform(sibling) == initial_sibling);
  draw_inspector();
  assert(std::abs((*EditorImGuiTestAccess::InspectorEulerAngles(host, *root_key))[0] - 10.0) <
         1e-9);
  assert(scene.Undo());

  // Exercise the real text widget via public key/text events; typing alone must not mutate the
  // scene.
  assert(scene.Select(selected_root));
  const auto before_text_edit = *scene.Transform(root);
  EditorImGuiTestAccess::FocusInspectorEulerField(host, 1);
  draw_inspector();
  draw_inspector();
  const auto key_event = [&](Nexora::Window::Key key, bool down, bool control = false) {
    Nexora::Window::WindowEvent event{};
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<std::int32_t>(key);
    event.value1 = down ? 1 : 0;
    event.modifiers =
        control ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
    const std::array events{event};
    host.ProcessEvents(events);
    draw_inspector();
  };
  key_event(Nexora::Window::Key::Z, true, true);
  key_event(Nexora::Window::Key::Z, false);
  assert(shell.LastCommand() != "editor.scene.undo" && scene.Transform(root) == before_text_edit);
  key_event(Nexora::Window::Key::A, true, true);
  key_event(Nexora::Window::Key::A, false);
  for (const char character : std::string_view{"450"}) {
    Nexora::Window::WindowEvent event{};
    event.type = Nexora::Window::WindowEventType::Text;
    event.value0 = character;
    const std::array events{event};
    host.ProcessEvents(events);
    draw_inspector();
  }
  assert(scene.Transform(root) == before_text_edit);
  key_event(Nexora::Window::Key::Enter, true);
  key_event(Nexora::Window::Key::Enter, false);
  assert(nexora::editor::SameRotation(
      *scene.Transform(root),
      *nexora::editor::WithEulerDegrees(before_text_edit, {0.0, 450.0, 0.0})));
  assert((*EditorImGuiTestAccess::InspectorEulerAngles(host, *root_key))[1] == 450.0);
  const auto persisted_rotation = content_root / "InspectorRotation.scene";
  assert(scene.Save(persisted_rotation));
  assert(scene.Select(multi_selection));
  draw_inspector();
  assert((*EditorImGuiTestAccess::InspectorEulerAngles(host, *root_key))[1] == 450.0);
  nexora::runtime::World reopened_rotation_world;
  nexora::editor::SceneDocument reopened_rotation(reopened_rotation_world,
                                                  reopened_rotation_world.LoadScene("Reopened"));
  assert(reopened_rotation.Reload(persisted_rotation));
  const std::array reopened_selection{*reopened_rotation.Key(root)};
  assert(reopened_rotation.Select(reopened_selection));
  {
    nexora::editor::imgui::EditorImGuiHost reopened_host;
    reopened_host.SetDisplay(1600.0F, 900.0F, 1.5F);
    for (int frame = 0; frame < 2; ++frame) {
      reopened_host.BeginFrame();
      reopened_host.DrawProductShell(shell, &reopened_rotation);
      static_cast<void>(reopened_host.EndFrame());
    }
    assert((*EditorImGuiTestAccess::InspectorEulerAngles(reopened_host,
                                                         reopened_selection[0]))[1] == 450.0);
  }
  assert(scene.Undo() && scene.Transform(root) == before_text_edit);
  assert(scene.Select(multi_selection));

  EditorImGuiTestAccess::QueueContentConflictChoice(host, material->id,
                                                    nexora::editor::DirtyConflictChoice::Reload);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(content.Conflicts().Find(mesh->id)->choice ==
         nexora::editor::DirtyConflictChoice::Pending);

  EditorImGuiTestAccess::QueueContentConflictChoice(host, mesh->id,
                                                    nexora::editor::DirtyConflictChoice::Compare);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  const auto compared_conflict_state = EditorImGuiTestAccess::Inspect(host);
  assert(content.Conflicts().Find(mesh->id)->choice ==
         nexora::editor::DirtyConflictChoice::Compare);
  assert(compared_conflict_state.content_conflicts == 1);
  assert(compared_conflict_state.content_conflict_visible);
  assert(compared_conflict_state.content_conflict_compare_visible);
  assert(compared_conflict_state.content_conflict_choice ==
         nexora::editor::DirtyConflictChoice::Compare);

  EditorImGuiTestAccess::QueueContentConflictChoice(host, mesh->id,
                                                    nexora::editor::DirtyConflictChoice::Keep);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  const auto kept_conflict_state = EditorImGuiTestAccess::Inspect(host);
  assert(content.Conflicts().Find(mesh->id)->choice == nexora::editor::DirtyConflictChoice::Keep);
  assert(kept_conflict_state.content_conflicts == 0);
  assert(!kept_conflict_state.content_conflict_visible);
  assert(!kept_conflict_state.content_conflict_compare_visible);
  assert(kept_conflict_state.content_conflict_choice == nexora::editor::DirtyConflictChoice::Keep);

  assert(content.Conflicts().Detect(material->id, "editor-material-v2", "disk-material-v3", true));
  EditorImGuiTestAccess::QueueContentConflictChoice(host, material->id,
                                                    nexora::editor::DirtyConflictChoice::Reload);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  const auto reloaded_conflict_state = EditorImGuiTestAccess::Inspect(host);
  assert(content.Conflicts().Find(material->id)->choice ==
         nexora::editor::DirtyConflictChoice::Reload);
  assert(reloaded_conflict_state.content_conflicts == 0);
  assert(!reloaded_conflict_state.content_conflict_visible);
  assert(reloaded_conflict_state.content_conflict_choice ==
         nexora::editor::DirtyConflictChoice::Reload);

  EditorImGuiTestAccess::SetHierarchyFilter(host, "");
  EditorImGuiTestAccess::QueueHierarchyExpansion(host, *root_key, true);
  EditorImGuiTestAccess::QueueHierarchySelection(host, *root_key, false, false);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  auto hierarchy_state = EditorImGuiTestAccess::Inspect(host);
  assert(hierarchy_state.hierarchy_visible_rows == 3);
  assert(hierarchy_state.hierarchy_selection == 1);
  assert(hierarchy_state.hierarchy_selection_anchor == *root_key);
  assert(scene.Selection().size() == 1 && scene.Selection().front() == root);

  EditorImGuiTestAccess::QueueHierarchySelection(host, *child_key, true, false);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  hierarchy_state = EditorImGuiTestAccess::Inspect(host);
  assert(hierarchy_state.hierarchy_selection == 2 &&
         hierarchy_state.hierarchy_selection_anchor == *child_key);
  assert(std::ranges::find(scene.Selection(), root) != scene.Selection().end() &&
         std::ranges::find(scene.Selection(), child) != scene.Selection().end());

  EditorImGuiTestAccess::QueueHierarchySelection(host, *sibling_key, false, true);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  hierarchy_state = EditorImGuiTestAccess::Inspect(host);
  assert(hierarchy_state.hierarchy_selection == 2 &&
         hierarchy_state.hierarchy_selection_anchor == *child_key);
  assert(std::ranges::find(scene.Selection(), child) != scene.Selection().end() &&
         std::ranges::find(scene.Selection(), sibling) != scene.Selection().end());

  EditorImGuiTestAccess::SetHierarchyFilter(host, "scene");
  EditorImGuiTestAccess::QueueHierarchySelection(host, *child_key, false, false);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  hierarchy_state = EditorImGuiTestAccess::Inspect(host);
  assert(hierarchy_state.hierarchy_visible_rows == 1 && hierarchy_state.hierarchy_selection == 2);
  assert(std::ranges::find(scene.Selection(), child) != scene.Selection().end() &&
         std::ranges::find(scene.Selection(), sibling) != scene.Selection().end());
  EditorImGuiTestAccess::SetHierarchyFilter(host, "");

  EditorImGuiTestAccess::QueueHierarchyMove(host, *sibling_key, *root_key, 1);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Parent(sibling) == root);
  EditorImGuiTestAccess::QueueHierarchySelection(host, *sibling_key, false, false);
  EditorImGuiTestAccess::QueueHierarchyReorder(host, -1);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(world.SiblingIndex(sibling) == 0);
  EditorImGuiTestAccess::QueueHierarchyMove(host, *root_key, *child_key, 0);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Parent(root) == 0);

  EditorImGuiTestAccess::QueueHierarchyRename(host, *sibling_key, "Renamed Sibling");
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Name(sibling) == "Renamed Sibling");
  assert(scene.Undo() && scene.Name(sibling) == "Sibling");

  const nexora::editor::SceneDocument::NodeKey stale_sibling{
      sibling_key->id, sibling_key->entity_generation, sibling_key->document_generation + 1};
  EditorImGuiTestAccess::QueueHierarchyRename(host, stale_sibling, "Stale Rename");
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Name(sibling) == "Sibling");

  for (int entity = 0; entity < 256; ++entity)
    assert(scene.Create("Virtualized " + std::to_string(entity)) != 0);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  hierarchy_state = EditorImGuiTestAccess::Inspect(host);
  assert(hierarchy_state.hierarchy_visible_rows == 259);
  assert(hierarchy_state.hierarchy_rendered_rows < hierarchy_state.hierarchy_visible_rows);

  EditorImGuiTestAccess::QueueHierarchyCreate(host, "Created Child", *root_key);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Nodes().size() == 260 && scene.Selection().size() == 1);
  const auto created_child = scene.Selection().front();
  assert(scene.Name(created_child) == "Created Child" && scene.Parent(created_child) == root);
  assert(EditorImGuiTestAccess::Inspect(host).hierarchy_selection_anchor ==
         *scene.Key(created_child));
  auto stale_parent = *root_key;
  ++stale_parent.document_generation;
  EditorImGuiTestAccess::QueueHierarchyCreate(host, "Rejected Child", stale_parent);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  assert(scene.Nodes().size() == 260);
  EditorImGuiTestAccess::QueueHierarchyCreate(host, "Created Root");
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  static_cast<void>(host.EndFrame());
  const auto created_root = scene.Selection().front();
  assert(scene.Nodes().size() == 261 && scene.Name(created_root) == "Created Root" &&
         scene.Parent(created_root) == 0);
  assert(scene.Undo() && scene.Nodes().size() == 260 && scene.Selection().empty());
  const auto created_scene_path = content_root / "Created.scene";
  assert(scene.Save(created_scene_path));
  nexora::runtime::World reopened_world;
  nexora::editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Placeholder"));
  assert(reopened.Reload(created_scene_path) && reopened.Name(created_child) == "Created Child" &&
         reopened.Name(created_root).empty());

  auto device = nexora::rhi::CreateValidationDevice();
  const auto target =
      device->CreateTexture({1280, 720, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::Undefined, "Editor ImGui offscreen"});
  const auto user_texture =
      device->CreateTexture({1, 1, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::ShaderRead, "Editor user texture"});
  const auto texture_id = host.RegisterTexture(*device, user_texture);
  assert(texture_id != 0);
  assert(EditorImGuiTestAccess::OverrideDrawTexture(host, texture_id) > 0);
  const auto draws =
      host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::Undefined, false);
  auto repeated_draws = draws;
  for (int frame = 0; frame < 3; ++frame)
    repeated_draws =
        host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false);
  const auto retained = host.GetRendererMetrics();
  assert(repeated_draws == draws && retained.frames == 4 && retained.buffer_reallocations == 6 &&
         retained.font_rebuilds == 1 && retained.completion_waits == 0 &&
         retained.rejected_textures == 0);
  const auto diagnostics = device->Diagnostics();
  assert(draws > 0 && diagnostics.draw_calls == draws * 4 && diagnostics.validation_errors == 0);
  assert(host.UnregisterTexture(texture_id));
  assert(!host.UnregisterTexture(texture_id));
  assert(EditorImGuiTestAccess::OverrideDrawTexture(host, texture_id) > 0);
  assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) ==
         draws);
  assert(host.GetRendererMetrics().rejected_textures > 0);
  const auto next_texture_id = host.RegisterTexture(*device, user_texture);
  assert(next_texture_id != texture_id && host.UnregisterTexture(next_texture_id));

  // Exercise every DPI bucket and a bounded long-running layout/render loop. Each bucket switch
  // rebuilds the atlas once; subsequent stable frames reuse allocations and reject no callbacks.
  constexpr std::array dpi_scales{1.25F, 1.5F, 2.0F, 1.0F};
  for (const float dpi : dpi_scales) {
    host.SetDisplay(1280.0F / dpi, 720.0F / dpi, dpi);
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, nullptr, &content);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  // Let all three upload slots observe the complete docked Content layout before taking the soak
  // baseline. A newly added panel may be selected only after docking settles, but must not cause
  // any allocation growth once every slot has rendered that layout.
  for (int frame = 0; frame < 6; ++frame) {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, nullptr, &content);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  const auto reallocations_before_soak = host.GetRendererMetrics().buffer_reallocations;
  for (int frame = 0; frame < 512; ++frame) {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, nullptr, &content);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  const auto soaked = host.GetRendererMetrics();
  assert(soaked.font_rebuilds == 5 && soaked.buffer_reallocations == reallocations_before_soak);
  const auto layout = host.SaveLayout();
  assert(!layout.empty() && host.LoadLayout(layout));
  assert(!host.LoadLayout("not an ImGui layout"));

  const auto recovery_root =
      std::filesystem::temp_directory_path() /
      ("nexora-imgui-recovery-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  nexora::editor::ProjectWorkspace workspace;
  std::string error;
  assert(workspace.Create(recovery_root, "Recovery", &error));
  std::ofstream(recovery_root / ".nexora/workspace.recovery") << "schema=1\ninvalid\n";
  assert(!host.ApplyRecoveryChoice(workspace, nexora::editor::imgui::RecoveryChoice::Recover));
  assert(workspace.HasRecoveryJournal() && !host.RecoveryError().empty());
  assert(host.ApplyRecoveryChoice(workspace, nexora::editor::imgui::RecoveryChoice::Discard));
  assert(host.TakeRecoveryChoice() == nexora::editor::imgui::RecoveryChoice::Discard);
  assert(host.TakeRecoveryChoice() == nexora::editor::imgui::RecoveryChoice::None);
  workspace = {};
  std::filesystem::remove_all(recovery_root);
  content_workspace = {};
  std::filesystem::remove_all(content_root);
  host.ReleaseRenderer(*device);
  device->DestroyTexture(user_texture);
  device->DestroyTexture(target);
}
