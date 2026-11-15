#include "Nexora/TwoD/GameSpriteSession.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace nexora::twod {
struct GameSpriteSession::State final {
  struct Entry final {
    SpriteScene metadata;
    runtime::AssetUuid asset;
    std::shared_ptr<const runtime::LoadedStaticProject> source;
    std::unordered_map<runtime::Id, runtime::Id> entities;
  };
  std::map<runtime::Id, Entry> scenes;
  runtime::Id primary{};
};

GameSpriteSession::GameSpriteSession(runtime::World &world, GameSpriteAssets assets,
                                     std::size_t capacity)
    : world_(&world), assets_(std::make_shared<const GameSpriteAssets>(std::move(assets))),
      pool_(capacity), state_(std::make_unique<State>()) {
  if (world.Kind() != runtime::WorldKind::Play)
    throw std::invalid_argument("Main game Sprite metadata requires a Play World.");
  bridge_ = std::make_unique<GameplayBridge>(
      world, *this, [assets = assets_](AssetId id) { return assets->ResolveClip(id); }, &pool_);
}
GameSpriteSession::~GameSpriteSession() = default;
bool GameSpriteSession::Owner() const noexcept { return thread_ == std::this_thread::get_id(); }
bool GameSpriteSession::Available() const noexcept {
  return Owner() && !preparing_ && !pool_.updating_;
}
runtime::Id GameSpriteSession::Primary() const noexcept { return state_->primary; }

GameSpriteSession::GameSpriteSession(const GameSpriteSession &source, runtime::World &world,
                                     ReloadCopy)
    : world_(&world), assets_(source.assets_), pool_(source.pool_, InstancePool::ReloadCopy{}),
      state_(std::make_unique<State>()), revision_(source.revision_) {
  state_->primary = source.state_->primary;
  for (const auto &[id, entry] : source.state_->scenes) {
    auto metadata = entry.metadata.ForkForReload(world);
    if (!metadata)
      throw std::invalid_argument("Main game Sprite reload scene membership changed.");
    state_->scenes.emplace(
        id, State::Entry{std::move(*metadata), entry.asset, entry.source, entry.entities});
  }
  bridge_ = std::make_unique<GameplayBridge>(
      world, *this, [assets = assets_](AssetId id) { return assets->ResolveClip(id); }, &pool_);
  source.bridge_->CopyStagedState(*bridge_);
}
std::unique_ptr<GameSpriteSession> GameSpriteSession::ForkForReload(runtime::World &world) const {
  if (!Available() || !bridge_->CanStage() || &world == world_ ||
      world.Kind() != runtime::WorldKind::Play)
    return {};
  for (const auto &[id, entry] : state_->scenes) {
    static_cast<void>(id);
    if (!entry.metadata.CanRebindForReload(world))
      return {};
  }
  return std::unique_ptr<GameSpriteSession>(new GameSpriteSession(*this, world, ReloadCopy{}));
}
bool GameSpriteSession::RebindForReload(runtime::World &world) noexcept {
  if (!Available() || !bridge_->CanStage() || world.Kind() != runtime::WorldKind::Play)
    return false;
  for (const auto &[id, entry] : state_->scenes) {
    static_cast<void>(id);
    if (!entry.metadata.CanRebindForReload(world))
      return false;
  }
  // All potentially failing admission precedes these nonallocating owner-thread writes.
  for (auto &[id, entry] : state_->scenes) {
    static_cast<void>(id);
    entry.metadata.world_ = &world;
  }
  world_ = &world;
  bridge_->RebindGameWorld(world);
  return true;
}

std::unique_ptr<GameSpriteSession::Prepared>
GameSpriteSession::Prepare(const runtime::GameSceneView &view, std::string *error) {
  if (error)
    error->clear();
  const auto reject = [&](const char *text) -> std::unique_ptr<Prepared> {
    if (error)
      *error = text;
    return {};
  };
  if (!Available() || revision_ == std::numeric_limits<std::uint64_t>::max())
    return reject("Main game Sprite preparation requires the idle owner thread.");
  if (view.world.Kind() != runtime::WorldKind::Play || view.instances.empty() ||
      view.instances.size() > runtime::kMaximumGameScenes)
    return reject("Main game Sprite scene scope is invalid.");
  auto next = std::make_unique<State>();
  next->primary = view.primary;
  std::unordered_set<runtime::Id> mapped;
  for (const auto &instance : view.instances) {
    const auto *scene = view.world.FindScene(instance.scene);
    if (!scene || scene->state != runtime::SceneState::Active || !instance.source ||
        instance.asset == runtime::AssetUuid{} || next->scenes.contains(instance.scene))
      return reject("Main game Sprite instance ownership is invalid.");
    const auto *source = instance.source->WorldView().FindScene(instance.source->SceneId());
    if (!source || instance.entities.size() != source->entities.size())
      return reject("Main game Sprite entity mapping is incomplete.");
    if (instance.entities.size() > runtime::kCookedSceneMaximumEntities - mapped.size())
      return reject("Main game Sprite mapping budget exceeded.");
    const auto retained = state_->scenes.find(instance.scene);
    if (retained != state_->scenes.end() &&
        (retained->second.asset != instance.asset || retained->second.source != instance.source ||
         retained->second.entities != instance.entities))
      return reject("A retained main game Sprite instance changed identity.");
    std::unordered_set<runtime::Id> alive;
    for (const auto &entity : scene->entities)
      alive.insert(entity.id);
    for (const auto &entity : source->entities) {
      const auto found = instance.entities.find(entity.id);
      if (found == instance.entities.end() || !found->second ||
          !mapped.insert(found->second).second ||
          (retained == state_->scenes.end() && !alive.contains(found->second)))
        return reject("Main game Sprite remapped entity is invalid.");
    }
    State::Entry entry{SpriteScene(*world_, instance.scene, SpriteScene::PreparedMetadata{}),
                       instance.asset, instance.source, instance.entities};
    if (retained != state_->scenes.end()) {
      entry.metadata.sprites_ = retained->second.metadata.sprites_;
      entry.metadata.colliders_ = retained->second.metadata.colliders_;
      std::erase_if(entry.metadata.sprites_,
                    [&](const auto &v) { return !alive.contains(v.first); });
      std::erase_if(entry.metadata.colliders_,
                    [&](const auto &v) { return !alive.contains(v.first); });
    } else {
      for (const auto &record : instance.source->SceneData().opaque) {
        if (record.type != runtime::kGameSpriteSourceReferenceType &&
            record.type_name != runtime::kGameSpriteSourceReferenceName)
          continue;
        const auto id =
            runtime::ReadGameSpriteSourceReference(record.type, record.type_name, record.data);
        const auto entity = instance.entities.find(record.entity);
        if (!id || !assets_->Source(*id) || entity == instance.entities.end())
          return reject("Main game Sprite source binding is unresolved.");
        Sprite sprite;
        sprite.source = *id;
        if (!entry.metadata.sprites_.emplace(entity->second, sprite).second)
          return reject("Main game Sprite source binding is duplicated.");
      }
    }
    next->scenes.emplace(instance.scene, std::move(entry));
  }
  if (!next->scenes.contains(next->primary))
    return reject("Main game Sprite primary scene is missing.");
  auto prepared = std::unique_ptr<Prepared>(
      new Prepared(*this, std::move(next), reinterpret_cast<std::uintptr_t>(&view.world)));
  preparing_ = true;
  return prepared;
}

GameSpriteSession::Prepared::Prepared(GameSpriteSession &owner, std::unique_ptr<State> state,
                                      std::uintptr_t world_token)
    : owner_(owner), state_(std::move(state)), world_token_(world_token) {}
GameSpriteSession::Prepared::~Prepared() {
  if (!committed_ && owner_.Owner())
    owner_.preparing_ = false;
}
std::optional<SpriteFrame> GameSpriteSession::Prepared::Extract(const runtime::World &world,
                                                                std::uint32_t width,
                                                                std::uint32_t height) {
  if (committed_ || !owner_.Owner() || reinterpret_cast<std::uintptr_t>(&world) != world_token_)
    return {};
  auto camera = owner_.bridge_->RenderCamera(width, height);
  if (state_->primary != owner_.Primary())
    camera = {{}, 20, 0, width, height};
  return owner_.ExtractState(*state_, world, camera);
}
void GameSpriteSession::Prepared::Commit() noexcept {
  if (committed_ || !owner_.Owner())
    return;
  owner_.state_.swap(state_);
  ++owner_.revision_;
  owner_.preparing_ = false;
  committed_ = true;
}

SpriteScene *GameSpriteSession::FindEntity(runtime::Id id) noexcept {
  if (!Owner())
    return nullptr;
  for (auto &[scene_id, entry] : state_->scenes) {
    const auto *scene = world_->FindScene(scene_id);
    if (scene && scene->state == runtime::SceneState::Active &&
        std::ranges::any_of(scene->entities, [id](const auto &entity) { return entity.id == id; }))
      return &entry.metadata;
  }
  return nullptr;
}
void GameSpriteSession::ForgetEntity(runtime::Id id) {
  if (!Available())
    throw std::logic_error("Main game Sprite teardown requires the idle owner thread.");
  for (auto &[scene, entry] : state_->scenes) {
    static_cast<void>(scene);
    entry.metadata.Remove(id);
    entry.metadata.RemoveCollider(id);
  }
}
bool GameSpriteSession::Reconcile() {
  if (!Available())
    return false;
  std::set<runtime::Id> removed;
  for (const auto &[id, entry] : state_->scenes) {
    const auto *scene = world_->FindScene(id);
    if (!scene || scene->state != runtime::SceneState::Active)
      return false;
    std::unordered_set<runtime::Id> alive;
    for (const auto &entity : scene->entities)
      alive.insert(entity.id);
    for (const auto &[entity, value] : entry.metadata.sprites_)
      if (!alive.contains(entity))
        removed.insert(entity);
    for (const auto &[entity, value] : entry.metadata.colliders_)
      if (!alive.contains(entity))
        removed.insert(entity);
  }
  for (const auto id : removed)
    bridge_->ForgetEntity(id);
  return true;
}
std::optional<std::vector<Shape>> GameSpriteSession::CollisionShapes() const {
  if (!Available())
    return {};
  std::vector<Shape> result;
  for (const auto &[id, entry] : state_->scenes) {
    static_cast<void>(id);
    const auto values = entry.metadata.CollisionShapes();
    if (!values || values->size() > 100000 - result.size())
      return {};
    result.insert(result.end(), values->begin(), values->end());
  }
  return result;
}
std::optional<SpriteFrame>
GameSpriteSession::ExtractState(const State &state, const runtime::World &world, Camera camera) {
  std::vector<SpriteInstance> instances;
  for (const auto &[id, entry] : state.scenes) {
    const auto poses = world.SceneWorldPoses(id);
    if (!poses)
      return {};
    for (const auto &pose : *poses)
      if (const auto *sprite = entry.metadata.Find(pose.id)) {
        if (instances.size() == 100000)
          return {};
        instances.push_back({pose.id, *sprite, pose.matrix});
      }
  }
  std::ranges::sort(instances, {}, &SpriteInstance::id);
  // Render-local identities preserve entity ordering then stable pool slot order, without
  // reserving any part of Runtime's full uint64 World identity space for pooled values.
  std::uint64_t identity{};
  for (auto &value : instances)
    value.id = ++identity;
  bool overflow{};
  pool_.ForEach([&](const Instance &value) {
    if (instances.size() == 100000) {
      overflow = true;
      return;
    }
    instances.push_back({++identity, value.sprite, runtime::ToMatrix(value.transform)});
  });
  return overflow ? std::nullopt : assets_->Extract(instances, camera);
}
std::optional<SpriteFrame> GameSpriteSession::Extract(std::uint32_t width, std::uint32_t height) {
  if (!Available())
    return {};
  return ExtractState(*state_, *world_, bridge_->RenderCamera(width, height));
}
} // namespace nexora::twod
