#include "Nexora/Runtime/RenderSync.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
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
// the scene (a cycle written directly into Entity::parent) fails instead of looping.
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
      if (chain_.size() >= entities_.size())
        return nullptr;
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

math::Sphere TransformBounds(const TransformMatrix &m, const math::Sphere &local) noexcept {
  const double x = local.center.x, y = local.center.y, z = local.center.z;
  const double cx = m[0] * x + m[4] * y + m[8] * z + m[12];
  const double cy = m[1] * x + m[5] * y + m[9] * z + m[13];
  const double cz = m[2] * x + m[6] * y + m[10] * z + m[14];
  // The spectral norm of the linear part L is sqrt(largest eigenvalue of L^T L). Gram entries are
  // dot products of L's columns.
  const auto dot = [&m](int a, int b) {
    return m[a * 4] * m[b * 4] + m[a * 4 + 1] * m[b * 4 + 1] + m[a * 4 + 2] * m[b * 4 + 2];
  };
  const std::array<double, 9> gram{dot(0, 0), dot(0, 1), dot(0, 2), dot(1, 0), dot(1, 1),
                                   dot(1, 2), dot(2, 0), dot(2, 1), dot(2, 2)};
  // A column's length never exceeds the norm; taking the larger keeps the bound safe against
  // rounding in the closed form.
  const double eigenvalue = std::max({LargestSymmetricEigenvalue(gram), gram[0], gram[4], gram[8]});
  const double radius = static_cast<double>(local.radius) * std::sqrt(eigenvalue);
  // Round the float radius up (with a relative margin for the float center) so narrowing never
  // shrinks the sphere.
  const double margin =
      radius * 1e-6 + (std::abs(cx) + std::abs(cy) + std::abs(cz)) *
                          static_cast<double>(std::numeric_limits<float>::epsilon());
  const auto padded = static_cast<float>(radius + margin);
  return {{static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)},
          std::nextafter(padded, std::numeric_limits<float>::infinity())};
}

RenderSyncStatistics RenderSceneSync::Sync(const World &world, renderer::GPUScene &scene,
                                           const RenderResourceResolver &resolve,
                                           std::uint64_t retire_fence) {
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
  return statistics;
}

std::optional<renderer::GPUObjectHandle> RenderSceneSync::Handle(Id entity) const {
  if (const auto found = objects_.find(entity); found != objects_.end())
    return found->second.handle;
  return std::nullopt;
}

void RenderSceneSync::Release(renderer::GPUScene &scene, std::uint64_t retire_fence) {
  std::vector<Id> ids;
  ids.reserve(objects_.size());
  for (const auto &[id, mirror] : objects_)
    ids.push_back(id);
  std::ranges::sort(ids);
  for (const auto id : ids)
    static_cast<void>(scene.Destroy(objects_.at(id).handle, retire_fence));
  objects_.clear();
}

} // namespace nexora::runtime
