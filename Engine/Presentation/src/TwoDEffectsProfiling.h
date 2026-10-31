#pragma once
#include "GpuTiming.h"
#include <algorithm>
namespace Nexora::Presentation::detail {
inline void PublishTwoDEffects(SurfaceDiagnostics &diagnostics,
                               TwoDEffectsFrameDiagnostics frame) noexcept {
  if (!frame.completedFrame || frame.completedFrame <= diagnostics.twoDEffects.completedFrame)
    return;
  for (unsigned pass = 0; pass < 4; ++pass) {
    if (!(frame.timingMask & (1U << pass)))
      frame.milliseconds[pass] = 0;
    if (!std::isfinite(frame.milliseconds[pass]) || frame.milliseconds[pass] < 0) {
      frame.timingMask &= ~(1U << pass);
      frame.milliseconds[pass] = 0;
    }
  }
  if (frame.timingMask)
    frame.timingStatus = 2;
  diagnostics.twoDEffects = frame;
}
inline void TrackTwoDResources(SurfaceDiagnostics &d, uint64_t uploadCapacity,
                               uint64_t textures) noexcept {
  d.twoDUploadCapacityBytes = uploadCapacity;
  d.twoDTextureCount = textures;
  d.twoDPeakTextureCount = std::max(d.twoDPeakTextureCount, textures);
  const auto bytes =
      d.twoDLinearTargetBytes + d.twoDShadowBufferBytes + d.twoDBloomTargetBytes + uploadCapacity;
  d.twoDEffectsPeakBytes = std::max(d.twoDEffectsPeakBytes, bytes);
}
} // namespace Nexora::Presentation::detail
