#pragma once
#include "Nexora/TwoD/Scene.h"
#include "nexora/gameplay_content.h"
namespace nexora::twod {
// Owner-thread registry. Cooked bytes are copied/decoded; immutable resolver owners survive
// replacement.
class NEXORA_2D_API GameplayContent final {
public:
  GameplayContent();
  ~GameplayContent();
  GameplayContent(const GameplayContent &) = delete;
  GameplayContent &operator=(const GameplayContent &) = delete;
  [[nodiscard]] int32_t GetApi(NexoraGameplayContentApiV1 *) noexcept;
  [[nodiscard]] std::optional<AtlasBinding> ResolveAtlas(AssetId) const;
  [[nodiscard]] std::shared_ptr<const Clip> ResolveClip(AssetId) const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
