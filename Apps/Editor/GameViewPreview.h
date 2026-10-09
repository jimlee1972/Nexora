#pragma once

#include "MaterialScenePreview.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/RenderSync.h"
#include "SceneMeshPreview.h"
#include <cstring>
#include <unordered_map>

namespace nexora::editor::preview {

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
} // namespace nexora::editor::preview
