#pragma once

#include "Nexora/Animation/Api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nexora::animation {

using ResourceId = std::uint64_t;

struct SyncMarker final {
  std::string name;
  double time{};
};

struct SyncGroupMember final {
  ResourceId clip{};
  double duration{};
  double time{};
  float weight{};
  std::vector<SyncMarker> markers;
};

struct SyncGroupSample final {
  ResourceId clip{};
  double time{};
  bool leader{};
  bool marker_synced{};
  friend bool operator==(const SyncGroupSample &, const SyncGroupSample &) = default;
};

// Caller-owned looping visual clocks. No clip sampling, gameplay events, or root motion.
class NEXORA_ANIMATION_API SyncGroup final {
public:
  static constexpr std::size_t MaxMembers = 256;
  static constexpr std::size_t MaxMarkers = 256;
  bool SetMembers(std::span<const SyncGroupMember> members);
  bool SetWeight(ResourceId clip, float weight);
  [[nodiscard]] std::optional<std::vector<SyncGroupSample>> Update(double seconds);

private:
  std::vector<SyncGroupMember> members_;
};

} // namespace nexora::animation
