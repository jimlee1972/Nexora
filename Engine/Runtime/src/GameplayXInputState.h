#pragma once
#include "Nexora/Runtime/GameplayInput.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace nexora::runtime::detail {
// Platform-neutral copy of the documented XInput fields. Native handles stay in the adapter.
struct XInputSample {
  std::uint16_t buttons{};
  std::array<std::int16_t, 4> sticks{}; // LX, LY (up positive), RX, RY (up positive)
  std::array<std::uint8_t, 2> triggers{};
};
inline bool SubmitXInputSample(GameplayInput &input, std::uint32_t user, std::uint32_t device,
                               const XInputSample *sample) {
  if (user >= 8 || device >= 4)
    return false;
  if (!sample)
    return input.Disconnect(NEXORA_INPUT_DEVICE_GAMEPAD, device);
  // A/B/X/Y, shoulders, back/start, thumb clicks, two reserved zeros, D-pad U/D/L/R.
  constexpr std::array<std::uint16_t, 16> masks{0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200,
                                                0x0020, 0x0010, 0x0040, 0x0080, 0,      0,
                                                0x0001, 0x0002, 0x0004, 0x0008};
  for (std::uint32_t i = 0; i < masks.size(); ++i)
    if (!input.Submit(user, NEXORA_INPUT_DEVICE_GAMEPAD, device, i,
                      (sample->buttons & masks[i]) ? 1.F : 0.F))
      return false;
  const auto axis = [](std::int16_t value) {
    return std::clamp(static_cast<float>(value) / 32767.F, -1.F, 1.F);
  };
  // Match the common Linux raw-axis convention: Y down positive; triggers 0..1.
  const std::array<float, 6> axes{axis(sample->sticks[0]),
                                  -axis(sample->sticks[1]),
                                  static_cast<float>(sample->triggers[0]) / 255.F,
                                  axis(sample->sticks[2]),
                                  -axis(sample->sticks[3]),
                                  static_cast<float>(sample->triggers[1]) / 255.F};
  for (std::uint32_t i = 0; i < axes.size(); ++i)
    if (!input.Submit(user, NEXORA_INPUT_DEVICE_GAMEPAD, device, 32 + i, axes[i]))
      return false;
  return true;
}
} // namespace nexora::runtime::detail
