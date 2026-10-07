#pragma once

#include "Nexora/Animation/Api.h"
#include "Nexora/Math/Math.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nexora::animation {

struct JointPose final {
  math::Vector3 translation{};
  math::Quaternion rotation{};
  math::Vector3 scale{1, 1, 1};
};

// Frames are contiguous, in joint order. Rotations are normalized and canonicalized.
class NEXORA_ANIMATION_API AnimationPoseStorage final {
public:
  AnimationPoseStorage() = default;
  AnimationPoseStorage(const AnimationPoseStorage &) = default;
  AnimationPoseStorage &operator=(const AnimationPoseStorage &) = default;
  AnimationPoseStorage(AnimationPoseStorage &&other) noexcept;
  AnimationPoseStorage &operator=(AnimationPoseStorage &&other) noexcept;
  static constexpr std::uint32_t MaximumJoints = 512;
  static constexpr std::uint32_t MaximumFrames = 4096;
  static constexpr std::uint32_t MaximumSamples = 1048576;
  bool Build(std::uint32_t joints, std::span<const JointPose> frames);
  bool Load(std::span<const std::uint8_t> bytes);
  [[nodiscard]] std::optional<std::vector<JointPose>> Sample(double frame) const;
  [[nodiscard]] std::span<const std::uint8_t> Bytes() const noexcept { return bytes_; }
  [[nodiscard]] std::uint32_t JointCount() const noexcept { return joints_; }
  [[nodiscard]] std::uint32_t FrameCount() const noexcept { return frames_; }

private:
  std::vector<std::uint8_t> bytes_;
  std::uint32_t joints_{}, frames_{};
};

struct RetargetJoint final {
  std::uint32_t source{}, target{};
  float translation_scale{1};
};

// Explicit local bind-space mapping; no implicit bone-name matching or IK.
class NEXORA_ANIMATION_API PoseRetargeter final {
public:
  bool Build(std::span<const JointPose> source_bind, std::span<const std::int32_t> source_parents,
             std::span<const JointPose> target_bind, std::span<const std::int32_t> target_parents,
             std::span<const RetargetJoint> mapping);
  [[nodiscard]] std::optional<std::vector<JointPose>>
  Apply(std::span<const JointPose> source) const;

private:
  std::vector<JointPose> source_bind_, target_bind_;
  std::vector<RetargetJoint> mapping_;
};

} // namespace nexora::animation
