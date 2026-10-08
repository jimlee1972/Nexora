#include "Nexora/Animation/Pose.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace nexora::animation;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Near(float a, float b, float tolerance = 0.001F) {
  Require(std::isfinite(a) && std::abs(double(a) - b) <= tolerance, "pose error exceeded budget");
}
void Storage() {
  AnimationPoseStorage storage;
  Require(!storage.Sample(0), "empty storage sampled");
  std::vector<JointPose> input(256 * 32);
  for (std::size_t f = 0; f < 256; ++f)
    for (std::size_t j = 0; j < 32; ++j) {
      auto &p = input[f * 32 + j];
      p.translation = {float(f) / 20, float(j), -float(f) / 30};
      p.rotation = nexora::math::Quaternion::FromAxisAngleRadians({0, 1, 0}, float(f) / 50);
      p.scale = {1 + float(f) / 300, 2, 3};
    }
  Require(storage.Build(32, input), "valid pose build failed");
  Require(storage.Bytes().size() == 16 + 32 * 48 + input.size() * 20,
          "compressed format size changed");
  Require(storage.Bytes().size() < input.size() * sizeof(JointPose), "poses did not compress");
  const std::vector<std::uint8_t> golden(storage.Bytes().begin(), storage.Bytes().end());
  AnimationPoseStorage loaded;
  Require(loaded.Load(golden), "serialized pose load failed");
  for (std::size_t f = 0; f < 256; ++f) {
    const auto sampled = loaded.Sample(double(f));
    Require(sampled && sampled->size() == 32, "pose frame missing");
    for (std::size_t j = 0; j < 32; ++j) {
      const auto &a = (*sampled)[j], &b = input[f * 32 + j];
      Near(a.translation.x, b.translation.x);
      Near(a.translation.y, b.translation.y);
      Near(a.translation.z, b.translation.z);
      Near(a.scale.x, b.scale.x);
      const double dot = double(a.rotation.x) * b.rotation.x + double(a.rotation.y) * b.rotation.y +
                         double(a.rotation.z) * b.rotation.z + double(a.rotation.w) * b.rotation.w;
      Require(std::abs(dot) > 0.99999, "quaternion rotation quantization failed");
    }
  }
  Near(loaded.Sample(123.5)->front().translation.x, 123.5F / 20);
  Require(!loaded.Sample(-1) && !loaded.Sample(256) &&
              !loaded.Sample(std::numeric_limits<double>::quiet_NaN()),
          "invalid frame sampled");
  for (auto &p : input) {
    const auto q = p.rotation;
    p.rotation = {-q.x * 2, -q.y * 2, -q.z * 2, -q.w * 2};
  }
  Require(storage.Build(32, input) && std::ranges::equal(storage.Bytes(), golden),
          "equivalent quaternion changed canonical bytes");
  input.front().scale.x = 0;
  Require(!storage.Build(32, input) && std::ranges::equal(storage.Bytes(), golden),
          "invalid build replaced good data");
  for (std::size_t length = 0; length < golden.size(); length += 79)
    Require(!storage.Load(std::span(golden).first(length)), "truncated pose accepted");
  auto bad = golden;
  bad.push_back(0);
  Require(!storage.Load(bad), "trailing pose bytes accepted");
  bad = golden;
  bad[4] = 2;
  Require(!storage.Load(bad), "unknown format version accepted");
  bad = golden;
  bad[8] = 0xff;
  bad[9] = 0xff;
  Require(!storage.Load(bad), "oversized joint count accepted");
  bad = golden;
  // NaN bound, encoded IEEE float32 little-endian.
  bad[16] = 0;
  bad[17] = 0;
  bad[18] = 0xc0;
  bad[19] = 0x7f;
  Require(!storage.Load(bad), "NaN bound accepted");
  bad = golden;
  const auto rotation = 16 + 32 * 48 + 6;
  std::fill(bad.begin() + rotation, bad.begin() + rotation + 8, std::uint8_t{0});
  Require(!storage.Load(bad), "zero quaternion accepted");
  Require(std::ranges::equal(storage.Bytes(), golden), "failed loads damaged pose storage");
  AnimationPoseStorage moved(std::move(storage));
  Require(moved.Sample(0).has_value() && !storage.Sample(0) && storage.JointCount() == 0,
          "moved-from storage is unsafe");
  loaded = std::move(moved);
  Require(!moved.Sample(0) && loaded.Sample(255).has_value(), "move assignment lost invariants");

  std::array<JointPose, 2> extremes{};
  extremes[0].translation.x = -std::numeric_limits<float>::max();
  extremes[1].translation.x = std::numeric_limits<float>::max();
  Require(storage.Build(1, extremes), "finite extremes rejected");
  Near(storage.Sample(0.5)->front().translation.x, 0, 1);
  Near(storage.Sample(0)->front().translation.x, extremes[0].translation.x, 0);
  Near(storage.Sample(1)->front().translation.x, extremes[1].translation.x, 0);
  std::array<JointPose, 1> single{};
  Require(storage.Build(1, single), "single static pose rejected");
  const std::array<std::uint8_t, 16> header{'N', 'P', 'S', '1', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
  Require(std::ranges::equal(storage.Bytes().first(16), header), "little-endian header changed");
  single[0].translation.x = -0.0F;
  AnimationPoseStorage zero;
  Require(zero.Build(1, single) && std::ranges::equal(zero.Bytes(), storage.Bytes()),
          "signed zero changed pose bytes");
}
void Retarget() {
  std::array<JointPose, 2> source{};
  std::array<JointPose, 3> target{};
  source[1].translation = {1, 0, 0};
  target[1].translation = {0, 2, 0};
  target[1].rotation =
      nexora::math::Quaternion::FromAxisAngleRadians({0, 0, 1}, nexora::math::kPi / 2);
  target[2].translation = {0, 3, 0};
  const std::array<std::int32_t, 2> sp{-1, 0};
  const std::array<std::int32_t, 3> tp{-1, 0, 1};
  std::array<RetargetJoint, 2> map{{{1, 1, 2}, {0, 0, 1}}};
  PoseRetargeter retarget;
  Require(!retarget.Apply(source), "unbuilt retargeter applied");
  Require(retarget.Build(source, sp, target, tp, map), "retarget mapping failed");
  auto pose = retarget.Apply(source);
  Require(pose && pose->size() == 3, "retarget pose missing");
  Near((*pose)[1].translation.y, 2);
  Near((*pose)[2].translation.y, 3);
  source[1].translation.x = 2;
  source[1].scale.x = 2;
  pose = retarget.Apply(source);
  Near((*pose)[1].translation.x, 0);
  Near((*pose)[1].translation.y, 4);
  Near((*pose)[1].scale.x, 2);
  Near((*pose)[1].rotation.z, target[1].rotation.z);
  auto bad = map;
  bad[0].target = 0;
  Require(!retarget.Build(source, sp, target, tp, bad), "duplicate target accepted");
  Require(!retarget.Build(source, sp, target, tp, std::span(map).first(1)),
          "missing mapped ancestor accepted");
  bad = map;
  bad[0].source = 2;
  Require(!retarget.Build(source, sp, target, tp, bad), "out-of-range source accepted");
  bad = map;
  bad[0].translation_scale = std::numeric_limits<float>::infinity();
  Require(!retarget.Build(source, sp, target, tp, bad), "nonfinite scale accepted");
  const std::array<std::int32_t, 2> cycle{1, 0};
  Require(!retarget.Build(source, cycle, target, tp, map), "cyclic skeleton accepted");
  pose = retarget.Apply(source);
  Require(pose.has_value(), "invalid rebuild destroyed retargeter");
  Near((*pose)[1].translation.y, 4);
  Require(!retarget.Apply(std::span(source).first(1)), "incomplete pose applied");
  source[1].rotation = {0, 0, 0, 0};
  Require(!retarget.Apply(source), "invalid quaternion applied");
  source[1].rotation = {};
  source[1].scale.x = std::numeric_limits<float>::max();
  map[0].translation_scale = std::numeric_limits<float>::max();
  Require(retarget.Build(source, sp, target, tp, map), "valid extreme map rejected");
  source[1].translation.x = -std::numeric_limits<float>::max();
  Require(!retarget.Apply(source), "overflowing retarget returned a pose");
}
} // namespace
int main() {
  try {
    Storage();
    Retarget();
    std::cout << "V2 pose storage and retarget contracts passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
