#include "Nexora/Animation/PoseGraph.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace nexora::animation {

bool SynchronizedPoseGraph::SetClips(std::span<const PoseGraphClip> clips) {
  if (clips.empty() || clips.size() > SyncGroup::MaxMembers || clips.front().skeleton == 0 ||
      clips.front().poses.JointCount() == 0)
    return false;
  std::size_t samples = 0;
  std::vector<SyncGroupMember> clocks;
  clocks.reserve(clips.size());
  for (const auto &clip : clips) {
    if (clip.skeleton != clips.front().skeleton ||
        clip.poses.JointCount() != clips.front().poses.JointCount() || clip.poses.FrameCount() == 0)
      return false;
    const auto count = std::size_t(clip.poses.JointCount()) * clip.poses.FrameCount();
    if (count > MaximumSamples - samples)
      return false;
    samples += count;
    clocks.push_back(clip.clock);
  }
  SyncGroup group;
  if (!group.SetMembers(clocks))
    return false;
  std::vector<PoseGraphClip> owned(clips.begin(), clips.end());
  std::ranges::sort(owned, {}, [](const auto &clip) { return clip.clock.clip; });
  clips_ = std::move(owned);
  group_ = std::move(group);
  return true;
}

bool SynchronizedPoseGraph::SetWeight(ResourceId clip, float weight) {
  const auto found =
      std::ranges::find(clips_, clip, [](const auto &entry) { return entry.clock.clip; });
  if (found == clips_.end() || !group_.SetWeight(clip, weight))
    return false;
  found->clock.weight = weight;
  return true;
}

std::optional<PoseGraphSample> SynchronizedPoseGraph::Update(double seconds) {
  // Publish clocks only after the complete pose is ready, including allocation success.
  auto next = group_;
  auto clocks = next.Update(seconds);
  if (!clocks)
    return std::nullopt;
  const auto leader = std::ranges::find(*clocks, true, &SyncGroupSample::leader);
  const auto leader_index = std::size_t(leader - clocks->begin());
  const auto sample = [&](std::size_t index) {
    const auto &clip = clips_[index];
    const double frame =
        ((*clocks)[index].time / clip.clock.duration) * double(clip.poses.FrameCount() - 1);
    return clip.poses.Sample(frame);
  };
  const auto reference = sample(leader_index);
  if (!reference)
    return std::nullopt;
  std::vector<std::array<double, 10>> sums(reference->size());
  double total = 0;
  for (std::size_t i = 0; i < clips_.size(); ++i) {
    const double weight = clips_[i].clock.weight;
    if (weight == 0)
      continue;
    const auto pose = i == leader_index ? reference : sample(i);
    if (!pose)
      return std::nullopt;
    total += weight;
    for (std::size_t j = 0; j < pose->size(); ++j) {
      const auto &p = (*pose)[j];
      const auto r = (*reference)[j].rotation;
      const auto q = p.rotation;
      const double dot =
          double(q.x) * r.x + double(q.y) * r.y + double(q.z) * r.z + double(q.w) * r.w;
      const double sign = dot < 0 ? -1 : 1;
      const std::array<double, 10> values{
          p.translation.x, p.translation.y, p.translation.z, p.scale.x,  p.scale.y,
          p.scale.z,       sign * q.x,      sign * q.y,      sign * q.z, sign * q.w};
      for (std::size_t c = 0; c < values.size(); ++c)
        sums[j][c] += weight * values[c];
    }
  }
  PoseGraphSample result;
  result.joints.reserve(sums.size());
  for (const auto &s : sums) {
    const double norm = std::hypot(std::hypot(s[6], s[7]), std::hypot(s[8], s[9]));
    if (!std::isfinite(norm) || norm == 0)
      return std::nullopt;
    result.joints.push_back(
        {{float(s[0] / total), float(s[1] / total), float(s[2] / total)},
         {float(s[6] / norm), float(s[7] / norm), float(s[8] / norm), float(s[9] / norm)},
         {float(s[3] / total), float(s[4] / total), float(s[5] / total)}});
  }
  result.clocks = std::move(*clocks);
  group_ = std::move(next);
  return result;
}

} // namespace nexora::animation
