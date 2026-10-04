#pragma once

#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/RenderSync.h"
#include "SceneMeshPreview.h"
#include <cstring>
#include <unordered_map>

namespace nexora::editor::preview {

// Owns every upload and matrix after the Play World borrow ends. Asset catalog copies made at
// Start retain the source revision throughout Play, including across editor reimport/delete.
struct GameFrame final {
  runtime::Id camera{};
  math::Matrix4 view_projection{};
  Geometry geometry;
  std::vector<Nexora::Presentation::SceneInstance> instances;
  std::vector<Nexora::Presentation::SceneMeshBatch> batches;
  std::size_t unavailable{};

  [[nodiscard]] Nexora::Presentation::SceneDrawData
  DrawData(Nexora::Presentation::SceneViewport viewport) const {
    Nexora::Presentation::SceneDrawData draw;
    draw.viewport = viewport;
    draw.vertices = geometry.vertices;
    draw.indices = geometry.indices;
    draw.instances = instances;
    draw.batches = batches;
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
                                              runtime::Id preferred_camera = 0) {
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
    const auto &pose = copied.world_transform;
    Nexora::Presentation::SceneInstance instance;
    instance.translation[0] = static_cast<float>(pose.x);
    instance.translation[1] = static_cast<float>(pose.y);
    instance.translation[2] = static_cast<float>(pose.z);
    instance.scale[0] = static_cast<float>(pose.sx);
    instance.scale[1] = static_cast<float>(pose.sy);
    instance.scale[2] = static_cast<float>(pose.sz);
    instance.rotation[0] = static_cast<float>(pose.qx);
    instance.rotation[1] = static_cast<float>(pose.qy);
    instance.rotation[2] = static_cast<float>(pose.qz);
    instance.rotation[3] = static_cast<float>(pose.qw);
    // Mirror Presentation's finite and nonzero float transform boundary before submission.
    if (!runtime::IsValidTransform(pose) ||
        !std::ranges::all_of(instance.translation, [](float v) { return std::isfinite(v); }) ||
        !std::ranges::all_of(instance.scale,
                             [](float v) { return std::isfinite(v) && std::abs(v) >= 0.00001F; })) {
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
    frame.batches.push_back({range->second->firstIndex, range->second->indexCount,
                             static_cast<std::uint32_t>(frame.instances.size()), 1});
    frame.instances.push_back(instance);
  }
  return frame;
}
} // namespace nexora::editor::preview
