#include "Nexora/PoseSearch/PoseSearch.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <tuple>

namespace nexora::animation {
namespace {
bool Finite(FeaturePoint value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float Canonical(float value) { return value == 0.0F ? 0.0F : value; }

// Versioned canonical little-endian FNV-1a identity; never hashes padding or std::hash.
class FingerprintBuilder final {
public:
  void Integer(std::uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
      value_ ^= (value >> (i * 8)) & 0xffU;
      value_ *= 1099511628211ULL;
    }
  }
  void Float(float value) { Integer(std::bit_cast<std::uint32_t>(value), 4); }
  [[nodiscard]] std::uint64_t Value() const { return value_; }

private:
  std::uint64_t value_{14695981039346656037ULL};
};
} // namespace

std::optional<std::vector<float>>
FeatureExtractor::Extract(std::span<const FeaturePoint> positions,
                          std::span<const FeaturePoint> previous_positions, float seconds,
                          std::span<const FeaturePoint> trajectory) {
  if (positions.empty() || positions.size() != previous_positions.size() ||
      positions.size() > PoseDatabase::MaximumDimensions / 6 ||
      trajectory.size() > (PoseDatabase::MaximumDimensions - positions.size() * 6) / 3 ||
      !std::isfinite(seconds) || seconds <= 0.0F || !std::ranges::all_of(positions, Finite) ||
      !std::ranges::all_of(previous_positions, Finite) || !std::ranges::all_of(trajectory, Finite))
    return std::nullopt;
  std::vector<float> features;
  features.reserve(positions.size() * 6 + trajectory.size() * 3);
  const auto append = [&features](double x, double y, double z) {
    for (const double value : {x, y, z}) {
      if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        return false;
      features.push_back(Canonical(static_cast<float>(value)));
    }
    return true;
  };
  const auto root = positions.front();
  for (const auto position : positions)
    if (!append(static_cast<double>(position.x) - root.x, static_cast<double>(position.y) - root.y,
                static_cast<double>(position.z) - root.z))
      return std::nullopt;
  for (std::size_t i = 0; i < positions.size(); ++i)
    if (!append((static_cast<double>(positions[i].x) - previous_positions[i].x) / seconds,
                (static_cast<double>(positions[i].y) - previous_positions[i].y) / seconds,
                (static_cast<double>(positions[i].z) - previous_positions[i].z) / seconds))
      return std::nullopt;
  for (const auto point : trajectory)
    if (!append(point.x, point.y, point.z))
      return std::nullopt;
  return features;
}

bool PoseDatabase::Build(PoseSearchSchema schema, std::span<const PoseSample> samples) {
  if (schema.version == 0 || schema.weights.empty() || schema.weights.size() > MaximumDimensions ||
      samples.empty() || samples.size() > MaximumSamples ||
      std::ranges::any_of(schema.weights,
                          [](float weight) { return !std::isfinite(weight) || weight < 0.0F; }) ||
      !std::ranges::any_of(schema.weights, [](float weight) { return weight > 0.0F; }))
    return false;
  // Validate borrowed input before copying any feature arrays.
  for (const auto &sample : samples)
    if (sample.clip == 0 || !std::isfinite(sample.time) || sample.time < 0.0F ||
        sample.features.size() != schema.weights.size() ||
        std::ranges::any_of(sample.features, [](float value) { return !std::isfinite(value); }))
      return false;

  std::vector<PoseSample> canonical(samples.begin(), samples.end());
  for (auto &weight : schema.weights)
    weight = Canonical(weight);
  for (auto &sample : canonical) {
    sample.time = Canonical(sample.time);
    for (auto &feature : sample.features)
      feature = Canonical(feature);
  }
  std::ranges::sort(canonical, [](const PoseSample &a, const PoseSample &b) {
    return std::tie(a.clip, a.time) < std::tie(b.clip, b.time);
  });
  for (std::size_t i = 1; i < canonical.size(); ++i)
    if (canonical[i - 1].clip == canonical[i].clip && canonical[i - 1].time == canonical[i].time)
      return false;

  FingerprintBuilder hash;
  hash.Integer(1, 4); // Canonical encoding version, independent of feature schema version.
  hash.Integer(schema.version, 4);
  hash.Integer(schema.weights.size(), 4);
  for (const auto weight : schema.weights)
    hash.Float(weight);
  hash.Integer(canonical.size(), 4);
  for (const auto &sample : canonical) {
    hash.Integer(sample.clip, 8);
    hash.Float(sample.time);
    hash.Integer(sample.tags, 8);
    for (const auto feature : sample.features)
      hash.Float(feature);
  }
  schema_ = std::move(schema);
  samples_ = std::move(canonical);
  fingerprint_ = hash.Value();
  return true;
}

std::optional<PoseSearchResult> PoseDatabase::Search(const PoseQuery &query) const {
  if (samples_.empty() || query.schema_version != schema_.version ||
      query.features.size() != schema_.weights.size() ||
      (query.required_tags & query.forbidden_tags) != 0 ||
      std::ranges::any_of(query.features, [](float value) { return !std::isfinite(value); }))
    return std::nullopt;
  std::optional<PoseSearchResult> best;
  for (const auto &sample : samples_) {
    if ((sample.tags & query.required_tags) != query.required_tags ||
        (sample.tags & query.forbidden_tags) != 0)
      continue;
    double distance = 0.0;
    for (std::size_t i = 0; i < query.features.size(); ++i) {
      const double delta = static_cast<double>(query.features[i]) - sample.features[i];
      distance += static_cast<double>(schema_.weights[i]) * delta * delta;
    }
    // Strict comparison keeps the first canonical (clip, time) on an exact tie.
    if (!best || distance < best->squared_distance)
      best = PoseSearchResult{sample.clip, sample.time, sample.tags, distance, fingerprint_};
  }
  return best;
}

} // namespace nexora::animation
