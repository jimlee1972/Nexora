#pragma once
#include "Nexora/Runtime/GameplayInput.h"
#include <array>
#include <chrono>
#include <memory>
namespace nexora::runtime {
// Linux joystick / Windows XInput + DirectInput adapter. Four device slots (0..3).
// See GameplayInput.md for raw button/axis mappings and per-model rebinding.
// Other platforms return false (no simulated controller is advertised).
class NEXORA_RUNTIME_API NativeGameplayGamepad final {
public:
  NativeGameplayGamepad();
  ~NativeGameplayGamepad();
  NativeGameplayGamepad(const NativeGameplayGamepad &) = delete;
  NativeGameplayGamepad &operator=(const NativeGameplayGamepad &) = delete;
  bool Poll(GameplayInput &input, std::uint32_t user = 0);

private:
#if defined(__linux__)
  std::array<int, 4> descriptors_{{-1, -1, -1, -1}};
  std::chrono::steady_clock::time_point next_scan_{};
#endif
#if defined(_WIN32)
  struct WindowsState;
  std::unique_ptr<WindowsState> windows_;
#endif
  std::thread::id owner_;
};
} // namespace nexora::runtime
