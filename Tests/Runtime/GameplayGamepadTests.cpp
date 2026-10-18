#include "../../Engine/Runtime/src/GameplayDirectInputState.h"
#include "../../Engine/Runtime/src/GameplayXInputState.h"
#include "Nexora/Runtime/GameplayGamepad.h"
#include <array>
#include <cassert>
#include <cmath>
#include <thread>

using namespace nexora::runtime;
int main() {
  GameplayInput input;
  const auto api = input.Api();
  const auto bind = [&](std::uint32_t action, std::uint32_t control, std::uint32_t device = 2) {
    NexoraGameplayInputBindingV1 binding{};
    binding.struct_size = sizeof(binding);
    binding.abi_version = 1;
    binding.device = NEXORA_INPUT_DEVICE_GAMEPAD;
    binding.device_id = device;
    binding.control = control;
    binding.scale = 1;
    assert(api.bind(api.context, 0, action, &binding) == 0);
  };
  const auto read = [&](std::uint32_t action) {
    NexoraGameplayActionStateV1 state{};
    state.struct_size = sizeof(state);
    state.abi_version = 1;
    assert(api.read_action(api.context, 0, action, &state) == 0);
    return state;
  };
  // Bind every published button/axis so the actual provider checks the decoder's semantics.
  for (std::uint32_t i = 0; i < 16; ++i)
    bind(i + 1, i);
  for (std::uint32_t i = 0; i < 6; ++i)
    bind(i + 20, 32 + i);
  detail::XInputSample state{0xffff, {-32768, 32767, 16384, -32768}, {0, 255}};
  assert(input.BeginFrame() && detail::SubmitXInputSample(input, 0, 2, &state));
  for (std::uint32_t i = 0; i < 16; ++i) {
    const auto value = read(i + 1);
    assert(value.value == ((i == 10 || i == 11) ? 0.F : 1.F));
    assert(value.pressed == ((i == 10 || i == 11) ? 0u : 1u));
  }
  assert(read(20).value == -1 && read(21).value == -1 && read(22).value == 0);
  assert(std::abs(read(23).value - 16384.F / 32767.F) < 1e-6F);
  assert(read(24).value == 1 && read(25).value == 1);
  // Another slot does not activate bindings scoped to slot 2.
  bind(30, 0, 1);
  assert(read(30).value == 0);
  // Repeated full-state polling keeps held state without generating new press edges.
  assert(input.BeginFrame() && detail::SubmitXInputSample(input, 0, 2, &state));
  assert(read(1).value == 1 && read(1).pressed == 0);
  assert(input.SetFocused(false) && read(1).released == 1);
  assert(input.BeginFrame() && detail::SubmitXInputSample(input, 0, 2, &state));
  assert(read(1).value == 0);
  assert(input.SetFocused(true) && detail::SubmitXInputSample(input, 0, 2, &state));
  assert(read(1).value == 1 && read(1).pressed == 1);
  assert(!detail::SubmitXInputSample(input, 8, 2, nullptr));
  assert(!detail::SubmitXInputSample(input, 0, 4, nullptr));
  assert(read(1).value == 1); // Invalid disconnect does not clear another user's actions.
  assert(detail::SubmitXInputSample(input, 0, 2, nullptr));
  assert(read(1).value == 0 && read(1).released == 1 && read(25).value == 0);
  assert(input.BeginFrame());
  state = {};
  state.buttons = 0x0002; // D-pad down is control 13, not A or an implementation bit index.
  assert(detail::SubmitXInputSample(input, 0, 2, &state));
  assert(read(14).value == 1 && read(1).value == 0);
  state.buttons = 0x1000; // A remaps to control 0.
  assert(input.BeginFrame() && detail::SubmitXInputSample(input, 0, 2, &state));
  assert(read(1).pressed == 1 && read(14).released == 1);
  bool rejected{};
  NativeGameplayGamepad native;
  std::thread wrongThread(
      [&] { rejected = !detail::SubmitXInputSample(input, 0, 2, &state) && !native.Poll(input); });
  wrongThread.join();
  assert(rejected && read(1).value == 1);
  assert(!native.Poll(input, 8));

  // Real provider integration for copied PS4 reports; no hardware is simulated by this test.
  assert(input.Disconnect(NEXORA_INPUT_DEVICE_GAMEPAD, 2) && input.BeginFrame());
  detail::DirectInputSample ps4{};
  ps4.dualshock4 = true;
  ps4.axes = {-32768, 32767, 16384, -32768, 32767, -16384, 0, 0};
  ps4.buttons[1] = 0x80; // Cross -> confirm, square -> X, circle -> cancel.
  ps4.buttons[0] = 0x80;
  ps4.buttons[9] = 0x80; // Options -> Start.
  ps4.pov = 4500;        // Diagonal up/right.
  assert(detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(1).pressed && read(3).pressed && read(8).pressed && !read(2).held);
  assert(read(13).held && read(16).held && !read(14).held && !read(15).held);
  assert(read(20).value == -1 && read(21).value == 1 && read(22).value == 0);
  assert(std::abs(read(23).value - 16384.F / 32767.F) < 1e-6F);
  assert(std::abs(read(24).value + 16384.F / 32767.F) < 1e-6F && read(25).value == 1);
  assert(input.BeginFrame() && detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(1).held && !read(1).pressed);
  assert(input.SetFocused(false) && read(1).released);
  assert(detail::SubmitDirectInputSample(input, 0, 2, &ps4) && !read(1).held);
  assert(input.SetFocused(true) && detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(1).pressed);
  assert(!detail::SubmitDirectInputSample(input, 8, 2, nullptr));
  assert(!detail::SubmitDirectInputSample(input, 0, 4, nullptr) && read(1).held);
  assert(detail::SubmitDirectInputSample(input, 0, 2, nullptr));
  assert(read(1).released && !read(13).held && read(25).value == 0);
  ps4.buttons = {};
  ps4.buttons[2] = 0x80;
  ps4.pov = 0xffffffffU;
  assert(input.BeginFrame() && detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(2).pressed && !read(1).held && !read(13).held && !read(16).held);
  // Unprofiled joystick buttons/sticks are raw and remain rebindable.
  ps4.dualshock4 = false;
  ps4.pov = 18000;
  assert(input.BeginFrame() && detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(3).held && !read(2).held && read(14).held);
  assert(read(22).value == 16384.F / 32767.F && read(23).value == -1);
  bind(40, 39);
  bind(41, 47);
  bind(42, 31);
  ps4.axes[7] = 32767;
  ps4.buttons[31] = 0x80;
  assert(detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(40).value == 1 && read(41).value == 0 && read(42).held);
  // Full samples clear controls when replacing a raw joystick with a DS4 in the same slot.
  ps4.dualshock4 = true;
  assert(detail::SubmitDirectInputSample(input, 0, 2, &ps4));
  assert(read(40).value == 0 && !read(42).held);
  std::thread directWrongThread(
      [&] { rejected = !detail::SubmitDirectInputSample(input, 0, 2, &ps4); });
  directWrongThread.join();
  assert(rejected);
}
