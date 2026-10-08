#pragma once

#include "Nexora/Animation/Pose.h"
#include "Nexora/Animation/SyncGroup.h"

namespace nexora::animation {

struct PoseGraphClip final {
  SyncGroupMember clock;
  AnimationPoseStorage poses;
  ResourceId skeleton{};
};

struct PoseGraphSample final {
  std::vector<JointPose> joints;
  std::vector<SyncGroupSample> clocks;
};

// Owned compressed clips sharing one caller-defined skeleton and joint order.
// Uniform frames include the authored loop endpoint at duration.
class NEXORA_ANIMATION_API SynchronizedPoseGraph final {
public:
  static constexpr std::size_t MaximumSamples = AnimationPoseStorage::MaximumSamples;
  bool SetClips(std::span<const PoseGraphClip> clips);
  bool SetWeight(ResourceId clip, float weight);
  [[nodiscard]] std::optional<PoseGraphSample> Update(double seconds);

private:
  std::vector<PoseGraphClip> clips_;
  SyncGroup group_;
};

} // namespace nexora::animation
