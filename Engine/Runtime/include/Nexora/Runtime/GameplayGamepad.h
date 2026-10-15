#pragma once
#include "Nexora/Runtime/GameplayInput.h"
#include <array>
#include <chrono>
namespace nexora::runtime {
// Linux joystick adapter. Device IDs match /dev/input/js0..3; raw driver button
// indices map to 0..31, raw signed axis indices to 32..47. Rebind for each model.
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
  std::thread::id owner_;
};
} // namespace nexora::runtime
