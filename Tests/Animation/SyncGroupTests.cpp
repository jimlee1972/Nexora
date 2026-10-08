#include "Nexora/Animation/SyncGroup.h"

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
  Require(std::abs(actual - expected) < 1e-10, message);
}
std::vector<SyncGroupMember> WalkRun() {
  return {{20, 2.0, 0.0, 0.5F, {{"left", 0.2}, {"right", 1.0}}},
          {10, 1.0, 0.0, 1.0F, {{"left", 0.0}, {"right", 0.5}}},
          {30, 4.0, 0.0, 0.2F, {}}};
}
void MarkerAndLeaderTests() {
  auto members = WalkRun();
  SyncGroup group;
  Require(group.SetMembers(members), "valid members rejected");
  members[0].markers.clear(); // Group owns its input.
  const auto first = group.Update(0.25);
  Require(first && first->size() == 3 && (*first)[0].clip == 10 && (*first)[0].leader &&
              (*first)[1].marker_synced && !(*first)[2].marker_synced,
          "canonical ordering, leadership, or fallback failed");
  Near((*first)[0].time, 0.25, "leader clock failed");
  Near((*first)[1].time, 0.6, "marker segment interpolation failed");
  Near((*first)[2].time, 1.0, "normalized fallback failed");
  const auto wrap = group.Update(0.5);
  Near((*wrap)[1].time, 1.6, "wraparound segment failed");
  const auto before_first = group.Update(0.1);
  Near((*before_first)[1].time, 1.84, "wrapped target segment failed");
  Require(group.SetWeight(20, 2.0F), "weight update rejected");
  const auto switched = group.Update(0.0);
  Require((*switched)[1].leader && !(*switched)[0].leader, "leader switch failed");
  Near((*switched)[0].time, 0.85, "leader switch lost synchronized phase");
  const auto continued = group.Update(0.25);
  Near((*continued)[0].time, 229.0 / 240.0, "switched leader did not advance its own clock");
  Near((*continued)[1].time, 0.09, "new leader cycle wrapping failed");

  SyncGroup reordered;
  auto input = WalkRun();
  std::reverse(input.begin(), input.end());
  Require(reordered.SetMembers(input), "reordered members rejected");
  Require(reordered.Update(0.25) == first, "input order changed deterministic result");
  Require(reordered.SetWeight(20, 1.0F), "tie weight rejected");
  Require((*reordered.Update(0.0))[0].leader, "weight tie did not select lowest clip ID");

  // Identical cyclic marker order can have different first authored labels.
  SyncGroup rotated;
  Require(rotated.SetMembers(
              std::vector<SyncGroupMember>{{1, 1, 0, 1, {{"A", 0}, {"B", 0.3}, {"C", 0.6}}},
                                           {2, 2, 0, 0.5F, {{"B", 0.2}, {"C", 0.8}, {"A", 1.4}}}}),
          "cyclic rotation rejected");
  const auto rotation = rotated.Update(0.15);
  Require((*rotation)[1].marker_synced, "cyclic rotation did not match");
  Near((*rotation)[1].time, 1.8, "cyclic rotation interpolation failed");

  SyncGroup mismatch;
  Require(mismatch.SetMembers(
              std::vector<SyncGroupMember>{{1, 1, 0, 1, {{"A", 0}, {"B", 0.3}, {"C", 0.6}}},
                                           {2, 2, 0, 1, {{"A", 0}, {"C", 0.5}, {"B", 1}}}}),
          "mismatched marker order rejected instead of falling back");
  const auto fallback = mismatch.Update(0.25);
  Require(!(*fallback)[1].marker_synced, "incompatible order used marker sync");
  Near((*fallback)[1].time, 0.5, "mismatched order fallback failed");
}
void ValidationTests() {
  SyncGroup group;
  Require(!group.Update(0), "empty group updated");
  auto good = WalkRun();
  Require(group.SetMembers(good), "baseline rejected");
  const auto baseline = group.Update(0);
  auto Reject = [&](std::vector<SyncGroupMember> input) {
    Require(!group.SetMembers(input), "malformed group accepted");
    Require(group.Update(0) == baseline, "failed replacement changed live clocks");
  };
  Reject({});
  auto bad = good;
  bad.push_back(good[0]);
  Reject(bad);
  for (const double invalid : {-1.0, 0.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    bad = good;
    bad[0].duration = invalid;
    Reject(bad);
  }
  for (const double invalid : {-1.0, 2.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    bad = good;
    bad[0].time = invalid;
    Reject(bad);
  }
  bad = good;
  bad[0].clip = 0;
  Reject(bad);
  bad = good;
  bad[0].markers.resize(1);
  Reject(bad);
  bad = good;
  bad[0].markers[1].name = "left";
  Reject(bad);
  bad = good;
  bad[0].markers[1].name.clear();
  Reject(bad);
  for (const double invalid : {-1.0, 0.2, 2.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    bad = good;
    bad[0].markers[1].time = invalid;
    Reject(bad);
  }
  bad = good;
  bad[0].markers.resize(SyncGroup::MaxMarkers + 1);
  Reject(bad);
  bad = good;
  bad.resize(SyncGroup::MaxMembers + 1);
  Reject(bad);
  for (const float invalid :
       {-1.0F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    bad = good;
    bad[0].weight = invalid;
    Reject(bad);
    Require(!group.SetWeight(10, invalid) && group.Update(0) == baseline,
            "invalid weight changed group");
  }
  Require(!group.SetWeight(999, 1), "unknown clip weight accepted");
  for (const double invalid :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    Require(!group.Update(invalid) && group.Update(0) == baseline, "invalid tick changed clocks");
  for (const auto &member : good)
    Require(group.SetWeight(member.clip, 0), "zero weight rejected");
  Require(!group.Update(100), "all-zero group advanced");
  Require(group.SetWeight(10, 1), "resume failed");
  Require(group.Update(0) == baseline, "inactive group lost clock state");
}
void ClockAndGraphTests() {
  SyncGroup single, divided;
  const auto input = WalkRun();
  Require(single.SetMembers(input) && divided.SetMembers(input), "clock setup failed");
  const auto whole = single.Update(123.375);
  for (int i = 0; i < 987; ++i)
    Require(divided.Update(0.125).has_value(), "partitioned tick failed");
  Require(divided.Update(0) == whole, "tick partition changed phase");
  const auto huge = single.Update(std::numeric_limits<double>::max());
  for (const auto &sample : *huge)
    Require(std::isfinite(sample.time) && sample.time >= 0 && sample.time < 4,
            "huge finite tick overflowed");

  // Capacity and numerical extremes, including subnormal-duration loops.
  std::vector<SyncGroupMember> maximum;
  for (std::size_t i = 0; i < SyncGroup::MaxMembers; ++i)
    maximum.push_back({i + 1, 2, 0, 1, {}});
  SyncGroup bounded;
  for (std::size_t i = 0; i < SyncGroup::MaxMarkers; ++i)
    maximum.front().markers.push_back({std::to_string(i), static_cast<double>(i) / 128.0});
  Require(bounded.SetMembers(maximum) && bounded.Update(0.5)->size() == SyncGroup::MaxMembers,
          "maximum member capacity failed");
  const double tiny = std::numeric_limits<double>::denorm_min();
  Require(bounded.SetMembers(std::vector<SyncGroupMember>{{1, tiny, 0, 1, {}}}) &&
              (*bounded.Update(std::numeric_limits<double>::max()))[0].time == 0,
          "subnormal clock failed");
  const double large = std::numeric_limits<double>::max();
  Require(bounded.SetMembers(std::vector<SyncGroupMember>{
              {1, large, large * 0.75, 1, {{"A", large * 0.25}, {"B", large * 0.5}}},
              {2, large, 0, 0.5F, {{"A", large * 0.125}, {"B", large * 0.625}}}}),
          "large marker clock rejected");
  const auto extreme = bounded.Update(large * 0.5);
  Near((*extreme)[0].time / large, 0.25, "large clock wrapping failed");
  Near((*extreme)[1].time / large, 0.125, "large marker clock overflowed");
}
} // namespace
int main() {
  try {
    MarkerAndLeaderTests();
    ValidationTests();
    ClockAndGraphTests();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
