#pragma once
#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace Nexora::Presentation {
inline std::size_t SceneRgbaTextureByteSize(std::uint32_t width, std::uint32_t height,
                                            std::uint32_t levels) noexcept {
  if (!width || !height || width > 4096 || height > 4096 || !levels || levels > 13)
    return 0;
  std::size_t size = 0;
  for (std::uint32_t level = 0; level < levels; ++level) {
    size += static_cast<std::size_t>(width) * height * 4;
    if (width == 1 && height == 1 && level + 1 < levels)
      return 0;
    width = std::max(1U, width / 2);
    height = std::max(1U, height / 2);
  }
  return size;
}
enum class SceneMipSemantic { None, Srgb, Linear, Normal };
// Ambiguous views, cutout masks and unlit atlases retain their original single level.
inline SceneMipSemantic ResolveSceneMipSemantic(const SceneDrawData &draw, std::uint64_t id) {
  if (!draw.pbr)
    return SceneMipSemantic::None;
  SceneMipSemantic semantic = SceneMipSemantic::None;
  bool found = false;
  const auto accept = [&](SceneMipSemantic candidate) {
    if (found && semantic != candidate)
      return false;
    semantic = candidate;
    found = true;
    return true;
  };
  for (const auto &material : draw.materials) {
    if (material.textureId == id || material.emissionTextureId == id) {
      if (material.alphaCutoff > 0 || material.unlit || !accept(SceneMipSemantic::Srgb))
        return SceneMipSemantic::None;
    }
    if (material.normalTextureId == id && !accept(SceneMipSemantic::Normal))
      return SceneMipSemantic::None;
    if (material.ormTextureId == id && !accept(SceneMipSemantic::Linear))
      return SceneMipSemantic::None;
  }
  return semantic;
}
struct SceneTextureMipChain final {
  std::vector<std::byte> bytes;
  std::uint32_t levels{1};
  [[nodiscard]] UiTextureUpload Upload(const UiTextureUpload &source) const {
    auto upload = source;
    if (!bytes.empty())
      upload.pixels = bytes;
    return upload;
  }
};
inline SceneTextureMipChain BuildSceneTextureMipmaps(const UiTextureUpload &source,
                                                     SceneMipSemantic semantic) {
  SceneTextureMipChain chain;
  if (source.rowPitch != source.width * 4 ||
      source.pixels.size() != SceneRgbaTextureByteSize(source.width, source.height, 1))
    return chain;
  if (semantic == SceneMipSemantic::None || (source.width == 1 && source.height == 1))
    return chain;
  chain.bytes.assign(source.pixels.begin(), source.pixels.end());
  auto width = source.width, height = source.height;
  std::size_t offset = 0;
  const auto decode = [](double value) {
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
  };
  const auto encode = [](double value) {
    return value <= 0.0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
  };
  while (width > 1 || height > 1) {
    const auto nextWidth = std::max(1U, width / 2), nextHeight = std::max(1U, height / 2);
    const auto nextOffset = chain.bytes.size();
    chain.bytes.resize(nextOffset + static_cast<std::size_t>(nextWidth) * nextHeight * 4);
    for (std::uint32_t y = 0; y < nextHeight; ++y)
      for (std::uint32_t x = 0; x < nextWidth; ++x) {
        std::array<double, 4> sum{};
        const auto x0 = x * width / nextWidth, x1 = (x + 1) * width / nextWidth;
        const auto y0 = y * height / nextHeight, y1 = (y + 1) * height / nextHeight;
        for (auto sy = y0; sy < y1; ++sy)
          for (auto sx = x0; sx < x1; ++sx)
            for (unsigned c = 0; c < 4; ++c) {
              double value =
                  std::to_integer<unsigned>(
                      chain.bytes[offset + (static_cast<std::size_t>(sy) * width + sx) * 4 + c]) /
                  255.0;
              if (c < 3 && semantic == SceneMipSemantic::Srgb)
                value = decode(value);
              if (c < 3 && semantic == SceneMipSemantic::Normal)
                value = value * 2 - 1;
              sum[c] += value;
            }
        for (auto &value : sum)
          value /= (x1 - x0) * (y1 - y0);
        if (semantic == SceneMipSemantic::Normal) {
          const auto length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
          if (length < 1e-6) {
            sum[0] = sum[1] = 0;
            sum[2] = 1;
          } else
            for (unsigned c = 0; c < 3; ++c)
              sum[c] /= length;
          for (unsigned c = 0; c < 3; ++c)
            sum[c] = sum[c] * 0.5 + 0.5;
        }
        for (unsigned c = 0; c < 4; ++c) {
          auto value = sum[c];
          if (c < 3 && semantic == SceneMipSemantic::Srgb)
            value = encode(value);
          chain.bytes[nextOffset + (static_cast<std::size_t>(y) * nextWidth + x) * 4 + c] =
              static_cast<std::byte>(std::clamp(std::lround(value * 255), 0L, 255L));
        }
      }
    offset = nextOffset;
    width = nextWidth;
    height = nextHeight;
    ++chain.levels;
  }
  return chain;
}
} // namespace Nexora::Presentation
