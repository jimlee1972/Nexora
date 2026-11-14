#pragma once
#include "Nexora/Editor/SpriteSourceCatalog.h"
#include "Nexora/Presentation/Surface.h"
#include "SceneMeshPreview.h"
#include <cmath>
#include <limits>
#include <unordered_map>

namespace nexora::editor::preview {
// Immutable imported geometry only. No authoring World, Content model or source path is retained.
struct SceneSpritePlane final {
  SpriteSourceSnapshot assets;
  MeshGeometry geometry;
};
struct SceneSpritePlanes final {
  std::uint64_t document_generation{};
  SpriteSourceContentVersion version;
  struct Reference final {
    SceneDocument::NodeKey key;
    runtime::AssetUuid source;
    bool operator==(const Reference &) const = default;
  };
  std::vector<Reference> references;
  std::unordered_map<runtime::Id, std::shared_ptr<const SceneSpritePlane>> entities;
  std::size_t unavailable{};
};

// One index per referenced immutable Atlas; unrelated unavailable assets are not preview roots.
// Imported payload validation belongs to Content. Cross-asset closure is checked again here.
[[nodiscard]] inline std::optional<SceneSpritePlanes>
CaptureSceneSpritePlanes(const SceneDocument &scene, const ContentBrowserModel &content) {
  const auto references =
      scene.InspectOpaqueComponents(kSpriteSourceReferenceType, kSpriteSourceReferenceName);
  if (!references)
    return {};
  SceneSpritePlanes result;
  result.document_generation = scene.Generation();
  result.version = {content.ProjectGeneration(), content.Revision()};
  struct AtlasIndex final {
    std::shared_ptr<const twod::Atlas> atlas;
    std::unordered_map<std::uint32_t, const twod::Region *> regions;
  };
  std::unordered_map<runtime::AssetUuid, AtlasIndex, runtime::AssetUuidHash> atlases;
  std::unordered_map<runtime::AssetUuid, std::shared_ptr<const SceneSpritePlane>,
                     runtime::AssetUuidHash>
      sources;
  for (const auto &reference : *references) {
    const auto id = ReadSpriteSourceReference(reference);
    if (!id)
      return {}; // Unsupported reserved metadata must not become a partial source frame.
    const auto key = scene.Key(reference.entity);
    if (!key)
      return {};
    result.references.push_back({*key, *id});
    auto [cached, inserted] = sources.try_emplace(*id);
    if (inserted) {
      const auto *item = content.Find(*id);
      const auto source = item && item->twod ? item->twod->SpriteSource() : nullptr;
      const auto *dependency = source ? content.Find(source->atlas) : nullptr;
      const auto atlas = dependency && dependency->twod ? dependency->twod->Atlas() : nullptr;
      if (source && atlas && item->id == source->id && item->id == item->twod->Id() &&
          item->type == item->twod->Type() && dependency->id == atlas->id &&
          dependency->id == dependency->twod->Id() &&
          dependency->type == dependency->twod->Type() && atlas->width <= 1024 &&
          atlas->height <= 1024) {
        auto [entry, fresh] = atlases.try_emplace(atlas->id);
        if (fresh) {
          entry->second.atlas = atlas;
          for (const auto &region : atlas->regions)
            entry->second.regions.emplace(region.id, &region);
        }
        const auto found = entry->second.regions.find(source->region);
        if (entry->second.atlas == atlas && found != entry->second.regions.end() &&
            found->second->width == source->trimWidth &&
            found->second->height == source->trimHeight) {
          const auto &region = *found->second;
          auto plane = std::make_shared<SceneSpritePlane>();
          plane->assets = {source, atlas};
          const double left = (double(source->trimX) - source->pivotX) / source->pixelsPerUnit;
          const double bottom =
              (double(source->sourceHeight) - source->trimY - source->trimHeight - source->pivotY) /
              source->pixelsPerUnit;
          const double right = left + double(source->trimWidth) / source->pixelsPerUnit;
          const double top = bottom + double(source->trimHeight) / source->pixelsPerUnit;
          bool finite = true;
          for (const auto value : {left, bottom, right, top})
            finite = finite && std::isfinite(value) &&
                     std::abs(value) <= std::numeric_limits<float>::max();
          if (!finite) {
            ++result.unavailable;
            continue;
          }
          plane->geometry.minimum = {float(left), float(bottom), 0};
          plane->geometry.maximum = {float(right), float(top), 0};
          const float u0 = float(region.x) / atlas->width, v0 = float(region.y) / atlas->height;
          const float u1 = float(region.x + region.width) / atlas->width;
          const float v1 = float(region.y + region.height) / atlas->height;
          plane->geometry.vertices = {{{float(left), float(bottom), 0}, {0, 0, 1}, {u0, v1}},
                                      {{float(right), float(bottom), 0}, {0, 0, 1}, {u1, v1}},
                                      {{float(right), float(top), 0}, {0, 0, 1}, {u1, v0}},
                                      {{float(left), float(top), 0}, {0, 0, 1}, {u0, v0}}};
          plane->geometry.indices = {0, 1, 2, 0, 2, 3};
          cached->second = std::move(plane);
        }
      }
    }
    if (cached->second) {
      if (!result.entities.emplace(reference.entity, cached->second).second)
        return {}; // Ambiguous reserved metadata must not select a fabricated binding.
    } else
      ++result.unavailable;
  }
  return result;
}
[[nodiscard]] inline bool SceneSpritePlanesMatch(const SceneSpritePlanes &planes,
                                                 const SceneDocument &scene,
                                                 const ContentBrowserModel &content) {
  if (planes.document_generation != scene.Generation() ||
      planes.version != SpriteSourceContentVersion{content.ProjectGeneration(), content.Revision()})
    return false;
  const auto references =
      scene.InspectOpaqueComponents(kSpriteSourceReferenceType, kSpriteSourceReferenceName);
  if (!references || references->size() != planes.references.size())
    return false;
  for (std::size_t index = 0; index < references->size(); ++index) {
    const auto &reference = (*references)[index];
    const auto source = ReadSpriteSourceReference(reference);
    if (!source || planes.references[index].source != *source ||
        scene.Key(reference.entity) != planes.references[index].key)
      return false;
  }
  return true;
}

// Match the existing PBR Scene sampler and alpha-cutout threshold, including transparent holes.
// World-space triangle admission uses the same affine geometry as rendering and framing.
[[nodiscard]] inline std::optional<double> PickSceneSprite(const ViewportRay &ray,
                                                           const SceneSpritePlane &plane,
                                                           const runtime::TransformMatrix &matrix,
                                                           double maximum = 500.0) {
  const auto distance = PickMesh(ray, plane.geometry, matrix, maximum);
  if (!distance)
    return {};
  const auto point = [&](std::size_t vertex) {
    const auto &p = plane.geometry.vertices[vertex].position;
    return std::array<double, 3>{
        matrix[0] * p[0] + matrix[4] * p[1] + matrix[8] * p[2] + matrix[12],
        matrix[1] * p[0] + matrix[5] * p[1] + matrix[9] * p[2] + matrix[13],
        matrix[2] * p[0] + matrix[6] * p[1] + matrix[10] * p[2] + matrix[14]};
  };
  const auto origin = point(0), right = point(1), top = point(3);
  const std::array hit{ray.origin.x + *distance * ray.direction.x,
                       ray.origin.y + *distance * ray.direction.y,
                       ray.origin.z + *distance * ray.direction.z};
  double uu{}, uv{}, vv{}, hu{}, hv{};
  for (std::size_t axis = 0; axis < 3; ++axis) {
    const auto u = right[axis] - origin[axis], v = top[axis] - origin[axis];
    const auto h = hit[axis] - origin[axis];
    uu += u * u;
    uv += u * v;
    vv += v * v;
    hu += h * u;
    hv += h * v;
  }
  const auto determinant = uu * vv - uv * uv;
  if (!std::isfinite(determinant) || determinant <= 0)
    return {};
  const auto u = std::clamp((hu * vv - hv * uv) / determinant, 0.0, 1.0);
  const auto v = std::clamp((hv * uu - hu * uv) / determinant, 0.0, 1.0);
  const auto &coordinates = plane.geometry.vertices;
  const auto &atlas = *plane.assets.atlas;
  const auto x =
      (coordinates[0].uv[0] + u * (coordinates[1].uv[0] - coordinates[0].uv[0])) * atlas.width - .5;
  const auto y =
      (coordinates[0].uv[1] + v * (coordinates[3].uv[1] - coordinates[0].uv[1])) * atlas.height -
      .5;
  if (!std::isfinite(x) || !std::isfinite(y))
    return {};
  const auto ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
  const auto sample = [&](int px, int py) {
    const auto column = static_cast<std::size_t>(std::clamp(px, 0, int(atlas.width) - 1));
    const auto row = static_cast<std::size_t>(std::clamp(py, 0, int(atlas.height) - 1));
    return double(std::to_integer<unsigned>(atlas.rgba[(row * atlas.width + column) * 4 + 3])) /
           255;
  };
  const auto fx = x - ix, fy = y - iy;
  const auto alpha = (sample(ix, iy) * (1 - fx) + sample(ix + 1, iy) * fx) * (1 - fy) +
                     (sample(ix, iy + 1) * (1 - fx) + sample(ix + 1, iy + 1) * fx) * fy;
  return alpha >= .5 ? distance : std::nullopt;
}
} // namespace nexora::editor::preview
