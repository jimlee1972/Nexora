#include "Nexora/PoseSearch/PoseSearch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace nexora::animation;

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

void DatabaseContracts() {
  PoseDatabase database;
  std::vector<PoseSample> samples{{9, 0.5F, 3, {1, 0}}, {2, 1, 1, {-1, 0}}, {2, 0, 2, {0, 2}}};
  const std::array<float, 2> origin{};
  Require(!database.Search({1, origin}), "unbuilt database returned a pose");
  Require(database.Build({7, {1, 4}}, samples), "valid database build failed");
  const auto fingerprint = database.Fingerprint();
  // Independent Python struct/FNV fixture fixes the canonical byte encoding.
  Require(fingerprint == 0x3f4b96b6fe5d6232ULL, "canonical fingerprint byte encoding changed");
  const auto canonical =
      std::vector<PoseSample>(database.Samples().begin(), database.Samples().end());
  Require(canonical.front().clip == 2 && canonical.front().time == 0,
          "database order is not canonical");
  const auto tie = database.Search({7, origin});
  Require(tie && tie->clip == 2 && tie->time == 1 && tie->squared_distance == 1,
          "nearest pose or canonical tie break failed");
  Require(tie->database_fingerprint == fingerprint, "search lost database provenance");
  const auto filtered = database.Search({7, origin, 2, 1});
  Require(filtered && filtered->clip == 2 && filtered->time == 0 &&
              filtered->squared_distance == 16,
          "tag filtering or feature weights were ignored");
  Require(!database.Search({7, origin, 4}) && !database.Search({7, origin, 1, 1}) &&
              !database.Search({8, origin}) && !database.Search({7, std::span{origin}.first(1)}),
          "incompatible or unsatisfiable query returned a pose");

  std::reverse(samples.begin(), samples.end());
  PoseDatabase rebuilt;
  Require(rebuilt.Build({7, {1, 4}}, samples) && rebuilt.Fingerprint() == fingerprint &&
              std::ranges::equal(rebuilt.Samples(), canonical) &&
              rebuilt.Search({7, origin}) == tie,
          "reordered source changed build or search results");
  samples[0].features[0] = 100;
  Require(database.Search({7, origin}) == tie, "database retained borrowed feature storage");

  const auto fails = [&](PoseSearchSchema schema, std::vector<PoseSample> invalid) {
    Require(!database.Build(std::move(schema), invalid), "invalid database build succeeded");
    Require(database.Fingerprint() == fingerprint && database.Search({7, origin}) == tie &&
                std::ranges::equal(database.Samples(), canonical),
            "failed rebuild damaged the previous database");
  };
  fails({0, {1, 4}}, canonical);
  fails({7, {}}, canonical);
  fails({7, {0, 0}}, canonical);
  fails({7, {-1, 4}}, canonical);
  fails({7, {std::numeric_limits<float>::infinity(), 4}}, canonical);
  fails({7, std::vector<float>(PoseDatabase::MaximumDimensions + 1, 1)}, canonical);
  fails({7, {1, 4}}, {});
  auto invalid = canonical;
  invalid.push_back(canonical[0]);
  fails({7, {1, 4}}, invalid);
  invalid = canonical;
  invalid[0].clip = 0;
  fails({7, {1, 4}}, invalid);
  invalid = canonical;
  invalid[0].time = -1;
  fails({7, {1, 4}}, invalid);
  invalid[0].time = std::numeric_limits<float>::quiet_NaN();
  fails({7, {1, 4}}, invalid);
  invalid = canonical;
  invalid[0].features.pop_back();
  fails({7, {1, 4}}, invalid);
  invalid = canonical;
  invalid[0].features[0] = std::numeric_limits<float>::quiet_NaN();
  fails({7, {1, 4}}, invalid);

  auto changed = canonical;
  changed[0].tags ^= 4;
  Require(rebuilt.Build({7, {1, 4}}, changed) && rebuilt.Fingerprint() != fingerprint,
          "tag changes did not invalidate artifact identity");
  Require(rebuilt.Build({7, {2, 4}}, canonical) && rebuilt.Fingerprint() != fingerprint,
          "weight changes did not invalidate artifact identity");
  Require(rebuilt.Build({8, {1, 4}}, canonical) && rebuilt.Fingerprint() != fingerprint,
          "schema version changes did not invalidate artifact identity");
  changed = canonical;
  changed[0].features[0] = -0.0F;
  changed[0].time = -0.0F;
  Require(rebuilt.Build({7, {1, 4}}, changed) && rebuilt.Fingerprint() == fingerprint &&
              !std::signbit(rebuilt.Samples()[0].time) &&
              !std::signbit(rebuilt.Samples()[0].features[0]),
          "signed zero did not canonicalize");

  const auto maximum = std::numeric_limits<float>::max();
  const std::array<float, 1> large_query{-maximum};
  const std::array<PoseSample, 1> large_sample{{{1, 0, 0, {maximum}}}};
  Require(rebuilt.Build({1, {maximum}}, large_sample), "finite extreme data was rejected");
  const auto extreme = rebuilt.Search({1, large_query});
  Require(extreme && std::isfinite(extreme->squared_distance) && extreme->squared_distance > 1e100,
          "distance computation overflowed float intermediates");
  const std::array<float, 1> nan_query{std::numeric_limits<float>::quiet_NaN()};
  Require(!rebuilt.Search({1, nan_query}), "non-finite query was accepted");
}

void FeatureContracts() {
  const std::array<FeaturePoint, 2> positions{{{10, 2, 3}, {11, 4, 6}}};
  const std::array<FeaturePoint, 2> previous{{{9, 2, 3}, {10, 3, 5}}};
  const std::array<FeaturePoint, 1> trajectory{{{2, 0, 4}}};
  const auto features = FeatureExtractor::Extract(positions, previous, 0.5F, trajectory);
  Require(features && *features == std::vector<float>{0, 0, 0, 1, 2, 3, 2, 0, 0, 2, 2, 2, 2, 0, 4},
          "translation/velocity/trajectory feature layout changed");
  auto translated = positions;
  auto translated_previous = previous;
  for (auto &point : translated)
    point.x += 100;
  for (auto &point : translated_previous)
    point.x += 100;
  Require(FeatureExtractor::Extract(translated, translated_previous, 0.5F, trajectory) == features,
          "common world translation changed extracted features");
  Require(!FeatureExtractor::Extract({}, {}, 1, {}) &&
              !FeatureExtractor::Extract(positions, std::span{previous}.first(1), 1, {}) &&
              !FeatureExtractor::Extract(positions, previous, 0, {}) &&
              !FeatureExtractor::Extract(positions, previous, -1, {}) &&
              !FeatureExtractor::Extract(positions, previous,
                                         std::numeric_limits<float>::infinity(), {}),
          "invalid feature extraction input was accepted");
  auto invalid = positions;
  invalid[1].y = std::numeric_limits<float>::quiet_NaN();
  Require(!FeatureExtractor::Extract(invalid, previous, 1, {}), "NaN joint was accepted");
  const std::array<FeaturePoint, 1> invalid_trajectory{
      {{0, 0, std::numeric_limits<float>::infinity()}}};
  Require(!FeatureExtractor::Extract(positions, previous, 1, invalid_trajectory),
          "non-finite trajectory was accepted");
  invalid = positions;
  invalid[0].x = -std::numeric_limits<float>::max();
  invalid[1].x = std::numeric_limits<float>::max();
  Require(!FeatureExtractor::Extract(invalid, previous, 1, {}),
          "unrepresentable relative position was accepted");
  Require(
      !FeatureExtractor::Extract(positions, previous, std::numeric_limits<float>::denorm_min(), {}),
      "unrepresentable velocity was accepted");
  std::vector<FeaturePoint> too_many(43);
  Require(!FeatureExtractor::Extract(too_many, too_many, 1, {}),
          "feature dimension limit was ignored");
}

void CapacityAndReference() {
  std::vector<PoseSample> samples;
  samples.reserve(PoseDatabase::MaximumSamples + 1);
  for (std::size_t i = 0; i < PoseDatabase::MaximumSamples; ++i)
    samples.push_back({i + 1, 0, i % 2, {static_cast<float>(i), static_cast<float>(i % 11)}});
  PoseDatabase database;
  Require(database.Build({1, {1, 2}}, samples), "maximum-sized database build failed");
  const auto fingerprint = database.Fingerprint();
  for (std::size_t query_index = 0; query_index < 16; ++query_index) {
    const std::array<float, 2> query{static_cast<float>(query_index * 4000) + 0.5F, 5};
    const auto hit = database.Search({1, query, 1});
    double reference_cost = std::numeric_limits<double>::max();
    std::uint64_t reference_clip{};
    for (const auto &sample : samples) {
      if (sample.tags != 1)
        continue;
      const double x = static_cast<double>(sample.features[0]) - query[0];
      const double y = static_cast<double>(sample.features[1]) - query[1];
      const auto cost = x * x + 2 * y * y;
      if (cost < reference_cost) {
        reference_cost = cost;
        reference_clip = sample.clip;
      }
    }
    Require(hit && hit->clip == reference_clip && hit->squared_distance == reference_cost,
            "search disagrees with independent nearest-neighbour reference");
  }
  samples.push_back({PoseDatabase::MaximumSamples + 1, 0, 0, {0, 0}});
  Require(!database.Build({1, {1, 2}}, samples) && database.Fingerprint() == fingerprint,
          "sample capacity or transactional failure contract failed");
}
} // namespace

int main() {
  DatabaseContracts();
  FeatureContracts();
  CapacityAndReference();
  std::cout << "V2-M8 portable pose database/search contracts passed\n";
}
