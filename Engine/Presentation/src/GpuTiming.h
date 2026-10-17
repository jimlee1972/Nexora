#pragma once
#include "Nexora/Presentation/Surface.h"
#include "Nexora/RHI/GpuTiming.h"
#include <cmath>

namespace Nexora::Presentation::detail {
using nexora::rhi::detail::CompletionWallMilliseconds;
using nexora::rhi::detail::TimestampMilliseconds;
inline void PublishGpuTiming(SurfaceGpuTiming &timing, GpuTimingSource source,
                             std::uint64_t sequence, std::optional<double> milliseconds) noexcept {
  if (!sequence || sequence <= timing.completedSubmission)
    return;
  if (source == GpuTimingSource::Unavailable ||
      (milliseconds && (!std::isfinite(*milliseconds) || *milliseconds < 0)))
    milliseconds.reset();
  timing = {source, sequence, milliseconds};
}
} // namespace Nexora::Presentation::detail
