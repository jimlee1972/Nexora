#pragma once
#include "Nexora/Presentation/Surface.h"
#include <array>
namespace Nexora::Presentation {
// Matches three float4s in scene_tonemap.slang; copied scalar values own their submission lifetime.
using ToneParametersUpload = std::array<float, 12>;
static_assert(sizeof(ToneParametersUpload) == 48);
inline ToneParametersUpload
PackToneParameters(float exposure, bool manualSrgb, const SceneBloom &bloom,
                   const SceneColorGrade &grade, std::uint32_t width, std::uint32_t height,
                   const SceneDepthOfField &focus = {10, 0, 12}) noexcept {
  return {exposure,
          manualSrgb ? 1.0F : 0.0F,
          grade.saturation,
          grade.contrast,
          bloom.intensity,
          bloom.threshold,
          bloom.radiusPixels / static_cast<float>(width),
          bloom.radiusPixels / static_cast<float>(height),
          focus.focusDistance,
          focus.strength,
          focus.radiusPixels / static_cast<float>(width),
          focus.radiusPixels / static_cast<float>(height)};
}
} // namespace Nexora::Presentation
