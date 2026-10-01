#include "Nexora/Runtime/RenderSync.h"

#include "Nexora/Renderer/FramePipeline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nexora::runtime {
namespace {

// Largest eigenvalue of the symmetric 3x3 matrix `a` (row-major), closed form (Smith 1961).
double LargestSymmetricEigenvalue(const std::array<double, 9> &a) noexcept {
  const double off = a[1] * a[1] + a[2] * a[2] + a[5] * a[5];
  if (off == 0.0)
    return std::max({a[0], a[4], a[8]});
  const double q = (a[0] + a[4] + a[8]) / 3.0;
  const double d0 = a[0] - q, d1 = a[4] - q, d2 = a[8] - q;
  const double p = std::sqrt((d0 * d0 + d1 * d1 + d2 * d2 + 2.0 * off) / 6.0);
  if (!(p > 0.0))
    return q;
  // det((A - qI) / p) / 2, clamped against rounding.
  const double b0 = d0 / p, b1 = a[1] / p, b2 = a[2] / p, b4 = d1 / p, b5 = a[5] / p, b8 = d2 / p;
  const double det = b0 * (b4 * b8 - b5 * b5) - b1 * (b1 * b8 - b5 * b2) + b2 * (b1 * b5 - b4 * b2);
  const double r = std::clamp(det / 2.0, -1.0, 1.0);
  return q + 2.0 * p * std::cos(std::acos(r) / 3.0);
}

bool Finite(const math::Matrix4 &matrix) noexcept {
  return std::ranges::all_of(matrix.values, [](float value) { return std::isfinite(value); });
}

bool Finite(const math::Sphere &sphere) noexcept {
  return std::isfinite(sphere.center.x) && std::isfinite(sphere.center.y) &&
         std::isfinite(sphere.center.z) && std::isfinite(sphere.radius);
}

bool Same(const math::Sphere &a, const math::Sphere &b) noexcept {
  return a.center.x == b.center.x && a.center.y == b.center.y && a.center.z == b.center.z &&
         a.radius == b.radius;
}

// World matrices of one scene, each computed once: an entity's matrix is its parent's matrix times
// its local matrix, the same product, in the same order, as World::WorldMatrix. A walk longer than
// the scene (a cycle written directly into Entity::parent) fails instead of looping, and every
// entity on a failed walk is remembered, so later lookups through it fail at once and a scene full
// of broken chains still costs linear time.
class SceneMatrices final {
public:
  explicit SceneMatrices(const Scene &scene) {
    entities_.reserve(scene.entities.size());
    for (const auto &entity : scene.entities)
      entities_.emplace(entity.id, &entity);
  }

  const TransformMatrix *Of(const Entity &entity) {
    if (const auto found = matrices_.find(entity.id); found != matrices_.end())
      return &found->second;
    // Walk up to the first ancestor already computed (or the root), then compose downward.
    chain_.clear();
    const Entity *current = &entity;
    const TransformMatrix *base = nullptr;
    while (current != nullptr) {
      if (const auto found = matrices_.find(current->id); found != matrices_.end()) {
        base = &found->second;
        break;
      }
      if (invalid_.contains(current->id) || chain_.size() >= entities_.size()) {
        for (const auto *link : chain_)
          invalid_.insert(link->id);
        return nullptr;
      }
      chain_.push_back(current);
      if (current->parent == 0)
        break;
      const auto parent = entities_.find(current->parent);
      current = parent == entities_.end() ? nullptr : parent->second;
    }
    TransformMatrix result{};
    auto it = chain_.rbegin();
    if (base != nullptr) {
      result = MultiplyMatrices(*base, ToMatrix((*it)->transform));
    } else {
      result = ToMatrix((*it)->transform);
    }
    matrices_.emplace((*it)->id, result);
    for (++it; it != chain_.rend(); ++it) {
      result = MultiplyMatrices(result, ToMatrix((*it)->transform));
      matrices_.emplace((*it)->id, result);
    }
    return &matrices_.at(entity.id);
  }

private:
  std::unordered_map<Id, const Entity *> entities_;
  std::unordered_map<Id, TransformMatrix> matrices_;
  std::unordered_set<Id> invalid_;
  std::vector<const Entity *> chain_;
};

} // namespace

math::Matrix4 ToRenderMatrix(const TransformMatrix &matrix) noexcept {
  math::Matrix4 result;
  for (std::size_t row = 0; row < 4; ++row)
    for (std::size_t column = 0; column < 4; ++column)
      result(row, column) = static_cast<float>(matrix[column * 4 + row]);
  return result;
}

math::Sphere TransformBounds(const TransformMatrix &matrix, const math::Sphere &local) noexcept {
  // Bound what the GPU draws: the float matrix ToRenderMatrix uploads, not the double one, since
  // narrowing each coefficient can move a far-off point by more than a small radius.
  TransformMatrix m{};
  for (std::size_t index = 0; index < m.size(); ++index)
    m[index] = static_cast<double>(static_cast<float>(matrix[index]));
  const std::array<double, 3> c{local.center.x, local.center.y, local.center.z};
  const double r = local.radius;
  std::array<double, 3> center{};
  double worst_terms = 0.0;
  for (std::size_t row = 0; row < 3; ++row) {
    center[row] = m[row] * c[0] + m[4 + row] * c[1] + m[8 + row] * c[2] + m[12 + row];
    // Magnitude of the terms the GPU sums for any point of the sphere (|p_j| <= |c_j| + r).
    double terms = std::abs(m[12 + row]);
    for (std::size_t column = 0; column < 3; ++column)
      terms += std::abs(m[column * 4 + row]) * (std::abs(c[column]) + r);
    worst_terms = std::max(worst_terms, terms);
  }
  // The spectral norm of the linear part L is sqrt(largest eigenvalue of L^T L). Gram entries are
  // dot products of L's columns.
  const auto dot = [&m](std::size_t a, std::size_t b) {
    return m[a * 4] * m[b * 4] + m[a * 4 + 1] * m[b * 4 + 1] + m[a * 4 + 2] * m[b * 4 + 2];
  };
  const std::array<double, 9> gram{dot(0, 0), dot(0, 1), dot(0, 2), dot(1, 0), dot(1, 1),
                                   dot(1, 2), dot(2, 0), dot(2, 1), dot(2, 2)};
  // A column's length never exceeds the norm; taking the larger keeps the bound safe against
  // rounding in the closed form.
  const double eigenvalue = std::max({LargestSymmetricEigenvalue(gram), gram[0], gram[4], gram[8]});
  const double radius = r * std::sqrt(eigenvalue);
  // Per coordinate, the GPU's float evaluation of a 4-term sum errs by at most about 2 epsilon
  // times the sum of the terms' magnitudes (Higham's gamma_4; epsilon is twice the unit roundoff),
  // and narrowing the coefficients and the center adds at most epsilon times as much. 4 epsilon,
  // across three coordinates, covers both with room to spare, so no drawn point leaves the sphere;
  // the relative term absorbs rounding in the closed-form eigenvalue.
  const double epsilon = std::numeric_limits<float>::epsilon();
  const double margin = radius * 1e-6 + 4.0 * epsilon * std::sqrt(3.0) * worst_terms;
  const auto padded = static_cast<float>(radius + margin);
  return {
      {static_cast<float>(center[0]), static_cast<float>(center[1]), static_cast<float>(center[2])},
      std::nextafter(padded, std::numeric_limits<float>::infinity())};
}

std::optional<renderer::GPUDrivenView> CameraView(const World &world, Id camera, float aspect) {
  const auto *entity = world.FindEntity(camera);
  if (entity == nullptr || !entity->camera || !std::isfinite(aspect) || !(aspect > 0.0F))
    return std::nullopt;
  const auto &data = entity->camera_data;
  if (!std::isfinite(data.vertical_field_of_view) || !(data.vertical_field_of_view > 0.0) ||
      !(data.vertical_field_of_view < 180.0) || !std::isfinite(data.near_plane) ||
      !std::isfinite(data.far_plane) || !(data.near_plane > 0.0) ||
      !(data.far_plane > data.near_plane))
    return std::nullopt;
  const auto matrix = world.WorldMatrix(camera);
  if (!matrix)
    return std::nullopt;
  const auto &m = *matrix;
  // Orthonormalize the matrix's Z (backward) and Y (up) axes: scale and shear do not distort the
  // view, and the basis stays right-handed even under a mirroring parent.
  using Axis = std::array<double, 3>;
  const auto normalized = [](Axis v) -> std::optional<Axis> {
    const double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!std::isfinite(length) || !(length > 1e-12))
      return std::nullopt;
    return Axis{v[0] / length, v[1] / length, v[2] / length};
  };
  const auto cross = [](const Axis &a, const Axis &b) {
    return Axis{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  };
  const auto back = normalized({m[8], m[9], m[10]});
  if (!back)
    return std::nullopt;
  const auto right = normalized(cross({m[4], m[5], m[6]}, *back));
  if (!right)
    return std::nullopt;
  const auto up = cross(*back, *right);
  const Axis eye{m[12], m[13], m[14]};
  if (!std::isfinite(eye[0]) || !std::isfinite(eye[1]) || !std::isfinite(eye[2]))
    return std::nullopt;
  math::Matrix4 view;
  const std::array<const Axis *, 3> rows{&*right, &up, &*back};
  for (std::size_t row = 0; row < 3; ++row) {
    const auto &axis = *rows[row];
    for (std::size_t column = 0; column < 3; ++column)
      view(row, column) = static_cast<float>(axis[column]);
    view(row, 3) = static_cast<float>(-(axis[0] * eye[0] + axis[1] * eye[1] + axis[2] * eye[2]));
  }
  const auto near_plane = static_cast<float>(data.near_plane);
  const auto far_plane = static_cast<float>(data.far_plane);
  renderer::GPUDrivenView result;
  result.view_projection =
      math::PerspectiveRadians(math::Radians(static_cast<float>(data.vertical_field_of_view)),
                               aspect, near_plane, far_plane) *
      view;
  result.camera_position = {static_cast<float>(eye[0]), static_cast<float>(eye[1]),
                            static_cast<float>(eye[2])};
  result.maximum_distance = far_plane;
  if (!Finite(result.view_projection))
    return std::nullopt;
  return result;
}

RenderSceneSync::RenderSceneSync(RenderSceneSync &&other) noexcept
    : objects_(std::exchange(other.objects_, {})), scene_(std::exchange(other.scene_, nullptr)) {}

std::optional<RenderSyncStatistics> RenderSceneSync::Sync(const World &world,
                                                          renderer::GPUScene &scene,
                                                          const RenderResourceResolver &resolve,
                                                          std::uint64_t retire_fence) {
  // Handles carry no scene identity, so objects held in one GPUScene must never be read, updated,
  // or forgotten through another.
  if (scene_ != nullptr && scene_ != &scene)
    return std::nullopt;
  RenderSyncStatistics statistics;
  std::unordered_set<Id> current;
  for (const auto &world_scene : world.scenes_) {
    if (world_scene.state != SceneState::Active)
      continue;
    SceneMatrices matrices(world_scene);
    for (const auto &entity : world_scene.entities) {
      if (!entity.mesh_renderer || !resolve)
        continue;
      const auto binding = resolve(entity.mesh_data);
      if (!binding)
        continue;
      const auto *world_matrix = matrices.Of(entity);
      if (world_matrix == nullptr) {
        ++statistics.rejected;
        continue;
      }
      const auto &matrix = *world_matrix;
      const auto transform = ToRenderMatrix(matrix);
      const auto bounds = TransformBounds(matrix, binding->local_bounds);
      if (!Finite(transform) || !Finite(bounds)) {
        ++statistics.rejected;
        continue;
      }
      current.insert(entity.id);
      if (const auto found = objects_.find(entity.id); found != objects_.end()) {
        auto &mirror = found->second;
        // Only fields this sync owns are compared and written, so another system's visibility or
        // LOD choices survive.
        if (scene.Read(mirror.handle)) {
          bool changed = false;
          if (mirror.descriptor.current_transform.values != transform.values) {
            changed = scene.UpdateTransform(mirror.handle, transform) || changed;
            mirror.descriptor.current_transform = transform;
          }
          if (!Same(mirror.descriptor.world_bounds, bounds)) {
            changed = scene.UpdateBounds(mirror.handle, bounds) || changed;
            mirror.descriptor.world_bounds = bounds;
          }
          if (mirror.descriptor.mesh_resource_index != binding->mesh_resource_index ||
              mirror.descriptor.material_resource_index != binding->material_resource_index) {
            changed = scene.UpdateResources(mirror.handle, binding->mesh_resource_index,
                                            binding->material_resource_index) ||
                      changed;
            mirror.descriptor.mesh_resource_index = binding->mesh_resource_index;
            mirror.descriptor.material_resource_index = binding->material_resource_index;
          }
          statistics.updated += changed ? 1U : 0U;
          continue;
        }
        // The object was destroyed behind the sync's back; mirror the entity again.
        objects_.erase(found);
      }
      renderer::GPUObjectDescriptor descriptor;
      descriptor.current_transform = transform;
      descriptor.previous_transform = transform;
      descriptor.world_bounds = bounds;
      descriptor.mesh_resource_index = binding->mesh_resource_index;
      descriptor.material_resource_index = binding->material_resource_index;
      const auto handle = scene.Create(descriptor);
      objects_.emplace(entity.id, Mirror{handle, descriptor});
      ++statistics.created;
    }
  }
  // Destroy in id order so slot reuse in the GPUScene stays deterministic.
  std::vector<Id> gone;
  for (const auto &[id, mirror] : objects_)
    if (!current.contains(id))
      gone.push_back(id);
  std::ranges::sort(gone);
  for (const auto id : gone) {
    static_cast<void>(scene.Destroy(objects_.at(id).handle, retire_fence));
    objects_.erase(id);
    ++statistics.destroyed;
  }
  scene_ = objects_.empty() ? nullptr : &scene;
  return statistics;
}

std::optional<CulledSceneFrame> RenderSceneSync::RenderFrame(
    const World &world, renderer::GPUScene &scene, const RenderResourceResolver &resolve,
    std::uint64_t retire_fence, rhi::Device &device, rhi::TextureHandle target,
    const rhi::TextureDescriptor &target_descriptor, rhi::PipelineHandle pipeline) {
  const auto synced = Sync(world, scene, resolve, retire_fence);
  if (!synced)
    return std::nullopt;
  CulledSceneFrame result;
  result.sync = *synced;
#if !NEXORA_SCENE_RENDERING_ENABLED
  static_cast<void>(device);
  static_cast<void>(target);
  static_cast<void>(target_descriptor);
  static_cast<void>(pipeline);
  return std::nullopt;
#else
  bool light = false;
  for (const auto &world_scene : world.scenes_) {
    if (world_scene.state != SceneState::Active)
      continue;
    for (const auto &entity : world_scene.entities) {
      if (result.camera == 0 && entity.camera)
        result.camera = entity.id;
      light = light || entity.light;
    }
  }
  if (result.camera == 0 || !light || target_descriptor.height == 0)
    return std::nullopt;
  const auto view = CameraView(world, result.camera,
                               static_cast<float>(target_descriptor.width) /
                                   static_cast<float>(target_descriptor.height));
  if (!view)
    return std::nullopt;
  const auto culled = renderer::BuildGPUDrivenCommands(scene.ExtractReferenceSnapshot(), *view);
  result.culling = culled.statistics;
  const auto executed = renderer::ExecuteSceneFrame(device, target, target_descriptor, pipeline,
                                                    culled.instances.size());
  result.frame = {culled.instances.size(), executed.passes, executed.barriers};
  return result;
#endif
}

std::optional<renderer::GPUObjectHandle> RenderSceneSync::Handle(Id entity) const {
  if (const auto found = objects_.find(entity); found != objects_.end())
    return found->second.handle;
  return std::nullopt;
}

bool RenderSceneSync::Release(renderer::GPUScene &scene, std::uint64_t retire_fence) {
  if (scene_ != nullptr && scene_ != &scene)
    return false;
  std::vector<Id> ids;
  ids.reserve(objects_.size());
  for (const auto &[id, mirror] : objects_)
    ids.push_back(id);
  std::ranges::sort(ids);
  for (const auto id : ids)
    static_cast<void>(scene.Destroy(objects_.at(id).handle, retire_fence));
  objects_.clear();
  scene_ = nullptr;
  return true;
}

} // namespace nexora::runtime
