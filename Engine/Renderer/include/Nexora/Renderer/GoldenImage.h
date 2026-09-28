#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace nexora::renderer {

struct GoldenImageComparison {
  bool matched = false;
  std::size_t differing_pixels = 0;
  std::uint8_t max_channel_delta = 0;
};

[[nodiscard]] inline std::uint64_t HashRgba8(std::span<const std::byte> pixels) noexcept {
  constexpr std::uint64_t offset = 1469598103934665603ULL;
  constexpr std::uint64_t prime = 1099511628211ULL;
  auto hash = offset;
  for (const auto value : pixels) {
    hash ^= std::to_integer<std::uint8_t>(value);
    hash *= prime;
  }
  return hash;
}

[[nodiscard]] inline bool CompareGoldenRgba8(std::span<const std::byte> actual,
                                             std::span<const std::byte> expected,
                                             std::uint32_t width, std::uint32_t height,
                                             std::uint8_t channel_tolerance,
                                             GoldenImageComparison &comparison) noexcept {
  comparison = {};
  if (width == 0 || height == 0 || width > (UINT32_MAX / 4U) ||
      actual.size() != expected.size() || actual.size() !=
          static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U)
    return false;
  for (std::size_t pixel = 0; pixel < actual.size(); pixel += 4) {
    std::uint8_t pixel_delta = 0;
    for (std::size_t channel = 0; channel < 4; ++channel) {
      const auto lhs = std::to_integer<std::uint8_t>(actual[pixel + channel]);
      const auto rhs = std::to_integer<std::uint8_t>(expected[pixel + channel]);
      const auto delta = lhs > rhs ? lhs - rhs : rhs - lhs;
      pixel_delta = static_cast<std::uint8_t>(delta > pixel_delta ? delta : pixel_delta);
    }
    if (pixel_delta > channel_tolerance)
      ++comparison.differing_pixels;
    comparison.max_channel_delta = pixel_delta > comparison.max_channel_delta
                                       ? pixel_delta
                                       : comparison.max_channel_delta;
  }
  comparison.matched = comparison.differing_pixels == 0;
  return true;
}

} // namespace nexora::renderer
