#include "Nexora/Runtime/Runtime.h"
#include "Nexora/Renderer/FramePipeline.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace nexora::runtime {

namespace {
struct Quat final {
  double x, y, z, w;
};
struct Vec3 final {
  double x, y, z;
};
Quat RotationOf(const Transform &t) noexcept { return {t.qx, t.qy, t.qz, t.qw}; }
Quat Conjugate(const Quat &q) noexcept { return {-q.x, -q.y, -q.z, q.w}; }
Quat Multiply(const Quat &a, const Quat &b) noexcept {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Vec3 Rotate(const Quat &q, const Vec3 &v) noexcept {
  // v' = v + 2w(u x v) + 2u x (u x v), with u the vector part of the unit quaternion.
  const Vec3 t{2.0 * (q.y * v.z - q.z * v.y), 2.0 * (q.z * v.x - q.x * v.z),
               2.0 * (q.x * v.y - q.y * v.x)};
  return {v.x + q.w * t.x + (q.y * t.z - q.z * t.y), v.y + q.w * t.y + (q.z * t.x - q.x * t.z),
          v.z + q.w * t.z + (q.x * t.y - q.y * t.x)};
}
void SetRotation(Transform &t, Quat q) noexcept {
  const auto length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (length > 0.0 && std::isfinite(length)) {
    q.x /= length;
    q.y /= length;
    q.z /= length;
    q.w /= length;
  }
  t.qx = q.x;
  t.qy = q.y;
  t.qz = q.z;
  t.qw = q.w;
}
TransformMatrix Multiply(const TransformMatrix &a, const TransformMatrix &b) noexcept {
  TransformMatrix result{};
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row) {
      double sum = 0.0;
      for (int k = 0; k < 4; ++k)
        sum += a[k * 4 + row] * b[column * 4 + k];
      result[column * 4 + row] = sum;
    }
  return result;
}
} // namespace

TransformMatrix ToMatrix(const Transform &t) noexcept {
  const auto q = RotationOf(t);
  const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
  const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
  const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  // Column-major translation * rotation * scale.
  return {(1.0 - 2.0 * (yy + zz)) * t.sx,
          2.0 * (xy + wz) * t.sx,
          2.0 * (xz - wy) * t.sx,
          0.0,
          2.0 * (xy - wz) * t.sy,
          (1.0 - 2.0 * (xx + zz)) * t.sy,
          2.0 * (yz + wx) * t.sy,
          0.0,
          2.0 * (xz + wy) * t.sz,
          2.0 * (yz - wx) * t.sz,
          (1.0 - 2.0 * (xx + yy)) * t.sz,
          0.0,
          t.x,
          t.y,
          t.z,
          1.0};
}

Transform ComposeTransforms(const Transform &parent, const Transform &child) noexcept {
  const auto rotation = RotationOf(parent);
  const auto offset =
      Rotate(rotation, {parent.sx * child.x, parent.sy * child.y, parent.sz * child.z});
  Transform result{parent.x + offset.x, parent.y + offset.y, parent.z + offset.z};
  SetRotation(result, Multiply(rotation, RotationOf(child)));
  result.sx = parent.sx * child.sx;
  result.sy = parent.sy * child.sy;
  result.sz = parent.sz * child.sz;
  return result;
}

Transform RelativeTransform(const Transform &parent, const Transform &world) noexcept {
  const auto inverse = Conjugate(RotationOf(parent));
  const auto local = Rotate(inverse, {world.x - parent.x, world.y - parent.y, world.z - parent.z});
  Transform result{local.x / parent.sx, local.y / parent.sy, local.z / parent.sz};
  SetRotation(result, Multiply(inverse, RotationOf(world)));
  result.sx = world.sx / parent.sx;
  result.sy = world.sy / parent.sy;
  result.sz = world.sz / parent.sz;
  return result;
}

bool IsValidTransform(const Transform &transform) noexcept {
  const double values[] = {transform.x,  transform.y,  transform.z,  transform.qx, transform.qy,
                           transform.qz, transform.qw, transform.sx, transform.sy, transform.sz};
  for (const auto value : values)
    if (!std::isfinite(value))
      return false;
  if (transform.sx == 0.0 || transform.sy == 0.0 || transform.sz == 0.0)
    return false;
  const auto length = std::sqrt(transform.qx * transform.qx + transform.qy * transform.qy +
                                transform.qz * transform.qz + transform.qw * transform.qw);
  // Tiny lengths cannot be normalized meaningfully; an overflowed length is not finite.
  return std::isfinite(length) && length > 1e-12;
}

std::optional<Transform> NormalizedTransform(Transform transform) noexcept {
  if (!IsValidTransform(transform))
    return std::nullopt;
  const auto length = std::sqrt(transform.qx * transform.qx + transform.qy * transform.qy +
                                transform.qz * transform.qz + transform.qw * transform.qw);
  transform.qx /= length;
  transform.qy /= length;
  transform.qz /= length;
  transform.qw /= length;
  return transform;
}

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
  // The classic locale keeps the text identical regardless of the process locale.
  output.imbue(std::locale::classic());
  output << "NEXORA_SCENE 3 " << std::quoted(scene->name) << ' ' << scene->persistent << ' '
         << scene->entities.size() << '\n';
  output << std::setprecision(17);
  for (const auto &entity : scene->entities)
    output << entity.id << ' ' << entity.parent << ' ' << entity.transform.x << ' '
           << entity.transform.y << ' ' << entity.transform.z << ' ' << entity.transform.qx << ' '
           << entity.transform.qy << ' ' << entity.transform.qz << ' ' << entity.transform.qw << ' '
           << entity.transform.sx << ' ' << entity.transform.sy << ' ' << entity.transform.sz << ' '
           << entity.camera << ' ' << entity.light << ' ' << entity.mesh_renderer << ' '
           << entity.camera_data.vertical_field_of_view << ' ' << entity.camera_data.near_plane
           << ' ' << entity.camera_data.far_plane << ' ' << entity.light_data.intensity << ' '
           << entity.mesh_data.mesh << ' ' << entity.mesh_data.material.shader << '\n';
  return output.str();
}

std::optional<Id> World::LoadSceneSnapshot(std::string_view snapshot) {
  std::istringstream input{std::string(snapshot)};
  input.imbue(std::locale::classic());
  std::string magic, name;
  unsigned version{};
  bool persistent{};
  std::size_t count{};
  // Version 1 stored only a position, version 2 added rotation and scale, and version 3 adds the
  // parent id. All stay readable: a version 1 entity loads with identity rotation and unit scale,
  // and version 1 and 2 entities load as roots.
  if (!(input >> magic >> version >> std::quoted(name) >> persistent >> count) ||
      magic != "NEXORA_SCENE" || version < 1 || version > 3 || name.empty())
    return std::nullopt;
  // `count` comes from the snapshot itself, so it can claim billions of entities and make the
  // reservation throw or allocate gigabytes. An entity record is 13, 20, or 21 whitespace-separated
  // tokens (versions 1, 2, 3), so it needs at least 25, 39, or 41 characters; reject counts the
  // text cannot hold, and never reserve more than a small constant up front (the vector grows as
  // records are actually parsed).
  const std::size_t min_entity_text_size = version == 1 ? 25 : version == 2 ? 39 : 41;
  constexpr std::size_t kMaxInitialReserve = 1024;
  if (count > snapshot.size() / min_entity_text_size)
    return std::nullopt;
  Scene loaded{next_id_, std::move(name), SceneState::LoadedInactive, persistent, {}};
  loaded.entities.reserve(std::min(count, kMaxInitialReserve));
  std::unordered_set<Id> ids;
  auto next_id = next_id_ + 1;
  for (std::size_t index = 0; index < count; ++index) {
    Entity entity;
    if (!(input >> entity.id) || (version >= 3 && !(input >> entity.parent)) ||
        !(input >> entity.transform.x >> entity.transform.y >> entity.transform.z))
      return std::nullopt;
    if (version >= 2 &&
        !(input >> entity.transform.qx >> entity.transform.qy >> entity.transform.qz >>
          entity.transform.qw >> entity.transform.sx >> entity.transform.sy >> entity.transform.sz))
      return std::nullopt;
    if (!(input >> entity.camera >> entity.light >> entity.mesh_renderer >>
          entity.camera_data.vertical_field_of_view >> entity.camera_data.near_plane >>
          entity.camera_data.far_plane >> entity.light_data.intensity >> entity.mesh_data.mesh >>
          entity.mesh_data.material.shader) ||
        entity.id == 0 || FindEntity(entity.id) != nullptr || !ids.insert(entity.id).second)
      return std::nullopt;
    // Rejects non-finite values, a zero scale, and a degenerate quaternion; a valid quaternion that
    // is not unit length is normalized so a loaded scene always holds unit rotations.
    const auto normalized = NormalizedTransform(entity.transform);
    if (!normalized)
      return std::nullopt;
    entity.transform = *normalized;
    next_id = std::max(next_id, entity.id + 1);
    loaded.entities.push_back(entity);
  }
  input >> std::ws;
  if (!input.eof())
    return std::nullopt;
  // Parents must be entities of this snapshot, never the entity itself, and the hierarchy must be
  // acyclic. Each entity is walked up only until it reaches an entity already proven to lead to a
  // root, so the whole check is linear in the entity count even for one long chain.
  std::unordered_map<Id, Id> parents;
  for (const auto &entity : loaded.entities)
    parents.emplace(entity.id, entity.parent);
  enum class Mark : unsigned char { Walking, Rooted };
  std::unordered_map<Id, Mark> marks;
  std::vector<Id> path;
  for (const auto &entity : loaded.entities) {
    path.clear();
    for (auto current = entity.id; current != 0 && !marks.contains(current);
         current = parents.at(current)) {
      const auto parent = parents.at(current);
      if (parent != 0 && !parents.contains(parent))
        return std::nullopt;
      marks.emplace(current, Mark::Walking);
      path.push_back(current);
      // Reaching an entity of the current walk again is a cycle (self-parenting included).
      if (const auto found = marks.find(parent);
          found != marks.end() && found->second == Mark::Walking)
        return std::nullopt;
    }
    for (const auto id : path)
      marks[id] = Mark::Rooted;
  }
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

std::optional<Id> World::Parent(Id entity) const {
  const auto *found = FindEntity(entity);
  return found == nullptr ? std::nullopt : std::optional<Id>(found->parent);
}

std::vector<Id> World::Children(Id entity) const {
  std::vector<Id> children;
  if (entity == 0)
    return children;
  for (const auto &scene : scenes_)
    if (scene.state != SceneState::Unloaded &&
        std::ranges::find(scene.entities, entity, &Entity::id) != scene.entities.end()) {
      for (const auto &candidate : scene.entities)
        if (candidate.parent == entity)
          children.push_back(candidate.id);
      break;
    }
  return children;
}

std::vector<Id> World::Subtree(Id entity) const {
  std::vector<Id> subtree;
  for (const auto &scene : scenes_) {
    if (scene.state == SceneState::Unloaded ||
        std::ranges::find(scene.entities, entity, &Entity::id) == scene.entities.end())
      continue;
    // One pass builds the child lists, so the walk is linear in the scene size. The visited set
    // keeps a hierarchy corrupted by direct writes to Entity::parent from looping.
    std::unordered_map<Id, std::vector<Id>> children;
    for (const auto &candidate : scene.entities)
      if (candidate.parent != 0)
        children[candidate.parent].push_back(candidate.id);
    std::unordered_set<Id> visited{entity};
    subtree.push_back(entity);
    for (std::size_t index = 0; index < subtree.size(); ++index)
      if (const auto found = children.find(subtree[index]); found != children.end())
        for (const auto child : found->second)
          if (visited.insert(child).second)
            subtree.push_back(child);
    break;
  }
  return subtree;
}

namespace {
// The entity followed by its ancestors, or empty when the entity does not exist. The hierarchy is
// validated acyclic on every mutation and load; the step limit only guards against corruption.
std::vector<const Entity *> Ancestry(const World &world, Id entity) {
  std::vector<const Entity *> chain;
  for (auto *current = world.FindEntity(entity); current != nullptr;
       current = current->parent == 0 ? nullptr : world.FindEntity(current->parent)) {
    if (chain.size() > 1'000'000)
      return {};
    chain.push_back(current);
  }
  return chain;
}
} // namespace

std::optional<Transform> World::WorldTransform(Id entity) const {
  const auto chain = Ancestry(*this, entity);
  if (chain.empty())
    return std::nullopt;
  auto result = chain.back()->transform;
  for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it)
    result = ComposeTransforms(result, (*it)->transform);
  return result;
}

std::optional<TransformMatrix> World::WorldMatrix(Id entity) const {
  const auto chain = Ancestry(*this, entity);
  if (chain.empty())
    return std::nullopt;
  auto result = ToMatrix(chain.back()->transform);
  for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it)
    result = Multiply(result, ToMatrix((*it)->transform));
  return result;
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
  Command command{};
  command.entity = entity;
  command.kind = Command::Kind::Transform;
  command.transform = transform;
  commands_.push_back(command);
}
void WorldCommandBuffer::SetParent(Id entity, Id parent, bool keep_world) {
  Command command{};
  command.entity = entity;
  command.kind = Command::Kind::Parent;
  command.parent = parent;
  command.keep_world = keep_world;
  commands_.push_back(command);
}
void WorldCommandBuffer::SetCamera(Id entity, std::optional<CameraComponent> camera) {
  Command command{};
  command.entity = entity;
  command.kind = Command::Kind::Camera;
  command.camera = camera;
  commands_.push_back(command);
}
void WorldCommandBuffer::SetLight(Id entity, std::optional<LightComponent> light) {
  Command command{};
  command.entity = entity;
  command.kind = Command::Kind::Light;
  command.light = light;
  commands_.push_back(command);
}
void WorldCommandBuffer::SetMeshRenderer(Id entity, std::optional<MeshComponent> mesh) {
  Command command{};
  command.entity = entity;
  command.kind = Command::Kind::MeshRenderer;
  command.mesh = mesh;
  commands_.push_back(command);
}
void WorldCommandBuffer::DestroyEntity(Id entity) {
  Command command{};
  command.entity = entity;
  command.kind = Command::Kind::Destroy;
  commands_.push_back(command);
}

bool WorldCommandBuffer::Apply(World &world) {
  last_destroyed_.clear();
  // Pass 1 validates the whole batch without mutating the world. Reparenting and destruction depend
  // on the hierarchy as it will be after the earlier commands of the same batch, so those batches
  // are checked against a simulated copy of each entity's scene and parent.
  const bool hierarchy = std::ranges::any_of(commands_, [](const Command &command) {
    return command.kind == Command::Kind::Parent || command.kind == Command::Kind::Destroy;
  });
  struct Node final {
    Id scene{};
    Id parent{};
  };
  std::unordered_map<Id, Node> nodes;
  if (hierarchy)
    for (const auto &scene : world.scenes_)
      if (scene.state != SceneState::Unloaded)
        for (const auto &entity : scene.entities)
          nodes.emplace(entity.id, Node{scene.id, entity.parent});
  std::unordered_set<Id> destroyed;
  const auto alive = [&](Id id) {
    if (destroyed.contains(id))
      return false;
    return hierarchy ? nodes.contains(id) : world.FindEntity(id) != nullptr;
  };
  for (const auto &command : commands_) {
    if (!alive(command.entity))
      return false;
    if (command.kind == Command::Kind::Transform && !IsValidTransform(command.transform))
      return false;
    if (command.kind == Command::Kind::Parent) {
      if (command.parent != 0) {
        if (command.parent == command.entity || !alive(command.parent) ||
            nodes.at(command.parent).scene != nodes.at(command.entity).scene)
          return false;
        // The new parent must not be the entity itself or one of its descendants.
        for (auto ancestor = command.parent; ancestor != 0; ancestor = nodes.at(ancestor).parent)
          if (ancestor == command.entity)
            return false;
      }
      nodes.at(command.entity).parent = command.parent;
    }
    if (command.kind == Command::Kind::Destroy) {
      std::unordered_map<Id, std::vector<Id>> children;
      for (const auto &[id, node] : nodes)
        if (node.parent != 0 && !destroyed.contains(id))
          children[node.parent].push_back(id);
      std::vector<Id> doomed{command.entity};
      destroyed.insert(command.entity);
      for (std::size_t index = 0; index < doomed.size(); ++index)
        if (const auto found = children.find(doomed[index]); found != children.end())
          for (const auto child : found->second)
            if (destroyed.insert(child).second)
              doomed.push_back(child);
    }
  }

  // Pass 2 applies in order; every command was proven valid against the state it will see. The one
  // thing pass 1 cannot prove is that a keep-world reparent yields a representable local transform
  // (two valid poses can still overflow when re-expressed), so a batch with one keeps a copy of the
  // scenes and restores it if that happens, keeping the batch all-or-nothing.
  std::optional<std::vector<Scene>> rollback;
  if (std::ranges::any_of(commands_, [](const Command &command) {
        return command.kind == Command::Kind::Parent && command.keep_world;
      }))
    rollback = world.scenes_;
  const auto locate = [&world](Id id) -> std::pair<Scene *, Entity *> {
    for (auto &scene : world.scenes_)
      if (const auto found = std::ranges::find(scene.entities, id, &Entity::id);
          found != scene.entities.end())
        return {&scene, &*found};
    return {nullptr, nullptr};
  };
  for (const auto &command : commands_) {
    const auto [scene, found] = locate(command.entity);
    if (found == nullptr)
      continue;
    if (command.kind == Command::Kind::Transform) {
      found->transform = *NormalizedTransform(command.transform);
    } else if (command.kind == Command::Kind::Parent) {
      if (command.keep_world) {
        // Keep the world pose (Unity's worldPositionStays): re-express it under the new parent.
        auto local = *world.WorldTransform(command.entity);
        if (command.parent != 0)
          local = RelativeTransform(*world.WorldTransform(command.parent), local);
        const auto normalized = NormalizedTransform(local);
        if (!normalized) {
          world.scenes_ = std::move(*rollback);
          last_destroyed_.clear();
          return false;
        }
        found->transform = *normalized;
      }
      found->parent = command.parent;
    } else if (command.kind == Command::Kind::Camera) {
      found->camera = command.camera.has_value();
      found->camera_data = command.camera.value_or(CameraComponent{});
    } else if (command.kind == Command::Kind::Light) {
      found->light = command.light.has_value();
      found->light_data = command.light.value_or(LightComponent{});
    } else if (command.kind == Command::Kind::MeshRenderer) {
      found->mesh_renderer = command.mesh.has_value();
      found->mesh_data = command.mesh.value_or(MeshComponent{});
    } else {
      const auto doomed = world.Subtree(command.entity);
      const std::unordered_set<Id> doomed_ids(doomed.begin(), doomed.end());
      std::erase_if(scene->entities,
                    [&doomed_ids](const Entity &entity) { return doomed_ids.contains(entity.id); });
      last_destroyed_.insert(last_destroyed_.end(), doomed.begin(), doomed.end());
    }
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

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
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
#endif

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

} // namespace nexora::runtime
