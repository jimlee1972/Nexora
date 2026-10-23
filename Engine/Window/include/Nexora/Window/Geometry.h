#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
namespace Nexora::Window {
// Native-pixel client size, preserving aspect within integer rounding and frame overhead.
inline std::optional<std::pair<uint32_t, uint32_t>>
FitClientExtent(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight,
                uint32_t borderWidth, uint32_t borderHeight) {
  if (!width || !height || workWidth <= borderWidth || workHeight <= borderHeight)
    return std::nullopt;
  const double scale = std::min(
      {1., double(workWidth - borderWidth) / width, double(workHeight - borderHeight) / height});
  return std::pair{std::max(1U, uint32_t(width * scale)), std::max(1U, uint32_t(height * scale))};
}
} // namespace Nexora::Window
