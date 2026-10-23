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
  // Copied masks from the latest poll; only XInput currently supports motor output.
  uint32_t ConnectedMask() const noexcept { return connected_; }
  uint32_t RumbleMask() const noexcept;
  int32_t SetRumble(uint32_t device, float low, float high);
  const std::array<uint64_t, 4> &DisconnectSequences() const noexcept { return disconnected_; }

private:
#if defined(__linux__)
  std::array<int, 4> descriptors_{{-1, -1, -1, -1}};
  std::chrono::steady_clock::time_point next_scan_{};
#endif
#if defined(_WIN32)
  struct WindowsState;
  std::unique_ptr<WindowsState> windows_;
#endif
  bool Disconnect(GameplayInput &input, uint32_t device);
  uint32_t connected_{};
  std::array<uint64_t, 4> disconnected_{};
  std::thread::id owner_;
};
} // namespace nexora::runtime
