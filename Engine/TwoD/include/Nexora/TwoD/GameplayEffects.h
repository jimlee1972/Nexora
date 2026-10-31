#pragma once
#include "Nexora/TwoD/Api.h"
#include "nexora/nexora.h"
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace nexora::twod {
[[nodiscard]] NEXORA_2D_API bool
ValidateEffectsOccluder(const NexoraTwoDEffectsOccluderV1 &) noexcept;
// A frozen owner frame: no pointer into the caller or mutable pending submission.
struct EffectsSnapshot final {
  bool enabled{};
  float ambient[3]{1, 1, 1};
  float strength{1};
  NexoraTwoDEffectsBloomV1 bloom{};
  std::vector<NexoraTwoDEffectsLightV1> lights;
  std::vector<NexoraTwoDEffectsOccluderV1> occluders;
  std::vector<NexoraTwoDEffectsSpriteV1> sprites;
  std::uint64_t revision{};
  std::uint32_t debugMode{}, debugShadowSlot{};
};
// Host-supplied resolvers must check the complete generation-bearing identity. They borrow
// World/pool/catalog until this service is destroyed and must not mutate/reenter this service.
struct EffectsPoolIdentity final {
  std::uint64_t owner{};
  std::uint32_t index{}, generation{};
};
class NEXORA_2D_API GameplayEffects final {
public:
  using SpriteResolver = std::function<bool(std::uint32_t, std::uint64_t)>;
  using MaskResolver = std::function<bool(std::uint64_t, std::uint64_t, std::uint32_t)>;
  using DiagnosticsProvider = std::function<bool(NexoraTwoDEffectsDiagnosticsV1 &)>;
  using PoolResolver = std::function<bool(EffectsPoolIdentity)>;
  explicit GameplayEffects(std::uint32_t features = 0, SpriteResolver = {}, MaskResolver = {},
                           PoolResolver = {}, DiagnosticsProvider = {});
  // Copies a currently live, full pool identity; stale/foreign tokens never resolve.
  [[nodiscard]] std::optional<EffectsPoolIdentity> ResolvePoolIdentity(std::uint64_t) const;
  ~GameplayEffects();
  GameplayEffects(const GameplayEffects &) = delete;
  GameplayEffects &operator=(const GameplayEffects &) = delete;
  [[nodiscard]] NexoraGameplayTwoDEffectsApiV1 Api() noexcept;
  // Publishes pending state once, before game updates. Retained snapshots own all arrays.
  [[nodiscard]] std::shared_ptr<const EffectsSnapshot> BeginFrame();

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
