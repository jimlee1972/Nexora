#pragma once
#include "Nexora/TwoD/GameplayContent.h"
#include "nexora/gameplay_assets.h"
#include <functional>
#include <string>

namespace nexora::twod {
struct GameplayAssetFile final {
  std::uint32_t kind{};
  std::uint64_t bytes{};
  std::string path;
};
// A single background reader, bounded reservations and owner-thread publication. The reader
// must be thread-safe, return exactly the catalog byte length and enforce its package root.
class NEXORA_2D_API GameplayAssets final {
public:
  using Reader = std::function<Bytes(const GameplayAssetFile &)>;
  GameplayAssets(GameplayContent &, std::vector<GameplayAssetFile>, Reader,
                 std::uint64_t staging_bytes = 64 * 1024 * 1024);
  ~GameplayAssets(); // Joins the outstanding reader; borrowed reader services must still exist.
  GameplayAssets(const GameplayAssets &) = delete;
  GameplayAssets &operator=(const GameplayAssets &) = delete;
  [[nodiscard]] int32_t GetApi(NexoraGameplayAssetsApiV1 *) noexcept;
  void Tick();     // Nonblocking IO poll, then synchronous cooked validation/publication.
  void Shutdown(); // Join/discard reads before shutting down borrowed IO services.

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
