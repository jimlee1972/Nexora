#include "Nexora/Animation/SyncGroup.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace nexora::animation {
namespace {
double Advance(double time, double seconds, double duration) {
  // Reduce before addition: even finite DBL_MAX ticks must not overflow the clock.
  const double delta = std::fmod(seconds, duration);
  const double result = delta >= duration - time ? delta - (duration - time) : time + delta;
  return result >= duration ? 0.0 : result;
}

bool Compatible(const SyncGroupMember &leader, const SyncGroupMember &follower) {
  const auto size = leader.markers.size();
  if (size < 2 || follower.markers.size() != size)
    return false;
  const auto start =
      std::ranges::find(follower.markers, leader.markers.front().name, &SyncMarker::name);
  if (start == follower.markers.end())
    return false;
  const auto offset = static_cast<std::size_t>(start - follower.markers.begin());
  for (std::size_t i = 0; i < size; ++i)
    if (leader.markers[i].name != follower.markers[(offset + i) % size].name)
      return false;
  return true;
}

double MapMarkers(const SyncGroupMember &leader, const SyncGroupMember &follower) {
  auto upper = std::ranges::upper_bound(leader.markers, leader.time, {}, &SyncMarker::time);
  const auto index = upper == leader.markers.begin()
                         ? leader.markers.size() - 1
                         : static_cast<std::size_t>(upper - leader.markers.begin() - 1);
  const auto &left = leader.markers[index];
  const auto &right = leader.markers[(index + 1) % leader.markers.size()];
  const double length =
      right.time > left.time ? right.time - left.time : (leader.duration - left.time) + right.time;
  const double elapsed = leader.time >= left.time ? leader.time - left.time
                                                  : (leader.duration - left.time) + leader.time;
  const double alpha = elapsed / length;
  const auto match = std::ranges::find(follower.markers, left.name, &SyncMarker::name);
  const auto next = match + 1 == follower.markers.end() ? follower.markers.begin() : match + 1;
  const double target_length = next->time > match->time
                                   ? next->time - match->time
                                   : (follower.duration - match->time) + next->time;
  return Advance(match->time, alpha * target_length, follower.duration);
}
} // namespace

bool SyncGroup::SetMembers(std::span<const SyncGroupMember> members) {
  if (members.empty() || members.size() > MaxMembers)
    return false;
  for (const auto &member : members) {
    if (member.clip == 0 || !std::isfinite(member.duration) || member.duration <= 0.0 ||
        !std::isfinite(member.time) || member.time < 0.0 || member.time >= member.duration ||
        !std::isfinite(member.weight) || member.weight < 0.0F ||
        member.markers.size() > MaxMarkers || member.markers.size() == 1)
      return false;
    double previous = -1.0;
    for (std::size_t i = 0; i < member.markers.size(); ++i) {
      const auto &marker = member.markers[i];
      if (marker.name.empty() || !std::isfinite(marker.time) || marker.time < 0.0 ||
          marker.time >= member.duration || marker.time <= previous)
        return false;
      for (std::size_t j = 0; j < i; ++j)
        if (member.markers[j].name == marker.name)
          return false;
      previous = marker.time;
    }
  }
  std::vector<SyncGroupMember> next(members.begin(), members.end());
  std::ranges::sort(next, {}, &SyncGroupMember::clip);
  for (std::size_t i = 1; i < next.size(); ++i)
    if (next[i - 1].clip == next[i].clip)
      return false;
  members_ = std::move(next);
  return true;
}

bool SyncGroup::SetWeight(ResourceId clip, float weight) {
  if (!std::isfinite(weight) || weight < 0.0F)
    return false;
  const auto member = std::ranges::find(members_, clip, &SyncGroupMember::clip);
  if (member == members_.end())
    return false;
  member->weight = weight;
  return true;
}

std::optional<std::vector<SyncGroupSample>> SyncGroup::Update(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0 || members_.empty())
    return std::nullopt;
  const auto leader = std::ranges::max_element(members_, {}, &SyncGroupMember::weight);
  if (leader->weight == 0.0F)
    return std::nullopt;
  std::vector<SyncGroupSample> result;
  result.reserve(members_.size());
  // Compute without mutation; allocation failure cannot leave partially advanced clocks.
  auto advanced = *leader;
  advanced.time = Advance(leader->time, seconds, leader->duration);
  for (const auto &member : members_) {
    const bool is_leader = member.clip == leader->clip;
    const bool marker_sync = !is_leader && Compatible(advanced, member);
    double time = is_leader     ? advanced.time
                  : marker_sync ? MapMarkers(advanced, member)
                                : (advanced.time / advanced.duration) * member.duration;
    // Rounding near the end of a loop can produce exactly duration.
    if (time >= member.duration)
      time = 0.0;
    result.push_back({member.clip, time, is_leader, marker_sync});
  }
  for (std::size_t i = 0; i < members_.size(); ++i)
    members_[i].time = result[i].time;
  return result;
}

} // namespace nexora::animation
