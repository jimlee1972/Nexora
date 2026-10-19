#pragma once
#include "Nexora/Runtime/GameplayInput.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace nexora::runtime::detail {
// Copy of DIJOYSTATE2's absolute controls after setting every axis to [-32768,32767].
struct DirectInputSample {
  std::array<std::int32_t, 8> axes{}; // X,Y,Z,Rx,Ry,Rz,slider0,slider1
  std::array<std::uint8_t, 32> buttons{};
  std::uint32_t pov{0xffffffffU};
  bool dualshock4{};
};
inline bool SubmitDirectInputSample(GameplayInput &input, std::uint32_t user, std::uint32_t device,
                                    const DirectInputSample *sample) {
  if (user >= 8 || device >= 4)
    return false;
  if (!sample)
    return input.Disconnect(NEXORA_INPUT_DEVICE_GAMEPAD, device);
  // DS4 face/shoulder/system controls match the existing Windows gameplay/UI layout.
  constexpr std::array<unsigned, 12> ps4{1, 2, 0, 3, 4, 5, 8, 9, 10, 11, 12, 13};
  for (std::uint32_t i = 0; i < 32; ++i) {
    bool held = (sample->buttons[i] & 0x80) != 0;
    if (sample->dualshock4 && i < ps4.size())
      held = (sample->buttons[ps4[i]] & 0x80) != 0;
    if (i >= 12 && i <= 15) {
      const auto pov = sample->pov;
      const bool valid = pov < 36000;
      if (i == 12)
        held = valid && (pov >= 31500 || pov <= 4500);
      if (i == 13)
        held = valid && pov >= 13500 && pov <= 22500;
      if (i == 14)
        held = valid && pov >= 22500 && pov <= 31500;
      if (i == 15)
        held = valid && pov >= 4500 && pov <= 13500;
    }
    if (sample->dualshock4 && i >= 16)
      held =
          i == 16 ? (sample->buttons[6] & 0x80) != 0 : i == 17 && (sample->buttons[7] & 0x80) != 0;
    if (!input.Submit(user, NEXORA_INPUT_DEVICE_GAMEPAD, device, i, held ? 1.F : 0.F))
      return false;
  }
  const auto signedAxis = [](std::int32_t value) {
    return std::clamp(static_cast<float>(value) / 32767.F, -1.F, 1.F);
  };
  const auto trigger = [](std::int32_t value) {
    return std::clamp((static_cast<float>(value) + 32768.F) / 65535.F, 0.F, 1.F);
  };
  for (std::uint32_t i = 0; i < 16; ++i) {
    float value = i < sample->axes.size() ? signedAxis(sample->axes[i]) : 0.F;
    if (sample->dualshock4) {
      constexpr std::array<unsigned, 6> axes{0, 1, 3, 2, 5, 4};
      value = i < axes.size() ? ((i == 2 || i == 5) ? trigger(sample->axes[axes[i]])
                                                    : signedAxis(sample->axes[axes[i]]))
                              : 0.F;
    }
    if (!input.Submit(user, NEXORA_INPUT_DEVICE_GAMEPAD, device, 32 + i, value))
      return false;
  }
  return true;
}
} // namespace nexora::runtime::detail
