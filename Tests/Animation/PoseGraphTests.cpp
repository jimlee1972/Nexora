#include "Nexora/Animation/PoseGraph.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora::animation;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Near(double actual, double expected, const char *message) {
  Require(std::isfinite(actual) && std::abs(actual - expected) < 0.0003, message);
}
PoseGraphClip Clip(ResourceId id, double duration, float weight, float offset) {
  PoseGraphClip clip;
  clip.clock = {id, duration, 0, weight, {}};
  clip.skeleton = 77;
  std::vector<JointPose> frames(4);
  frames[0].translation.x = offset;
  frames[1].translation.y = offset;
  frames[2].translation.x = offset + 4;
  frames[3].translation.y = offset + 8;
  frames[0].scale = frames[1].scale = {1, 2, 3};
  frames[2].scale = frames[3].scale = {3, 4, 5};
  Require(clip.poses.Build(2, frames), "fixture build failed");
  return clip;
}
void SamplingAndOwnership() {
  std::vector<PoseGraphClip> clips{Clip(20, 2, 1, 10), Clip(10, 1, 3, 0)};
  clips[0].clock.markers = {{"left", 0.2}, {"right", 1.0}};
  clips[1].clock.markers = {{"left", 0}, {"right", 0.5}};
  SynchronizedPoseGraph graph, reordered;
  Require(graph.SetClips(clips), "graph rejected clips");
  std::ranges::reverse(clips);
  Require(reordered.SetClips(clips), "reordered graph rejected");
  clips.clear();
  auto result = graph.Update(0.25);
  const auto other = reordered.Update(0.25);
  Require(result && other && result->clocks == other->clocks && result->joints.size() == 2,
          "ownership or canonical ordering failed");
  Near(result->clocks[1].time, 0.6, "marker sample time failed");
  // Leader at frame .25, follower at frame .30; weights 3:1.
  Near(result->joints[0].translation.x, 3.55, "weighted translation failed");
  Near(result->joints[1].translation.y, 4.6, "joint order failed");
  Near(result->joints[0].scale.y, 2.525, "weighted scale failed");
  Near(other->joints[0].translation.x, result->joints[0].translation.x,
       "input order changed blend");
  Require(graph.SetWeight(20, 4), "leader weight switch failed");
  result = graph.Update(0);
  Require(result && result->clocks[1].leader, "leader not switched");
  Near(result->clocks[0].time, 0.25, "switch reset clock");
  Near(result->joints[0].translation.x, (3.0 + 4.0 * 11.2) / 7, "switch blend failed");
  result = graph.Update(1.5);
  Require(result.has_value(), "wrap tick failed");
  Near(result->clocks[1].time, 0.1, "leader wrap failed");
  Near(result->clocks[0].time, 23.0 / 24.0, "follower wrap segment failed");
  Require(graph.SetWeight(10, 0) && graph.SetWeight(20, 0) && !graph.Update(100), "pause failed");
  Require(graph.SetWeight(20, 1), "resume failed");
  result = graph.Update(0);
  Near(result->clocks[1].time, 0.1, "pause advanced clocks");
  Near(result->joints[0].translation.x, 10.2, "zero-weight contribution leaked");

  PoseRetargeter retarget;
  std::vector<JointPose> bind(2), target(2);
  target[0].translation.x = 5;
  const std::vector<std::int32_t> parents{-1, 0};
  const std::vector<RetargetJoint> mapping{{0, 0, 2}, {1, 1, 2}};
  Require(retarget.Build(bind, parents, target, parents, mapping), "retarget build failed");
  const auto mapped = retarget.Apply(result->joints);
  Require(mapped.has_value(), "blended TRS cannot be retargeted");
  Near((*mapped)[0].translation.x, 25.4, "retarget integration failed");
}
void RotationsAndExtremes() {
  auto a = Clip(10, 1, 1, 0), b = Clip(20, 1, 1, 0);
  std::vector<JointPose> positive(2), negative(2);
  const double angle = 170.0 * 3.14159265358979323846 / 360.0;
  for (auto &p : positive)
    p.rotation = {0, 0, float(std::sin(angle)), float(std::cos(angle))};
  for (auto &p : negative)
    p.rotation = {0, 0, -float(std::sin(angle)), float(std::cos(angle))};
  Require(a.poses.Build(2, positive) && b.poses.Build(2, negative), "rotation fixture failed");
  SynchronizedPoseGraph graph;
  std::vector<PoseGraphClip> clips{b, a};
  Require(graph.SetClips(clips), "rotation graph failed");
  auto sample = graph.Update(0.5);
  Near(std::abs(sample->joints[0].rotation.z), 1, "rotation took long arc");
  Near(sample->joints[0].rotation.w, 0, "rotation midpoint failed");
  for (auto &p : negative)
    p.rotation = {-p.rotation.x, -p.rotation.y, -p.rotation.z, -p.rotation.w};
  Require(clips[0].poses.Build(2, negative) && graph.SetClips(clips), "antipodal fixture failed");
  sample = graph.Update(0.5);
  Near(std::abs(sample->joints[0].rotation.z), 1, "antipodal input changed result");
  const float maximum = std::numeric_limits<float>::max();
  const float tiny = std::numeric_limits<float>::denorm_min();
  for (auto &p : positive) {
    p.translation = {maximum, -maximum, tiny};
    p.scale = {maximum, tiny, 1};
  }
  for (auto &clip : clips) {
    clip.clock.duration = std::numeric_limits<double>::max();
    clip.clock.weight = maximum;
    Require(clip.poses.Build(2, positive), "extreme pose rejected");
  }
  Require(graph.SetClips(clips), "extreme graph rejected");
  sample = graph.Update(std::numeric_limits<double>::max());
  Require(sample && sample->joints[0].translation.x == maximum && sample->joints[0].scale.y == tiny,
          "extreme blend overflow or underflow");
  for (const auto &clip : clips)
    Require(graph.SetWeight(clip.clock.clip, tiny), "tiny weight rejected");
  sample = graph.Update(0);
  Require(sample && sample->joints[0].scale.x == maximum, "tiny weights collapsed pose");
}
void RejectionAndBudget() {
  auto clip = Clip(10, 1, 1, 0);
  std::vector<PoseGraphClip> valid{clip};
  SynchronizedPoseGraph graph, reference;
  Require(graph.SetClips(valid) && reference.SetClips(valid), "initial graph failed");
  Require(graph.Update(0.2).has_value() && reference.Update(0.2).has_value(),
          "initial tick failed");
  const auto reject = [&](std::vector<PoseGraphClip> bad) {
    Require(!graph.SetClips(bad), "invalid graph accepted");
  };
  reject({});
  clip.skeleton = 0;
  reject({clip});
  clip = valid[0];
  clip.poses = AnimationPoseStorage{};
  reject({clip});
  clip = valid[0];
  clip.clock.clip = 20;
  clip.skeleton = 88;
  reject({valid[0], clip});
  clip.skeleton = 77;
  const std::vector<JointPose> one_joint(1);
  Require(clip.poses.Build(1, one_joint), "mismatch fixture failed");
  reject({valid[0], clip});
  reject({valid[0], valid[0]});
  Require(!graph.SetWeight(999, 1) && !graph.SetWeight(10, -1) &&
              !graph.SetWeight(10, std::numeric_limits<float>::infinity()),
          "bad weight accepted");
  Require(!graph.Update(-1) && !graph.Update(std::numeric_limits<double>::quiet_NaN()),
          "invalid tick accepted");
  const auto actual = graph.Update(0.1), expected = reference.Update(0.1);
  Require(actual && expected && actual->clocks == expected->clocks, "rejection changed clocks");
  Near(actual->joints[0].translation.x, expected->joints[0].translation.x,
       "rejection changed poses");
  std::vector<JointPose> frames(2 * AnimationPoseStorage::MaximumFrames);
  clip = valid[0];
  Require(clip.poses.Build(2, frames), "budget fixture failed");
  std::vector<PoseGraphClip> capacity(128, clip);
  for (std::size_t i = 0; i < capacity.size(); ++i)
    capacity[i].clock.clip = ResourceId(i + 1);
  Require(graph.SetClips(capacity) && graph.Update(0).has_value(), "exact sample budget rejected");
  clip.clock.clip = 129;
  capacity.push_back(clip);
  reject(capacity);
  Require(graph.Update(0).has_value(), "budget rejection destroyed previous graph");
}
} // namespace

int main() {
  try {
    SamplingAndOwnership();
    RotationsAndExtremes();
    RejectionAndBudget();
    std::cout << "Synchronized compressed TRS graph tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
