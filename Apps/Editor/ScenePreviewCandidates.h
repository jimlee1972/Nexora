#pragma once

#include "Nexora/EditorImGui/EditorImGui.h"
#include "SceneMeshPreview.h"

#include <unordered_map>
#include <unordered_set>

namespace nexora::editor::preview {

// The same bounded, owning CPU submission model drives drawing, picking and Frame all.
struct NativeSceneProxyMesh final {
  std::array<Nexora::Presentation::SceneVertex, 24> vertices{};
  std::array<std::uint16_t, 36> indices{};

  NativeSceneProxyMesh() {
    constexpr std::array<std::array<float, 3>, 8> corners{{{-1, -1, 1},
                                                           {1, -1, 1},
                                                           {1, 1, 1},
                                                           {-1, 1, 1},
                                                           {-1, -1, -1},
                                                           {1, -1, -1},
                                                           {1, 1, -1},
                                                           {-1, 1, -1}}};
    constexpr std::array<std::array<std::uint16_t, 4>, 6> faces{
        {{0, 1, 2, 3}, {1, 5, 6, 2}, {5, 4, 7, 6}, {4, 0, 3, 7}, {3, 2, 6, 7}, {4, 5, 1, 0}}};
    constexpr std::array<std::array<float, 3>, 6> normals{
        {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
    for (std::size_t face = 0; face < faces.size(); ++face) {
      const auto base = face * 4;
      for (std::size_t corner = 0; corner < 4; ++corner) {
        const auto &point = corners[faces[face][corner]];
        const auto &normal = normals[face];
        vertices[base + corner] = {
            {point[0], point[1], point[2]}, {normal[0], normal[1], normal[2]}, {0, 0}};
      }
      for (std::size_t index = 0; index < 6; ++index)
        indices[face * 6 + index] = static_cast<std::uint16_t>(
            base + std::array<std::uint16_t, 6>{0, 1, 2, 2, 3, 0}[index]);
    }
  }
};

struct NativeSceneMeshes final {
  nexora::editor::preview::Geometry geometry;
  std::unordered_map<nexora::runtime::Id, nexora::editor::MeshAssetSnapshot> entities;
  std::unordered_map<std::uint64_t, nexora::editor::preview::MeshRange> ranges;
  std::size_t unavailable{};
};

[[nodiscard]] inline std::vector<nexora::editor::PickCandidate>
NativeSceneProxyCandidates(const nexora::editor::SceneDocument &scene,
                           const NativeSceneMeshes *meshes = nullptr) {
  std::vector<nexora::editor::PickCandidate> candidates;
  const auto nodes = scene.Nodes();
  candidates.reserve(std::min<std::size_t>(
      nodes.size(), nexora::editor::imgui::kMaximumNativeSceneFrameCandidates));
  for (const auto &node : nodes) {
    if (candidates.size() == nexora::editor::imgui::kMaximumNativeSceneFrameCandidates)
      break;
    const auto pose = scene.WorldTransform(node.id);
    if (!pose || !nexora::runtime::IsValidTransform(*pose) || std::abs(pose->x) > 100000.0 ||
        std::abs(pose->y) > 100000.0 || std::abs(pose->z) > 100000.0 ||
        std::abs(pose->sx) < 0.0001 || std::abs(pose->sy) < 0.0001 || std::abs(pose->sz) < 0.0001 ||
        std::abs(pose->sx) > 100000.0 || std::abs(pose->sy) > 100000.0 ||
        std::abs(pose->sz) > 100000.0)
      continue;
    auto matrix = nexora::runtime::ToMatrix(*pose);
    if (meshes && meshes->entities.contains(node.id)) {
      const auto exact = scene.WorldMatrix(node.id);
      if (!exact || !nexora::editor::preview::AffineInstance(*exact))
        continue;
      matrix = *exact;
    }
    nexora::editor::PickCandidate candidate;
    candidate.entity = node.id;
    candidate.min = {std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity()};
    candidate.max = {-candidate.min.x, -candidate.min.y, -candidate.min.z};
    const auto found =
        meshes ? meshes->entities.find(node.id)
               : std::unordered_map<nexora::runtime::Id,
                                    nexora::editor::MeshAssetSnapshot>::const_iterator{};
    const auto *geometry =
        meshes && found != meshes->entities.end() ? found->second.geometry.get() : nullptr;
    const std::array<double, 3> low =
        geometry ? std::array<double, 3>{geometry->minimum[0], geometry->minimum[1],
                                         geometry->minimum[2]}
                 : std::array<double, 3>{-0.45, -0.45, -0.45};
    const std::array<double, 3> high =
        geometry ? std::array<double, 3>{geometry->maximum[0], geometry->maximum[1],
                                         geometry->maximum[2]}
                 : std::array<double, 3>{0.45, 0.45, 0.45};
    for (const double x : {low[0], high[0]})
      for (const double y : {low[1], high[1]})
        for (const double z : {low[2], high[2]}) {
          const double px = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12];
          const double py =
              matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13] + (geometry ? 0.0 : 0.5);
          const double pz = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14];
          candidate.min.x = std::min(candidate.min.x, px);
          candidate.min.y = std::min(candidate.min.y, py);
          candidate.min.z = std::min(candidate.min.z, pz);
          candidate.max.x = std::max(candidate.max.x, px);
          candidate.max.y = std::max(candidate.max.y, py);
          candidate.max.z = std::max(candidate.max.z, pz);
        }
    if (!std::isfinite(candidate.min.x) || !std::isfinite(candidate.min.y) ||
        !std::isfinite(candidate.min.z) || !std::isfinite(candidate.max.x) ||
        !std::isfinite(candidate.max.y) || !std::isfinite(candidate.max.z))
      continue;
    candidates.push_back(candidate);
  }
  return candidates;
}

[[nodiscard]] inline NativeSceneMeshes PrepareNativeSceneMeshes(
    const nexora::editor::SceneDocument &scene, const nexora::editor::MeshAssetCatalog &catalog,
    const nexora::editor::ProjectContentSession &content, std::uint64_t generation) {
  static const NativeSceneProxyMesh proxy;
  NativeSceneMeshes result;
  result.geometry.vertices.assign(proxy.vertices.begin(), proxy.vertices.end());
  result.geometry.indices.assign(proxy.indices.begin(), proxy.indices.end());
  std::unordered_set<std::uint64_t> rejected;
  for (const auto &candidate : NativeSceneProxyCandidates(scene)) {
    const auto key = scene.Key(candidate.entity);
    const auto component = key ? scene.MeshRenderer(*key) : std::nullopt;
    if (!component)
      continue;
    const auto matrix = scene.WorldMatrix(candidate.entity);
    if (!matrix || !nexora::editor::preview::AffineInstance(*matrix)) {
      ++result.unavailable;
      continue;
    }
    const auto mesh = catalog.ResolveResource(component->mesh, generation);
    if (!mesh || !content.Browser().Find(mesh->asset) || rejected.contains(mesh->resource)) {
      ++result.unavailable;
      continue;
    }
    if (!result.ranges.contains(mesh->resource)) {
      const auto range = result.geometry.Append(*mesh->geometry);
      if (!range) {
        rejected.insert(mesh->resource);
        ++result.unavailable;
        continue;
      }
      result.ranges.emplace(mesh->resource, *range);
    }
    result.entities.emplace(candidate.entity, *mesh);
  }
  return result;
}

} // namespace nexora::editor::preview
