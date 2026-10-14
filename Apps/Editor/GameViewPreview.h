#pragma once

#include "MaterialScenePreview.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/RenderSync.h"
#include "PlayDebugLines.h"
#include "SceneMeshPreview.h"
#include <cstring>
#include <unordered_map>

namespace nexora::editor::preview {

// One immutable owning CPU catalog serves module startup and rendering throughout this Play
// binding. Reimport/clear of the authoring catalog cannot change it; no native resource is
// borrowed.
[[nodiscard]] inline std::shared_ptr<const MeshAssetCatalog>
FreezeGameMeshes(const MeshAssetCatalog &catalog, std::uint64_t generation) noexcept try {
  if (!generation || catalog.Generation() != generation)
    return {};
  return std::make_shared<const MeshAssetCatalog>(catalog);
} catch (...) {
  return {};
}

// Freeze converted material values and authoring assignments before Start. There are no catalog,
// document or native-resource borrows; cloned Runtime IDs identify this one Play session only.
[[nodiscard]] inline std::optional<MaterialPalette>
FreezeGameMaterials(const SceneDocument &scene, const MaterialAssetCatalog &catalog,
                    std::uint64_t generation) {
  if (!generation || catalog.Generation() != generation)
    return std::nullopt;
  const auto nodes = scene.Nodes();
  if (nodes.size() > SceneDocument::kMaximumRuntimeCaptureEntities)
    return std::nullopt;
  std::vector<runtime::Id> entities;
  entities.reserve(nodes.size());
  for (const auto &node : nodes)
    if (scene.MeshRenderer(node.Key()))
      entities.push_back(node.id);
  return PrepareMaterialPalette(scene, catalog, generation, entities);
}

// Owns every upload and matrix after the Play World borrow ends. Asset catalog copies made at
// Start retain the source revision throughout Play, including across editor reimport/delete.
struct GameFrame final {
  runtime::Id camera{};
  math::Matrix4 view_projection{};
  Geometry geometry;
  std::vector<Nexora::Presentation::SceneInstance> instances;
  std::vector<Nexora::Presentation::SceneMeshBatch> batches;
  std::size_t unavailable{};
  std::size_t unavailable_materials{};
  std::size_t debug_lines{}, unavailable_debug_lines{};
  std::vector<Nexora::Presentation::SceneMaterial> materials;
  std::array<float, 3> camera_position{};
  bool pbr{};

  [[nodiscard]] Nexora::Presentation::SceneDrawData
  DrawData(Nexora::Presentation::SceneViewport viewport) const {
    Nexora::Presentation::SceneDrawData draw;
    draw.viewport = viewport;
    draw.vertices = geometry.vertices;
    draw.indices = geometry.indices;
    draw.instances = instances;
    draw.batches = batches;
    draw.pbr = pbr;
    draw.materials = materials;
    draw.cameraPosition = camera_position;
    std::memcpy(draw.model_view_projection, view_projection.values.data(),
                sizeof(draw.model_view_projection));
    return draw;
  }
};

// Call only after commands and fixed ticks, serialized on the authoring thread. Borrowed World
// data never escapes. Inspect is owning and ordered by entity ID. A valid preferred camera wins;
// otherwise the first valid active camera is the deterministic fallback. Recheck the live World
// after ticks even when the owning snapshot or UI selection is stale. Unresolved/budget-rejected
// meshes are omitted rather than drawn as boxes.
[[nodiscard]] inline GameFrame BuildGameFrame(const runtime::World &world,
                                              const runtime::RuntimeInspectionSnapshot &snapshot,
                                              const MeshAssetCatalog &assets, float aspect,
                                              runtime::Id preferred_camera = 0,
                                              const MaterialPalette *frozen_materials = nullptr) {
  GameFrame frame;
  const auto active = [&](runtime::Id id) {
    const auto *scene = world.FindScene(id);
    return scene && scene->state == runtime::SceneState::Active;
  };
  const auto choose = [&](const runtime::RuntimeEntitySnapshot &entity) {
    const auto *live = world.FindEntity(entity.id);
    if (!entity.camera || !live || !active(entity.scene))
      return false;
    if (const auto view = runtime::CameraView(world, entity.id, aspect)) {
      frame.camera = entity.id;
      frame.view_projection = view->view_projection;
      const auto matrix = world.WorldMatrix(entity.id);
      if (!matrix) {
        frame.camera = 0;
        return false;
      }
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
        const auto value = (*matrix)[12 + coordinate];
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
          frame.camera = 0;
          return false;
        }
        frame.camera_position[coordinate] = static_cast<float>(value);
      }
      return true;
    }
    return false;
  };
  if (preferred_camera) {
    const auto preferred =
        std::ranges::find(snapshot.entities, preferred_camera, &runtime::RuntimeEntitySnapshot::id);
    if (preferred != snapshot.entities.end())
      static_cast<void>(choose(*preferred));
  }
  if (!frame.camera)
    for (const auto &entity : snapshot.entities)
      if (choose(entity))
        break;
  if (!frame.camera)
    return frame;
  std::unordered_map<std::uint64_t, std::optional<MeshRange>> ranges;
  for (const auto &copied : snapshot.entities) {
    if (!copied.mesh_renderer || !active(copied.scene))
      continue;
    const auto *entity = world.FindEntity(copied.id);
    if (!entity || !entity->mesh_renderer)
      continue;
    const auto resource = entity->mesh_data.mesh;
    const auto asset = assets.ResolveResource(resource, assets.Generation());
    if (!asset || frame.instances.size() == 4096) {
      ++frame.unavailable;
      continue;
    }
    const auto matrix = world.WorldMatrix(entity->id);
    auto instance = matrix ? AffineInstance(*matrix) : std::nullopt;
    if (!instance) {
      ++frame.unavailable;
      continue;
    }
    auto [range, inserted] = ranges.try_emplace(resource);
    if (inserted)
      range->second = frame.geometry.Append(*asset->geometry);
    if (!range->second) {
      ++frame.unavailable;
      continue;
    }
    std::uint32_t material_index = 0;
    if (frozen_materials) {
      if (frozen_materials->unavailable_entities.contains(entity->id))
        ++frame.unavailable_materials;
      if (const auto material = frozen_materials->entities.find(entity->id);
          material != frozen_materials->entities.end() &&
          material->second < frozen_materials->materials.size())
        material_index = material->second;
    }
    if (material_index) {
      std::fill(std::begin(instance->color), std::end(instance->color), 1.0F);
      frame.pbr = true;
    }
    frame.batches.push_back({range->second->firstIndex, range->second->indexCount,
                             static_cast<std::uint32_t>(frame.instances.size()), 1,
                             material_index});
    frame.instances.push_back(*instance);
  }
  if (frame.pbr && frozen_materials) {
    if (PrepareMaterialTangents(frame.geometry))
      frame.materials = frozen_materials->materials;
    else {
      // Optional material preview failure must preserve Play and the legacy geometry preview.
      frame.pbr = false;
      for (auto &batch : frame.batches) {
        frame.unavailable_materials += batch.materialIndex != 0;
        batch.materialIndex = 0;
      }
    }
  }
  return frame;
}

// Append opaque, depth-tested, camera-facing 0.06-world-unit ribbons to the same scene pass.
// Ordinary batch-zero shading preserves the existing material policy. No native/UI pass or
// borrowed line storage survives this call. Geometry/instance limits take priority over debug.
inline void AppendGameDebugLines(GameFrame &frame, std::span<const NexoraDebugLine> lines) {
  if (!frame.camera || lines.empty())
    return;
  const auto count = std::min(lines.size(), PlayDebugLines::kCapacity);
  frame.unavailable_debug_lines += lines.size() - count;
  frame.instances.reserve(std::min<std::size_t>(4096, frame.instances.size() + count));
  frame.batches.reserve(std::min<std::size_t>(4096, frame.batches.size() + count));
  MeshGeometry ribbon;
  ribbon.vertices.resize(4);
  ribbon.indices = {0, 1, 2, 2, 1, 3, 2, 1, 0, 3, 1, 2};
  for (const auto &line : lines.first(count)) {
    if (!ValidPlayDebugLine(line) || frame.instances.size() >= 4096 ||
        frame.batches.size() >= 4096) {
      ++frame.unavailable_debug_lines;
      continue;
    }
    const std::array<double, 3> start{line.start.x, line.start.y, line.start.z},
        end{line.end.x, line.end.y, line.end.z};
    std::array<double, 3> direction{}, view{}, side{};
    for (std::size_t i = 0; i < 3; ++i) {
      direction[i] = end[i] - start[i];
      view[i] = frame.camera_position[i] - (start[i] + end[i]) * 0.5;
    }
    const auto cross = [](const auto &a, const auto &b) {
      return std::array<double, 3>{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                   a[0] * b[1] - a[1] * b[0]};
    };
    const auto length = [](const auto &v) { return std::hypot(v[0], v[1], v[2]); };
    side = cross(direction, view);
    if (length(side) < 1e-12) {
      const std::array<double, 3> axis = std::abs(direction[0]) < std::abs(direction[1])
                                             ? std::array<double, 3>{1, 0, 0}
                                             : std::array<double, 3>{0, 1, 0};
      side = cross(direction, axis);
    }
    const auto side_length = length(side);
    auto normal = cross(side, direction);
    const auto normal_length = length(normal);
    for (std::size_t vertex = 0; vertex < 4; ++vertex) {
      auto &value = ribbon.vertices[vertex];
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
        value.position[coordinate] =
            static_cast<float>((vertex < 2 ? start[coordinate] : end[coordinate]) +
                               (vertex % 2 ? 0.03 : -0.03) * side[coordinate] / side_length);
        value.normal[coordinate] = static_cast<float>(normal[coordinate] / normal_length);
      }
    }
    const auto range = frame.geometry.Append(ribbon);
    if (!range) {
      ++frame.unavailable_debug_lines;
      continue;
    }
    // A camera-facing normal may parallel the default +X tangent. The segment direction
    // always lies in the ribbon plane and supplies a finite orthogonal PBR tangent.
    const auto direction_length = length(direction);
    for (auto &vertex : std::span{frame.geometry.vertices}.last(4))
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate)
        vertex.tangent[coordinate] = static_cast<float>(direction[coordinate] / direction_length);
    Nexora::Presentation::SceneInstance instance;
    instance.color[0] = static_cast<float>((line.rgba >> 24) & 255U) / 255;
    instance.color[1] = static_cast<float>((line.rgba >> 16) & 255U) / 255;
    instance.color[2] = static_cast<float>((line.rgba >> 8) & 255U) / 255;
    frame.batches.push_back({range->firstIndex, range->indexCount,
                             static_cast<std::uint32_t>(frame.instances.size()), 1, 0});
    frame.instances.push_back(instance);
    ++frame.debug_lines;
  }
}
} // namespace nexora::editor::preview
