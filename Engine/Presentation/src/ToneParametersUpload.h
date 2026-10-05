#pragma once
#include "Nexora/Presentation/Surface.h"
#include <array>
namespace Nexora::Presentation {
// Matches two float4s in scene_tonemap.slang; copied scalar values own their submission lifetime.
using ToneParametersUpload = std::array<float, 8>;
static_assert(sizeof(ToneParametersUpload) == 32);
inline ToneParametersUpload PackToneParameters(float exposure, bool manualSrgb,
                                               const SceneBloom &bloom,
                                               const SceneColorGrade &grade) noexcept {
  return {exposure,        manualSrgb ? 1.0F : 0.0F, grade.saturation,   grade.contrast,
          bloom.intensity, bloom.threshold,          bloom.radiusPixels, 0};
}
} // namespace Nexora::Presentation
