#include "Nexora/Runtime/Runtime.h"
#include "Nexora/Renderer/FramePipeline.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace nexora::runtime {

Id World::LoadScene(std::string name, bool persistent) {
  const auto id = next_id_++;
  scenes_.push_back({id, std::move(name), SceneState::LoadedInactive, persistent, {}});
  return id;
}

std::optional<std::string> World::SaveScene(Id id) const {
  const auto *scene = FindScene(id);
  if (scene == nullptr || scene->state == SceneState::Unloading ||
      scene->state == SceneState::Unloaded)
    return std::nullopt;
  std::ostringstream output;
  output << "NEXORA_SCENE 1 " << std::quoted(scene->name) << ' ' << scene->persistent << ' '
         << scene->entities.size() << '\n';
  output << std::setprecision(17);
  for (const auto &entity : scene->entities)
    output << entity.id << ' ' << entity.transform.x << ' ' << entity.transform.y << ' '
           << entity.transform.z << ' ' << entity.camera << ' ' << entity.light << ' '
           << entity.mesh_renderer << ' ' << entity.camera_data.vertical_field_of_view << ' '
           << entity.camera_data.near_plane << ' ' << entity.camera_data.far_plane << ' '
           << entity.light_data.intensity << ' ' << entity.mesh_data.mesh << ' '
           << entity.mesh_data.material.shader << '\n';
  return output.str();
}

std::optional<Id> World::LoadSceneSnapshot(std::string_view snapshot) {
  std::istringstream input{std::string(snapshot)};
  std::string magic, name;
  unsigned version{};
  bool persistent{};
  std::size_t count{};
  if (!(input >> magic >> version >> std::quoted(name) >> persistent >> count) ||
      magic != "NEXORA_SCENE" || version != 1 || name.empty())
    return std::nullopt;
  Scene loaded{next_id_, std::move(name), SceneState::LoadedInactive, persistent, {}};
  loaded.entities.reserve(count);
  std::unordered_set<Id> ids;
  auto next_id = next_id_ + 1;
  for (std::size_t index = 0; index < count; ++index) {
    Entity entity;
    if (!(input >> entity.id >> entity.transform.x >> entity.transform.y >> entity.transform.z >>
          entity.camera >> entity.light >> entity.mesh_renderer >>
          entity.camera_data.vertical_field_of_view >> entity.camera_data.near_plane >>
          entity.camera_data.far_plane >> entity.light_data.intensity >> entity.mesh_data.mesh >>
          entity.mesh_data.material.shader) ||
        entity.id == 0 || FindEntity(entity.id) != nullptr || !ids.insert(entity.id).second ||
        !std::isfinite(entity.transform.x) || !std::isfinite(entity.transform.y) ||
        !std::isfinite(entity.transform.z))
      return std::nullopt;
    next_id = std::max(next_id, entity.id + 1);
    loaded.entities.push_back(entity);
  }
  input >> std::ws;
  if (!input.eof())
    return std::nullopt;
  const auto id = loaded.id;
  scenes_.push_back(std::move(loaded));
  next_id_ = next_id;
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

const Entity *World::FindEntity(Id id) const {
  for (const auto &scene : scenes_)
    if (scene.state != SceneState::Unloaded)
      if (const auto found = std::ranges::find(scene.entities, id, &Entity::id);
          found != scene.entities.end())
        return &*found;
  return nullptr;
}

std::size_t World::ActiveSceneCount() const {
  return static_cast<std::size_t>(
      std::count_if(scenes_.begin(), scenes_.end(),
                    [](const auto &scene) { return scene.state == SceneState::Active; }));
}

World World::CloneForPlay() const {
  World clone{WorldKind::Play};
  clone.next_id_ = next_id_;
  clone.scenes_ = scenes_;
  return clone;
}

void WorldCommandBuffer::SetTransform(Id entity, Transform transform) {
  commands_.push_back({entity, transform});
}
void WorldCommandBuffer::DestroyEntity(Id entity) { commands_.push_back({entity, std::nullopt}); }

bool WorldCommandBuffer::Apply(World &world) {
  for (const auto &command : commands_)
    if (world.FindEntity(command.entity) == nullptr)
      return false;
  for (const auto &command : commands_)
    for (auto &scene : world.scenes_)
      if (const auto found = std::ranges::find(scene.entities, command.entity, &Entity::id);
          found != scene.entities.end()) {
        if (command.transform)
          found->transform = *command.transform;
        else
          scene.entities.erase(found);
        break;
      }
  commands_.clear();
  return true;
}

bool SystemScheduler::Add(std::string name, std::vector<std::string> after, System system) {
  if (name.empty() || !system ||
      std::ranges::any_of(systems_, [&name](const auto &entry) { return entry.name == name; }))
    return false;
  systems_.push_back({std::move(name), std::move(after), std::move(system)});
  return true;
}

bool SystemScheduler::Execute(World &world) const {
  std::unordered_set<std::string> completed;
  WorldCommandBuffer commands;
  while (completed.size() != systems_.size()) {
    bool progressed = false;
    for (const auto &entry : systems_) {
      if (completed.contains(entry.name) ||
          !std::ranges::all_of(entry.after, [&completed](const auto &dependency) {
            return completed.contains(dependency);
          }))
        continue;
      entry.system(world, commands);
      completed.insert(entry.name);
      progressed = true;
    }
    if (!progressed)
      return false;
  }
  return commands.Apply(world);
}

std::optional<SceneFrameResult> RenderSceneFrame(const World &world, rhi::Device &device,
                                                 rhi::TextureHandle target,
                                                 const rhi::TextureDescriptor &target_descriptor,
                                                 rhi::PipelineHandle pipeline) {
#if !NEXORA_SCENE_RENDERING_ENABLED
  static_cast<void>(world);
  static_cast<void>(device);
  static_cast<void>(target);
  static_cast<void>(target_descriptor);
  static_cast<void>(pipeline);
  return std::nullopt;
#else
  bool camera = false, light = false;
  std::size_t meshes = 0;
  for (const auto &scene : world.scenes_) {
    if (scene.state != SceneState::Active)
      continue;
    for (const auto &entity : scene.entities) {
      camera = camera || entity.camera;
      light = light || entity.light;
      meshes += static_cast<std::size_t>(entity.mesh_renderer && entity.mesh_data.mesh != 0 &&
                                         entity.mesh_data.material.shader != 0);
    }
  }
  if (!camera || !light || meshes == 0)
    return std::nullopt;
  const auto result =
      renderer::ExecuteSceneFrame(device, target, target_descriptor, pipeline, meshes);
  return SceneFrameResult{meshes, result.passes, result.barriers};
#endif
}

bool SceneRenderingEnabled() noexcept {
#if NEXORA_SCENE_RENDERING_ENABLED
  return true;
#else
  return false;
#endif
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

void UndoStack::Execute(const std::function<void()> &apply, std::function<void()> undo) {
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

bool NavigationGraph::AddNode(NavigationNode node) {
  if (node.id == 0 || !std::isfinite(node.x) || !std::isfinite(node.z) || nodes_.contains(node.id))
    return false;
  nodes_.emplace(node.id, std::move(node));
  return true;
}

std::vector<Id> NavigationGraph::FindPath(Id start, Id goal) const {
  if (!nodes_.contains(start) || !nodes_.contains(goal))
    return {};
  std::queue<Id> open;
  std::unordered_map<Id, Id> parent;
  open.push(start);
  parent.emplace(start, 0);
  while (!open.empty() && !parent.contains(goal)) {
    const auto current = open.front();
    open.pop();
    for (const auto neighbour : nodes_.at(current).neighbours) {
      if (!nodes_.contains(neighbour) || parent.contains(neighbour))
        continue;
      parent.emplace(neighbour, current);
      open.push(neighbour);
    }
  }
  if (!parent.contains(goal))
    return {};
  std::vector<Id> path;
  for (auto cursor = goal; cursor != 0; cursor = parent.at(cursor))
    path.push_back(cursor);
  std::ranges::reverse(path);
  return path;
}

bool LocalizationCatalog::Add(LocalizedEntry entry) {
  if (entry.key.empty() || entry.translations.empty() || entries_.contains(entry.key))
    return false;
  return entries_.emplace(entry.key, std::move(entry)).second;
}

// The two string views deliberately mirror the conventional lookup(key, locale) API.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::string_view LocalizationCatalog::Resolve(std::string_view key, std::string_view locale) const {
  const auto entry = entries_.find(std::string(key));
  if (entry == entries_.end())
    return key;
  if (const auto exact = entry->second.translations.find(std::string(locale));
      exact != entry->second.translations.end())
    return exact->second;
  if (const auto fallback = entry->second.translations.find(fallback_locale_);
      fallback != entry->second.translations.end())
    return fallback->second;
  return key;
}

bool AnimationPlayer::Play(AnimationClip clip) {
  if (clip.id == 0 || !std::isfinite(clip.duration) || clip.duration <= 0.0)
    return false;
  clip_ = clip;
  time_ = 0.0;
  playing_ = true;
  return true;
}

void AnimationPlayer::Advance(double seconds) {
  if (!playing_ || !std::isfinite(seconds) || seconds <= 0.0)
    return;
  time_ += seconds;
  if (time_ < clip_.duration)
    return;
  if (clip_.looping)
    time_ = std::fmod(time_, clip_.duration);
  else {
    time_ = clip_.duration;
    playing_ = false;
  }
}

bool AudioMixer::Play(AudioVoice voice) {
  if (voice.resource == 0 || !std::isfinite(voice.gain) || voice.gain < 0.0F ||
      voices_.size() >= voice_limit_)
    return false;
  voices_.push_back(voice);
  return true;
}

bool AudioMixer::Stop(Id resource) {
  const auto voice = std::ranges::find_if(
      voices_, [resource](const auto &candidate) { return candidate.resource == resource; });
  if (voice == voices_.end())
    return false;
  voices_.erase(voice);
  return true;
}

void AudioMixer::SetMasterGain(float gain) noexcept {
  master_gain_ = std::isfinite(gain) ? std::clamp(gain, 0.0F, 1.0F) : 0.0F;
}

bool MediaQueue::Push(VideoFrame frame) {
  if (capacity_ == 0 || frames_.size() >= capacity_ || frame.sequence <= last_sequence_ ||
      !std::isfinite(frame.presentation_time) || frame.presentation_time < seek_time_)
    return false;
  last_sequence_ = frame.sequence;
  frames_.push_back(frame);
  return true;
}

std::optional<VideoFrame> MediaQueue::PopReady(double clock) {
  if (frames_.empty() || !std::isfinite(clock) || frames_.front().presentation_time > clock)
    return std::nullopt;
  const auto frame = frames_.front();
  frames_.pop_front();
  return frame;
}

void MediaQueue::Seek(double presentation_time) {
  frames_.clear();
  last_sequence_ = 0;
  seek_time_ = std::isfinite(presentation_time) ? std::max(0.0, presentation_time) : 0.0;
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
                                // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
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
