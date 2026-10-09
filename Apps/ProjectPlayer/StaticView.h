#pragma once

#include "Nexora/Presentation/Surface.h"
#include "Nexora/Runtime/ProjectPackage.h"
#include "Nexora/Runtime/RenderSync.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace nexora::player {

// CPU-owned native admission. No package/World borrow survives construction. Per-frame descriptors
// borrow this owner's immutable upload vectors only until DrawScene returns.
struct StaticView final {
  std::vector<Nexora::Presentation::SceneVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<Nexora::Presentation::SceneInstance> instances;
  std::vector<Nexora::Presentation::SceneMeshBatch> batches;
  std::vector<Nexora::Presentation::SceneMaterial> materials{1}; // Explicit neutral slot.
  runtime::Id camera{}, light{};
  std::array<float, 3> light_direction{-0.4F, -1, -0.2F};
  std::array<float, 3> light_color{1, 0.95F, 0.85F};

  [[nodiscard]] std::optional<Nexora::Presentation::SceneDrawData>
  DrawData(const runtime::LoadedStaticProject &project, float aspect) const {
    const auto view = runtime::CameraView(project.WorldView(), camera, aspect);
    if (!view)
      return std::nullopt;
    Nexora::Presentation::SceneDrawData draw;
    draw.vertices = vertices;
    draw.indices = indices;
    draw.instances = instances;
    draw.batches = batches;
    draw.materials = materials;
    draw.pbr = true;
    draw.cameraPosition = {view->camera_position.x, view->camera_position.y,
                           view->camera_position.z};
    std::memcpy(draw.model_view_projection, view->view_projection.values.data(),
                sizeof(draw.model_view_projection));
    std::ranges::copy(light_direction, draw.light_direction);
    std::ranges::copy(light_color, draw.light_color);
    return draw;
  }
};

// Fail the entire native admission explicitly rather than drawing a truncated subset. Package
// verification has wider data limits; valid StaticView packages need not fit one native draw.
[[nodiscard]] inline std::optional<StaticView>
PrepareStaticView(const runtime::LoadedStaticProject &project, std::string *error = nullptr) {
  const auto fail = [&](const char *message) -> std::optional<StaticView> {
    if (error)
      *error = message;
    return std::nullopt;
  };
  StaticView result;
  const auto items = project.RenderItems();
  if (items.empty() || items.size() > 4096)
    return fail("Native StaticView requires 1..4096 render instances");
  const auto *scene = project.WorldView().FindScene(project.SceneId());
  if (!scene || scene->state != runtime::SceneState::Active)
    return fail("StaticView entry scene is not active");
  for (const auto &entity : scene->entities) {
    if (entity.camera && (!result.camera || entity.id < result.camera))
      result.camera = entity.id;
    if (entity.light && (!result.light || entity.id < result.light))
      result.light = entity.id;
  }
  if (!result.camera || !runtime::CameraView(project.WorldView(), result.camera, 1))
    return fail("Native StaticView requires a valid authored camera; lowest camera ID is selected");
  if (result.light) {
    const auto *light = project.WorldView().FindEntity(result.light);
    const auto pose = project.WorldView().WorldTransform(result.light);
    if (!light || !pose)
      return fail("Selected light pose is unavailable");
    runtime::Transform orientation;
    orientation.qx = pose->qx;
    orientation.qy = pose->qy;
    orientation.qz = pose->qz;
    orientation.qw = pose->qw;
    const auto rotation = runtime::ToMatrix(orientation);
    for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
      // Runtime LightComponent currently supplies intensity only: use explicit neutral white.
      const double radiance = light->light_data.intensity;
      if (!std::isfinite(radiance) || radiance < 0 || radiance > 65504)
        return fail("Selected light intensity is outside the native PBR range [0,65504]");
      result.light_color[coordinate] = static_cast<float>(radiance);
      result.light_direction[coordinate] = static_cast<float>(-rotation[8 + coordinate]);
    }
  }
  std::unordered_map<const runtime::CookedMesh *, std::pair<std::uint32_t, std::uint32_t>> meshes;
  std::unordered_map<const runtime::CookedScalarPbr *, std::uint32_t> materials;
  for (const auto &item : items) {
    if (!item.mesh)
      return fail("Native StaticView mesh ownership is unavailable");
    auto [mesh, inserted] = meshes.try_emplace(item.mesh.get());
    if (inserted) {
      const auto &source = *item.mesh;
      if (source.vertices.empty() || source.indices.empty() || source.indices.size() % 3 ||
          source.vertices.size() > 65535 - result.vertices.size() ||
          source.indices.size() > 1048576 - result.indices.size())
        return fail("Native StaticView shared geometry exceeds vertex/index budget");
      const auto base = result.vertices.size();
      mesh->second = {static_cast<std::uint32_t>(result.indices.size()),
                      static_cast<std::uint32_t>(source.indices.size())};
      for (const auto &vertex : source.vertices) {
        double normal_length{}, tangent_length{}, dot{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
          const double normal = vertex.normal[axis], tangent = vertex.tangent[axis];
          normal_length += normal * normal;
          tangent_length += tangent * tangent;
          dot += normal * tangent;
        }
        if (normal_length == 0 || tangent_length == 0 ||
            std::abs(dot) > 1e-3 * std::sqrt(normal_length * tangent_length))
          return fail("Cooked mesh tangent is incompatible with native PBR orthogonality");
        Nexora::Presentation::SceneVertex converted;
        std::ranges::copy(vertex.position, converted.position);
        std::ranges::copy(vertex.normal, converted.normal);
        std::ranges::copy(vertex.uv, converted.uv);
        std::ranges::copy(vertex.tangent, converted.tangent);
        result.vertices.push_back(converted);
      }
      for (const auto index : source.indices) {
        if (index >= source.vertices.size())
          return fail("Native StaticView mesh index is invalid");
        result.indices.push_back(static_cast<std::uint16_t>(base + index));
      }
    }
    Nexora::Presentation::SceneInstance instance;
    std::array<float, 16> affine;
    for (std::size_t row = 0; row < 4; ++row)
      for (std::size_t column = 0; column < 4; ++column) {
        const auto value = item.world_transform[column * 4 + row];
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
          return fail("Native StaticView affine transform exceeds finite float range");
        affine[row * 4 + column] = static_cast<float>(value);
      }
    instance.model_transform = affine;
    if (!Nexora::Presentation::ValidateSceneInstance(instance))
      return fail("Native StaticView affine model/normal transform is invalid");
    std::uint32_t slot{};
    if (item.material) {
      auto [material, fresh] = materials.try_emplace(item.material.get());
      if (fresh) {
        if (result.materials.size() == 64)
          return fail("Native StaticView supports 63 scalar materials plus neutral");
        Nexora::Presentation::SceneMaterial converted;
        std::ranges::copy(item.material->base_color, converted.baseColor.begin());
        converted.emission = item.material->emission;
        converted.metallic = item.material->metallic;
        converted.roughness = item.material->roughness;
        converted.occlusion = item.material->occlusion;
        material->second = static_cast<std::uint32_t>(result.materials.size());
        result.materials.push_back(converted);
      }
      slot = material->second;
    }
    result.batches.push_back({mesh->second.first, mesh->second.second,
                              static_cast<std::uint32_t>(result.instances.size()), 1, slot});
    result.instances.push_back(instance);
  }
  // 48-byte vertices, uint16 indices, 112-byte native affine instances and conservative 1024-byte
  // aligned material slots stay below the shared 8 MiB per-frame native upload allowance.
  const auto upload_bytes = result.vertices.size() * 48 + result.indices.size() * 2 +
                            result.instances.size() * 112 + result.materials.size() * 1024 + 4096;
  if (upload_bytes > 8 * 1024 * 1024)
    return fail("Native StaticView aggregate upload exceeds 8 MiB");
  if (!Nexora::Presentation::ValidateSceneMaterials(result.materials, result.batches) ||
      !Nexora::Presentation::ValidateSceneMeshBatches(result.batches, result.indices.size(),
                                                      result.instances.size()))
    return fail("Cooked scalar materials or batches are incompatible with native admission");
  if (error)
    error->clear();
  return result;
}
} // namespace nexora::player
