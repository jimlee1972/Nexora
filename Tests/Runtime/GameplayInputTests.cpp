#include "Nexora/Runtime/GameplayInput.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  nexora::runtime::GameplayInput input;
  auto api = input.Api();
  const auto bind = [&](uint32_t device, uint32_t control, float scale = 1.0F) {
    NexoraGameplayInputBindingV1 binding{sizeof(binding), 1, device, 0, control, scale, 0.2F, 0};
    Require(api.bind(api.context, 0, 1, &binding) == 0, "bind");
  };
  auto read = [&] {
    NexoraGameplayActionStateV1 state{sizeof(state), 1, 0, 0, 0, 0, 0};
    Require(api.read_action(api.context, 0, 1, &state) == 0, "read");
    return state;
  };
  bind(1, 69);
  bind(3, 32);
  Require(input.BeginFrame() && input.Submit(0, 1, 0, 69, 1), "keyboard press");
  auto state = read();
  Require(state.held && state.pressed && !state.released, "press edge");
  Require(read().pressed, "reads do not consume");
  Require(input.BeginFrame(), "frame");
  Require(read().held && !read().pressed, "held across frames");
  Require(input.Submit(0, 3, 0, 32, 0.8F), "axis");
  NexoraGameplayDeviceStateV1 device{sizeof(device), 1, 0, 0, 0, 0, 0};
  Require(api.read_device(api.context, 0, &device) == 0 && device.active_device == 3,
          "device switch");
  Require(input.Submit(0, 1, 0, 69, 0), "keyboard release");
  Require(std::abs(read().value - 0.8F) < 0.0001F && !read().released, "other binding stays held");
  Require(input.Disconnect(3, 0), "disconnect");
  Require(!read().held && read().released, "disconnect release");
  Require(input.BeginFrame() && input.Submit(0, 3, 0, 32, 0.1F), "deadzone");
  Require(!read().held && !read().pressed, "deadzone neutral");
  Require(input.Submit(0, 1, 0, 69, 1) && input.SetFocused(false), "blur");
  Require(!read().held && read().released && !read().pressed, "blur releases and cancels press");
  Require(input.Submit(0, 1, 0, 69, 1) && !read().held, "ignore unfocused events");
  Require(input.SetFocused(true) && !read().held, "focus does not resurrect");
  Require(input.Submit(0, 1, 0, 69, 1), "fresh event");
  Require(api.clear_bindings(api.context, 0, 1) == 0 && read().released && !read().held,
          "clear releases");
  bind(1, 70, -1);
  Require(input.Submit(0, 1, 0, 69, 1) && !read().held, "old binding removed");
  Require(input.Submit(0, 1, 0, 70, 1) && read().value == -1, "rebound binding");
  Require(input.BeginFrame() && input.Submit(0, 1, 0, 70, 0) && input.Submit(0, 1, 0, 70, 1),
          "tap");
  Require(read().pressed && read().released && read().held, "both transitions retained");
  NexoraGameplayInputBindingV1 secondUser{sizeof(secondUser), 1, 2, 0, 0, 1, 0, 0};
  Require(api.bind(api.context, 1, 1, &secondUser) == 0, "second user binding");
  Require(input.Submit(1, 2, 0, 0, 1), "second user input");
  NexoraGameplayActionStateV1 secondState{sizeof(secondState), 1, 0, 0, 0, 0, 0};
  Require(api.read_action(api.context, 1, 1, &secondState) == 0 && secondState.value == 1 &&
              read().value == -1,
          "user actions isolated");
  Require(input.BeginFrame() && input.Submit(0, 1, 0, 70, 0), "release before opposing test");
  bind(1, 71, 1);
  Require(input.Submit(0, 1, 0, 70, 1) && input.Submit(0, 1, 0, 71, 1) && !read().held,
          "opposing bindings cancel");
  Require(input.Submit(0, 1, 0, 70, 0) && read().value == 1, "opposing release restores held");
  const auto original = read();
  state = original;
  state.abi_version = 99;
  const auto invalid = state;
  Require(api.read_action(api.context, 0, 1, &state) == -1 &&
              std::memcmp(&state, &invalid, sizeof(state)) == 0,
          "errors preserve");
  Require(!input.Submit(0, 1, 0, 70, std::numeric_limits<float>::quiet_NaN()), "reject nan");
  Require(!input.Submit(8, 1, 0, 70, 1), "bound users");
  int32_t threadResult = 0;
  std::thread worker([&] {
    state = original;
    threadResult = api.read_action(api.context, 0, 1, &state);
  });
  worker.join();
  Require(threadResult == -3 && std::memcmp(&state, &original, sizeof(state)) == 0, "owner thread");
  const auto pointer = [&] {
    NexoraGameplayPointerStateV1 result{sizeof(result), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Require(api.read_pointer(api.context, 0, &result) == 0, "read pointer");
    return result;
  };
  Require(input.BeginFrame() && input.SubmitPointer(0, 100, 200), "pointer anchor");
  Require(pointer().x == 100 && pointer().y == 200 && pointer().delta_x == 0,
          "first sample anchors");
  Require(input.SubmitPointer(0, 110, 180, 1, 120) && input.SubmitPointer(0, 115, 185, 2, -40),
          "pointer accumulation");
  Require(pointer().delta_x == 15 && pointer().delta_y == -15 && pointer().wheel_x == 3 &&
              pointer().wheel_y == 80,
          "copied motion and wheel");
  Require(pointer().delta_x == 15, "pointer repeated read does not consume");
  Require(input.Submit(0, 2, 0, 0, 1) && input.Submit(0, 2, 0, 4, 1) && pointer().buttons == 17,
          "pointer held buttons");
  Require(input.BeginFrame() && pointer().delta_x == 0 && pointer().wheel_y == 0 &&
              pointer().buttons == 17 && pointer().x == 115,
          "next frame preserves position and held");
  Require(input.SetFocused(false) && pointer().focused == 0 && pointer().buttons == 0 &&
              pointer().delta_x == 0,
          "pointer focus reset");
  Require(input.SubmitPointer(0, 999, 999, 0, 120) && pointer().x == 115 && pointer().wheel_y == 0,
          "unfocused pointer ignored");
  Require(input.SetFocused(true) && input.SubmitPointer(0, 500, 600) && pointer().delta_x == 0,
          "focus recovery anchors without jump");
  Require(input.SubmitPointer(0, 510, 610, 0, 120) && input.Disconnect(2, 0) &&
              pointer().delta_x == 0 && pointer().wheel_y == 0,
          "mouse disconnect resets");
  Require(input.SubmitPointer(0, -500, -600) && pointer().delta_y == 0, "mouse reconnect anchors");
  Require(!input.SubmitPointer(0, std::numeric_limits<double>::infinity(), 0),
          "reject invalid pointer");
  Require(input.SubmitPointer(0, 1e20, -1e20, 1e20, -1e20) && pointer().delta_x == 65536 &&
              pointer().delta_y == -65536 && pointer().wheel_x == 122880 &&
              pointer().wheel_y == -122880,
          "finite pointer saturation");
  const auto pointerOriginal = pointer();
  auto invalidPointer = pointerOriginal;
  invalidPointer.abi_version = 99;
  const auto invalidPointerOriginal = invalidPointer;
  Require(api.read_pointer(api.context, 0, &invalidPointer) == -1 &&
              std::memcmp(&invalidPointer, &invalidPointerOriginal, sizeof(invalidPointer)) == 0,
          "pointer invalid output preserved");
  struct ExtendedPointer {
    NexoraGameplayPointerStateV1 state;
    uint64_t tail;
  } extendedPointer{pointerOriginal, 456};
  extendedPointer.state.struct_size = sizeof(extendedPointer);
  Require(api.read_pointer(api.context, 0, &extendedPointer.state) == 0 &&
              extendedPointer.tail == 456,
          "pointer future suffix preserved");
  struct Extended {
    NexoraGameplayActionStateV1 state;
    uint64_t tail;
  } extended{{sizeof(Extended), 1, 0, 0, 0, 0, 0}, 123};
  Require(api.read_action(api.context, 0, 1, &extended.state) == 0 && extended.tail == 123 &&
              extended.state.struct_size == sizeof(Extended),
          "future suffix preserved");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
