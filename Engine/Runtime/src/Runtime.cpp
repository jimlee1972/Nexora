#include "Nexora/Runtime/Runtime.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nexora::runtime {

Id World::LoadScene(std::string name, bool persistent) {
  const auto id = next_id_++;
  scenes_.push_back({id, std::move(name), SceneState::LoadedInactive, persistent, {}});
  return id;
}

bool World::Activate(Id id) {
  auto *scene = const_cast<Scene *>(FindScene(id));
  if (scene == nullptr || scene->state != SceneState::LoadedInactive)
    return false;
  scene->state = SceneState::Active;
  return true;
}

bool World::RequestUnload(Id id) {
  auto *scene = const_cast<Scene *>(FindScene(id));
  if (scene == nullptr || scene->persistent || scene->state == SceneState::Unloaded ||
      scene->state == SceneState::Unloading)
    return false;
  scene->state = SceneState::Unloading;
  return true;
}

void World::EndFrame() {
  for (auto &scene : scenes_)
    if (scene.state == SceneState::Unloading) {
      scene.entities.clear();
      scene.state = SceneState::Unloaded;
    }
}

Entity &World::CreateEntity(Id id) {
  auto *scene = const_cast<Scene *>(FindScene(id));
  if (scene == nullptr || scene->state == SceneState::Unloaded ||
      scene->state == SceneState::Unloading)
    throw std::invalid_argument("entity requires a loaded scene");
  scene->entities.push_back({next_id_++});
  return scene->entities.back();
}

const Scene *World::FindScene(Id id) const {
  const auto found = std::find_if(scenes_.begin(), scenes_.end(),
                                  [id](const auto &scene) { return scene.id == id; });
  return found == scenes_.end() ? nullptr : &*found;
}

std::size_t World::ActiveSceneCount() const {
  return static_cast<std::size_t>(
      std::count_if(scenes_.begin(), scenes_.end(),
                    [](const auto &scene) { return scene.state == SceneState::Active; }));
}

bool AssetRegistry::IsAcyclic(const std::vector<AssetRecord> &assets) const {
  std::unordered_map<Id, std::vector<Id>> graph;
  for (const auto &asset : assets)
    graph.emplace(asset.id, asset.dependencies);
  std::unordered_set<Id> visiting, visited;
  std::function<bool(Id)> visit = [&](Id id) {
    if (visiting.contains(id))
      return false;
    if (visited.contains(id))
      return true;
    visiting.insert(id);
    if (const auto node = graph.find(id); node != graph.end())
      for (const auto dependency : node->second)
        if (graph.contains(dependency) && !visit(dependency))
          return false;
    visiting.erase(id);
    visited.insert(id);
    return true;
  };
  for (const auto &[id, unused] : graph)
    if (!visit(id))
      return false;
  return true;
}

bool AssetRegistry::Stage(std::vector<AssetRecord> assets) {
  if (assets.empty() || !IsAcyclic(assets))
    return false;
  const auto generation = assets.front().generation;
  if (generation == 0 || std::ranges::any_of(assets, [generation](const auto &asset) {
        return asset.generation != generation || asset.hash.empty();
      }))
    return false;
  staged_ = generation;
  return true;
}

bool AssetRegistry::ActivateStaged() {
  if (staged_ == 0)
    return false;
  previous_ = active_;
  active_ = staged_;
  staged_ = 0;
  return true;
}

bool AssetRegistry::Rollback() {
  if (previous_ == 0)
    return false;
  std::swap(active_, previous_);
  return true;
}
void AssetRegistry::PinGeneration(std::uint64_t generation) { pins_.insert(generation); }
void AssetRegistry::UnpinGeneration(std::uint64_t generation) { pins_.erase(generation); }
bool AssetRegistry::IsPinned(std::uint64_t generation) const { return pins_.contains(generation); }

bool ExtensionRegistry::Load(PluginDescriptor descriptor) {
  if (descriptor.name.empty() || descriptor.abi != host_abi_)
    return false;
  plugins_.push_back(std::move(descriptor));
  return true;
}
bool ExtensionRegistry::HasService(std::string_view service) const {
  return std::ranges::any_of(plugins_, [service](const auto &plugin) {
    return std::ranges::find(plugin.services, service) != plugin.services.end();
  });
}

void UndoStack::Execute(std::function<void()> apply, std::function<void()> undo) {
  if (!apply || !undo)
    throw std::invalid_argument("transaction callbacks must be valid");
  apply();
  undo_.push_back(std::move(undo));
}
bool UndoStack::Undo() {
  if (undo_.empty())
    return false;
  auto operation = std::move(undo_.back());
  undo_.pop_back();
  operation();
  return true;
}

bool InputRouter::Route(InputEvent event) {
  if (event.consumed)
    return false;
  if (event.kind == InputKind::Touch && !touch_ids_.insert(event.pointer_id).second)
    return false;
  ++delivered_;
  return true;
}
void InputRouter::EndFrame() { touch_ids_.clear(); }
std::size_t VirtualList::ElementCount() const noexcept {
  return std::min(item_count - std::min(item_count, first_visible), visible_count);
}

CharacterMotion CharacterMotor::Simulate(CharacterIntent intent, double max_speed,
                                         bool ground_contact) const {
  const auto length = std::hypot(intent.requested_x, intent.requested_z);
  const auto scale = length > max_speed && length > 0.0 ? max_speed / length : 1.0;
  return {intent.requested_x * scale, intent.requested_z * scale, ground_contact};
}

void ResidencySet::Acquire(Id resource) { ++references_[resource]; }
void ResidencySet::Release(Id resource) {
  const auto found = references_.find(resource);
  if (found != references_.end() && --found->second == 0)
    references_.erase(found);
}
bool ResidencySet::Resident(Id resource) const { return references_.contains(resource); }

bool StreamingWorld::Add(StreamingCell cell) {
  if (cell.cell_id == 0 || cell.bundle_id == 0 || Find(cell.cell_id) != nullptr)
    return false;
  cells_.push_back(cell);
  return true;
}
bool StreamingWorld::UnloadFull(Id id) {
  auto found = std::ranges::find_if(cells_, [id](const auto &cell) { return cell.cell_id == id; });
  if (found == cells_.end() || found->occupied)
    return false;
  found->full = false;
  return true;
}
std::pair<std::size_t, std::size_t> StreamingWorld::Usage() const {
  std::pair<std::size_t, std::size_t> result{};
  for (const auto &cell : cells_) {
    if (cell.full || cell.hlod) {
      result.first += cell.ram;
      result.second += cell.vram;
    }
  }
  return result;
}
const StreamingCell *StreamingWorld::Find(Id id) const {
  const auto found =
      std::ranges::find_if(cells_, [id](const auto &cell) { return cell.cell_id == id; });
  return found == cells_.end() ? nullptr : &*found;
}

PlatformPolicy PlatformRuntime::Pressure(bool thermal, bool memory) const noexcept {
  return {thermal, memory};
}
bool PlatformRuntime::RouteWebViewPointer(bool inside_native_view) const noexcept {
  return inside_native_view;
}

PackageManifest Packager::Build(ShippingProfile profile, const PackageInput &input,
                                std::span<const std::string> enabled_plugins,
                                std::span<const std::string> enabled_shaders) const {
  PackageManifest result;
  result.presentation = profile != ShippingProfile::Dedicated;
  result.files = input.assets;
  const auto enabled = [](std::string_view value, std::span<const std::string> values) {
    return std::ranges::find(values, value) != values.end();
  };
  for (const auto &plugin : input.plugins)
    if (enabled(plugin, enabled_plugins))
      result.files.push_back("plugins/" + plugin);
  if (result.presentation)
    for (const auto &shader : input.shader_families)
      if (enabled(shader, enabled_shaders))
        result.files.push_back("shaders/" + shader);
  if (profile == ShippingProfile::Minimal)
    result.files.erase(
        std::remove_if(result.files.begin(), result.files.end(),
                       [](const auto &file) { return file.starts_with("optional/"); }),
        result.files.end());
  return result;
}

} // namespace nexora::runtime
