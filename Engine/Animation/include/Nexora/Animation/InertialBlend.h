#pragma once

#include "Nexora/Animation/Pose.h"

#include <array>

namespace nexora::animation {

// Derivatives of outgoing-minus-incoming translation, relative rotation vector,
// and log-scale offset. Rotation-vector velocity is not world angular velocity.
struct JointInertialVelocity final {
  math::Vector3 translation{};
  math::Vector3 rotation_vector{};
  math::Vector3 log_scale{};
};

class NEXORA_ANIMATION_API InertialPoseBlend final {
public:
  bool Begin(std::span<const JointPose> outgoing, std::span<const JointPose> incoming,
             double duration, std::span<const JointInertialVelocity> velocities = {});
  [[nodiscard]] std::optional<std::vector<JointPose>> Update(std::span<const JointPose> incoming,
                                                             double seconds);
  void Reset() noexcept;
  [[nodiscard]] bool Active() const noexcept;

private:
  struct Offset final {
    std::array<double, 9> position{}, scaled_velocity{};
    std::array<double, 9> source{}, target{};
  };
  std::vector<Offset> offsets_;
  double duration_{}, elapsed_{};
};

} // namespace nexora::animation
