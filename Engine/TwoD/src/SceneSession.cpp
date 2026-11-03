#include "Nexora/TwoD/SceneSession.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace nexora::twod {
namespace {
bool Path(std::string_view path) {
  if (path.empty() || path.size() > 1024 || path.front() == '/' || path.back() == '/')
    return false;
  std::size_t at{};
  while (at < path.size()) {
    const auto end = path.find('/', at);
    const auto part = path.substr(at, end == path.npos ? path.size() - at : end - at);
    if (part.empty() || part == "." || part == "..")
      return false;
    at += part.size() + 1;
  }
  return std::ranges::all_of(
      path, [](unsigned char c) { return c >= 32 && c < 127 && c != '\\' && c != ':'; });
}
} // namespace
SceneSession::SceneSession(runtime::World &world, std::vector<SceneEntry> catalog)
    : world_(world), catalog_(std::move(catalog)) {
  if (world.Kind() != runtime::WorldKind::Play || catalog_.empty() || catalog_.size() > 64)
    throw std::invalid_argument("2D scene session needs a Play World and 1..64 catalog entries");
  std::set<std::string> paths;
  std::set<std::pair<std::uint64_t, std::uint64_t>> ids;
  for (const auto &entry : catalog_)
    if (!Path(entry.path) || (!entry.id.high && !entry.id.low) || !entry.source ||
        !entry.source->scene || !paths.insert(entry.path).second ||
        !ids.emplace(entry.id.high, entry.id.low).second)
      throw std::invalid_argument("Invalid 2D scene catalog");
}
SceneSession::~SceneSession() { Stop(); }
bool SceneSession::Owner() const noexcept {
  return owner_ == std::this_thread::get_id() && !stopped_;
}
int SceneSession::Load(AssetId asset, std::uint32_t mode, std::uint32_t flags,
                       std::uint64_t &request) {
  if (mode > 1 || flags > 1)
    return -1;
  if (pending_ || processing_)
    return -4;
  if (std::ranges::find(catalog_, asset, &SceneEntry::id) == catalog_.end())
    return -5;
  if (mode == 1 &&
      std::ranges::any_of(instances_, [asset](const auto &i) { return i.asset == asset; }))
    return -1;
  if (next_ == UINT64_MAX)
    return -6;
  pending_ = Request{next_++, asset, 0, mode, flags};
  status_ = {sizeof(status_), 1, pending_->id, 0, 0, 0};
  request = pending_->id;
  return 0;
}
int SceneSession::Unload(runtime::Id scene, std::uint64_t &request) {
  if (pending_ || processing_)
    return -4;
  const auto found = std::ranges::find_if(
      instances_, [scene](const auto &i) { return i.sprites.SceneId() == scene; });
  const auto *live = world_.FindScene(scene);
  if (found == instances_.end() || !live || live->state != runtime::SceneState::Active)
    return -5;
  if (live->persistent || instances_.size() == 1)
    return -1;
  if (next_ == UINT64_MAX)
    return -6;
  pending_ = Request{next_++, {}, scene, 0, 0};
  status_ = {sizeof(status_), 1, pending_->id, 0, 0, 0};
  request = pending_->id;
  return 0;
}
void SceneSession::Stop() noexcept {
  if (owner_ != std::this_thread::get_id())
    return;
  stopped_ = true;
  if (!processing_ && pending_) {
    status_.state = 4;
    status_.result = -3;
    completed_ = status_;
    pending_.reset();
  }
}
bool SceneSession::Process(const Prepare &prepare) {
  if (!Owner() || processing_)
    return false;
  if (!pending_)
    return true;
  processing_ = true;
  struct Guard {
    bool &value;
    ~Guard() { value = false; }
  } guard{processing_};
  const auto request = *pending_;
  auto fail = [&](int error) {
    status_.state = stopped_ ? 4 : 3;
    status_.result = error;
    completed_ = status_;
    pending_.reset();
    return false;
  };
  try {
    auto candidate = world_;
    const bool replace = !request.unload && request.mode == 0;
    for (const auto &instance : instances_) {
      const auto id = instance.sprites.SceneId();
      const auto *live = world_.FindScene(id);
      if (!live || live->state != runtime::SceneState::Active)
        return fail(-3);
      if (id == request.unload || (replace && !live->persistent))
        if (!candidate.RequestUnload(id))
          return fail(-3);
    }
    candidate.EndFrame();
    candidate.CollectUnloadedScenes();
    std::vector<SceneInstance> instances;
    instances.reserve(instances_.size() + 1);
    std::size_t entityCount{}, tileCount{};
    auto selected = primary_;
    for (const auto &old : instances_) {
      const auto *retained = candidate.FindScene(old.sprites.SceneId());
      if (!retained)
        continue;
      entityCount += retained->entities.size();
      tileCount += old.tiles.Tiles().size();
      auto sprites = old.sprites;
      sprites.world_ = &candidate;
      instances.push_back({old.asset, std::move(sprites), old.tiles, old.entities});
    }
    runtime::Id added{};
    if (!request.unload) {
      const auto &entry = *std::ranges::find(catalog_, request.asset, &SceneEntry::id);
      const auto &source = *entry.source;
      const auto snapshot = source.world.SaveScene(source.scene->SceneId());
      const auto *authored = source.world.FindScene(source.scene->SceneId());
      if (!snapshot || !authored || instances.size() >= 64 || entityCount > 100000 ||
          tileCount > 100000 - entityCount ||
          authored->entities.size() > 100000 - entityCount - tileCount ||
          source.tiles.Tiles().size() >
              100000 - entityCount - tileCount - authored->entities.size())
        return fail(-6);
      const auto imported = candidate.InstantiateScene(*snapshot, request.flags != 0);
      if (!imported)
        return fail(-1);
      added = imported->scene;
      SpriteScene sprites(candidate, added);
      for (const auto &[id, sprite] : source.scene->sprites_)
        if (!sprites.Set(imported->entities.at(id), sprite))
          return fail(-1);
      for (const auto &[id, collider] : source.scene->colliders_)
        if (!sprites.SetCollider(imported->entities.at(id), collider))
          return fail(-1);
      if (!candidate.Activate(added))
        return fail(-3);
      instances.push_back({entry.id, std::move(sprites), source.tiles, imported->entities});
      if (replace || !selected)
        selected = added;
    } else if (selected == request.unload) {
      selected = instances.empty() ? 0 : instances.front().sprites.SceneId();
    }
    if (prepare && !prepare(candidate, instances))
      return fail(-3);
    if (stopped_)
      return fail(-3);
    static_assert(std::is_nothrow_move_assignable_v<runtime::World>);
    world_ = std::move(candidate);
    instances_.swap(instances);
    for (auto &instance : instances_)
      instance.sprites.world_ = &world_;
    primary_ = selected;
    ++revision_;
    status_ = {sizeof(status_), 2, request.id, request.unload ? request.unload : added, 0, 0};
    completed_ = status_;
    pending_.reset();
    return true;
  } catch (const std::overflow_error &) {
    return fail(-6);
  } catch (...) {
    return fail(-3);
  }
}
void SceneSession::ForgetEntity(runtime::Id entity) {
  if (!Owner() || processing_)
    throw std::logic_error("2D scene teardown requires serialized owner thread");
  for (auto &instance : instances_) {
    instance.sprites.Remove(entity);
    instance.sprites.RemoveCollider(entity);
  }
}
SpriteScene *SceneSession::FindEntity(runtime::Id entity) noexcept {
  if (!Owner())
    return nullptr;
  for (auto &instance : instances_) {
    const auto *scene = world_.FindScene(instance.sprites.SceneId());
    if (scene && scene->state == runtime::SceneState::Active &&
        std::ranges::any_of(scene->entities, [entity](const auto &e) { return e.id == entity; }))
      return &instance.sprites;
  }
  return nullptr;
}
Tilemap *SceneSession::PrimaryTiles() noexcept {
  if (!Owner())
    return nullptr;
  for (auto &instance : instances_)
    if (instance.sprites.SceneId() == primary_ &&
        (instance.tiles.atlas.high || instance.tiles.atlas.low))
      return &instance.tiles;
  return nullptr;
}
std::optional<std::vector<Shape>> SceneSession::CollisionShapes() const {
  if (!Owner())
    return std::nullopt;
  std::vector<Shape> result;
  for (const auto &instance : instances_) {
    auto shapes = instance.sprites.CollisionShapes();
    if (!shapes || shapes->size() > 100000 - result.size())
      return std::nullopt;
    result.insert(result.end(), shapes->begin(), shapes->end());
  }
  return result;
}
NexoraGameplaySceneApiV1 SceneSession::Api() noexcept {
  return {
      sizeof(NexoraGameplaySceneApiV1),
      1,
      this,
      [](void *p, const char *path, std::uint32_t size, std::uint64_t *high,
         std::uint64_t *low) -> int {
        auto *self = static_cast<SceneSession *>(p);
        if (!self || !self->Owner())
          return -3;
        if (!path || !high || !low || high == low || size > 1024 || !Path({path, size}))
          return -1;
        const auto found =
            std::ranges::find(self->catalog_, std::string_view(path, size), &SceneEntry::path);
        if (found == self->catalog_.end())
          return -5;
        *high = found->id.high;
        *low = found->id.low;
        return 0;
      },
      [](void *p, std::uint64_t high, std::uint64_t low, std::uint32_t mode, std::uint32_t flags,
         std::uint64_t *request) -> int {
        auto *self = static_cast<SceneSession *>(p);
        if (!self || !self->Owner())
          return -3;
        if (!request)
          return -1;
        return self->Load({high, low}, mode, flags, *request);
      },
      [](void *p, std::uint64_t scene, std::uint64_t *request) -> int {
        auto *self = static_cast<SceneSession *>(p);
        if (!self || !self->Owner())
          return -3;
        if (!request)
          return -1;
        return self->Unload(scene, *request);
      },
      [](void *p, std::uint64_t request, NexoraSceneTransitionV1 *out) -> int {
        auto *self = static_cast<SceneSession *>(p);
        if (!self || self->owner_ != std::this_thread::get_id())
          return -3;
        if (!out)
          return -1;
        if (out->struct_size < sizeof(*out))
          return -2;
        if (!request || (request != self->status_.request && request != self->completed_.request))
          return -5;
        *out = request == self->status_.request ? self->status_ : self->completed_;
        return 0;
      },
      [](void *p, std::uint64_t *scene) -> int {
        auto *self = static_cast<SceneSession *>(p);
        if (!self || !self->Owner())
          return -3;
        if (!scene)
          return -1;
        *scene = self->primary_;
        return 0;
      },
      [](void *p, std::uint64_t scene, NexoraSceneInstanceV1 *out) -> int {
        auto *self = static_cast<SceneSession *>(p);
        if (!self || !self->Owner())
          return -3;
        if (!out)
          return -1;
        if (out->struct_size < sizeof(*out))
          return -2;
        const auto found = std::ranges::find_if(
            self->instances_, [scene](const auto &i) { return i.sprites.SceneId() == scene; });
        const auto *live = self->world_.FindScene(scene);
        if (found == self->instances_.end() || !live || live->state != runtime::SceneState::Active)
          return -5;
        *out = {sizeof(*out),
                (live->persistent ? NEXORA_SCENE_PERSISTENT : 0U) |
                    (scene == self->primary_ ? NEXORA_SCENE_PRIMARY : 0U),
                scene,
                found->asset.high,
                found->asset.low,
                0};
        return 0;
      }};
}
} // namespace nexora::twod
