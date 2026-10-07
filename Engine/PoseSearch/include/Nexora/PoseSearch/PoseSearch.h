#pragma once

#include "Nexora/PoseSearch/Api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nexora::animation {

struct FeaturePoint final {
  float x{}, y{}, z{};
};

// Translation-only reference layout: root-relative joint positions, joint velocities,
// then caller-provided root-relative trajectory points, all in caller-defined XYZ axes.
class NEXORA_POSE_SEARCH_API FeatureExtractor final {
public:
  [[nodiscard]] static std::optional<std::vector<float>>
  Extract(std::span<const FeaturePoint> positions, std::span<const FeaturePoint> previous_positions,
          float seconds, std::span<const FeaturePoint> trajectory);
};

struct PoseSearchSchema final {
  std::uint32_t version{1};
  std::vector<float> weights;
};

struct PoseSample final {
  std::uint64_t clip{};
  float time{};
  std::uint64_t tags{};
  std::vector<float> features;
  friend bool operator==(const PoseSample &, const PoseSample &) = default;
};

struct PoseQuery final {
  std::uint32_t schema_version{1};
  std::span<const float> features;
  std::uint64_t required_tags{};
  std::uint64_t forbidden_tags{};
};

struct PoseSearchResult final {
  std::uint64_t clip{};
  float time{};
  std::uint64_t tags{};
  double squared_distance{};
  std::uint64_t database_fingerprint{};
  friend bool operator==(const PoseSearchResult &, const PoseSearchResult &) = default;
};

// Owning, synchronous, immutable between successful builds. No filesystem or GPU work.
class NEXORA_POSE_SEARCH_API PoseDatabase final {
public:
  static constexpr std::size_t MaximumDimensions = 256;
  static constexpr std::size_t MaximumSamples = 65536;

  // Failure preserves the last database. Canonical order is (clip, time).
  bool Build(PoseSearchSchema schema, std::span<const PoseSample> samples);
  [[nodiscard]] std::optional<PoseSearchResult> Search(const PoseQuery &query) const;
  [[nodiscard]] std::uint64_t Fingerprint() const noexcept { return fingerprint_; }
  [[nodiscard]] std::span<const PoseSample> Samples() const noexcept { return samples_; }
  [[nodiscard]] std::size_t Dimensions() const noexcept { return schema_.weights.size(); }

private:
  PoseSearchSchema schema_;
  std::vector<PoseSample> samples_;
  std::uint64_t fingerprint_{};
};

} // namespace nexora::animation
