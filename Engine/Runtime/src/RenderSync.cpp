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
// its local matrix, the same product, in the same order, as World::WorldMatrix. A walk that reaches
// an entity it already passed (a cycle written directly into Entity::parent) fails at once instead
// of looping, and every entity on a failed walk is remembered, so later lookups through it fail
// immediately. Each entity is walked over at most once per successful or failed chain, so even a
// scene of many small cycles costs linear time.
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
    on_chain_.clear();
    const Entity *current = &entity;
    const TransformMatrix *base = nullptr;
    while (current != nullptr) {
      if (const auto found = matrices_.find(current->id); found != matrices_.end()) {
        base = &found->second;
        break;
      }
      if (invalid_.contains(current->id) || !on_chain_.insert(current->id).second) {
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
  std::unordered_set<Id> on_chain_;
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
  // That bound only holds without overflow: every partial sum the GPU forms is at most the sum of
  // the terms' magnitudes, so if that sum (with rounding room) could exceed the float range, an
  // intermediate value may be inf even when the result is finite. Report such bounds as unbounded
  // (an infinite radius), which the sync rejects.
  if (!(worst_terms * (1.0 + 8.0 * epsilon) <= std::numeric_limits<float>::max()))
    return {{}, std::numeric_limits<float>::infinity()};
  // Gradual underflow adds an absolute error of at most half the smallest subnormal per operation.
  const double margin = radius * 1e-6 + 4.0 * epsilon * std::sqrt(3.0) * worst_terms +
                        4.0 * std::numeric_limits<float>::denorm_min();
  const auto padded = static_cast<float>(radius + margin);
  return {
      {static_cast<float>(center[0]), static_cast<float>(center[1]), static_cast<float>(center[2])},
      std::nextafter(padded, std::numeric_limits<float>::infinity())};
}

std::optional<renderer::GPUDrivenView> CameraView(const World &world, Id camera, float aspect) {
  const auto *entity = world.FindEntity(camera);
  if (entity == nullptr || !entity->camera || !std::isfinite(aspect) || !(aspect > 0.0F))
    return std::nullopt;
  // Validate the values the projection actually uses: narrowing can turn an accepted double into a
  // degenerate float (a field of view just under 180 rounds to 180; a tiny near plane rounds to 0).
  const auto &data = entity->camera_data;
  const auto field_of_view = static_cast<float>(data.vertical_field_of_view);
  const auto near_plane = static_cast<float>(data.near_plane);
  const auto far_plane = static_cast<float>(data.far_plane);
  if (!std::isfinite(field_of_view) || !(field_of_view > 0.0F) || !(field_of_view < 180.0F) ||
      !std::isfinite(near_plane) || !std::isfinite(far_plane) || !(near_plane > 0.0F) ||
      !(far_plane > near_plane))
    return std::nullopt;
  // Unity's Camera: the position comes from the exact world matrix, the orientation from the world
  // rotation (the product of the chain's rotations), so a non-uniformly or negatively scaled parent
  // neither skews nor flips the view.
  const auto matrix = world.WorldMatrix(camera);
  const auto pose = world.WorldTransform(camera);
  if (!matrix || !pose)
    return std::nullopt;
  Transform orientation{};
  orientation.qx = pose->qx;
  orientation.qy = pose->qy;
  orientation.qz = pose->qz;
  orientation.qw = pose->qw;
  const auto rotation = ToMatrix(orientation);
  const auto &m = *matrix;
  using Axis = std::array<double, 3>;
  const Axis eye{m[12], m[13], m[14]};
  if (!std::isfinite(eye[0]) || !std::isfinite(eye[1]) || !std::isfinite(eye[2]))
    return std::nullopt;
  // Rows of the view matrix: the world rotation's right (+X), up (+Y), and back (+Z) axes; the
  // camera looks down -Z (Nexora is right-handed).
  math::Matrix4 view;
  for (std::size_t row = 0; row < 3; ++row) {
    const Axis axis{rotation[row * 4], rotation[row * 4 + 1], rotation[row * 4 + 2]};
    for (std::size_t column = 0; column < 3; ++column)
      view(row, column) = static_cast<float>(axis[column]);
    view(row, 3) = static_cast<float>(-(axis[0] * eye[0] + axis[1] * eye[1] + axis[2] * eye[2]));
  }
  renderer::GPUDrivenView result;
  result.view_projection =
      math::PerspectiveRadians(math::Radians(field_of_view), aspect, near_plane, far_plane) * view;
  result.camera_position = {static_cast<float>(eye[0]), static_cast<float>(eye[1]),
                            static_cast<float>(eye[2])};
  // The far plane is already part of the frustum; a radial distance limit at the same value would
  // cut off the frustum's far corners.
  result.maximum_distance = std::numeric_limits<float>::max();
  if (!Finite(result.view_projection))
    return std::nullopt;
  return result;
}

RenderSceneSync::RenderSceneSync(RenderSceneSync &&other) noexcept
    : objects_(std::exchange(other.objects_, {})), scene_id_(std::exchange(other.scene_id_, 0)) {}

std::optional<RenderSyncStatistics> RenderSceneSync::Sync(const World &world,
                                                          renderer::GPUScene &scene,
                                                          const RenderResourceResolver &resolve,
                                                          std::uint64_t retire_fence) {
  // Handles carry no scene identity, so objects held in one GPUScene must never be read, updated,
  // or forgotten through another.
  if (scene_id_ != 0 && scene_id_ != scene.InstanceId())
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
  scene_id_ = objects_.empty() ? 0 : scene.InstanceId();
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
  if (scene_id_ != 0 && scene_id_ != scene.InstanceId())
    return false;
  std::vector<Id> ids;
  ids.reserve(objects_.size());
  for (const auto &[id, mirror] : objects_)
    ids.push_back(id);
  std::ranges::sort(ids);
  for (const auto id : ids)
    static_cast<void>(scene.Destroy(objects_.at(id).handle, retire_fence));
  objects_.clear();
  scene_id_ = 0;
  return true;
}

void RenderSceneSync::Abandon() noexcept {
  objects_.clear();
  scene_id_ = 0;
}

} // namespace nexora::runtime
