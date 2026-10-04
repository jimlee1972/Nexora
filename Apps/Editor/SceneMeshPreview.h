#pragma once

#include "Nexora/Editor/MeshImport.h"
#include "Nexora/Editor/ViewportMath.h"
#include "Nexora/Presentation/Surface.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexora::editor::preview {

struct MeshRange final {
  std::uint32_t firstIndex{}, indexCount{};
};

// Frame-owned combined geometry. Publication validates the complete append before mutation;
// indices remain absolute within the shared 16-bit upload used by Presentation batches.
struct Geometry final {
  std::vector<Nexora::Presentation::SceneVertex> vertices;
  std::vector<std::uint16_t> indices;

  [[nodiscard]] std::optional<MeshRange> Append(const MeshGeometry &mesh) {
    if (vertices.size() > 65535 || indices.size() > 1048576 || mesh.vertices.empty() ||
        mesh.indices.empty() || mesh.indices.size() % 3 != 0 ||
        mesh.vertices.size() > 65535 - vertices.size() ||
        mesh.indices.size() > 1048576 - indices.size())
      return std::nullopt;
    for (const auto index : mesh.indices)
      if (index >= mesh.vertices.size())
        return std::nullopt;
    for (const auto &vertex : mesh.vertices) {
      for (const auto value : vertex.position)
        if (!std::isfinite(value) || std::abs(value) > 100000.0F)
          return std::nullopt;
      for (const auto value : vertex.normal)
        if (!std::isfinite(value))
          return std::nullopt;
      for (const auto value : vertex.uv)
        if (!std::isfinite(value))
          return std::nullopt;
    }
    const auto base = vertices.size();
    const MeshRange range{static_cast<std::uint32_t>(indices.size()),
                          static_cast<std::uint32_t>(mesh.indices.size())};
    for (const auto &vertex : mesh.vertices) {
      Nexora::Presentation::SceneVertex converted;
      std::ranges::copy(vertex.position, converted.position);
      std::ranges::copy(vertex.normal, converted.normal);
      std::ranges::copy(vertex.uv, converted.uv);
      vertices.push_back(converted);
    }
    for (const auto index : mesh.indices)
      indices.push_back(static_cast<std::uint16_t>(base + index));
    return range;
  }
};

// Runtime matrices are owning column-major doubles; Presentation consumes row-major floats.
// Validate the complete conversion before publishing an instance or selecting native geometry.
[[nodiscard]] inline std::optional<Nexora::Presentation::SceneInstance>
AffineInstance(const runtime::TransformMatrix &matrix) {
  Nexora::Presentation::SceneInstance instance;
  std::array<float, 16> converted;
  for (std::size_t row = 0; row < 4; ++row)
    for (std::size_t column = 0; column < 4; ++column) {
      const auto value = matrix[column * 4 + row];
      if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        return std::nullopt;
      converted[row * 4 + column] = static_cast<float>(value);
    }
  instance.model_transform = converted;
  if (!Nexora::Presentation::ValidateSceneInstance(instance))
    return std::nullopt;
  return instance;
}

// Two-sided triangle picking of the same exact affine geometry submitted to the native preview. No
// proxy offset/scale is applied. The caller performs the world-bounds broad phase before this work.
[[nodiscard]] inline std::optional<double> PickMesh(const ViewportRay &ray,
                                                    const MeshGeometry &mesh,
                                                    const runtime::TransformMatrix &matrix,
                                                    double maximum = 500.0) {
  if (!AffineInstance(matrix) || !std::isfinite(maximum) || maximum < 0 ||
      mesh.indices.size() % 3 != 0)
    return std::nullopt;
  const auto point = [&](std::uint16_t index) {
    const auto &p = mesh.vertices[index].position;
    return std::array<double, 3>{
        matrix[0] * p[0] + matrix[4] * p[1] + matrix[8] * p[2] + matrix[12],
        matrix[1] * p[0] + matrix[5] * p[1] + matrix[9] * p[2] + matrix[13],
        matrix[2] * p[0] + matrix[6] * p[1] + matrix[10] * p[2] + matrix[14]};
  };
  const auto subtract = [](const auto &a, const auto &b) {
    return std::array<double, 3>{a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  };
  const auto cross = [](const auto &a, const auto &b) {
    return std::array<double, 3>{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                 a[0] * b[1] - a[1] * b[0]};
  };
  const auto dot = [](const auto &a, const auto &b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  };
  std::array origin{ray.origin.x, ray.origin.y, ray.origin.z};
  std::array direction{ray.direction.x, ray.direction.y, ray.direction.z};
  const auto length = std::hypot(direction[0], direction[1], direction[2]);
  if (!std::isfinite(length) || length == 0 ||
      !std::ranges::all_of(origin, [](double value) { return std::isfinite(value); }))
    return std::nullopt;
  for (auto &value : direction)
    value /= length;
  std::optional<double> nearest;
  for (std::size_t index = 0; index < mesh.indices.size(); index += 3) {
    if (mesh.indices[index] >= mesh.vertices.size() ||
        mesh.indices[index + 1] >= mesh.vertices.size() ||
        mesh.indices[index + 2] >= mesh.vertices.size())
      return std::nullopt;
    const auto a = point(mesh.indices[index]);
    const auto e1 = subtract(point(mesh.indices[index + 1]), a);
    const auto e2 = subtract(point(mesh.indices[index + 2]), a);
    const auto p = cross(direction, e2);
    const auto determinant = dot(e1, p);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12)
      continue;
    const auto t = subtract(origin, a);
    const auto u = dot(t, p) / determinant;
    const auto q = cross(t, e1);
    const auto v = dot(direction, q) / determinant;
    const auto distance = dot(e2, q) / determinant;
    if (u >= 0 && v >= 0 && u + v <= 1 && std::isfinite(distance) && distance >= 0 &&
        distance <= maximum && (!nearest || distance < *nearest))
      nearest = distance;
  }
  return nearest;
}

// Compatibility for standalone TRS callers; the native authored path supplies WorldMatrix.
[[nodiscard]] inline std::optional<double> PickMesh(const ViewportRay &ray,
                                                    const MeshGeometry &mesh,
                                                    const runtime::Transform &pose,
                                                    double maximum = 500.0) {
  if (!runtime::IsValidTransform(pose))
    return std::nullopt;
  return PickMesh(ray, mesh, runtime::ToMatrix(pose), maximum);
}

} // namespace nexora::editor::preview
