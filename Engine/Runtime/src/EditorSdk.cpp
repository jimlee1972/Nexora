#include "Nexora/Runtime/EditorSdk.h"

#include "Nexora/Foundation/PluginAbi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_set>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace nexora::runtime {
namespace {

template <typename Function, typename Pointer> Function FunctionCast(Pointer pointer) {
  static_assert(sizeof(Function) == sizeof(Pointer));
  Function function{};
  std::memcpy(&function, &pointer, sizeof(function));
  return function;
}

#if defined(_WIN32)
void *OpenLibrary(const std::string &path) {
  return FunctionCast<void *>(LoadLibraryA(path.c_str()));
}
void CloseLibrary(void *handle) { FreeLibrary(FunctionCast<HMODULE>(handle)); }
void *ResolveSymbol(void *handle, const char *name) {
  return FunctionCast<void *>(GetProcAddress(FunctionCast<HMODULE>(handle), name));
}
#else
void *OpenLibrary(const std::string &path) { return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL); }
void CloseLibrary(void *handle) { dlclose(handle); }
void *ResolveSymbol(void *handle, const char *name) { return dlsym(handle, name); }
#endif

void RegisterServiceTrampoline(void *context, const char *name, void *service) {
  static_cast<ServiceRegistry *>(context)->Register(name, service);
}

template <typename Node> Node *FindNodeImpl(Node &node, std::string_view path) {
  if (path.empty())
    return &node;
  const auto slash = path.find('/');
  const auto head = path.substr(0, slash);
  const auto rest = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
  for (auto &child : node.children)
    if (child.name == head)
      return FindNodeImpl(child, rest);
  return nullptr;
}

} // namespace

TypeId HashTypeName(std::string_view name) noexcept {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char c : name) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool ReflectionRegistry::Register(TypeDescriptor descriptor) {
  if (descriptor.name.empty() || by_name_.contains(descriptor.name))
    return false;
  if (descriptor.id == 0)
    descriptor.id = HashTypeName(descriptor.name);
  if (by_id_.contains(descriptor.id))
    return false;
  by_name_.emplace(descriptor.name, descriptor.id);
  by_id_.emplace(descriptor.id, std::move(descriptor));
  return true;
}
const TypeDescriptor *ReflectionRegistry::Find(std::string_view name) const {
  const auto found = by_name_.find(std::string(name));
  return found == by_name_.end() ? nullptr : FindById(found->second);
}
const TypeDescriptor *ReflectionRegistry::FindById(TypeId id) const {
  const auto found = by_id_.find(id);
  return found == by_id_.end() ? nullptr : &found->second;
}

PluginHost::~PluginHost() { UnloadAll(); }

PluginLoadResult PluginHost::Load(const std::string &library_path, ServiceRegistry *services) {
  void *handle = OpenLibrary(library_path);
  if (!handle)
    return {false, PluginLoadError::OpenFailed, 0, false};
  void *abi_symbol = ResolveSymbol(handle, "NexoraPluginAbiVersion");
  if (!abi_symbol) {
    CloseLibrary(handle);
    return {false, PluginLoadError::MissingAbiSymbol, 0, false};
  }
  using AbiVersionFn = std::uint32_t (*)();
  const auto reported = FunctionCast<AbiVersionFn>(abi_symbol)();
  if (reported != engine_abi_) {
    CloseLibrary(handle);
    return {false, PluginLoadError::AbiMismatch, reported, false};
  }
  bool registered = false;
  if (services != nullptr) {
    if (void *register_symbol = ResolveSymbol(handle, "NexoraPluginRegister")) {
      FunctionCast<NexoraPluginRegisterFn>(register_symbol)(services, &RegisterServiceTrampoline);
      registered = true;
    }
  }
  handles_.push_back(handle);
  return {true, PluginLoadError::None, reported, registered};
}
void PluginHost::UnloadAll() noexcept {
  for (auto *handle : handles_)
    CloseLibrary(handle);
  handles_.clear();
}

bool ServiceRegistry::Register(std::string name, void *service) {
  if (name.empty() || service == nullptr)
    return false;
  return services_.emplace(std::move(name), service).second;
}
bool ServiceRegistry::Unregister(std::string_view name) {
  return services_.erase(std::string(name)) != 0;
}
void *ServiceRegistry::Find(std::string_view name) const {
  const auto found = services_.find(std::string(name));
  return found == services_.end() ? nullptr : found->second;
}

Id SceneEditor::CreateEntity(Id scene, Id parent) {
  const auto *target_scene = world_.FindScene(scene);
  if (target_scene == nullptr ||
      (parent != 0 && std::ranges::find(target_scene->entities, parent, &Entity::id) ==
                          target_scene->entities.end()))
    return 0;
  const auto id = world_.CreateEntity(scene).id;
  if (parent != 0) {
    WorldCommandBuffer attach;
    attach.SetParent(id, parent, false);
    if (!attach.Apply(world_)) {
      WorldCommandBuffer discard;
      discard.DestroyEntity(id);
      static_cast<void>(discard.Apply(world_));
      return 0;
    }
  }
  const Entity created = *world_.FindEntity(id);
  undo_.Record(
      [this, id] {
        WorldCommandBuffer commands;
        commands.DestroyEntity(id);
        return commands.Apply(world_);
      },
      [this, scene, created] {
        auto *target = const_cast<Scene *>(world_.FindScene(scene));
        if (target == nullptr || target->state == SceneState::Unloading ||
            target->state == SceneState::Unloaded || world_.FindEntity(created.id) != nullptr ||
            (created.parent != 0 && std::ranges::find(target->entities, created.parent,
                                                      &Entity::id) == target->entities.end()))
          return false;
        target->entities.push_back(created);
        world_.next_id_ = std::max(world_.next_id_, created.id + 1);
        return true;
      });
  ++depth_;
  return id;
}
bool SceneEditor::SetTransform(Id entity, Transform transform) {
  const std::array entities{entity};
  const std::array transforms{transform};
  return SetTransforms(entities, transforms);
}
bool SceneEditor::ApplyComponentEdit(WorldCommandBuffer apply, WorldCommandBuffer restore) {
  // Apply consumes the buffer. Keep owning immutable templates and replay a fresh copy each time.
  auto initial = apply;
  if (!initial.Apply(world_))
    return false;
  undo_.Record(
      [this, restore = std::move(restore)] {
        auto commands = restore;
        return commands.Apply(world_);
      },
      [this, apply = std::move(apply)] {
        auto commands = apply;
        return commands.Apply(world_);
      });
  ++depth_;
  return true;
}
bool SceneEditor::SetCamera(Id entity, std::optional<CameraComponent> camera) {
  return SetCameras(std::array{entity}, std::array{camera});
}
bool SceneEditor::SetCameras(std::span<const Id> entities,
                             std::span<const std::optional<CameraComponent>> cameras) {
  if (entities.empty() || entities.size() != cameras.size())
    return false;
  WorldCommandBuffer apply, restore;
  std::unordered_set<Id> unique;
  for (std::size_t i = 0; i < entities.size(); ++i) {
    const auto *existing = world_.FindEntity(entities[i]);
    const auto &camera = cameras[i];
    if (!existing || !unique.insert(entities[i]).second ||
        (camera &&
         (!std::isfinite(camera->vertical_field_of_view) || !std::isfinite(camera->near_plane) ||
          !std::isfinite(camera->far_plane) || camera->vertical_field_of_view <= 0.0 ||
          camera->vertical_field_of_view >= 180.0 || camera->near_plane <= 0.0 ||
          camera->far_plane <= camera->near_plane)))
      return false;
    apply.SetCamera(entities[i], camera);
    restore.SetCamera(entities[i],
                      existing->camera ? std::optional(existing->camera_data) : std::nullopt);
  }
  return ApplyComponentEdit(std::move(apply), std::move(restore));
}
bool SceneEditor::SetLight(Id entity, std::optional<LightComponent> light) {
  return SetLights(std::array{entity}, std::array{light});
}
bool SceneEditor::SetLights(std::span<const Id> entities,
                            std::span<const std::optional<LightComponent>> lights) {
  if (entities.empty() || entities.size() != lights.size())
    return false;
  WorldCommandBuffer apply, restore;
  std::unordered_set<Id> unique;
  for (std::size_t i = 0; i < entities.size(); ++i) {
    const auto *existing = world_.FindEntity(entities[i]);
    const auto &light = lights[i];
    if (!existing || !unique.insert(entities[i]).second ||
        (light && (!std::isfinite(light->intensity) || light->intensity < 0.0F)))
      return false;
    apply.SetLight(entities[i], light);
    restore.SetLight(entities[i],
                     existing->light ? std::optional(existing->light_data) : std::nullopt);
  }
  return ApplyComponentEdit(std::move(apply), std::move(restore));
}
bool SceneEditor::SetMeshRenderer(Id entity, std::optional<MeshComponent> mesh) {
  const auto *existing = world_.FindEntity(entity);
  if (!existing)
    return false;
  const std::optional<MeshComponent> previous =
      existing->mesh_renderer ? std::optional(existing->mesh_data) : std::nullopt;
  WorldCommandBuffer apply;
  apply.SetMeshRenderer(entity, mesh);
  if (!apply.Apply(world_))
    return false;
  undo_.Record(
      [this, entity, previous] {
        WorldCommandBuffer commands;
        commands.SetMeshRenderer(entity, previous);
        return commands.Apply(world_);
      },
      [this, entity, mesh] {
        WorldCommandBuffer commands;
        commands.SetMeshRenderer(entity, mesh);
        return commands.Apply(world_);
      });
  ++depth_;
  return true;
}
bool SceneEditor::SetTransforms(std::span<const Id> entities,
                                std::span<const Transform> transforms) {
  if (entities.empty() || entities.size() != transforms.size())
    return false;
  std::vector<Transform> previous;
  previous.reserve(entities.size());
  std::unordered_set<Id> unique;
  WorldCommandBuffer apply;
  for (std::size_t index = 0; index < entities.size(); ++index) {
    const auto *existing = world_.FindEntity(entities[index]);
    if (!existing || !unique.insert(entities[index]).second)
      return false;
    previous.push_back(existing->transform);
    apply.SetTransform(entities[index], transforms[index]);
  }
  if (!apply.Apply(world_))
    return false;
  const std::vector<Id> owned_entities(entities.begin(), entities.end());
  const std::vector<Transform> next(transforms.begin(), transforms.end());
  undo_.Record(
      [this, owned_entities, previous] {
        WorldCommandBuffer commands;
        for (std::size_t index = 0; index < owned_entities.size(); ++index)
          commands.SetTransform(owned_entities[index], previous[index]);
        return commands.Apply(world_);
      },
      [this, owned_entities, next] {
        WorldCommandBuffer commands;
        for (std::size_t index = 0; index < owned_entities.size(); ++index)
          commands.SetTransform(owned_entities[index], next[index]);
        return commands.Apply(world_);
      });
  ++depth_;
  return true;
}
bool SceneEditor::SetParent(Id entity, Id parent, bool keep_world) {
  WorldCommandBuffer apply;
  apply.SetParent(entity, parent, keep_world);
  return ApplyHierarchyEdit(entity, apply);
}
bool SceneEditor::SetSiblingIndex(Id entity, std::size_t index) {
  WorldCommandBuffer apply;
  apply.SetSiblingIndex(entity, index);
  return ApplyHierarchyEdit(entity, apply);
}
bool SceneEditor::Move(Id entity, Id parent, std::size_t index, bool keep_world) {
  WorldCommandBuffer apply;
  // Reordering within the same parent needs no reparent, which could fail on a pose that cannot
  // be re-expressed even though the order change is always possible.
  if (world_.Parent(entity) != parent)
    apply.SetParent(entity, parent, keep_world);
  apply.SetSiblingIndex(entity, index);
  return ApplyHierarchyEdit(entity, apply);
}
bool SceneEditor::ApplyHierarchyEdit(Id entity, WorldCommandBuffer &apply) {
  const auto *existing = world_.FindEntity(entity);
  if (!existing)
    return false;
  const auto previous_parent = existing->parent;
  const auto previous_transform = existing->transform;
  const auto previous_index = *world_.SiblingIndex(entity);
  if (!apply.Apply(world_))
    return false;
  const auto *changed = world_.FindEntity(entity);
  const auto next_parent = changed->parent;
  const auto next_transform = changed->transform;
  const auto next_index = *world_.SiblingIndex(entity);
  const auto place = [this, entity](Id parent, Transform transform, std::size_t index) {
    WorldCommandBuffer commands;
    commands.SetParent(entity, parent, false);
    commands.SetTransform(entity, transform);
    commands.SetSiblingIndex(entity, index);
    return commands.Apply(world_);
  };
  undo_.Record(
      [place, previous_parent, previous_transform, previous_index] {
        return place(previous_parent, previous_transform, previous_index);
      },
      [place, next_parent, next_transform, next_index] {
        return place(next_parent, next_transform, next_index);
      });
  ++depth_;
  return true;
}
bool SceneEditor::DestroyEntity(Id scene, Id entity) {
  const auto *target_scene = world_.FindScene(scene);
  const auto *existing = world_.FindEntity(entity);
  if (!target_scene || !existing ||
      std::ranges::find(target_scene->entities, entity, &Entity::id) ==
          target_scene->entities.end())
    return false;
  // Destruction cascades to descendants, so undo must restore the whole subtree.
  std::vector<Entity> subtree;
  for (const auto id : world_.Subtree(entity))
    subtree.push_back(*world_.FindEntity(id));
  const auto root_index = *world_.SiblingIndex(entity);
  // If the subtree root's parent is gone by the time this is undone, the root comes back as a root
  // at the world pose it had, rather than under a dangling parent.
  const auto root_world = world_.WorldTransform(entity).value_or(existing->transform);
  WorldCommandBuffer apply;
  apply.DestroyEntity(entity);
  if (!apply.Apply(world_))
    return false;
  undo_.Record(
      [this, scene, subtree, root_world, root_index] {
        auto *target = const_cast<Scene *>(world_.FindScene(scene));
        if (target == nullptr || target->state == SceneState::Unloading ||
            target->state == SceneState::Unloaded ||
            std::ranges::any_of(subtree, [this](const Entity &restored) {
              return world_.FindEntity(restored.id) != nullptr;
            }))
          return false;
        const auto parent = subtree.front().parent;
        const bool orphaned =
            parent != 0 &&
            std::ranges::find(target->entities, parent, &Entity::id) == target->entities.end();
        for (const auto &restored : subtree) {
          target->entities.push_back(restored);
          if (orphaned && restored.id == subtree.front().id) {
            target->entities.back().parent = 0;
            if (const auto normalized = NormalizedTransform(root_world))
              target->entities.back().transform = *normalized;
          }
          world_.next_id_ = std::max(world_.next_id_, restored.id + 1);
        }
        // Back to the sibling position it had (it was appended last).
        WorldCommandBuffer place;
        place.SetSiblingIndex(subtree.front().id, root_index);
        return place.Apply(world_);
      },
      [this, scene, entity] {
        const auto *target = world_.FindScene(scene);
        if (target == nullptr ||
            std::ranges::find(target->entities, entity, &Entity::id) == target->entities.end())
          return false;
        WorldCommandBuffer commands;
        commands.DestroyEntity(entity);
        return commands.Apply(world_);
      });
  ++depth_;
  return true;
}
bool SceneEditor::Undo() {
  if (!undo_.Undo())
    return false;
  --depth_;
  return true;
}
bool SceneEditor::Redo() {
  if (!undo_.Redo())
    return false;
  ++depth_;
  return true;
}
void SceneEditor::ClearUndo() noexcept {
  undo_ = {};
  depth_ = 0;
}

bool RuntimeConsole::Push(RuntimeLogRecord record) {
  std::scoped_lock lock(mutex_);
  if (capacity_ == 0) {
    ++dropped_;
    return false;
  }
  record.sequence = next_sequence_++;
  if (records_.size() == capacity_) {
    records_.erase(records_.begin());
    ++dropped_;
  }
  records_.push_back(std::move(record));
  return true;
}

std::vector<RuntimeLogRecord> RuntimeConsole::Snapshot() const {
  std::scoped_lock lock(mutex_);
  return records_;
}

std::uint64_t RuntimeConsole::DroppedCount() const {
  std::scoped_lock lock(mutex_);
  return dropped_;
}

bool PlaySession::Start(double fixed_delta_seconds, FixedUpdate fixed_update) {
  if (state_ != PlayState::Stopped || !std::isfinite(fixed_delta_seconds) ||
      fixed_delta_seconds <= 0.0 || !fixed_update)
    return false;
  play_world_.emplace(editor_world_.CloneForPlay());
  fixed_update_ = std::move(fixed_update);
  fixed_delta_seconds_ = fixed_delta_seconds;
  state_ = PlayState::Playing;
  input_focused_ = false;
  stats_ = {};
  source_transforms_.clear();
  source_parents_.clear();
  for (const auto &scene : editor_world_.scenes_)
    for (const auto &entity : scene.entities) {
      source_transforms_.emplace(entity.id, entity.transform);
      source_parents_.emplace(entity.id, entity.parent);
    }
  pause_reason_ = PauseReason::None;
  apply_status_ = ApplyBackStatus::Discarded;
  if (++generation_ == 0)
    ++generation_;
  return true;
}

bool PlaySession::Pause() noexcept {
  if (state_ != PlayState::Playing)
    return false;
  state_ = PlayState::Paused;
  pause_reason_ = PauseReason::User;
  return true;
}

bool PlaySession::ReportRuntimeFailure() noexcept {
  if (!play_world_ || state_ == PlayState::Stopped)
    return false;
  state_ = PlayState::Paused;
  input_focused_ = false;
  pause_reason_ = PauseReason::RuntimeFailure;
  ++stats_.crashes;
  return true;
}

bool PlaySession::Resume() noexcept {
  if (state_ != PlayState::Paused)
    return false;
  state_ = PlayState::Playing;
  pause_reason_ = PauseReason::None;
  return true;
}

bool PlaySession::ExecuteFixedTick(bool manual) {
  if (!play_world_ || !fixed_update_)
    return false;
  if (!fixed_update_(*play_world_, fixed_delta_seconds_)) {
    static_cast<void>(ReportRuntimeFailure());
    return false;
  }
  play_world_->EndFrame();
  ++stats_.fixed_ticks;
  stats_.manual_steps += static_cast<std::uint64_t>(manual);
  if (manual)
    pause_reason_ = PauseReason::StepComplete;
  return true;
}

bool PlaySession::Tick() { return state_ == PlayState::Playing && ExecuteFixedTick(false); }

bool PlaySession::Step() { return state_ == PlayState::Paused && ExecuteFixedTick(true); }

bool PlaySession::Stop(ApplyBackPolicy policy) {
  if (state_ == PlayState::Stopped || !play_world_)
    return false;
  bool applied = true;
  apply_status_ =
      policy == ApplyBackPolicy::Discard ? ApplyBackStatus::Discarded : ApplyBackStatus::Applied;
  if (policy == ApplyBackPolicy::Transforms) {
    WorldCommandBuffer commands;
    const auto diffs = PreviewTransformApplyBack();
    if (std::ranges::any_of(diffs, &TransformApplyDiff::conflict)) {
      applied = false;
      apply_status_ = ApplyBackStatus::Conflict;
    } else {
      for (const auto &diff : diffs) {
        commands.SetTransform(diff.entity, diff.runtime);
        ++stats_.applied_transforms;
      }
      if (commands.Size() != 0 && !commands.Apply(editor_world_)) {
        applied = false;
        apply_status_ = ApplyBackStatus::Failed;
      }
    }
  }
  play_world_.reset();
  fixed_update_ = {};
  fixed_delta_seconds_ = 0.0;
  state_ = PlayState::Stopped;
  input_focused_ = false;
  source_transforms_.clear();
  source_parents_.clear();
  pause_reason_ = PauseReason::None;
  return applied;
}

std::vector<TransformApplyDiff> PlaySession::PreviewTransformApplyBack() const {
  std::vector<TransformApplyDiff> diffs;
  if (!play_world_)
    return diffs;
  for (const auto &scene : play_world_->scenes_)
    for (const auto &entity : scene.entities) {
      const auto original = source_transforms_.find(entity.id);
      if (original == source_transforms_.end())
        continue;
      const auto original_parent = source_parents_.at(entity.id);
      const bool reparented = entity.parent != original_parent;
      if (!reparented && entity.transform == original->second)
        continue;
      const auto *editor = editor_world_.FindEntity(entity.id);
      // Transforms are local, so a value from under a different parent would mean a different pose
      // in the editor world. Apply-back copies transforms only, so an entity reparented in either
      // world is a conflict rather than something to move, even when its local values are
      // unchanged.
      diffs.push_back({entity.id, original->second, editor ? editor->transform : Transform{},
                       entity.transform, editor != nullptr,
                       editor == nullptr || editor->transform != original->second ||
                           editor->parent != original_parent || reparented});
    }
  std::ranges::sort(diffs, {}, &TransformApplyDiff::entity);
  return diffs;
}

RuntimeInspectionSnapshot PlaySession::Inspect() const {
  RuntimeInspectionSnapshot snapshot{stats_.fixed_ticks, {}};
  if (!play_world_)
    return snapshot;
  for (const auto &scene : play_world_->scenes_)
    for (const auto &entity : scene.entities)
      snapshot.entities.push_back(
          {entity.id, scene.id, entity.transform, entity.camera, entity.light, entity.mesh_renderer,
           play_world_->WorldTransform(entity.id).value_or(entity.transform), entity.parent,
           scene.state, entity.camera ? std::optional{entity.camera_data} : std::nullopt,
           entity.light ? std::optional{entity.light_data} : std::nullopt,
           entity.mesh_renderer ? std::optional{entity.mesh_data} : std::nullopt});
  std::ranges::sort(snapshot.entities, {}, &RuntimeEntitySnapshot::id);
  return snapshot;
}

bool PlaySession::PollDebugger(DebuggerAdapter &debugger) {
  if (state_ == PlayState::Stopped)
    return false;
  const auto snapshot = debugger.Poll();
  if (snapshot.state != DebuggerState::Paused)
    return false;
  state_ = PlayState::Paused;
  input_focused_ = false;
  pause_reason_ = PauseReason::DebuggerBreak;
  return true;
}

const PrefabNode *Prefab::Find(std::string_view path) const { return FindNodeImpl(root_, path); }

bool PrefabInstance::SetOverride(std::string path, std::string key, std::string value) {
  if (!prefab_ || !prefab_->Find(path))
    return false;
  for (auto &existing : overrides_)
    if (existing.path == path && existing.key == key) {
      existing.value = std::move(value);
      return true;
    }
  overrides_.push_back({std::move(path), std::move(key), std::move(value)});
  return true;
}
std::optional<std::string> PrefabInstance::Resolve(std::string_view path,
                                                   std::string_view key) const {
  for (const auto &override_entry : overrides_)
    if (override_entry.path == path && override_entry.key == key)
      return override_entry.value;
  const auto *node = prefab_ ? prefab_->Find(path) : nullptr;
  if (!node)
    return std::nullopt;
  for (const auto &property : node->properties)
    if (property.key == key)
      return property.value;
  return std::nullopt;
}
bool PrefabInstance::Rebase(std::shared_ptr<const Prefab> new_prefab) {
  if (!new_prefab)
    return false;
  prefab_ = std::move(new_prefab);
  overrides_.erase(std::remove_if(overrides_.begin(), overrides_.end(),
                                  [this](const auto &override_entry) {
                                    return prefab_->Find(override_entry.path) == nullptr;
                                  }),
                   overrides_.end());
  return true;
}

bool PrefabInstance::RevertOverride(std::string_view path, std::string_view key) {
  const auto found = std::ranges::find_if(overrides_, [path, key](const auto &entry) {
    return entry.path == path && entry.key == key;
  });
  if (found == overrides_.end())
    return false;
  overrides_.erase(found);
  return true;
}

std::shared_ptr<const Prefab> PrefabInstance::ApplyOverrides() {
  if (!prefab_)
    return nullptr;
  PrefabNode root = prefab_->Root();
  for (const auto &override_entry : overrides_) {
    auto *node = FindNodeImpl(root, std::string_view(override_entry.path));
    if (!node)
      continue;
    const auto property =
        std::ranges::find(node->properties, override_entry.key, &PrefabProperty::key);
    if (property == node->properties.end())
      node->properties.push_back({override_entry.key, override_entry.value});
    else
      property->value = override_entry.value;
  }
  prefab_ = std::make_shared<Prefab>(std::move(root));
  overrides_.clear();
  return prefab_;
}

std::shared_ptr<Prefab> PrefabVariant::Bake() const {
  if (!base_)
    return nullptr;
  PrefabNode root = base_->Root();
  for (const auto &override_entry : overrides_) {
    auto *node = FindNodeImpl(root, std::string_view(override_entry.path));
    if (!node)
      continue;
    bool replaced = false;
    for (auto &property : node->properties)
      if (property.key == override_entry.key) {
        property.value = override_entry.value;
        replaced = true;
        break;
      }
    if (!replaced)
      node->properties.push_back({override_entry.key, override_entry.value});
  }
  return std::make_shared<Prefab>(std::move(root));
}

void ProjectSettings::SetString(std::string key, std::string value) {
  values_.insert_or_assign(std::move(key), std::move(value));
}
std::optional<std::string> ProjectSettings::GetString(std::string_view key) const {
  const auto found = values_.find(std::string(key));
  return found == values_.end() ? std::nullopt : std::optional<std::string>(found->second);
}
std::string ProjectSettings::GetStringOr(std::string_view key, std::string fallback) const {
  const auto found = values_.find(std::string(key));
  return found == values_.end() ? std::move(fallback) : found->second;
}

bool EditorSdkEnabled() noexcept {
#if NEXORA_EDITOR_SDK_ENABLED
  return true;
#else
  return false;
#endif
}

} // namespace nexora::runtime
