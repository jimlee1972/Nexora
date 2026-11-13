#include "Nexora/TwoD/GameplayGeneration.h"
#include <stdexcept>
#include <utility>

namespace nexora::twod {
GameplayGeneration::GameplayGeneration(std::size_t capacity) : pool_(capacity) {}
GameplayGeneration::~GameplayGeneration() = default;

std::unique_ptr<GameplayGeneration>
GameplayGeneration::CreateSingleScene(std::string name, std::size_t capacity,
                                      ClipResolver clips) noexcept {
  try {
    if (name.empty() || name.size() > 256 || name.find('\0') != std::string::npos)
      return {};
    auto result = std::unique_ptr<GameplayGeneration>(new GameplayGeneration(capacity));
    const auto scene = result->world_.LoadScene(std::move(name));
    result->single_ = std::make_unique<SpriteScene>(result->world_, scene);
    result->bridge_ = std::make_unique<GameplayBridge>(
        result->world_, *result->single_, std::move(clips), &result->tiles_, &result->pool_);
    return result;
  } catch (...) {
    return {};
  }
}
std::unique_ptr<GameplayGeneration>
GameplayGeneration::CreateCatalog(std::vector<SceneEntry> catalog, std::size_t capacity,
                                  ClipResolver clips) noexcept {
  try {
    auto result = std::unique_ptr<GameplayGeneration>(new GameplayGeneration(capacity));
    result->catalog_ = std::make_unique<SceneSession>(result->world_, std::move(catalog));
    result->bridge_ = std::make_unique<GameplayBridge>(result->world_, *result->catalog_,
                                                       std::move(clips), &result->pool_);
    return result;
  } catch (...) {
    return {};
  }
}
GameplayGeneration::GameplayGeneration(const GameplayGeneration &source, StagedCopy)
    : world_(source.world_), tiles_(source.tiles_),
      pool_(source.pool_, InstancePool::StagedCopy{}) {
  if (source.single_) {
    single_ = std::make_unique<SpriteScene>(*source.single_);
    single_->world_ = &world_;
    bridge_ = std::make_unique<GameplayBridge>(world_, *single_, ClipResolver{}, &tiles_, &pool_);
  } else {
    catalog_ = source.catalog_->StageInto(world_);
    if (!catalog_)
      throw std::logic_error("A 2D scene transition is not quiescent");
    bridge_ = std::make_unique<GameplayBridge>(world_, *catalog_, ClipResolver{}, &pool_);
  }
  source.bridge_->CopyStagedState(*bridge_);
}
std::unique_ptr<GameplayGeneration> GameplayGeneration::Stage() const noexcept {
  if (owner_ != std::this_thread::get_id() || staging_ ||
      world_.Kind() != runtime::WorldKind::Play || pool_.updating_ || !bridge_->CanStage() ||
      (single_ && (&single_->WorldOwner() != &world_ || !world_.FindScene(single_->SceneId()))) ||
      (catalog_ && (!catalog_->Owner() || catalog_->processing_)))
    return {};
  staging_ = true;
  struct Leave final {
    bool &staging;
    ~Leave() { staging = false; }
  } leave{staging_};
  try {
    return std::unique_ptr<GameplayGeneration>(new GameplayGeneration(*this, StagedCopy{}));
  } catch (...) {
    return {};
  }
}
} // namespace nexora::twod
