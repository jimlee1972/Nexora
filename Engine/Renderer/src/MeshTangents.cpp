#include "Nexora/Renderer/SceneFrame.h"
#include <algorithm>
#include <cmath>

namespace nexora::renderer {
namespace {
using Vector = std::array<double, 3>;
Vector Cross(const Vector &a, const Vector &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double Dot(const Vector &a, const Vector &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vector Unit(Vector value) {
  const auto length = std::hypot(value[0], value[1], value[2]);
  if (length > 0)
    for (auto &component : value)
      component /= length;
  return value;
}
} // namespace
std::optional<std::vector<std::array<float, 4>>> GenerateMeshTangents(const Mesh &mesh) {
  if (mesh.vertices.empty() || mesh.vertices.size() > 65535 || mesh.indices.empty() ||
      mesh.indices.size() > 1048576 || mesh.indices.size() % 3 != 0)
    return std::nullopt;
  for (const auto &vertex : mesh.vertices) {
    for (const auto value : vertex.position)
      if (!std::isfinite(value))
        return std::nullopt;
    for (const auto value : vertex.normal)
      if (!std::isfinite(value))
        return std::nullopt;
    for (const auto value : vertex.uv)
      if (!std::isfinite(value))
        return std::nullopt;
    if (vertex.normal == std::array<float, 3>{})
      return std::nullopt;
  }
  for (const auto index : mesh.indices)
    if (index >= mesh.vertices.size())
      return std::nullopt;
  std::vector<Vector> tangents(mesh.vertices.size()), bitangents(mesh.vertices.size());
  for (std::size_t face = 0; face < mesh.indices.size(); face += 3) {
    const auto &a = mesh.vertices[mesh.indices[face]];
    const auto &b = mesh.vertices[mesh.indices[face + 1]];
    const auto &c = mesh.vertices[mesh.indices[face + 2]];
    const double du1 = static_cast<double>(b.uv[0]) - a.uv[0];
    const double dv1 = static_cast<double>(b.uv[1]) - a.uv[1];
    const double du2 = static_cast<double>(c.uv[0]) - a.uv[0];
    const double dv2 = static_cast<double>(c.uv[1]) - a.uv[1];
    const auto determinant = du1 * dv2 - du2 * dv1;
    if (std::abs(determinant) <= 1e-12)
      continue;
    for (std::size_t axis = 0; axis < 3; ++axis) {
      const double edge1 = static_cast<double>(b.position[axis]) - a.position[axis];
      const double edge2 = static_cast<double>(c.position[axis]) - a.position[axis];
      const auto tangent = (edge1 * dv2 - edge2 * dv1) / determinant;
      const auto bitangent = (edge2 * du1 - edge1 * du2) / determinant;
      for (std::size_t corner = 0; corner < 3; ++corner) {
        const auto index = mesh.indices[face + corner];
        tangents[index][axis] += tangent;
        bitangents[index][axis] += bitangent;
      }
    }
  }
  std::vector<std::array<float, 4>> result(mesh.vertices.size());
  for (std::size_t i = 0; i < result.size(); ++i) {
    const auto &normal = mesh.vertices[i].normal;
    const auto n = Unit(Vector{normal[0], normal[1], normal[2]});
    auto t = tangents[i];
    const auto projected = Dot(t, n);
    for (std::size_t axis = 0; axis < 3; ++axis)
      t[axis] -= n[axis] * projected;
    if (std::hypot(t[0], t[1], t[2]) <= 1e-12) {
      Vector reference{};
      const auto axis = std::min_element(
          n.begin(), n.end(), [](double a, double b) { return std::abs(a) < std::abs(b); });
      reference[static_cast<std::size_t>(axis - n.begin())] = 1;
      t = Cross(reference, n);
    }
    t = Unit(t);
    result[i] = {static_cast<float>(t[0]), static_cast<float>(t[1]), static_cast<float>(t[2]),
                 Dot(Cross(n, t), bitangents[i]) < 0 ? -1.0F : 1.0F};
  }
  return result;
}
} // namespace nexora::renderer
