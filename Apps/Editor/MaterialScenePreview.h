#pragma once

#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Renderer/SceneFrame.h"
#include "SceneMeshPreview.h"
#include <unordered_map>
#include <unordered_set>

namespace nexora::editor::preview {

struct MaterialPalette final {
  std::vector<Nexora::Presentation::SceneMaterial> materials{1};
  std::unordered_map<runtime::Id, std::uint32_t> entities;
  std::size_t unavailable{};
  std::unordered_set<runtime::Id> unavailable_entities;
  bool authored{};
};

// Owning frame palette. Slot zero is the explicit neutral fallback. Unknown references remain
// stored in the document; they never become invented colors or legacy shader-ID lookups.
[[nodiscard]] inline MaterialPalette PrepareMaterialPalette(const SceneDocument &scene,
                                                            const MaterialAssetCatalog &catalog,
                                                            std::uint64_t generation,
                                                            std::span<const runtime::Id> entities) {
  MaterialPalette result;
  std::vector<runtime::AssetUuid> slots;
  for (const auto entity : entities) {
    const auto key = scene.Key(entity);
    const auto reference = key ? ReadMaterialAssetReference(scene, *key) : std::nullopt;
    const auto resolved = reference ? catalog.ResolveAsset(*reference, generation) : std::nullopt;
    if (!resolved) {
      if (reference) {
        ++result.unavailable;
        result.unavailable_entities.insert(entity);
      }
      result.entities.emplace(entity, 0);
      continue;
    }
    const auto found = std::ranges::find(slots, resolved->asset);
    if (found != slots.end()) {
      result.entities.emplace(entity, static_cast<std::uint32_t>(found - slots.begin() + 1));
      continue;
    }
    if (result.materials.size() == 64) {
      ++result.unavailable;
      result.unavailable_entities.insert(entity);
      result.entities.emplace(entity, 0);
      continue;
    }
    const auto &source = *resolved->material;
    Nexora::Presentation::SceneMaterial material;
    std::ranges::copy(source.base_color, material.baseColor.begin());
    material.metallic = source.metallic;
    material.roughness = source.roughness;
    material.occlusion = source.occlusion;
    material.emission = source.emission;
    result.entities.emplace(entity, static_cast<std::uint32_t>(result.materials.size()));
    result.materials.push_back(material);
    slots.push_back(resolved->asset);
    result.authored = true;
  }
  return result;
}

// Reuse Renderer policy, including the degenerate-UV fallback. Prepare a complete owning result
// before mutation. This CPU conversion does not claim persistent GPU geometry residency.
[[nodiscard]] inline bool PrepareMaterialTangents(Geometry &geometry) {
  renderer::Mesh mesh;
  mesh.indices = geometry.indices;
  for (const auto &source : geometry.vertices) {
    renderer::SceneVertex vertex;
    std::ranges::copy(source.position, vertex.position.begin());
    std::ranges::copy(source.normal, vertex.normal.begin());
    std::ranges::copy(source.uv, vertex.uv.begin());
    mesh.vertices.push_back(vertex);
  }
  const auto tangents = renderer::GenerateMeshTangents(mesh);
  if (!tangents)
    return false;
  for (std::size_t index = 0; index < tangents->size(); ++index)
    std::ranges::copy((*tangents)[index], geometry.vertices[index].tangent);
  return true;
}

} // namespace nexora::editor::preview
