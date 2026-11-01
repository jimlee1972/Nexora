#include "EditorImGuiTestAccess.h"
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
void ObservePlayShortcuts(float dpi, bool macos_behaviors = false) {
  using namespace nexora;
  using namespace editor;
  using Access = imgui::EditorImGuiTestAccess;
  using Command = imgui::PlayCommand;
  runtime::World world;
  const auto scene = world.LoadScene("Play shortcut observation");
  Require(world.Activate(scene), "Play shortcut scene activation failed");
  SceneDocument document(world, scene);
  const auto entity = document.Create("Keyboard target");
  Require(entity && document.Select(std::array{entity}), "Play shortcut entity setup failed");
  runtime::PlaySession play(world);
  ProductShell shell;
  imgui::EditorImGuiHost host;
  host.SetDisplay(1280, 720, dpi);
  Access::ConfigureSyntheticInput(host, macos_behaviors);
  bool attached = true;
  const auto draw = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr,
                          attached ? &play : nullptr);
    static_cast<void>(host.EndFrame());
  };
  WindowEvent focus;
  focus.type = WindowEventType::FocusChanged;
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  for (int frame = 0; frame < 4; ++frame)
    draw();
  Access::FocusHierarchy(host);
  draw();
  Require(Access::SceneShortcutInput(host).play_allowed,
          "Initial Play shortcut admission was not observed");
  const auto press = [&](Key key, std::size_t index, Command expected) {
    const auto before = Access::SceneShortcutInput(host);
    const auto actual_state = play.State();
    host.ProcessEvents(std::array{KeyEvent(key, true)});
    draw();
    const auto down = Access::SceneShortcutInput(host);
    const auto increment = expected == Command::None ? 0U : 1U;
    Require(down.play_keys[index] && down.frame > before.frame &&
                down.play_shortcut_requests == before.play_shortcut_requests + increment &&
                down.play_command == (increment ? expected : before.play_command) &&
                host.TakePlayCommand() == expected && play.State() == actual_state,
            "Actual Play key down lost its phase, single request or Runtime ownership");
    host.ProcessEvents(std::array{KeyEvent(key, false)});
    draw();
    const auto up = Access::SceneShortcutInput(host);
    Require(!up.play_keys[0] && !up.play_keys[1] && !up.play_keys[2] && up.frame > down.frame &&
                up.play_shortcut_requests == down.play_shortcut_requests &&
                up.play_command == down.play_command && host.TakePlayCommand() == Command::None,
            "Play key release repeated a request or cleared its last-command observation");
  };
  const auto unity_press = [&](KeyModifiers modifiers, Command expected) {
    const auto before = Access::SceneShortcutInput(host).play_shortcut_requests;
    auto event = KeyEvent(Key::P, true);
    event.modifiers = modifiers;
    host.ProcessEvents(std::array{event});
    draw();
    const auto actual = host.TakePlayCommand();
    const auto observed = Access::SceneShortcutInput(host);
    if (actual != expected ||
        observed.play_shortcut_requests != before + (expected == Command::None ? 0U : 1U))
      std::cerr << "Unity chord modifiers=" << static_cast<unsigned>(modifiers)
                << " expected=" << static_cast<unsigned>(expected)
                << " actual=" << static_cast<unsigned>(actual) << " before=" << before
                << " after=" << observed.play_shortcut_requests
                << " admitted=" << observed.play_allowed << '\n';
    Require(actual == expected &&
                observed.play_shortcut_requests == before + (expected == Command::None ? 0U : 1U),
            "Unity Play chord was lost, duplicated, or bypassed admission");
    event = KeyEvent(Key::P, false);
    host.ProcessEvents(std::array{event});
    draw();
    Require(host.TakePlayCommand() == Command::None, "Unity chord repeated on release");
  };
  const auto ctrl = macos_behaviors ? KeyModifiers::Super : KeyModifiers::Control;
  const auto pause_chord = static_cast<KeyModifiers>(static_cast<unsigned>(ctrl) |
                                                     static_cast<unsigned>(KeyModifiers::Shift));
  const auto step_chord = static_cast<KeyModifiers>(static_cast<unsigned>(ctrl) |
                                                    static_cast<unsigned>(KeyModifiers::Alt));
  unity_press(ctrl, Command::Start);
  unity_press(pause_chord, Command::None);
  unity_press(step_chord, Command::None);
  press(Key::F6, 1, Command::None);
  press(Key::F10, 2, Command::None);
  press(Key::F5, 0, Command::Start);
  Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
          "Observed Play Start could not be applied by its owner");
  draw();
  unity_press(pause_chord, Command::Pause);
  press(Key::F6, 1, Command::Pause);
  Require(play.Pause(), "Observed Pause could not be applied by its owner");
  draw();
  unity_press(step_chord, Command::Step);
  press(Key::F10, 2, Command::Step);
  press(Key::F6, 1, Command::Resume);
  Require(play.Resume(), "Observed Resume could not be applied by its owner");
  draw();
  unity_press(ctrl, Command::Stop);
  press(Key::F5, 0, Command::Stop);
  Require(play.Stop(), "Observed Stop could not be applied by its owner");
  attached = false;
  draw();
  Require(!Access::SceneShortcutInput(host).play_allowed,
          "Missing Play owner retained shortcut admission");
  unity_press(ctrl, Command::None);
  press(Key::F5, 0, Command::None);
  attached = true;
  draw();
  Access::FocusHierarchy(host);
  draw();
  host.ProcessEvents(std::array{KeyEvent(Key::F2, true)});
  draw();
  host.ProcessEvents(std::array{KeyEvent(Key::F2, false)});
  draw();
  draw();
  const auto modal = Access::SceneShortcutInput(host);
  Require(modal.want_text && !modal.play_allowed && Access::HierarchyRenameOpen(host),
          "Real Rename text input retained Play shortcut admission");
  unity_press(ctrl, Command::None);
  press(Key::F5, 0, Command::None);
  host.ProcessEvents(std::array{KeyEvent(Key::Escape, true)});
  draw();
  host.ProcessEvents(std::array{KeyEvent(Key::Escape, false)});
  draw();
  draw();
  Require(Access::SceneShortcutInput(host).play_allowed,
          "Closed Rename did not restore Play shortcut admission");
  press(Key::F5, 0, Command::Start);
}
} // namespace
int main() {
  try {
    ObservePlayShortcuts(1);
    ObservePlayShortcuts(2);
    ObservePlayShortcuts(1, true);
    ObservePlayShortcuts(2, true);
    using namespace nexora;
    using namespace editor;
    for (const auto action : {KeyModifiers::Control, KeyModifiers::Super}) {
      preview::PlayInputState reserved;
      PlayInputBindings bindings;
      bindings.controls[4] = {PlayInputControl::P, PlayInputControl::None};
      Require(reserved.SetBindings(bindings), "P binding fixture rejected");
      reserved.SetFocused(true);
      reserved.Process({}); // Acquisition is deliberately neutral.
      auto key = KeyEvent(Key::P, true);
      key.modifiers = action;
      reserved.Process(std::array{key});
      Require((reserved.Snapshot().buttons & 1U) == 0,
              "Editor Play chord also fired a game action");
      key.modifiers = KeyModifiers::None;
      reserved.Process(std::array{key});
      Require((reserved.Snapshot().buttons & 1U) == 0,
              "modifier release/autorepeat exposed the consumed P press");
      reserved.Process(std::array{KeyEvent(Key::P, false), KeyEvent(Key::P, true)});
      Require((reserved.Snapshot().buttons & 1U) != 0,
              "ordinary P binding did not recover after release");
    }
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
    WindowEvent acquisition_blur;
    acquisition_blur.type = WindowEventType::FocusChanged;
    acquisition_blur.value0 = 0;
    for (const auto &release : {acquisition_blur, KeyEvent(Key::Escape, true)}) {
      for (const bool release_first : {false, true}) {
        preview::PlayInputState acquiring;
        acquiring.SetFocused(true);
        const std::array batch =
            release_first ? std::array{release, held[0]} : std::array{held[0], release};
        acquiring.Process(batch);
        const auto released = acquiring.Snapshot();
        acquiring.Process(held);
        acquiring.Process({});
        Require(acquiring.Snapshot().move_x == 0 && acquiring.Snapshot().buttons == 0 &&
                    acquiring.Snapshot().sequence > released.sequence,
                "acquisition release admitted later input without reacquisition");
        acquiring.SetFocused(true);
        acquiring.Process(held);
        Require(acquiring.Snapshot().move_x == 0 && acquiring.Snapshot().buttons == 0,
                "release recovery skipped acquisition suppression");
        acquiring.Process(held);
        Require(acquiring.Snapshot().move_x == 1 && acquiring.Snapshot().buttons == 3 &&
                    released.move_x == 0 && released.buttons == 0,
                "release recovery failed or changed the owning released snapshot");
      }
    }
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

    for (int cycle = 0; cycle < 32; ++cycle) {
      preview::PlayInputState pointer;
      PlayInputBindings pointer_bindings;
      for (std::size_t index = 4; index < pointer_bindings.controls.size(); ++index)
        pointer_bindings.controls[index] = {
            static_cast<Control>(static_cast<int>(Control::MouseLeft) + index - 4), Control::None};
      Require(pointer.SetBindings(pointer_bindings), "five-button profile rejected");
      pointer.SetFocused(true);
      pointer.Process(std::array{Mouse(2, true), Mouse(3, true), Mouse(4, true)});
      pointer.Process({});
      Require(pointer.Snapshot().buttons == 0, "pointer acquisition retained held buttons");
      for (int button = 0; button < 5; ++button) {
        const auto expected = 1U << button;
        pointer.Process(
            std::array{Mouse(button, true), Mouse(-1, true), Mouse(5, true), Mouse(1000000, true)});
        const auto held_pointer = pointer.Snapshot();
        pointer.Process({});
        Require(pointer.Snapshot().buttons == expected && held_pointer.buttons == expected &&
                    held_pointer.reserved == 0,
                "pointer index mapped to the wrong copied action");
        pointer.Process(std::array{Mouse(button, false)});
        Require(pointer.Snapshot().buttons == 0 && held_pointer.buttons == expected,
                "pointer release retained held state or changed an owning snapshot");
        auto bad_pointer = pointer_bindings;
        bad_pointer.controls[0][0] = pointer_bindings.controls[4 + button][0];
        Require(!pointer.SetBindings(bad_pointer), "pointer movement binding admitted");
      }
      pointer.Process(std::array{Mouse(0, true), Mouse(1, true), Mouse(2, true), Mouse(3, true),
                                 Mouse(4, true)});
      Require(pointer.Snapshot().buttons == 31, "simultaneous pointer buttons lost an owner");
      Require(pointer.SetBindings(pointer_bindings) && pointer.Snapshot().buttons == 31,
              "unchanged pointer bindings cleared held input");
      pointer.Process(std::array{KeyEvent(Key::Escape, true)});
      pointer.Process(std::array{Mouse(3, true), Mouse(4, true)});
      Require(pointer.Snapshot().buttons == 0, "released pointer admitted later held input");
      pointer.SetFocused(true);
      pointer.Process({});
      pointer.Process(std::array{Mouse(4, true)});
      Require(pointer.SetBindings({}) && pointer.Snapshot().buttons == 0,
              "pointer profile replacement retained held input");
      Require(pointer.SetBindings(pointer_bindings), "pointer blur profile reset failed");
      pointer.Process({});
      pointer.Process(std::array{Mouse(2, true), Mouse(3, true), Mouse(4, true)});
      Require(pointer.Snapshot().buttons == 28, "pointer blur fixture did not acquire buttons");
      pointer.Process(std::array{lost});
      pointer.Process(std::array{Mouse(2, true), Mouse(3, true), Mouse(4, true)});
      Require(pointer.Snapshot().buttons == 0, "native blur retained extra pointer buttons");
    }

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
    using Access = imgui::EditorImGuiTestAccess;
    Require(host.GameInputUser() == 0 && !host.SetGameInputUser(4),
            "Default or invalid graphical user selection failed");
    const auto click_user_point = [&](std::optional<std::array<float, 2>> point) {
      Require(point.has_value(), "Rendered input user control missing");
      WindowEvent move;
      move.type = WindowEventType::Pointer;
      move.value0 = static_cast<int>((*point)[0]);
      move.value1 = static_cast<int>((*point)[1]);
      host.ProcessEvents(std::array{move, Mouse(0, false)});
      draw();
      host.ProcessEvents(std::array{Mouse(0, true)});
      draw();
      host.ProcessEvents(std::array{Mouse(0, false)});
      draw();
      draw();
    };
    click_user_point(Access::GameInputUserPosition(host));
    click_user_point(Access::GameInputUserPosition(host, 3));
    Require(host.GameInputUser() == 3 && !host.GameInputFocused() && !host.SetGameInputUser(4) &&
                host.GameInputUser() == 3,
            "Rendered selected user or invalid-candidate conservation failed");
    auto viewport = host.NativeGameViewport();
    Require(viewport.has_value(), "Game canvas absent");
    WindowEvent pointer;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = viewport->x + viewport->width / 2;
    pointer.value1 = viewport->y + viewport->height / 2;
    const auto before_capture = Access::SceneShortcutInput(host);
    host.ProcessEvents(std::array{pointer, Mouse(0, true)});
    draw();
    const auto captured_input = Access::SceneShortcutInput(host);
    Require(host.GameInputFocused() && captured_input.game_focused && captured_input.game_hovered &&
                captured_input.game_clicked && captured_input.game_capture_allowed &&
                captured_input.game_capture_requests == before_capture.game_capture_requests + 1,
            "Actual canvas click, admission or capture count was not observed");
    Require(host.SetGameInputUser(3) && host.GameInputFocused(), "Unchanged user revoked capture");
    Require(host.SetGameInputUser(2) && !host.GameInputFocused(),
            "Changing user retained UI capture");
    host.ProcessEvents(std::array{Mouse(0, false)});
    draw();
    const auto released_capture = Access::SceneShortcutInput(host);
    Require(!released_capture.game_clicked && !released_capture.game_focused &&
                released_capture.game_capture_requests == captured_input.game_capture_requests,
            "Canvas release repeated capture or lost the changed user's focus observation");
    host.ProcessEvents(std::array{pointer, Mouse(0, true)});
    draw();
    Require(host.GameInputFocused(), "User switch could not reacquire canvas");
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
    for (const bool macos : {false, true}) {
      Access::ConfigureSyntheticInput(host, macos);
      const auto action = macos ? KeyModifiers::Super : KeyModifiers::Control;
      auto unity_pause = KeyEvent(Key::P, true);
      unity_pause.modifiers = static_cast<KeyModifiers>(static_cast<unsigned>(action) | 2U);
      host.ProcessEvents(std::array{unity_pause});
      draw();
      Require(host.TakePlayCommand() == imgui::PlayCommand::Pause && host.GameInputFocused() &&
                  document.Nodes().empty(),
              "Game capture blocked Ctrl/Command Unity Pause or leaked authoring input");
      auto release_shift = KeyEvent(Key::LeftShift, false);
      release_shift.modifiers = action;
      host.ProcessEvents(std::array{release_shift});
      draw();
      host.ProcessEvents(std::array{KeyEvent(macos ? Key::LeftSuper : Key::LeftControl, false)});
      draw();
      Require(!Access::SceneShortcutInput(host).keys[0] &&
                  !Access::SceneShortcutInput(host).keys[2] && !Access::SuperModifierDown(host),
              "captured modifier release left Ctrl/Command latched while P was held");
      host.ProcessEvents(std::array{KeyEvent(Key::P, false)});
      draw();
      Require(host.TakePlayCommand() == imgui::PlayCommand::None,
              "Unity chord release repeated its Play request");
    }
#if defined(__APPLE__)
    Access::ConfigureSyntheticInput(host, true);
#else
    Access::ConfigureSyntheticInput(host);
#endif
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
