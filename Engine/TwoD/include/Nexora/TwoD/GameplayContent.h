#pragma once
#include "Nexora/TwoD/Scene.h"
#include "nexora/gameplay_content.h"
namespace nexora::twod {
// Owner-thread registry. Cooked bytes are copied/decoded; immutable resolver owners survive
// replacement.
class NEXORA_2D_API GameplayContent final {
public:
  struct Limits final {
    std::uint32_t assets{256};
    std::uint64_t active_bytes{64 * 1024 * 1024};
    std::uint64_t live_bytes{128 * 1024 * 1024};
  };
  struct Usage final {
    std::uint32_t active_assets{}, retired_generations{};
    std::uint64_t active_bytes{}, retired_bytes{}, texture_bytes{};
  };
  GameplayContent();
  explicit GameplayContent(Limits);
  ~GameplayContent();
  GameplayContent(const GameplayContent &) = delete;
  GameplayContent &operator=(const GameplayContent &) = delete;
  [[nodiscard]] int32_t GetApi(NexoraGameplayContentApiV1 *) noexcept;
  [[nodiscard]] std::optional<AtlasBinding> ResolveAtlas(AssetId) const;
  [[nodiscard]] std::shared_ptr<const Clip> ResolveClip(AssetId) const;
  [[nodiscard]] int32_t Remove(AssetId) noexcept;
  [[nodiscard]] Usage Statistics() const;
  [[nodiscard]] std::vector<std::uint64_t> CollectRetiredTextures();

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
