#include "Nexora/Runtime/EditorSdk.h"

#include <algorithm>
#include <cstring>

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

PluginLoadResult PluginHost::Load(const std::string &library_path) {
  void *handle = OpenLibrary(library_path);
  if (!handle)
    return {false, PluginLoadError::OpenFailed, 0};
  void *symbol = ResolveSymbol(handle, "NexoraPluginAbiVersion");
  if (!symbol) {
    CloseLibrary(handle);
    return {false, PluginLoadError::MissingAbiSymbol, 0};
  }
  using AbiVersionFn = std::uint32_t (*)();
  const auto reported = FunctionCast<AbiVersionFn>(symbol)();
  if (reported != engine_abi_) {
    CloseLibrary(handle);
    return {false, PluginLoadError::AbiMismatch, reported};
  }
  handles_.push_back(handle);
  return {true, PluginLoadError::None, reported};
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

Entity &SceneEditor::CreateEntity(Id scene) {
  auto &entity = world_.CreateEntity(scene);
  const auto id = entity.id;
  undo_.Execute([] {},
                [this, id] {
                  WorldCommandBuffer commands;
                  commands.DestroyEntity(id);
                  (void)commands.Apply(world_);
                });
  ++depth_;
  return entity;
}
bool SceneEditor::SetTransform(Id entity, Transform transform) {
  const auto *existing = world_.FindEntity(entity);
  if (!existing)
    return false;
  const auto previous = existing->transform;
  WorldCommandBuffer apply;
  apply.SetTransform(entity, transform);
  if (!apply.Apply(world_))
    return false;
  undo_.Execute([] {},
                [this, entity, previous] {
                  WorldCommandBuffer commands;
                  commands.SetTransform(entity, previous);
                  (void)commands.Apply(world_);
                });
  ++depth_;
  return true;
}
bool SceneEditor::DestroyEntity(Id scene, Id entity) {
  const auto *existing = world_.FindEntity(entity);
  if (!existing)
    return false;
  const Entity snapshot = *existing;
  WorldCommandBuffer apply;
  apply.DestroyEntity(entity);
  if (!apply.Apply(world_))
    return false;
  undo_.Execute([] {},
                [this, scene, snapshot] {
                  auto &restored = world_.CreateEntity(scene);
                  restored.transform = snapshot.transform;
                  restored.camera = snapshot.camera;
                  restored.light = snapshot.light;
                  restored.mesh_renderer = snapshot.mesh_renderer;
                  restored.camera_data = snapshot.camera_data;
                  restored.light_data = snapshot.light_data;
                  restored.mesh_data = snapshot.mesh_data;
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
