#include "Nexora/Renderer/GPUDrivenPipeline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace nexora::renderer {
namespace {

bool InFrustum(const math::Frustum &frustum, const math::Sphere &sphere) {
  return std::ranges::all_of(frustum.planes, [&](const math::Plane &plane) {
    return math::Dot(plane.normal, sphere.center) + plane.distance >= -sphere.radius;
  });
}

std::uint32_t SelectLOD(float distance, std::uint32_t count, const std::vector<float> &thresholds) {
  std::uint32_t lod{};
  while (lod + 1 < count && lod < thresholds.size() && distance >= thresholds[lod])
    ++lod;
  return lod;
}

bool IsOccluded(const GPUObjectDescriptor &object, const GPUDrivenView &view) {
  if (view.hi_z == nullptr || view.hi_z_policy == HiZPolicy::Invalidated || view.hi_z->mips.empty())
    return false;
  const auto &m = view.view_projection;
  const auto c = object.world_bounds.center;
  const float x = m(0, 0) * c.x + m(0, 1) * c.y + m(0, 2) * c.z + m(0, 3);
  const float y = m(1, 0) * c.x + m(1, 1) * c.y + m(1, 2) * c.z + m(1, 3);
  const float z = m(2, 0) * c.x + m(2, 1) * c.y + m(2, 2) * c.z + m(2, 3);
  const float w = m(3, 0) * c.x + m(3, 1) * c.y + m(3, 2) * c.z + m(3, 3);
  if (w <= math::kEpsilon)
    return false;
  const float radius_ndc = object.world_bounds.radius / w;
  float diameter = radius_ndc * static_cast<float>(std::max(view.hi_z->width, view.hi_z->height));
  std::size_t mip{};
  while (mip + 1 < view.hi_z->mips.size() && diameter > 2.0F) {
    diameter *= 0.5F;
    ++mip;
  }
  const std::uint32_t mip_width = std::max(1U, view.hi_z->width >> mip);
  const std::uint32_t mip_height = std::max(1U, view.hi_z->height >> mip);
  const auto px = static_cast<std::uint32_t>(
      std::clamp((x / w * 0.5F + 0.5F) * mip_width, 0.0F, static_cast<float>(mip_width - 1)));
  const auto py = static_cast<std::uint32_t>(
      std::clamp((-y / w * 0.5F + 0.5F) * mip_height, 0.0F, static_cast<float>(mip_height - 1)));
  const float nearest_depth = z / w - radius_ndc;
  const float bias =
      view.hi_z_policy == HiZPolicy::Relaxed ? view.occlusion_bias * 4.0F : view.occlusion_bias;
  return nearest_depth > view.hi_z->mips[mip][py * mip_width + px] + bias;
}

} // namespace

HiZPyramid HiZPyramid::Build(std::uint32_t width, std::uint32_t height,
                             std::span<const float> depth) {
  HiZPyramid result;
  if (width == 0 || height == 0 || depth.size() != static_cast<std::size_t>(width) * height)
    return result;
  result.width = width;
  result.height = height;
  result.mips.emplace_back(depth.begin(), depth.end());
  auto source_width = width;
  auto source_height = height;
  while (source_width > 1 || source_height > 1) {
    const auto target_width = std::max(1U, source_width / 2U);
    const auto target_height = std::max(1U, source_height / 2U);
    std::vector<float> target(target_width * target_height);
    const auto &source = result.mips.back();
    for (std::uint32_t y = 0; y < target_height; ++y) {
      for (std::uint32_t x = 0; x < target_width; ++x) {
        float farthest{};
        for (std::uint32_t oy = 0; oy < 2; ++oy)
          for (std::uint32_t ox = 0; ox < 2; ++ox)
            farthest =
                std::max(farthest, source[std::min(y * 2 + oy, source_height - 1) * source_width +
                                          std::min(x * 2 + ox, source_width - 1)]);
        target[y * target_width + x] = farthest;
      }
    }
    result.mips.push_back(std::move(target));
    source_width = target_width;
    source_height = target_height;
  }
  return result;
}

GPUDrivenResult BuildGPUDrivenCommands(const GPUSceneReferenceSnapshot &scene,
                                       const GPUDrivenView &view) {
  GPUDrivenResult result;
  const auto frustum = math::ExtractFrustum(view.view_projection);
  result.statistics.candidates = static_cast<std::uint32_t>(scene.objects.size());
  for (const auto &entry : scene.objects) {
    const auto &object = entry.descriptor;
    if ((object.visibility_flags & VisibilityFlags(GPUObjectVisibility::Visible)) == 0)
      continue;
    if (!InFrustum(frustum, object.world_bounds)) {
      ++result.statistics.frustum_rejected;
      continue;
    }
    const float distance = math::Length(object.world_bounds.center - view.camera_position);
    if (distance - object.world_bounds.radius > view.maximum_distance) {
      ++result.statistics.distance_rejected;
      continue;
    }
    if (IsOccluded(object, view)) {
      ++result.statistics.occlusion_rejected;
      continue;
    }
    result.instances.push_back({entry.object, object.mesh_resource_index,
                                object.material_resource_index,
                                SelectLOD(distance, object.lod.lod_count, view.lod_distances)});
  }
  std::ranges::sort(result.instances, {}, [](const VisibleInstance &instance) {
    return std::tuple{instance.material_resource_index, instance.mesh_resource_index,
                      instance.lod_index, instance.object.slot, instance.object.generation};
  });
  for (std::uint32_t index = 0; index < result.instances.size(); ++index) {
    const auto &instance = result.instances[index];
    if (result.commands.empty() ||
        result.commands.back().mesh_resource_index != instance.mesh_resource_index ||
        result.commands.back().material_resource_index != instance.material_resource_index ||
        result.commands.back().lod_index != instance.lod_index) {
      result.commands.push_back({instance.mesh_resource_index, instance.material_resource_index,
                                 instance.lod_index, index, 1});
    } else {
      ++result.commands.back().instance_count;
    }
  }
  return result;
}

} // namespace nexora::renderer
