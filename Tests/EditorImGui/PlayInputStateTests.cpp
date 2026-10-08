#include "Nexora/EditorImGui/EditorImGui.h"
#include "PlayInputState.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace Nexora::Window;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
WindowEvent KeyEvent(Key key, bool down) {
  WindowEvent event;
  event.type = WindowEventType::Key;
  event.value0 = static_cast<int>(key);
  event.value1 = down;
  return event;
}
WindowEvent Mouse(int button, bool down) {
  WindowEvent event;
  event.type = WindowEventType::PointerButton;
  event.value0 = button;
  event.value1 = down;
  return event;
}
} // namespace
int main() {
  try {
    using namespace nexora;
    using namespace editor;
    preview::PlayInputState input;
    const std::array held{KeyEvent(Key::D, true), KeyEvent(Key::Space, true), Mouse(0, true)};
    input.Process(held);
    Require(input.Snapshot().move_x == 0 && input.Snapshot().buttons == 0,
            "unfocused input admitted");
    input.SetFocused(true);
    input.Process(held);
    Require(input.Snapshot().move_x == 0 && input.Snapshot().buttons == 0,
            "acquisition click fired in game");
    input.Process(held);
    const auto copied = input.Snapshot();
    Require(copied.move_x == 1 && copied.buttons == 3 && copied.reserved == 0,
            "copied input mapping failed");
    input.Process(std::array{KeyEvent(Key::A, true), KeyEvent(Key::W, true),
                             KeyEvent(Key::RightShift, true), KeyEvent(Key::LeftControl, true),
                             Mouse(1, true)});
    Require(input.Snapshot().move_x == 0 && input.Snapshot().move_y == 1 &&
                input.Snapshot().buttons == 31,
            "opposed axes or held modifier/button mapping failed");
    input.Process({});
    Require(input.Snapshot().buttons == 31 && input.Snapshot().sequence > copied.sequence,
            "held keys or frame sequence lost");
    input.Process(std::array{KeyEvent(Key::Escape, true)});
    Require(input.Snapshot().buttons == 0 && input.Snapshot().move_y == 0 && copied.buttons == 3,
            "Escape did not clear held input or owning snapshot changed");
    input.SetFocused(true);
    input.Process({});
    input.Process(held);
    WindowEvent lost;
    lost.type = WindowEventType::FocusChanged;
    lost.value0 = 0;
    input.Process(std::array{lost});
    Require(input.Snapshot().buttons == 0 && input.Snapshot().move_x == 0,
            "window blur retained pressed keys");
    input.SetFocused(true);
    input.Process({});
    input.Process(held);
    input.SetFocused(false);
    Require(input.Snapshot().buttons == 0 && input.Snapshot().move_x == 0,
            "pause/hide retained pressed keys");

    PlayInputBindings bindings;
    using Control = PlayInputControl;
    bindings.controls[0] = {Control::J, Control::Left};
    bindings.controls[1] = {Control::I, Control::Right};
    bindings.controls[2] = {Control::K, Control::Up};
    bindings.controls[3] = {Control::L, Control::Down};
    bindings.controls[4] = {Control::B, Control::None};
    bindings.controls[5] = {Control::C, Control::None};
    bindings.controls[6] = {Control::MouseLeft, Control::MouseRight};
    Require(bindings.Valid() && input.SetBindings(bindings), "custom profile rejected");
    input.SetFocused(true);
    input.Process({});
    const std::array rebound{KeyEvent(Key::I, true), KeyEvent(Key::K, true), KeyEvent(Key::B, true),
                             KeyEvent(Key::C, true), Mouse(0, true)};
    input.Process(rebound);
    Require(input.Snapshot().move_x == 1 && input.Snapshot().move_y == 1 &&
                input.Snapshot().buttons == 7,
            "rebound keyboard/mouse mapping failed");
    auto invalid = bindings;
    invalid.controls[2][0] = Control::I;
    Require(!invalid.Valid() && !input.SetBindings(invalid) && input.Snapshot().buttons == 7,
            "duplicate candidate changed held input");
    invalid = bindings;
    invalid.controls[0][0] = Control::MouseLeft;
    Require(!input.SetBindings(invalid), "mouse movement binding admitted");
    invalid.controls[0][0] = static_cast<Control>(255);
    Require(!input.SetBindings(invalid), "out-of-range control admitted");
    const auto custom_copy = input.Snapshot();
    Require(input.SetBindings({}) && input.Snapshot().buttons == 0 &&
                input.Snapshot().move_x == 0 && custom_copy.buttons == 7,
            "binding replacement retained held input or changed owning snapshot");
    input.Process(held);
    Require(input.Snapshot().buttons == 0, "replacement did not discard acquisition batch");
    input.Process(held);
    Require(input.Snapshot().buttons == 3 && input.Snapshot().move_x == 1,
            "default mapping did not resume after replacement");
    Require(input.SetBindings({}), "unchanged mapping rejected");
    input.Process({});
    Require(input.Snapshot().buttons == 3, "unchanged mapping cleared held input");
    PlayInputBindings unbound;
    for (std::size_t index = 4; index < unbound.controls.size(); ++index)
      unbound.controls[index] = {Control::None, Control::None};
    Require(input.SetBindings(unbound), "unbound buttons rejected");
    input.Process({});
    input.Process(held);
    Require(input.Snapshot().buttons == 0 && input.Snapshot().move_x == 1,
            "unbound buttons still fired or changed movement mapping");

    runtime::World world;
    const auto scene = world.LoadScene("Game input");
    Require(world.Activate(scene), "activate failed");
    SceneDocument document(world, scene);
    ProductShell shell;
    runtime::PlaySession play(world);
    Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
            "Play start failed");
    imgui::EditorImGuiHost host;
    host.SetDisplay(1280, 720, 1);
    WindowEvent focused;
    focused.type = WindowEventType::FocusChanged;
    focused.value0 = 1;
    host.ProcessEvents(std::array{focused});
    const auto draw = [&] {
      host.BeginFrame();
      host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
      static_cast<void>(host.EndFrame());
    };
    for (int frame = 0; frame < 4; ++frame)
      draw();
    auto viewport = host.NativeGameViewport();
    Require(viewport.has_value(), "Game canvas absent");
    WindowEvent pointer;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = viewport->x + viewport->width / 2;
    pointer.value1 = viewport->y + viewport->height / 2;
    host.ProcessEvents(std::array{pointer, Mouse(0, true)});
    draw();
    Require(host.GameInputFocused(), "canvas click did not capture Game input");
    host.ProcessEvents(std::array{Mouse(0, false)});
    draw();
    auto create = KeyEvent(Key::N, true);
    create.modifiers = static_cast<KeyModifiers>(3);
    host.ProcessEvents(std::array{create});
    draw();
    Require(document.Nodes().empty() && host.GameInputFocused(),
            "captured keys reached editor authoring shortcut");
    host.ProcessEvents(std::array{KeyEvent(Key::F6, true)});
    draw();
    Require(host.TakePlayCommand() == imgui::PlayCommand::Pause && host.GameInputFocused(),
            "captured input blocked the reserved Pause shortcut");
    host.ProcessEvents(std::array{KeyEvent(Key::F6, false)});
    draw();
    host.ProcessEvents(std::array{KeyEvent(Key::Escape, true)});
    draw();
    Require(!host.GameInputFocused(), "Escape did not release UI capture");
    host.ProcessEvents(std::array{KeyEvent(Key::Escape, false), pointer, Mouse(0, true)});
    draw();
    Require(host.GameInputFocused(), "capture could not be reacquired");
    host.ProcessEvents(std::array{lost});
    draw();
    Require(!host.GameInputFocused(), "window blur retained UI capture");
    const auto capture = [&] {
      host.ProcessEvents(std::array{focused, pointer, Mouse(0, false)});
      draw();
      host.ProcessEvents(std::array{Mouse(0, true)});
      draw();
      Require(host.GameInputFocused(), "capture transition failed");
      host.ProcessEvents(std::array{Mouse(0, false)});
      draw();
    };
    capture();
    Require(play.Pause(), "Pause failed");
    draw();
    Require(!host.GameInputFocused(), "Pause retained capture");
    Require(play.Resume(), "Resume failed");
    draw();
    capture();
    auto outside = pointer;
    outside.value0 = outside.value1 = -10;
    host.ProcessEvents(std::array{outside});
    draw();
    Require(!host.GameInputFocused(), "pointer exit retained capture");
    capture();
    host.RequestCloseConfirmation();
    Require(!host.GameInputFocused(), "close prompt retained capture");
    Require(play.Stop(), "Stop failed");
    std::cout << "Game input focus and copied snapshot contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
