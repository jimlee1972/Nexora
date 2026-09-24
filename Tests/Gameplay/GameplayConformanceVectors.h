#pragma once

#include <array>
#include <cstdint>

namespace nexora::test {

enum class GameplayConformanceOperation : std::uint8_t { FixedUpdate, Update };

struct GameplayConformanceVector final {
  GameplayConformanceOperation operation;
  double delta_seconds;
};

// This sequence is consumed unchanged by the in-tree C++ fake module and the
// Zig module. Keep expected values beside it so neither consumer can silently
// redefine the contract.
inline constexpr std::array<GameplayConformanceVector, 3> kGameplayConformanceVectors{{
    {GameplayConformanceOperation::FixedUpdate, 1.0 / 60.0},
    {GameplayConformanceOperation::Update, 0.25},
    {GameplayConformanceOperation::Update, 0.5},
}};
inline constexpr std::uint32_t kExpectedFixedUpdates = 1;
inline constexpr std::uint32_t kExpectedUpdates = 2;
inline constexpr double kExpectedElapsedSeconds = 0.75;

template <typename Host> bool RunGameplayConformanceVectors(Host &host) {
  for (const auto &vector : kGameplayConformanceVectors) {
    const bool accepted = vector.operation == GameplayConformanceOperation::FixedUpdate
                              ? host.FixedUpdate(vector.delta_seconds)
                              : host.Update(vector.delta_seconds);
    if (!accepted)
      return false;
  }
  return true;
}

} // namespace nexora::test
