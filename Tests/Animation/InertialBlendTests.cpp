#include "Nexora/Animation/InertialBlend.h"
#include "Nexora/Animation/PoseGraph.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora::animation;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Near(double actual, double expected, const char *message, double tolerance = 0.00002) {
  Require(std::isfinite(actual) && std::abs(actual - expected) < tolerance, message);
}
nexora::math::Quaternion ZRotation(double angle) {
  return {0, 0, float(std::sin(angle / 2)), float(std::cos(angle / 2))};
}
double ZAngle(nexora::math::Quaternion rotation) {
  return 2 * std::atan2(double(rotation.z), double(rotation.w));
}
void DecayAndVelocity() {
  std::vector<JointPose> outgoing(2), target(2);
  outgoing[0].translation.x = 8;
  outgoing[0].scale.x = 16;
  outgoing[0].rotation = ZRotation(1.2);
  outgoing[1].translation.y = -4;
  std::vector<JointInertialVelocity> velocities(2);
  velocities[0].translation.x = 2;
  velocities[0].rotation_vector.z = 0.4F;
  velocities[0].log_scale.x = 0.3F;
  InertialPoseBlend blend;
  Require(blend.Begin(outgoing, target, 2, velocities) && blend.Active(), "begin failed");
  outgoing.clear(); // Begin owns offsets, not caller spans.
  auto pose = blend.Update(target, 0);
  Require(pose.has_value(), "initial pose rejected");
  Near((*pose)[0].translation.x, 8, "initial pose popped");
  Near((*pose)[0].scale.x, 16, "initial scale popped");
  Near(ZAngle((*pose)[0].rotation), 1.2, "initial rotation popped");
  const double step = 0.001;
  auto early = blend.Update(target, step);
  Near(((*early)[0].translation.x - 8) / step, 2, "initial velocity lost", 0.001);
  Near((ZAngle((*early)[0].rotation) - ZAngle((*pose)[0].rotation)) / step, 0.4,
       "rotation-vector velocity lost", 0.0003);
  Near((std::log((*early)[0].scale.x) - std::log((*pose)[0].scale.x)) / step, 0.3,
       "log-scale velocity lost", 0.0003);
  pose = blend.Update(target, 1 - step);
  // At u=.5: position basis=.5, velocity basis=5/32, duration=2.
  Near((*pose)[0].translation.x, 4.625, "quintic midpoint failed");
  Near(ZAngle((*pose)[0].rotation), 0.725, "rotation midpoint failed");
  Near((*pose)[0].scale.x, 4 * std::exp(0.09375), "log-scale midpoint failed");
  Near((*pose)[1].translation.y, -2, "joint order changed");
  target[0].translation.x = 10;
  pose = blend.Update(target, 0);
  Near((*pose)[0].translation.x, 14.625, "moving target was frozen");
  pose = blend.Update(target, 1);
  Require(pose && !blend.Active(), "blend did not complete");
  Near((*pose)[0].translation.x, 10, "terminal offset remained");
  Near((*pose)[0].scale.x, 1, "terminal scale remained");
  Near(ZAngle((*pose)[0].rotation), 0, "terminal rotation remained");
  Require(blend.Update(target, std::numeric_limits<double>::max()).has_value(),
          "completed tick overflow");
}
void RotationAndComposition() {
  std::vector<JointPose> outgoing(1), target(1);
  outgoing[0].rotation = ZRotation(170 * 3.14159265358979323846 / 180);
  target[0].rotation = ZRotation(-170 * 3.14159265358979323846 / 180);
  InertialPoseBlend blend, equivalent;
  Require(blend.Begin(outgoing, target, 1), "short arc begin failed");
  const auto q = outgoing[0].rotation;
  outgoing[0].rotation = {-q.x, -q.y, -q.z, -q.w};
  Require(equivalent.Begin(outgoing, target, 1), "antipodal begin failed");
  const auto halfway = blend.Update(target, 0.5), same = equivalent.Update(target, 0.5);
  Require(halfway && same, "rotation blend failed");
  Near(std::abs((*halfway)[0].rotation.z), 1, "rotation took long arc");
  Near((*halfway)[0].rotation.w, 0, "short arc midpoint failed");
  Near((*halfway)[0].rotation.z, (*same)[0].rotation.z, "q/-q changed result");

  PoseGraphClip clip;
  clip.clock = {1, 1, 0, 1, {}};
  clip.skeleton = 9;
  Require(clip.poses.Build(1, target), "compressed fixture failed");
  SynchronizedPoseGraph graph;
  const std::vector<PoseGraphClip> clips{clip};
  Require(graph.SetClips(clips), "graph fixture failed");
  const auto sample = graph.Update(0);
  Require(sample && blend.Begin(outgoing, sample->joints, 1), "graph-to-inertia begin failed");
  const auto output = blend.Update(sample->joints, 1);
  Require(output && !blend.Active(), "graph-to-inertia update failed");
  Near((*output)[0].rotation.z, sample->joints[0].rotation.z, "graph target not reached");

  // Noncommuting axes exercise multiplication order, with a nonunit outgoing quaternion.
  const float half = float(std::sqrt(0.5));
  outgoing[0].rotation = {2 * half, 0, 0, 2 * half};
  target[0].rotation = {0, half, 0, half};
  Require(blend.Begin(outgoing, target, 1), "noncommuting begin failed");
  const auto initial = blend.Update(target, 0);
  Near((*initial)[0].rotation.x, half, "initial normalization failed");
  Near((*initial)[0].rotation.y, 0, "correction multiplied in wrong order");
  const auto midpoint = blend.Update(target, 0.5);
  const auto expected = nexora::math::Slerp({half, 0, 0, half}, target[0].rotation, 0.5F);
  Near((*midpoint)[0].rotation.x, expected.x, "noncommuting X interpolation failed");
  Near((*midpoint)[0].rotation.y, expected.y, "noncommuting Y interpolation failed");
  Near((*midpoint)[0].rotation.z, expected.z, "noncommuting Z interpolation failed");
  Near((*midpoint)[0].rotation.w, expected.w, "noncommuting W interpolation failed");
}
void PolynomialReference() {
  std::vector<JointPose> outgoing(1), target(1);
  std::vector<JointInertialVelocity> velocity(1);
  outgoing[0].translation = {7, -2, 3};
  target[0].translation = {1, 4, -5};
  outgoing[0].scale = {4, 0.5F, 2};
  velocity[0].translation = {-3, 2, 1};
  velocity[0].log_scale.x = -0.7F;
  InertialPoseBlend blend;
  Require(blend.Begin(outgoing, target, 1, velocity), "reference begin failed");
  for (int tick = 0; tick <= 100; ++tick) {
    const double u = double(tick) / 100;
    const double u2 = u * u, u3 = u2 * u, u4 = u3 * u, u5 = u4 * u;
    // Expanded boundary-constrained polynomial, independent of factored implementation.
    const double position = 1 - 10 * u3 + 15 * u4 - 6 * u5;
    const double speed = u - 6 * u3 + 8 * u4 - 3 * u5;
    const auto output = blend.Update(target, tick == 0 ? 0 : 0.01);
    Require(output.has_value(), "reference tick failed");
    Near((*output)[0].translation.x, 1 + 6 * position - 3 * speed, "dense X reference failed");
    Near((*output)[0].translation.y, 4 - 6 * position + 2 * speed, "dense Y reference failed");
    Near((*output)[0].translation.z, -5 + 8 * position + speed, "dense Z reference failed");
    Near((*output)[0].scale.x, std::exp(std::log(4.0) * position - 0.7 * speed),
         "dense scale reference failed");
  }
  Require(!blend.Active(), "partitioned deadline not reached");
}
void RejectionAndClocks() {
  std::vector<JointPose> outgoing(1), target(1);
  outgoing[0].translation.x = 8;
  InertialPoseBlend blend, reference;
  Require(blend.Begin(outgoing, target, 1) && reference.Begin(outgoing, target, 1),
          "fixture failed");
  Require(!blend.Begin({}, target, 1) && !blend.Begin(outgoing, target, 0) &&
              !blend.Begin(outgoing, target, std::numeric_limits<double>::infinity()),
          "bad begin accepted");
  std::vector<JointInertialVelocity> velocities(2);
  Require(!blend.Begin(outgoing, target, 1, velocities), "wrong velocity count accepted");
  velocities.resize(1);
  velocities[0].translation.x = std::numeric_limits<float>::infinity();
  Require(!blend.Begin(outgoing, target, 1, velocities), "bad velocity accepted");
  velocities[0].translation.x = std::numeric_limits<float>::max();
  Require(!blend.Begin(outgoing, target, std::numeric_limits<double>::max(), velocities),
          "scaled velocity overflow accepted");
  velocities[0].translation.x = std::numeric_limits<float>::denorm_min();
  Require(!blend.Begin(outgoing, target, std::numeric_limits<double>::denorm_min(), velocities),
          "scaled velocity silently vanished");
  auto bad = target;
  bad[0].scale.y = 0;
  Require(!blend.Begin(outgoing, bad, 1) && !blend.Update(bad, 0.3), "collapsed scale accepted");
  bad = target;
  bad[0].rotation = {0, 0, 0, 0};
  Require(!blend.Update(bad, 0.3), "zero quaternion accepted");
  Require(!blend.Update({}, 0.3) && !blend.Update(target, -1) &&
              !blend.Update(target, std::numeric_limits<double>::quiet_NaN()),
          "invalid tick accepted");
  const auto actual = blend.Update(target, 0.3), expected = reference.Update(target, 0.3);
  Near((*actual)[0].translation.x, (*expected)[0].translation.x, "rejection advanced clock");
  InertialPoseBlend partition;
  Require(partition.Begin(outgoing, target, 1), "partition begin failed");
  Require(partition.Update(target, 0.1).has_value(), "partition first tick failed");
  const auto split = partition.Update(target, 0.2);
  Near((*split)[0].translation.x, (*expected)[0].translation.x, "tick partition changed result");
  blend.Reset();
  Require(!blend.Active() && blend.Update(target, 0).has_value(), "reset passthrough failed");
  Require(blend.Begin(outgoing, target, std::numeric_limits<double>::denorm_min()),
          "tiny duration rejected");
  Require(blend.Update(target, 0).has_value() && blend.Active(), "tiny initial state failed");
  Require(blend.Update(target, std::numeric_limits<double>::max()).has_value() && !blend.Active(),
          "large tick failed");
}
void OverflowAndCapacity() {
  std::vector<JointPose> outgoing(1), target(1);
  std::vector<JointInertialVelocity> velocities(1);
  const float maximum = std::numeric_limits<float>::max();
  outgoing[0].translation.x = 1;
  target[0].translation.x = maximum;
  InertialPoseBlend anchored;
  Require(anchored.Begin(outgoing, target, 1), "anchored begin failed");
  const auto start = anchored.Update(target, 0);
  Require(start && (*start)[0].translation.x == 1, "large-target cancellation lost outgoing pose");
  const double u = 1e-6;
  const auto small_step = anchored.Update(target, u);
  const double small_reference = 1 + double(maximum) * u * u * u * (10 - 15 * u + 6 * u * u);
  Require(small_step && std::abs((*small_step)[0].translation.x / small_reference - 1) < 0.000001,
          "small departure from large target vanished");
  const float minimum = std::numeric_limits<float>::denorm_min();
  outgoing[0].scale = {maximum, minimum, 1};
  target[0].scale = {minimum, maximum, 1};
  Require(anchored.Begin(outgoing, target, 1), "extreme scale begin failed");
  const auto scaled_start = anchored.Update(target, 0);
  Require(scaled_start && (*scaled_start)[0].scale.x == maximum &&
              (*scaled_start)[0].scale.y == minimum,
          "extreme initial scale lost");
  outgoing[0].scale = target[0].scale = {1, 1, 1};
  outgoing[0].translation.x = maximum;
  target[0].translation.x = -maximum;
  InertialPoseBlend blend;
  Require(blend.Begin(outgoing, target, 1), "extreme begin failed");
  auto result = blend.Update(target, 0);
  Require(result && (*result)[0].translation.x == maximum, "initial cancellation overflowed");
  target[0].translation.x = maximum;
  Require(!blend.Update(target, 0.5) && blend.Active(), "overflow result accepted");
  target[0].translation.x = -maximum;
  result = blend.Update(target, 0.5);
  Near((*result)[0].translation.x, 0, "failed output advanced clock");
  target[0] = JointPose{};
  outgoing[0] = JointPose{};
  velocities[0].log_scale.x = maximum;
  Require(blend.Begin(outgoing, target, 1, velocities) && !blend.Update(target, 0.5),
          "scale overflow accepted");
  velocities[0].log_scale.x = -maximum;
  Require(blend.Begin(outgoing, target, 1, velocities) && !blend.Update(target, 0.5),
          "scale collapse accepted");
  outgoing.assign(AnimationPoseStorage::MaximumJoints, JointPose{});
  target = outgoing;
  Require(blend.Begin(outgoing, target, 1) && blend.Update(target, 0).has_value(),
          "joint capacity rejected");
  outgoing.push_back(JointPose{});
  target = outgoing;
  Require(!blend.Begin(outgoing, target, 1), "over-capacity accepted");
}
} // namespace
int main() {
  try {
    DecayAndVelocity();
    RotationAndComposition();
    PolynomialReference();
    RejectionAndClocks();
    OverflowAndCapacity();
    std::cout << "Inertial local TRS blend tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
