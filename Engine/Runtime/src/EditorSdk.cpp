#include "Nexora/Runtime/EditorSdk.h"

#include "Nexora/Foundation/PluginAbi.h"
#include "Nexora/Foundation/Types.h"

#include "RuntimeConsoleAdmission.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
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

std::unordered_set<Id> SelectedSubtrees(const Scene &scene, const std::unordered_set<Id> &roots) {
  std::unordered_map<Id, std::vector<Id>> children;
  for (const auto &entity : scene.entities)
    if (entity.parent != 0)
      children[entity.parent].push_back(entity.id);
  auto selected = roots;
  std::vector<Id> pending(roots.begin(), roots.end());
  for (std::size_t index = 0; index < pending.size(); ++index)
    if (const auto found = children.find(pending[index]); found != children.end())
      for (const auto child : found->second)
        if (selected.insert(child).second)
          pending.push_back(child);
  return selected;
}

#if defined(_WIN32)
void *OpenLibrary(const std::string &path) {
  try {
    const auto native =
        std::filesystem::absolute(std::filesystem::path{std::u8string(path.begin(), path.end())});
    return FunctionCast<void *>(LoadLibraryW(native.c_str()));
  } catch (...) {
    return nullptr;
  }
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

PluginHost::~PluginHost() {
  UnloadAll();
  PollShutdown();
  // Never force-unload legacy or non-quiescent native code. Registration contexts are call-scoped,
  // so release the revoked provider normally; the OS loader retains each unclosed native mapping.
  for (auto &entry : entries_)
    if (entry.handle) {
      entry.provider->active = entry.provider->publishing = false;
      entry.provider->registering_registry = nullptr;
    }
}

PluginLoadResult PluginHost::Load(const std::string &library_path, ServiceRegistry *services) {
  if (library_path.empty() || library_path.size() > kMaximumPathBytes ||
      library_path.find('\0') != std::string::npos)
    return {false, PluginLoadError::InvalidPath};
  if (entries_.size() == kMaximumPlugins || next_id_ == std::numeric_limits<std::uint64_t>::max())
    return {false, PluginLoadError::BudgetExceeded};
  Entry candidate;
  candidate.observation.library_path = library_path;
  candidate.observation.id = next_id_;
  candidate.provider = std::make_shared<ServiceRegistry::Provider>();
  candidate.provider->self = candidate.provider;
  void *handle = OpenLibrary(library_path);
  if (!handle)
    return {false, PluginLoadError::OpenFailed, 0, false};
  struct Guard final {
    void *handle;
    ~Guard() {
      if (handle)
        CloseLibrary(handle);
    }
  } guard{handle};
  void *abi_symbol = ResolveSymbol(handle, "NexoraPluginAbiVersion");
  if (!abi_symbol)
    return {false, PluginLoadError::MissingAbiSymbol, 0, false};
  using AbiVersionFn = std::uint32_t (*)();
  std::uint32_t reported{};
  try {
    reported = FunctionCast<AbiVersionFn>(abi_symbol)();
  } catch (...) {
    return {false, PluginLoadError::InspectionFailed};
  }
  if (reported != engine_abi_)
    return {false, PluginLoadError::AbiMismatch, reported, false};
  candidate.observation.reported_abi = reported;
  if (void *symbol = ResolveSymbol(handle, "NexoraPluginGetLifecycleV1")) {
    auto &lifecycle = candidate.lifecycle;
    lifecycle.struct_size = sizeof(lifecycle);
    try {
      if (FunctionCast<NexoraPluginGetLifecycleV1Fn>(symbol)(NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1,
                                                             &lifecycle) != 0 ||
          lifecycle.struct_size < sizeof(lifecycle) ||
          lifecycle.schema_version != NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1 ||
          !lifecycle.request_shutdown || !lifecycle.poll_quiescence)
        return {false, PluginLoadError::InvalidLifecycle, reported, false};
    } catch (...) {
      return {false, PluginLoadError::InspectionFailed, reported, false};
    }
    candidate.observation.cooperative = true;
  }
  candidate.handle = handle;
  entries_.push_back(std::move(candidate)); // Allocation precedes any activation/registration.
  guard.handle = nullptr;
  ++next_id_;
  auto &entry = entries_.back();
  auto &provider = *entry.provider;
  bool registered = false;
  if (services != nullptr) {
    if (void *register_symbol = ResolveSymbol(handle, "NexoraPluginRegister")) {
      provider.publishing = true;
      provider.registering_registry = services;
      try {
        FunctionCast<NexoraPluginRegisterFn>(register_symbol)(
            &provider, [](void *context, const char *name, void *service) {
              auto &owner = *static_cast<ServiceRegistry::Provider *>(context);
              if (!owner.publishing || !owner.registering_registry)
                return;
              if (owner.attempts == kMaximumServices) {
                owner.failed = true;
                return;
              }
              ++owner.attempts;
              if (!name || !service) {
                owner.failed = true;
                return;
              }
              std::size_t length{};
              while (length <= kMaximumServiceNameBytes && name[length] != '\0')
                ++length;
              if (!length || length > kMaximumServiceNameBytes ||
                  !foundation::IsValidUtf8(std::string_view{name, length})) {
                owner.failed = true;
                return;
              }
              try {
                if (owner.registering_registry->RegisterOwned(std::string(name, length), service,
                                                              owner.self))
                  ++owner.registrations;
                else
                  owner.failed = true;
              } catch (...) {
                owner.failed = true;
              }
            });
      } catch (...) {
        provider.failed = true;
      }
      provider.publishing = false;
      provider.registering_registry = nullptr;
      registered = true;
    }
  }
  entry.observation.registered_services = provider.registrations;
  if (provider.failed) {
    entry.observation.load_error = PluginLoadError::RegistrationRejected;
    static_cast<void>(RequestUnload(entry.observation.id));
    return {false,
            PluginLoadError::RegistrationRejected,
            reported,
            false,
            entry.observation.id,
            entry.observation.cooperative};
  }
  provider.active = true; // Publish all registered services only after registration completes.
  return {true,       PluginLoadError::None, reported,
          registered, entry.observation.id,  entry.observation.cooperative};
}
void *PluginHost::FindService(std::uint64_t id, const ServiceRegistry &services,
                              std::string_view name) const {
  if (name.empty() || name.size() > kMaximumServiceNameBytes ||
      name.find('\0') != std::string_view::npos)
    return nullptr;
  const auto admission =
      std::ranges::find(entries_, id, [](const Entry &entry) { return entry.observation.id; });
  if (admission == entries_.end() || admission->observation.state != PluginState::Loaded ||
      !admission->handle || !admission->provider || !admission->provider->active)
    return nullptr;
  const auto service = services.services_.find(std::string(name));
  if (service == services.services_.end() || !service->second.owned ||
      !ServiceRegistry::Visible(service->second) ||
      service->second.provider.lock() != admission->provider)
    return nullptr;
  return service->second.service;
}

void PluginHost::UnloadAll() noexcept {
  for (auto &entry : entries_)
    static_cast<void>(RequestUnload(entry.observation.id));
}
PluginState PluginHost::RequestUnload(std::uint64_t id) noexcept {
  const auto found =
      std::ranges::find(entries_, id, [](const Entry &entry) { return entry.observation.id; });
  if (found == entries_.end())
    return PluginState::Missing;
  auto &entry = *found;
  if (entry.observation.state != PluginState::Loaded)
    return entry.observation.state;
  entry.provider->active = false; // Revoke visibility before invoking native shutdown.
  if (!entry.observation.cooperative) {
    entry.observation.state = PluginState::RestartRequired;
    entry.observation.lifecycle_error = PluginLifecycleError::LegacyNeedsRestart;
    return entry.observation.state;
  }
  try {
    entry.observation.lifecycle_result = entry.lifecycle.request_shutdown(entry.lifecycle.context);
  } catch (...) {
    entry.observation.lifecycle_result = -1;
  }
  if (entry.observation.lifecycle_result != 0) {
    entry.observation.state = PluginState::RestartRequired;
    entry.observation.lifecycle_error = PluginLifecycleError::RequestFailed;
  } else {
    entry.observation.state = PluginState::ShutdownPending;
    Poll(entry);
  }
  return entry.observation.state;
}
void PluginHost::Poll(Entry &entry) noexcept {
  if (entry.observation.state != PluginState::ShutdownPending)
    return;
  try {
    entry.observation.lifecycle_result = entry.lifecycle.poll_quiescence(entry.lifecycle.context);
  } catch (...) {
    entry.observation.lifecycle_result = -1;
  }
  if (entry.observation.lifecycle_result == 1) {
    CloseLibrary(entry.handle);
    entry.handle = nullptr;
    entry.lifecycle = {};
    entry.observation.state = PluginState::Unloaded;
  } else if (entry.observation.lifecycle_result != 0) {
    entry.observation.state = PluginState::RestartRequired;
    entry.observation.lifecycle_error = PluginLifecycleError::QuiescenceFailed;
  }
}
void PluginHost::PollShutdown() noexcept {
  for (auto &entry : entries_)
    Poll(entry);
}
std::vector<PluginSnapshot> PluginHost::Snapshot() const {
  std::vector<PluginSnapshot> result;
  result.reserve(entries_.size());
  for (const auto &entry : entries_) {
    result.push_back(entry.observation);
    if (!entry.provider->active)
      result.back().registered_services = 0;
  }
  return result;
}
std::size_t PluginHost::LoadedCount() const noexcept {
  return static_cast<std::size_t>(
      std::ranges::count_if(entries_, [](const Entry &entry) { return entry.handle != nullptr; }));
}

bool ServiceRegistry::Visible(const Entry &entry) noexcept {
  if (!entry.owned)
    return true;
  const auto owner = entry.provider.lock();
  return owner && owner->active;
}
void ServiceRegistry::PruneRevoked() {
  std::erase_if(services_, [](const auto &item) {
    if (!item.second.owned)
      return false;
    const auto owner = item.second.provider.lock();
    return !owner || (!owner->active && !owner->publishing);
  });
}
bool ServiceRegistry::RegisterOwned(std::string name, void *service,
                                    const std::weak_ptr<Provider> &provider) {
  PruneRevoked();
  return services_.emplace(std::move(name), Entry{service, provider, true}).second;
}

bool ServiceRegistry::Register(std::string name, void *service) {
  if (name.empty() || service == nullptr)
    return false;
  PruneRevoked();
  return services_.emplace(std::move(name), Entry{service, {}, false}).second;
}
bool ServiceRegistry::Unregister(std::string_view name) {
  return services_.erase(std::string(name)) != 0;
}
void *ServiceRegistry::Find(std::string_view name) const {
  const auto found = services_.find(std::string(name));
  return found == services_.end() || !Visible(found->second) ? nullptr : found->second.service;
}
std::size_t ServiceRegistry::Size() const noexcept {
  return static_cast<std::size_t>(
      std::ranges::count_if(services_, [](const auto &item) { return Visible(item.second); }));
}

Id SceneEditor::CreateEntity(Id scene, Id parent) {
  return CreateInitializedEntity(scene, parent, {}, std::nullopt);
}
Id SceneEditor::CreateCameraEntity(Id scene, Id parent) {
  return CreateInitializedEntity(scene, parent, {}, std::nullopt, CameraComponent{});
}
Id SceneEditor::CreateLightEntity(Id scene, Id parent) {
  return CreateInitializedEntity(scene, parent, {}, std::nullopt, std::nullopt, LightComponent{});
}
Id SceneEditor::CreateMeshEntity(Id scene, MeshComponent mesh, Transform transform) {
  if (mesh.mesh == 0)
    return 0;
  return CreateInitializedEntity(scene, 0, transform, mesh);
}
Id SceneEditor::CreateInitializedEntity(Id scene, Id parent, Transform transform,
                                        std::optional<MeshComponent> mesh,
                                        std::optional<CameraComponent> camera,
                                        std::optional<LightComponent> light) {
  const auto *target_scene = world_.FindScene(scene);
  const auto normalized = NormalizedTransform(transform);
  if (!normalized || !target_scene || target_scene->state == SceneState::Unloading ||
      target_scene->state == SceneState::Unloaded ||
      (parent != 0 && std::ranges::find(target_scene->entities, parent, &Entity::id) ==
                          target_scene->entities.end()))
    return 0;
  const auto id = world_.CreateEntity(scene).id;
  WorldCommandBuffer initialize;
  initialize.SetTransform(id, *normalized);
  if (mesh)
    initialize.SetMeshRenderer(id, mesh);
  if (camera)
    initialize.SetCamera(id, camera);
  if (light)
    initialize.SetLight(id, light);
  if (parent != 0)
    initialize.SetParent(id, parent, false);
  if (!initialize.Apply(world_)) {
    WorldCommandBuffer discard;
    discard.DestroyEntity(id);
    static_cast<void>(discard.Apply(world_));
    return 0;
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
std::vector<Id> SceneEditor::CloneEntityForest(Id scene, std::span<const Entity> prototypes) {
  const auto *target = world_.FindScene(scene);
  if (!target || !world_.next_id_ || prototypes.empty() || target->state == SceneState::Unloading ||
      target->state == SceneState::Unloaded ||
      prototypes.size() > std::numeric_limits<Id>::max() - world_.next_id_)
    return {};
  std::unordered_map<Id, Id> parents, mapped;
  std::vector<Entity> created(prototypes.begin(), prototypes.end());
  std::vector<Id> ids, roots;
  auto next_id = world_.next_id_;
  for (auto &entity : created) {
    const auto pose = NormalizedTransform(entity.transform);
    const auto &camera = entity.camera_data;
    if (!entity.id || !parents.emplace(entity.id, entity.parent).second || !pose ||
        (entity.camera &&
         (!std::isfinite(camera.vertical_field_of_view) || !std::isfinite(camera.near_plane) ||
          !std::isfinite(camera.far_plane) || camera.vertical_field_of_view <= 0 ||
          camera.vertical_field_of_view >= 180 || camera.near_plane <= 0 ||
          camera.far_plane <= camera.near_plane)) ||
        (entity.light &&
         (!std::isfinite(entity.light_data.intensity) || entity.light_data.intensity < 0)))
      return {};
    entity.transform = *pose;
    mapped.emplace(entity.id, next_id);
    ids.push_back(next_id++);
  }
  // Prove all prototype chains reach a root once, including forward parent references.
  enum class Mark { Walking, Rooted };
  std::unordered_map<Id, Mark> marks;
  std::vector<Id> path;
  for (const auto &entity : created) {
    path.clear();
    for (auto id = entity.id; id != 0 && !marks.contains(id); id = parents.at(id)) {
      const auto parent = parents.at(id);
      if (parent && !parents.contains(parent))
        return {};
      marks.emplace(id, Mark::Walking);
      path.push_back(id);
      if (const auto found = marks.find(parent);
          found != marks.end() && found->second == Mark::Walking)
        return {};
    }
    for (const auto id : path)
      marks[id] = Mark::Rooted;
  }
  for (auto &entity : created) {
    entity.id = mapped.at(entity.id);
    if (entity.parent)
      entity.parent = mapped.at(entity.parent);
    else
      roots.push_back(entity.id);
  }
  std::unordered_set<Id> created_ids(ids.begin(), ids.end()), root_ids(roots.begin(), roots.end());
  auto published = target->entities;
  published.insert(published.end(), created.begin(), created.end());
  undo_.Record(
      [this, scene, roots, root_ids, created_ids] {
        const auto *current = world_.FindScene(scene);
        if (!current || current->state == SceneState::Unloading ||
            current->state == SceneState::Unloaded ||
            SelectedSubtrees(*current, root_ids) != created_ids)
          return false;
        WorldCommandBuffer discard;
        for (const auto root : roots) {
          if (std::ranges::find(current->entities, root, &Entity::id) == current->entities.end())
            return false;
          discard.DestroyEntity(root);
        }
        return discard.Apply(world_);
      },
      [this, scene, created] {
        auto *current = const_cast<Scene *>(world_.FindScene(scene));
        if (!current || current->state == SceneState::Unloading ||
            current->state == SceneState::Unloaded ||
            std::ranges::any_of(created, [this](const Entity &entity) {
              return world_.FindEntity(entity.id) != nullptr;
            }))
          return false;
        auto restored = current->entities;
        restored.insert(restored.end(), created.begin(), created.end());
        current->entities = std::move(restored);
        for (const auto &entity : created)
          world_.next_id_ = std::max(world_.next_id_, entity.id + 1);
        return true;
      });
  const_cast<Scene *>(target)->entities = std::move(published);
  world_.next_id_ = next_id;
  ++depth_;
  return ids;
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
  return SetMeshRenderers(std::array{entity}, std::array{mesh});
}
bool SceneEditor::SetMeshRenderers(std::span<const Id> entities,
                                   std::span<const std::optional<MeshComponent>> meshes) {
  if (entities.empty() || entities.size() != meshes.size())
    return false;
  std::vector<std::optional<MeshComponent>> previous;
  previous.reserve(entities.size());
  std::unordered_set<Id> unique;
  WorldCommandBuffer apply;
  for (std::size_t index = 0; index < entities.size(); ++index) {
    const auto *existing = world_.FindEntity(entities[index]);
    if (!existing || !unique.insert(entities[index]).second)
      return false;
    previous.push_back(existing->mesh_renderer ? std::optional(existing->mesh_data) : std::nullopt);
    apply.SetMeshRenderer(entities[index], meshes[index]);
  }
  if (!apply.Apply(world_))
    return false;
  const std::vector<Id> owned_entities(entities.begin(), entities.end());
  const std::vector<std::optional<MeshComponent>> next(meshes.begin(), meshes.end());
  undo_.Record(
      [this, owned_entities, previous] {
        WorldCommandBuffer commands;
        for (std::size_t index = 0; index < owned_entities.size(); ++index)
          commands.SetMeshRenderer(owned_entities[index], previous[index]);
        return commands.Apply(world_);
      },
      [this, owned_entities, next] {
        WorldCommandBuffer commands;
        for (std::size_t index = 0; index < owned_entities.size(); ++index)
          commands.SetMeshRenderer(owned_entities[index], next[index]);
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
  return DestroyEntities(scene, std::array{entity});
}
bool SceneEditor::DestroyEntities(Id scene, std::span<const Id> entities) {
  const auto *target = world_.FindScene(scene);
  if (!target || entities.empty() || target->state == SceneState::Unloading ||
      target->state == SceneState::Unloaded)
    return false;
  std::unordered_map<Id, Id> parents;
  for (const auto &entity : target->entities)
    parents.emplace(entity.id, entity.parent);
  const std::unordered_set<Id> selected(entities.begin(), entities.end());
  if (selected.size() != entities.size())
    return false;
  std::unordered_set<Id> root_ids;
  for (const auto id : entities) {
    if (!parents.contains(id))
      return false;
    bool selected_ancestor = false;
    std::size_t steps = 0;
    for (auto ancestor = parents.at(id); ancestor != 0; ancestor = parents.at(ancestor)) {
      if (!parents.contains(ancestor) || ++steps > parents.size())
        return false;
      selected_ancestor |= selected.contains(ancestor);
    }
    if (!selected_ancestor)
      root_ids.insert(id);
  }
  struct Root final {
    Id id;
    std::size_t sibling_index;
    Transform world_pose;
  };
  std::vector<Root> roots;
  std::unordered_map<Id, std::size_t> sibling_counts;
  // Capture roots in original storage/sibling order, independent of selection order.
  for (const auto &entity : target->entities) {
    const auto index = sibling_counts[entity.parent]++;
    if (!root_ids.contains(entity.id))
      continue;
    roots.push_back({entity.id, index,
                     entity.parent == 0
                         ? entity.transform
                         : world_.WorldTransform(entity.id).value_or(entity.transform)});
  }
  const auto removed_ids = SelectedSubtrees(*target, root_ids);
  std::vector<Entity> removed;
  std::vector<Id> original_order;
  for (const auto &entity : target->entities) {
    original_order.push_back(entity.id);
    if (removed_ids.contains(entity.id))
      removed.push_back(entity);
  }
  WorldCommandBuffer apply;
  for (const auto &root : roots)
    apply.DestroyEntity(root.id);
  if (!apply.Apply(world_))
    return false;
  undo_.Record(
      [this, scene, removed, roots, original_order] {
        auto *current = const_cast<Scene *>(world_.FindScene(scene));
        if (!current || current->state == SceneState::Unloading ||
            current->state == SceneState::Unloaded ||
            std::ranges::any_of(removed, [this](const Entity &entity) {
              return world_.FindEntity(entity.id) != nullptr;
            }))
          return false;
        // Rehearse restoration and sibling placement before publishing any restored entity.
        World staged{world_.kind_};
        staged.scenes_ = world_.scenes_;
        staged.next_id_ = world_.next_id_;
        auto *restored = const_cast<Scene *>(staged.FindScene(scene));
        // Insert removed records before their next surviving original neighbor. Preserve current
        // survivors and unrelated new entities, without needlessly changing stable snapshot order.
        std::unordered_map<Id, Entity> payloads;
        for (const auto &entity : removed)
          payloads.emplace(entity.id, entity);
        std::unordered_set<Id> survivors;
        for (const auto &entity : current->entities)
          survivors.insert(entity.id);
        std::unordered_map<Id, std::vector<Entity>> preceding;
        std::vector<Entity> pending, merged;
        for (const auto id : original_order) {
          if (const auto found = payloads.find(id); found != payloads.end())
            pending.push_back(found->second);
          else if (survivors.contains(id) && !pending.empty()) {
            preceding.emplace(id, std::move(pending));
            pending.clear();
          }
        }
        merged.reserve(current->entities.size() + removed.size());
        for (const auto &entity : current->entities) {
          if (const auto found = preceding.find(entity.id); found != preceding.end())
            merged.insert(merged.end(), found->second.begin(), found->second.end());
          merged.push_back(entity);
        }
        merged.insert(merged.end(), pending.begin(), pending.end());
        restored->entities = std::move(merged);
        bool orphaned_roots = false;
        for (const auto &root : roots) {
          auto entity = std::ranges::find(restored->entities, root.id, &Entity::id);
          if (entity->parent != 0 && std::ranges::find(restored->entities, entity->parent,
                                                       &Entity::id) == restored->entities.end()) {
            const auto pose = NormalizedTransform(root.world_pose);
            if (!pose)
              return false;
            entity->parent = 0;
            entity->transform = *pose;
            orphaned_roots = true;
          }
        }
        // Different missing parents had independent indexes. Once roots join the same group,
        // retain its merged order (including existing unrelated roots), rather than reusing them.
        std::unordered_map<Id, std::size_t> merged_root_indexes;
        if (orphaned_roots)
          for (const auto &entity : restored->entities)
            if (entity.parent == 0)
              merged_root_indexes.emplace(entity.id, merged_root_indexes.size());
        WorldCommandBuffer place;
        for (const auto &root : roots) {
          const auto entity = std::ranges::find(restored->entities, root.id, &Entity::id);
          const auto index = orphaned_roots && entity->parent == 0 ? merged_root_indexes.at(root.id)
                                                                   : root.sibling_index;
          if (staged.SiblingIndex(root.id) != index)
            place.SetSiblingIndex(root.id, index);
        }
        if (!place.Apply(staged))
          return false;
        for (const auto &entity : removed)
          staged.next_id_ = std::max(staged.next_id_, entity.id + 1);
        current->entities = std::move(restored->entities);
        world_.next_id_ = staged.next_id_;
        return true;
      },
      [this, scene, roots, root_ids, removed_ids] {
        const auto *current = world_.FindScene(scene);
        if (!current || current->state == SceneState::Unloading ||
            current->state == SceneState::Unloaded)
          return false;
        WorldCommandBuffer commands;
        for (const auto &root : roots) {
          if (std::ranges::find(current->entities, root.id, &Entity::id) == current->entities.end())
            return false;
          commands.DestroyEntity(root.id);
        }
        // External additions/reparenting may not silently expand the recorded deletion.
        if (SelectedSubtrees(*current, root_ids) != removed_ids)
          return false;
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
  return detail::PushConsoleRecord(std::move(record), capacity_, records_, next_sequence_,
                                   dropped_);
}

std::vector<RuntimeLogRecord> RuntimeConsole::Snapshot() const {
  std::scoped_lock lock(mutex_);
  return records_;
}

void RuntimeConsole::ReportDropped(std::uint64_t count) {
  std::lock_guard lock{mutex_};
  dropped_ = count > UINT64_MAX - dropped_ ? UINT64_MAX : dropped_ + count;
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
