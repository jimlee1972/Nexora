#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Double-precision scalar oracle, separate from production plan/layout/shader implementation.
namespace TwoDBloomReference {
struct Image {
  unsigned width{}, height{};
  std::vector<double> pixels;
  double Sample(double u, double v) const {
    const double x = u * width - 0.5, y = v * height - 0.5;
    const int ix = int(std::floor(x)), iy = int(std::floor(y));
    const double fx = x - std::floor(x), fy = y - std::floor(y);
    const auto at = [&](int px, int py) {
      return pixels[std::size_t(std::clamp(py, 0, int(height) - 1)) * width +
                    unsigned(std::clamp(px, 0, int(width) - 1))];
    };
    return (1 - fy) * ((1 - fx) * at(ix, iy) + fx * at(ix + 1, iy)) +
           fy * ((1 - fx) * at(ix, iy + 1) + fx * at(ix + 1, iy + 1));
  }
};
inline Image Filter(Image input, unsigned levels, double radius) {
  std::vector<Image> down;
  const double tapPixels = radius / (0.5 * levels + levels - 1);
  const double du = tapPixels / input.width, dv = tapPixels / input.height;
  for (unsigned level = 0; level < levels; ++level) {
    const auto &source = level ? down.back() : input;
    Image result{std::max(1U, (source.width + 1) / 2), std::max(1U, (source.height + 1) / 2), {}};
    result.pixels.resize(std::size_t(result.width) * result.height);
    for (unsigned y = 0; y < result.height; ++y)
      for (unsigned x = 0; x < result.width; ++x) {
        double value{};
        for (const double dy : {-0.5, 0.5})
          for (const double dx : {-0.5, 0.5})
            value += source.Sample((x + 0.5) / result.width + dx * du,
                                   (y + 0.5) / result.height + dy * dv) *
                     0.25;
        result.pixels[std::size_t(y) * result.width + x] = value;
      }
    down.push_back(std::move(result));
  }
  Image current = std::move(down.back());
  for (int level = int(levels) - 2; level >= 0; --level) {
    const auto &detail = down[unsigned(level)];
    Image result{detail.width, detail.height, {}};
    result.pixels.resize(detail.pixels.size());
    for (unsigned y = 0; y < result.height; ++y)
      for (unsigned x = 0; x < result.width; ++x) {
        const double u = (x + 0.5) / result.width, v = (y + 0.5) / result.height;
        double value{};
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx)
            value += current.Sample(u + dx * du, v + dy * dv) *
                     ((dx == 0 ? 2 : 1) * (dy == 0 ? 2 : 1)) / 16.;
        result.pixels[std::size_t(y) * result.width + x] =
            (value + detail.pixels[std::size_t(y) * result.width + x]) * 0.5;
      }
    current = std::move(result);
  }
  return current;
}
} // namespace TwoDBloomReference
