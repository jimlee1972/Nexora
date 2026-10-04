#include "Nexora/Presentation/Surface.h"
#include "SceneInstanceUpload.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace Nexora::Presentation;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::abs(a - b) < 1e-5F; }
void Run() {
  SceneInstance instance;
  Require(ValidateSceneInstance(instance), "identity instance rejected");
  instance.model_transform =
      std::array<float, 16>{-2, 1, 0, 7, 0, 3, 0, -4, 0.5F, 0, 1, 2, 0, 0, 0, 1};
  // Override means stale/invalid TRS is ignored, while tint is always validated.
  instance.translation[0] = std::numeric_limits<float>::quiet_NaN();
  instance.scale[0] = 0;
  instance.rotation[3] = 0;
  SceneInstanceUpload packed;
  Require(ValidateSceneInstance(instance) && PackSceneInstance(instance, packed),
          "valid affine override rejected because of unused TRS");
  const float point[4]{1, 2, 3, 1};
  const float expected[3]{7, 2, 5.5F};
  for (std::size_t row = 0; row < 3; ++row) {
    float actual = 0;
    for (std::size_t column = 0; column < 4; ++column)
      actual += packed.model[row][column] * point[column];
    Require(Near(actual, expected[row]), "row-major model transform changed the affine point");
  }
  // A^-T times +Z is (0.25, -1/12, 1); it remains orthogonal to both transformed tangents.
  Require(Near(packed.normal[0][2], 0.25F) && Near(packed.normal[1][2], -1.0F / 12) &&
              Near(packed.normal[2][2], 1),
          "mirrored/sheared inverse-transpose normal is wrong");
  for (std::size_t tangent = 0; tangent < 2; ++tangent) {
    float dot = 0;
    for (std::size_t row = 0; row < 3; ++row)
      dot += packed.normal[row][2] * packed.model[row][tangent];
    Require(Near(dot, 0), "normal lost orthogonality to a transformed surface tangent");
  }
  const auto valid = instance;
  const auto retained = packed;
  const auto rejects = [&](SceneInstance invalid) {
    Require(!ValidateSceneInstance(invalid) && !PackSceneInstance(invalid, packed),
            "invalid affine instance accepted");
    for (std::size_t row = 0; row < 3; ++row)
      for (std::size_t column = 0; column < 4; ++column)
        Require(packed.model[row][column] == retained.model[row][column] &&
                    packed.normal[row][column] == retained.normal[row][column],
                "failed packing changed caller output");
  };
  for (std::size_t index = 0; index < 16; ++index) {
    auto invalid = valid;
    (*invalid.model_transform)[index] = std::numeric_limits<float>::infinity();
    rejects(invalid);
  }
  for (const auto index : {12, 13, 14, 15}) {
    auto invalid = valid;
    (*invalid.model_transform)[index] = index == 15 ? 2 : 0.1F;
    rejects(invalid);
  }
  auto singular = valid;
  (*singular.model_transform)[4] = (*singular.model_transform)[0];
  (*singular.model_transform)[5] = (*singular.model_transform)[1];
  (*singular.model_transform)[6] = (*singular.model_transform)[2];
  rejects(singular);
  auto bad_color = valid;
  bad_color.color[2] = std::numeric_limits<float>::quiet_NaN();
  rejects(bad_color);
  auto unrepresentable = valid;
  unrepresentable.model_transform = std::array<float, 16>{
      std::numeric_limits<float>::denorm_min(), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  rejects(unrepresentable);
  SceneInstance legacy;
  legacy.translation[0] = 3;
  legacy.scale[0] = -2;
  legacy.scale[1] = 3;
  legacy.rotation[2] = legacy.rotation[3] = std::sqrt(0.5F);
  Require(PackSceneInstance(legacy, packed) && Near(packed.model[0][3], 3) &&
              Near(packed.model[1][0], -2) && Near(packed.model[0][1], -3) &&
              Near(packed.normal[1][0], -0.5F) && Near(packed.normal[0][1], -1.0F / 3),
          "legacy quaternion/nonuniform mirrored scale semantics changed");
  legacy.scale[0] = 0;
  Require(!ValidateSceneInstance(legacy), "legacy zero scale accepted");
  legacy = {};
  legacy.rotation[3] = 0;
  Require(!ValidateSceneInstance(legacy), "legacy nonunit quaternion accepted");
  for (const auto factor : {1e-30F, std::numeric_limits<float>::max()}) {
    SceneInstance extreme;
    extreme.model_transform =
        std::array<float, 16>{factor, 0, 0, 0, 0, factor, 0, 0, 0, 0, factor, 0, 0, 0, 0, 1};
    Require(PackSceneInstance(extreme, packed), "representable extreme affine scale rejected");
    const auto direction = packed.normal[2][2];
    Require(Near(direction, 1) && std::isfinite(direction),
            "normal packing is not bounded for stable shader normalization");
  }
  const auto identity = PackSceneInstances({});
  Require(identity && identity->size() == 1 && identity->front().model[0][0] == 1 &&
              identity->front().normal[2][2] == 1,
          "empty instance span did not select one packed identity");
  std::vector<SceneInstance> bounded(4096);
  Require(PackSceneInstances(bounded).has_value(), "maximum instance count rejected");
  bounded.emplace_back();
  Require(!PackSceneInstances(bounded), "unbounded upload accepted");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Scene instance affine contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
