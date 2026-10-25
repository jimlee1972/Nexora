#include "Nexora/Runtime/GameSceneSession.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace nexora::runtime {
namespace {
constexpr std::uint32_t kQueued = 1, kApplied = 2, kFailed = 3, kCancelled = 4;
std::int32_t Code(GameSceneError error) { return static_cast<std::int32_t>(error); }
} // namespace
GameSceneSession::GameSceneSession(game::GameWorld &world,
                                   std::shared_ptr<const LoadedGameProject> project)
    : game_(world), project_(std::move(project)) {
  if (game_.InternalWorld().Kind() != WorldKind::Play || !project_)
    throw std::invalid_argument(
        "Game scene sessions require a Play World and an owning verified catalog.");
}
GameSceneSession::~GameSceneSession() { Stop(); }
bool GameSceneSession::Owner() const noexcept { return owner_ == std::this_thread::get_id(); }
void GameSceneSession::Error(std::string_view text) noexcept {
  error_.fill('\0');
  if (!foundation::IsValidUtf8(text))
    text = "Scene transition failed.";
  auto bytes = std::min(text.size(), error_.size() - 1);
  while (bytes && !foundation::IsValidUtf8(text.substr(0, bytes)))
    --bytes;
  std::copy_n(text.begin(), bytes, error_.begin());
}
std::string_view GameSceneSession::ErrorText() const noexcept { return error_.data(); }
GameSceneError GameSceneSession::RequestLoad(AssetUuid asset, GameSceneLoadMode mode,
                                             bool persistent, std::uint64_t &request) {
  if (!Owner() || stopped_)
    return GameSceneError::Lifecycle;
  if (pending_ || processing_)
    return GameSceneError::Busy;
  if (mode != GameSceneLoadMode::Replace && mode != GameSceneLoadMode::Additive)
    return GameSceneError::InvalidArgument;
  if (!project_->ResolveScene(asset))
    return GameSceneError::NotFound;
  if (mode == GameSceneLoadMode::Additive &&
      std::ranges::any_of(instances_,
                          [asset](const auto &instance) { return instance.asset == asset; }))
    return GameSceneError::InvalidArgument;
  if (next_request_ == std::numeric_limits<std::uint64_t>::max())
    return GameSceneError::Capacity;
  pending_ = Request{next_request_++, asset, 0, mode, persistent};
  status_ = {sizeof(status_), kQueued, pending_->id, 0, 0, 0};
  error_.fill('\0');
  request = pending_->id;
  return GameSceneError::None;
}
GameSceneError GameSceneSession::RequestUnload(Id scene, std::uint64_t &request) {
  if (!Owner() || stopped_)
    return GameSceneError::Lifecycle;
  if (pending_ || processing_)
    return GameSceneError::Busy;
  const auto instance = std::ranges::find(instances_, scene, &GameSceneInstance::scene);
  const auto *live = game_.InternalWorld().FindScene(scene);
  if (instance == instances_.end() || !live || live->state != SceneState::Active)
    return GameSceneError::NotFound;
  if (live->persistent || instances_.size() == 1)
    return GameSceneError::InvalidArgument;
  if (next_request_ == std::numeric_limits<std::uint64_t>::max())
    return GameSceneError::Capacity;
  pending_ = Request{next_request_++, {}, scene, GameSceneLoadMode::Replace, false};
  status_ = {sizeof(status_), kQueued, pending_->id, 0, 0, 0};
  error_.fill('\0');
  request = pending_->id;
  return GameSceneError::None;
}
Id GameSceneSession::Camera(const World &world, Id scene) const noexcept {
  if (const auto *primary = world.FindScene(scene); primary && primary->state == SceneState::Active)
    for (const auto &entity : primary->entities)
      if (entity.camera)
        return entity.id;
  return 0;
}
GameSceneView GameSceneSession::View() const {
  return {game_.InternalWorld(), instances_, primary_, Camera(game_.InternalWorld(), primary_)};
}
GameSceneStats GameSceneSession::Stats() const noexcept {
  GameSceneStats result{game_.world_.scenes_.size(),
                        instances_.size(),
                        0,
                        game_.entity_audio_.size(),
                        game_.playing_audio_.size(),
                        0,
                        0};
  for (const auto &scene : game_.world_.scenes_)
    result.stored_entities += scene.entities.size();
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  result.physics_bindings = game_.physics_entities_.size();
  result.character_bindings = game_.characters_.size();
#endif
  return result;
}
GameSceneError GameSceneSession::Status(std::uint64_t request,
                                        NexoraSceneTransitionV1 &output) const noexcept {
  if (!Owner())
    return GameSceneError::Lifecycle;
  if (!request || (request != status_.request && request != completed_.request))
    return GameSceneError::NotFound;
  if (output.struct_size < sizeof(output))
    return GameSceneError::Unsupported;
  output = request == status_.request ? status_ : completed_;
  return GameSceneError::None;
}
void GameSceneSession::Stop() noexcept {
  if (!Owner())
    return;
  stopped_ = true;
  if (processing_)
    return;
  if (pending_) {
    status_.state = kCancelled;
    status_.result = Code(GameSceneError::Lifecycle);
    completed_ = status_;
    pending_.reset();
    Error("Scene session stopped; pending request cancelled.");
  }
}
bool GameSceneSession::Process(const Prepare &prepare) {
  if (!Owner() || stopped_ || processing_)
    return false;
  if (!pending_)
    return true;
  const auto request = *pending_;
  processing_ = true;
  struct Guard {
    bool &flag;
    ~Guard() { flag = false; }
  } guard{processing_};
  const auto fail = [&](GameSceneError code, std::string_view text) {
    status_.state = stopped_ ? kCancelled : kFailed;
    status_.result = Code(code);
    completed_ = status_;
    pending_.reset();
    Error(text);
    return false;
  };
  try {
    auto &live = game_.InternalWorld();
    for (const auto &instance : instances_) {
      const auto *scene = live.FindScene(instance.scene);
      if (!scene || scene->state != SceneState::Active)
        return fail(GameSceneError::Lifecycle, "A session scene was changed outside its owner.");
    }
    World candidate = live;
    std::vector<Id> retired;
    const bool replace = !request.unload && request.mode == GameSceneLoadMode::Replace;
    std::erase_if(candidate.scenes_, [&](const Scene &scene) {
      const bool remove = scene.state == SceneState::Unloaded ||
                          scene.state == SceneState::Unloading || scene.id == request.unload ||
                          (replace && !scene.persistent);
      if (remove)
        for (const auto &entity : scene.entities)
          retired.push_back(entity.id);
      return remove;
    });
    auto instances = instances_;
    std::erase_if(instances,
                  [&](const auto &instance) { return !candidate.FindScene(instance.scene); });
    Id selected = primary_, added = 0;
    if (!request.unload) {
      const auto source = project_->ResolveScene(request.asset);
      const auto *scene = source->WorldView().FindScene(source->SceneId());
      std::size_t entity_count = scene->entities.size();
      for (const auto &retained : candidate.scenes_) {
        if (retained.entities.size() > kCookedSceneMaximumEntities - entity_count)
          return fail(GameSceneError::Capacity, "Game scene live entity budget exceeded.");
        entity_count += retained.entities.size();
      }
      if (candidate.scenes_.size() >= kMaximumGameScenes)
        return fail(GameSceneError::Capacity, "Game scene instance budget exceeded.");
      added = candidate.LoadScene(scene->name, request.persistent);
      candidate.scenes_.back().entities.reserve(scene->entities.size());
      GameSceneInstance instance{added, request.asset, source, {}};
      instance.entities.reserve(scene->entities.size());
      for (const auto &entity : scene->entities) {
        auto &created = candidate.CreateEntity(added);
        const auto id = created.id;
        created = entity;
        created.id = id;
        created.parent = 0;
        instance.entities.emplace(entity.id, id);
      }
      auto &entities = candidate.scenes_.back().entities;
      for (std::size_t i = 0; i < entities.size(); ++i)
        if (scene->entities[i].parent)
          entities[i].parent = instance.entities.at(scene->entities[i].parent);
      if (!candidate.Activate(added))
        return fail(GameSceneError::Lifecycle, "Prepared scene could not be activated.");
      instances.push_back(std::move(instance));
      if (replace || !selected)
        selected = added;
    } else if (selected == request.unload) {
      selected = instances.empty() ? 0 : instances.front().scene;
    }
    const GameSceneView view{candidate, instances, selected, Camera(candidate, selected)};
    std::unique_ptr<GameScenePresentation> presentation;
    if (prepare) {
      std::string error;
      presentation = prepare(view, error);
      if (!presentation)
        return fail(GameSceneError::Lifecycle,
                    error.empty() ? "Scene presentation preparation failed." : error);
    }
    if (stopped_)
      return fail(GameSceneError::Lifecycle, "Scene session stopped before publication.");
    // All allocations, validation and native preparation precede this no-throw publication block.
    static_assert(std::is_nothrow_move_assignable_v<World>);
    for (const auto entity : retired)
      game_.RemoveBindings(entity);
    live = std::move(candidate);
    instances_.swap(instances);
    primary_ = selected;
    if (presentation)
      presentation->Commit();
    status_ = {
        sizeof(status_), kApplied, request.id, request.unload ? request.unload : added, 0, 0};
    completed_ = status_;
    pending_.reset();
    error_.fill('\0');
    return true;
  } catch (const std::overflow_error &error) {
    return fail(GameSceneError::Capacity, error.what());
  } catch (const std::exception &error) {
    return fail(GameSceneError::Lifecycle, error.what());
  } catch (...) {
    return fail(GameSceneError::Lifecycle, "Scene preparation raised an unknown exception.");
  }
}

NexoraGameplaySceneApiV1 GameSceneSession::Api() noexcept {
  return {
      sizeof(NexoraGameplaySceneApiV1),
      1,
      this,
      [](void *context, const char *path, std::uint32_t length, std::uint64_t *high,
         std::uint64_t *low) -> std::int32_t {
        auto *self = static_cast<GameSceneSession *>(context);
        if (!self || !self->Owner())
          return Code(GameSceneError::Lifecycle);
        if (!path || !high || !low || high == low || !length || length > kMaximumGameScenePathBytes)
          return Code(GameSceneError::InvalidArgument);
        const std::string_view name(path, length);
        if (!foundation::IsValidUtf8(name) || name.find('\0') != name.npos)
          return Code(GameSceneError::InvalidArgument);
        const auto found = std::ranges::find(self->project_->Scenes(), name, &GameSceneEntry::path);
        if (found == self->project_->Scenes().end())
          return Code(GameSceneError::NotFound);
        *high = found->id.high;
        *low = found->id.low;
        return 0;
      },
      [](void *context, std::uint64_t high, std::uint64_t low, std::uint32_t mode,
         std::uint32_t flags, std::uint64_t *request) -> std::int32_t {
        auto *self = static_cast<GameSceneSession *>(context);
        if (!self || !self->Owner())
          return Code(GameSceneError::Lifecycle);
        if (!request || mode > 1 || flags > 1)
          return Code(GameSceneError::InvalidArgument);
        try {
          return Code(self->RequestLoad({high, low}, static_cast<GameSceneLoadMode>(mode),
                                        flags != 0, *request));
        } catch (...) {
          return Code(GameSceneError::Lifecycle);
        }
      },
      [](void *context, std::uint64_t scene, std::uint64_t *request) -> std::int32_t {
        auto *self = static_cast<GameSceneSession *>(context);
        if (!self || !self->Owner())
          return Code(GameSceneError::Lifecycle);
        if (!request)
          return Code(GameSceneError::InvalidArgument);
        try {
          return Code(self->RequestUnload(scene, *request));
        } catch (...) {
          return Code(GameSceneError::Lifecycle);
        }
      },
      [](void *context, std::uint64_t request, NexoraSceneTransitionV1 *status) -> std::int32_t {
        auto *self = static_cast<GameSceneSession *>(context);
        if (!self || !self->Owner())
          return Code(GameSceneError::Lifecycle);
        if (!status)
          return Code(GameSceneError::InvalidArgument);
        return Code(self->Status(request, *status));
      },
      [](void *context, std::uint64_t *scene) -> std::int32_t {
        auto *self = static_cast<GameSceneSession *>(context);
        if (!self || !self->Owner())
          return Code(GameSceneError::Lifecycle);
        if (!scene)
          return Code(GameSceneError::InvalidArgument);
        *scene = self->primary_;
        return 0;
      },
      [](void *context, std::uint64_t scene, NexoraSceneInstanceV1 *info) -> std::int32_t {
        auto *self = static_cast<GameSceneSession *>(context);
        if (!self || !self->Owner())
          return Code(GameSceneError::Lifecycle);
        if (!info)
          return Code(GameSceneError::InvalidArgument);
        if (info->struct_size < sizeof(*info))
          return Code(GameSceneError::Unsupported);
        const auto found = std::ranges::find(self->instances_, scene, &GameSceneInstance::scene);
        const auto *live = self->game_.InternalWorld().FindScene(scene);
        if (found == self->instances_.end() || !live || live->state != SceneState::Active)
          return Code(GameSceneError::NotFound);
        *info = {sizeof(*info),
                 (live->persistent ? NEXORA_SCENE_PERSISTENT : 0U) |
                     (self->primary_ == scene ? NEXORA_SCENE_PRIMARY : 0U),
                 scene,
                 found->asset.high,
                 found->asset.low,
                 self->primary_ == scene ? self->Camera(self->game_.InternalWorld(), scene) : 0};
        return 0;
      }};
}

std::optional<std::vector<StaticRenderItem>> CaptureGameSceneRenderItems(const GameSceneView &view,
                                                                         std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) -> std::optional<std::vector<StaticRenderItem>> {
    if (error)
      *error = message;
    return std::nullopt;
  };
  std::vector<StaticRenderItem> result;
  for (const auto &instance : view.instances) {
    const auto *scene = view.world.FindScene(instance.scene);
    const auto poses = view.world.SceneWorldPoses(instance.scene);
    if (!scene || scene->state != SceneState::Active || !poses)
      return fail("Live scene/render pose is unavailable.");
    std::unordered_map<Id, const Entity *> entities;
    std::unordered_map<Id, TransformMatrix> matrices;
    std::unordered_map<Id, std::uint64_t> resources;
    for (const auto &entity : scene->entities)
      entities.emplace(entity.id, &entity);
    for (const auto &pose : *poses)
      matrices.emplace(pose.id, pose.matrix);
    for (const auto &binding : instance.source->SceneData().bindings)
      resources.emplace(binding.entity, binding.mesh_resource);
    for (const auto &source : instance.source->RenderItems()) {
      const auto remapped = instance.entities.find(source.entity);
      if (remapped == instance.entities.end())
        return fail("Live scene entity mapping is incomplete.");
      const auto entity = entities.find(remapped->second);
      if (entity == entities.end() || !entity->second->mesh_renderer)
        continue; // Runtime destruction/removal is allowed; source templates remain unchanged.
      if (entity->second->mesh_data.mesh != resources.at(source.entity))
        return fail("Runtime mesh replacement requires an unsupported resource binding.");
      auto item = source;
      item.entity = remapped->second;
      item.world_transform = matrices.at(item.entity);
      result.push_back(std::move(item));
    }
  }
  return result;
}
} // namespace nexora::runtime
