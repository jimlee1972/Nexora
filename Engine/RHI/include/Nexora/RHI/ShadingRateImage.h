#pragma once
#include "Nexora/RHI/Types.h"
#include <span>
namespace nexora::rhi {
struct ShadingRateImageTag;
using ShadingRateImageHandle = core::Handle<ShadingRateImageTag>;
inline constexpr std::uint32_t ShadingRateImageMaximumRasterExtent = 4096;
inline constexpr std::uint32_t ShadingRateImageMaximumBindingsPerList = 16;
struct ShadingRateImageDescriptor final {
  std::uint32_t raster_width{}, raster_height{};
  std::string debug_name;
};
// Native baseline rate encodings:1x1,1x2,2x1,2x2. Additional rates are not admitted.
constexpr bool IsShadingRateImageCodeValid(std::uint8_t code) noexcept {
  return code == 0 || code == 1 || code == 4 || code == 5;
}
constexpr bool IsShadingRateImageShapeValid(const ShadingRateImageDescriptor &descriptor,
                                            std::uint32_t tile, std::size_t count) noexcept {
  if (!descriptor.raster_width || !descriptor.raster_height ||
      descriptor.raster_width > ShadingRateImageMaximumRasterExtent ||
      descriptor.raster_height > ShadingRateImageMaximumRasterExtent || tile < 8 || tile > 32 ||
      (tile & (tile - 1)))
    return false;
  return count == static_cast<std::size_t>((descriptor.raster_width + tile - 1) / tile) *
                      ((descriptor.raster_height + tile - 1) / tile);
}
} // namespace nexora::rhi
