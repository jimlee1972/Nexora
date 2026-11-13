#pragma once
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/SceneSession.h"
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace nexora::twod {
// Host-owned values for one published, staged or retired Play generation. No renderer or game
// library is owned here. Drain every module callback, work lease and metadata borrow before Stage.
// ClipResolver asset storage must outlive every generation; captures cannot borrow a generation's
// World/state or game-library functions. External audio/window/storage/services need separate
// embedding isolation or rejection; this owner stages only its complete TwoD gameplay values.
class NEXORA_2D_API GameplayGeneration final {
public:
  [[nodiscard]] static std::unique_ptr<GameplayGeneration>
  CreateSingleScene(std::string name, std::size_t poolCapacity = 1024, ClipResolver = {}) noexcept;
  [[nodiscard]] static std::unique_ptr<GameplayGeneration>
  CreateCatalog(std::vector<SceneEntry>, std::size_t poolCapacity = 1024,
                ClipResolver = {}) noexcept;
  ~GameplayGeneration();
  GameplayGeneration(const GameplayGeneration &) = delete;
  GameplayGeneration &operator=(const GameplayGeneration &) = delete;
  // Owner-thread, quiescent copy. Failure leaves live state unchanged. The candidate preserves
  // complete pool handles and stable World IDs; it must stay unpublished until an embedding
  // transaction commits. Keep the retired generation through all old Stop/Destroy/finalizers.
  [[nodiscard]] std::unique_ptr<GameplayGeneration> Stage() const noexcept;
  [[nodiscard]] runtime::World &World() noexcept { return world_; }
  [[nodiscard]] const runtime::World &World() const noexcept { return world_; }
  [[nodiscard]] SpriteScene *Sprites() noexcept { return single_.get(); }
  [[nodiscard]] const SpriteScene *Sprites() const noexcept { return single_.get(); }
  [[nodiscard]] Tilemap *Tiles() noexcept { return single_ ? &tiles_ : nullptr; }
  [[nodiscard]] const Tilemap *Tiles() const noexcept { return single_ ? &tiles_ : nullptr; }
  [[nodiscard]] SceneSession *Scenes() noexcept { return catalog_.get(); }
  [[nodiscard]] const SceneSession *Scenes() const noexcept { return catalog_.get(); }
  [[nodiscard]] InstancePool &Pool() noexcept { return pool_; }
  [[nodiscard]] const InstancePool &Pool() const noexcept { return pool_; }
  [[nodiscard]] GameplayBridge &Bridge() noexcept { return *bridge_; }
  [[nodiscard]] const GameplayBridge &Bridge() const noexcept { return *bridge_; }

private:
  struct StagedCopy {};
  explicit GameplayGeneration(std::size_t capacity);
  GameplayGeneration(const GameplayGeneration &, StagedCopy);
  runtime::World world_{runtime::WorldKind::Play};
  std::unique_ptr<SpriteScene> single_;
  Tilemap tiles_;
  std::unique_ptr<SceneSession> catalog_;
  InstancePool pool_;
  std::unique_ptr<GameplayBridge> bridge_;
  mutable bool staging_{};
  const std::thread::id owner_{std::this_thread::get_id()};
};
} // namespace nexora::twod
