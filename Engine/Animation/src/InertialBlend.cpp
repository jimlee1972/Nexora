#include "Nexora/Animation/InertialBlend.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace nexora::animation {
namespace {
using Rotation = std::array<double, 4>;
Rotation Normalize(Rotation q) {
  const double norm = std::hypot(std::hypot(q[0], q[1]), std::hypot(q[2], q[3]));
  for (auto &value : q)
    value /= norm;
  for (std::size_t i : {3U, 2U, 1U, 0U}) {
    if (q[i] != 0) {
      if (q[i] < 0)
        for (auto &value : q)
          value = -value;
      break;
    }
  }
  return q;
}
Rotation Multiply(Rotation a, Rotation b) {
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
          a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
          a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}
Rotation RotationOf(const JointPose &pose) {
  return Normalize({pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w});
}
bool Valid(const JointPose &p) {
  const std::array<float, 10> values{
      p.translation.x, p.translation.y, p.translation.z, p.rotation.x, p.rotation.y,
      p.rotation.z,    p.rotation.w,    p.scale.x,       p.scale.y,    p.scale.z};
  return std::ranges::all_of(values, [](float v) { return std::isfinite(v); }) && p.scale.x > 0 &&
         p.scale.y > 0 && p.scale.z > 0 &&
         std::hypot(std::hypot(double(p.rotation.x), double(p.rotation.y)),
                    std::hypot(double(p.rotation.z), double(p.rotation.w))) > 1e-12;
}
std::array<double, 9> Channels(const JointPose &pose) {
  return {pose.translation.x,
          pose.translation.y,
          pose.translation.z,
          0,
          0,
          0,
          std::log(double(pose.scale.x)),
          std::log(double(pose.scale.y)),
          std::log(double(pose.scale.z))};
}
std::array<double, 9> Velocity(const JointInertialVelocity &velocity) {
  return {velocity.translation.x,     velocity.translation.y,     velocity.translation.z,
          velocity.rotation_vector.x, velocity.rotation_vector.y, velocity.rotation_vector.z,
          velocity.log_scale.x,       velocity.log_scale.y,       velocity.log_scale.z};
}
} // namespace

bool InertialPoseBlend::Begin(std::span<const JointPose> outgoing,
                              std::span<const JointPose> incoming, double duration,
                              std::span<const JointInertialVelocity> velocities) {
  if (outgoing.empty() || outgoing.size() > AnimationPoseStorage::MaximumJoints ||
      incoming.size() != outgoing.size() ||
      (!velocities.empty() && velocities.size() != outgoing.size()) || !std::isfinite(duration) ||
      duration <= 0 || !std::ranges::all_of(outgoing, Valid) ||
      !std::ranges::all_of(incoming, Valid))
    return false;
  std::vector<Offset> offsets(outgoing.size());
  for (std::size_t i = 0; i < outgoing.size(); ++i) {
    auto a = Channels(outgoing[i]);
    const auto b = Channels(incoming[i]);
    const auto q = RotationOf(outgoing[i]);
    auto inverse = RotationOf(incoming[i]);
    for (std::size_t j = 0; j < 3; ++j)
      inverse[j] = -inverse[j];
    const auto relative = Normalize(Multiply(q, inverse));
    const double sine = std::hypot(relative[0], relative[1], relative[2]);
    const double angle = 2 * std::atan2(sine, relative[3]);
    for (std::size_t j = 0; j < 3; ++j)
      a[j + 3] = sine == 0 ? 0 : (relative[j] / sine) * angle;
    offsets[i].source = a;
    offsets[i].target = b;
    const auto velocity = velocities.empty() ? std::array<double, 9>{} : Velocity(velocities[i]);
    for (std::size_t j = 0; j < a.size(); ++j) {
      const double scaled = velocity[j] * duration;
      if (!std::isfinite(scaled) || (velocity[j] != 0 && scaled == 0))
        return false;
      offsets[i].position[j] = a[j] - b[j];
      offsets[i].scaled_velocity[j] = scaled;
    }
  }
  offsets_ = std::move(offsets);
  duration_ = duration;
  elapsed_ = 0;
  return true;
}

std::optional<std::vector<JointPose>> InertialPoseBlend::Update(std::span<const JointPose> incoming,
                                                                double seconds) {
  if (incoming.empty() || incoming.size() > AnimationPoseStorage::MaximumJoints ||
      (!offsets_.empty() && incoming.size() != offsets_.size()) || !std::isfinite(seconds) ||
      seconds < 0 || !std::ranges::all_of(incoming, Valid))
    return std::nullopt;
  const double next =
      offsets_.empty() || seconds >= duration_ - elapsed_ ? duration_ : elapsed_ + seconds;
  const bool correcting = !offsets_.empty() && next < duration_;
  const double u = correcting ? next / duration_ : 1;
  const double remaining = 1 - u;
  const double cube = remaining * remaining * remaining;
  const double position_weight = cube * (1 + 3 * u + 6 * u * u);
  const double velocity_weight = u * cube * (1 + 3 * u);
  // A-1 factored at the start avoids losing tiny departures from a large target.
  const double departure_weight = -u * u * u * (10 - 15 * u + 6 * u * u);
  std::vector<JointPose> result;
  result.reserve(incoming.size());
  for (std::size_t i = 0; i < incoming.size(); ++i) {
    JointPose pose = incoming[i];
    auto rotation = RotationOf(pose);
    if (correcting) {
      auto values = Channels(pose);
      for (std::size_t j = 0; j < values.size(); ++j) {
        if (u <= 0.5) {
          values[j] = (values[j] - offsets_[i].target[j]) + offsets_[i].source[j] +
                      offsets_[i].position[j] * departure_weight;
        } else {
          values[j] += offsets_[i].position[j] * position_weight;
        }
        values[j] += offsets_[i].scaled_velocity[j] * velocity_weight;
        if (!std::isfinite(values[j]))
          return std::nullopt;
      }
      const double angle = std::hypot(values[3], values[4], values[5]);
      if (!std::isfinite(angle))
        return std::nullopt;
      Rotation correction{0, 0, 0, 1};
      if (angle != 0) {
        const double sine = std::sin(angle * 0.5);
        for (std::size_t j = 0; j < 3; ++j)
          correction[j] = (values[j + 3] / angle) * sine;
        correction[3] = std::cos(angle * 0.5);
      }
      rotation = Normalize(Multiply(correction, rotation));
      std::array<double, 3> scale{};
      for (std::size_t j = 0; j < 3; ++j) {
        const double maximum = std::numeric_limits<float>::max();
        if (values[j + 6] > std::log(maximum))
          return std::nullopt;
        scale[j] = std::exp(values[j + 6]);
        if (std::abs(values[j]) > maximum || !std::isfinite(scale[j]))
          return std::nullopt;
        // exp(log(max_float)) may round just above the known representable endpoint.
        scale[j] = std::min(scale[j], maximum);
      }
      pose.translation = {float(values[0]), float(values[1]), float(values[2])};
      pose.scale = {float(scale[0]), float(scale[1]), float(scale[2])};
    }
    pose.rotation = {float(rotation[0]), float(rotation[1]), float(rotation[2]),
                     float(rotation[3])};
    if (!Valid(pose))
      return std::nullopt;
    result.push_back(pose);
  }
  elapsed_ = next;
  return result;
}

void InertialPoseBlend::Reset() noexcept {
  offsets_.clear();
  duration_ = elapsed_ = 0;
}

bool InertialPoseBlend::Active() const noexcept {
  return !offsets_.empty() && elapsed_ < duration_;
}

} // namespace nexora::animation
