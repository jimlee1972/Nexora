#pragma once

#include "Nexora/Presentation/Surface.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

namespace Nexora::Presentation {
// Private GPU vertex-input layout. The public descriptor (including std::optional) is never
// copied into a GPU upload; adapters consume the same packed affine and inverse-transpose rows.
struct SceneInstanceUpload final {
  float model[3][4]{};
  float normal[3][4]{}; // normal[0][3] carries the exact model determinant sign for tangents.
  float color[4]{};
};
static_assert(std::is_standard_layout_v<SceneInstanceUpload> &&
              std::is_trivially_copyable_v<SceneInstanceUpload>);
static_assert(sizeof(SceneInstanceUpload) == 112 && offsetof(SceneInstanceUpload, normal) == 48 &&
              offsetof(SceneInstanceUpload, color) == 96);

// Each pair of finite binary32 inputs multiplies exactly in binary64. Split the third
// multiplication with fma and accumulate its error using error-free TwoSum expansions.
// All possible binary32 triple products fit binary64's exponent range, including subnormals.
// This preserves exact singularity even when rounded determinant terms cancel catastrophically.
inline double AffineDeterminant(const std::array<float, 16> &m) noexcept {
  std::array<double, 12> expansion{};
  std::size_t size = 0;
  const auto add = [&](double term) {
    std::array<double, 12> next{};
    std::size_t count = 0;
    for (std::size_t index = 0; index < size; ++index) {
      const auto value = expansion[index];
      const auto sum = term + value;
      const auto recovered = sum - term;
      const auto error = (term - (sum - recovered)) + (value - recovered);
      if (error != 0)
        next[count++] = error;
      term = sum;
    }
    if (term != 0)
      next[count++] = term;
    expansion = next;
    size = count;
  };
  const auto product = [&](float a, float b, float c) {
    const double pair = static_cast<double>(a) * b;
    const double rounded = pair * c;
    add(std::fma(pair, static_cast<double>(c), -rounded));
    add(rounded);
  };
  product(m[0], m[5], m[10]);
  product(m[1], m[6], m[8]);
  product(m[2], m[4], m[9]);
  product(-m[0], m[6], m[9]);
  product(-m[1], m[4], m[10]);
  product(-m[2], m[5], m[8]);
  double result = 0;
  for (std::size_t index = 0; index < size; ++index)
    result += expansion[index];
  return result;
}

inline bool PackSceneInstance(const SceneInstance &instance, SceneInstanceUpload &output) noexcept {
  SceneInstanceUpload packed;
  float handedness = 1;
  for (std::size_t i = 0; i < 4; ++i) {
    if (!std::isfinite(instance.color[i]))
      return false;
    packed.color[i] = instance.color[i];
  }
  const auto store = [](double value, float &destination) {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
      return false;
    destination = static_cast<float>(value);
    return true;
  };
  if (instance.model_transform) {
    const auto &m = *instance.model_transform;
    if (!std::ranges::all_of(m, [](float value) { return std::isfinite(value); }) || m[12] != 0 ||
        m[13] != 0 || m[14] != 0 || m[15] != 1)
      return false;
    const double a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9],
                 i = m[10];
    const double cofactors[3][3]{{e * i - f * h, f * g - d * i, d * h - e * g},
                                 {c * h - b * i, a * i - c * g, b * g - a * h},
                                 {b * f - c * e, c * d - a * f, a * e - b * d}};
    const double determinant = AffineDeterminant(m);
    if (!std::isfinite(determinant) || determinant == 0)
      return false;
    handedness = determinant < 0 ? -1.0F : 1.0F;
    for (std::size_t row = 0; row < 3; ++row)
      for (std::size_t column = 0; column < 4; ++column) {
        packed.model[row][column] = m[row * 4 + column];
        if (column < 3 && !store(cofactors[row][column] / determinant, packed.normal[row][column]))
          return false;
      }
  } else {
    for (std::size_t i = 0; i < 3; ++i)
      if (!std::isfinite(instance.translation[i]) || !std::isfinite(instance.scale[i]) ||
          std::abs(instance.scale[i]) < 0.00001F)
        return false;
    float length = 0;
    for (const auto value : instance.rotation) {
      if (!std::isfinite(value))
        return false;
      length += value * value;
    }
    if (std::abs(length - 1.0F) > 0.01F)
      return false;
    handedness = ((instance.scale[0] < 0) ^ (instance.scale[1] < 0) ^ (instance.scale[2] < 0))
                     ? -1.0F
                     : 1.0F;
    const double x = instance.rotation[0], y = instance.rotation[1], z = instance.rotation[2],
                 w = instance.rotation[3];
    const double rotation[3][3]{
        {1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)},
        {2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)},
        {2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)}};
    for (std::size_t row = 0; row < 3; ++row) {
      packed.model[row][3] = instance.translation[row];
      for (std::size_t column = 0; column < 3; ++column)
        if (!store(rotation[row][column] * instance.scale[column], packed.model[row][column]) ||
            !store(rotation[row][column] / instance.scale[column], packed.normal[row][column]))
          return false;
    }
  }
  // Lighting uses normalized directions. A uniform positive rescale preserves inverse-transpose
  // direction while bounding shader dot products, even for tiny but invertible affine scales.
  float maximum = 0;
  for (const auto &row : packed.normal)
    for (std::size_t column = 0; column < 3; ++column)
      maximum = std::max(maximum, std::abs(row[column]));
  if (maximum == 0)
    return false;
  for (auto &row : packed.normal)
    for (std::size_t column = 0; column < 3; ++column)
      row[column] /= maximum;
  packed.normal[0][3] = handedness;
  output = packed;
  return true;
}

inline std::optional<std::vector<SceneInstanceUpload>>
PackSceneInstances(std::span<const SceneInstance> instances) {
  if (instances.size() > 4096)
    return std::nullopt;
  std::vector<SceneInstanceUpload> result(std::max<std::size_t>(instances.size(), 1));
  if (instances.empty()) {
    if (!PackSceneInstance(SceneInstance{}, result.front()))
      return std::nullopt;
  } else {
    for (std::size_t i = 0; i < instances.size(); ++i)
      if (!PackSceneInstance(instances[i], result[i]))
        return std::nullopt;
  }
  return result;
}
} // namespace Nexora::Presentation
