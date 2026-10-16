#pragma once

#include "Nexora/RHI/Types.h"

#include <optional>

namespace nexora::rhi::detail {
// SDK tier integers use different encodings. Unknown future values remain unknown rather than
// accidentally reporting a higher supported tier. Zero is an explicit NOT_SUPPORTED result.
inline std::optional<FeatureVersion> DecodeD3D12Tier(GraphicsFeature feature,
                                                     std::uint32_t value) noexcept {
  if (value == 0)
    return FeatureVersion{};
  switch (feature) {
  case GraphicsFeature::RayTracing:
    if (value == 10 || value == 11 || value == 12)
      return FeatureVersion{1, static_cast<std::uint16_t>(value - 10)};
    break;
  case GraphicsFeature::MeshShader:
  case GraphicsFeature::WorkGraphs:
    if (value == 10)
      return FeatureVersion{1, 0};
    break;
  case GraphicsFeature::VariableRateShading:
    if (value == 1 || value == 2)
      return FeatureVersion{static_cast<std::uint16_t>(value), 0};
    break;
  case GraphicsFeature::SamplerFeedback:
    if (value == 90)
      return FeatureVersion{0, 9};
    if (value == 100)
      return FeatureVersion{1, 0};
    break;
  default:
    break;
  }
  return std::nullopt;
}
} // namespace nexora::rhi::detail
