#include "SceneMeshPreview.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  try {
    using namespace nexora::editor;
    auto imported = ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Require(imported.geometry.has_value(), "triangle fixture failed");
    auto mesh = *imported.geometry;
    preview::Geometry geometry;
    const auto first = geometry.Append(mesh);
    const auto second = geometry.Append(mesh);
    Require(first && second && first->firstIndex == 0 && second->firstIndex == 3 &&
                geometry.indices == std::vector<std::uint16_t>{0, 1, 2, 3, 4, 5},
            "distinct shared geometry ranges did not rebase indices");
    mesh.indices[2] = 9;
    Require(!geometry.Append(mesh) && geometry.vertices.size() == 6 && geometry.indices.size() == 6,
            "invalid indices partially appended geometry");
    mesh = *imported.geometry;
    mesh.vertices[0].position[0] = std::numeric_limits<float>::infinity();
    Require(!geometry.Append(mesh) && geometry.vertices.size() == 6,
            "nonfinite mesh partially appended geometry");
    mesh = *imported.geometry;
    mesh.vertices[0].position[0] = 100001;
    Require(!geometry.Append(mesh) && geometry.vertices.size() == 6,
            "preview coordinate limits were not bounded");
    mesh = *imported.geometry;
    mesh.vertices.resize(65535 - 6);
    Require(geometry.Append(mesh).has_value() && geometry.vertices.size() == 65535 &&
                !geometry.Append(*imported.geometry),
            "shared 16-bit geometry budget was not bounded");
    const auto &triangle = *imported.geometry;
    nexora::runtime::Transform pose;
    const ViewportRay ray{{0.25, 0.25, 5}, {0, 0, -1}};
    const auto hit = preview::PickMesh(ray, triangle, pose);
    Require(hit && std::abs(*hit - 5) < 1e-12 &&
                !preview::PickMesh({{0.9, 0.9, 5}, {0, 0, -1}}, triangle, pose) &&
                !preview::PickMesh(ray, triangle, pose, 4),
            "triangle picking used proxy bounds or ignored maximum distance");
    pose.x = 2;
    pose.y = 3;
    pose.z = 1;
    pose.sx = -2;
    pose.sy = 3;
    pose.qz = std::sqrt(0.5);
    pose.qw = std::sqrt(0.5);
    // Local (0.25,0.25,0) becomes (1.25,2.5,1) after mirrored scale and a Z quarter-turn.
    const auto transformed = preview::PickMesh({{1.25, 2.5, 5}, {0, 0, -1}}, triangle, pose);
    Require(transformed && std::abs(*transformed - 4) < 1e-10,
            "triangle picking diverged from mirrored/rotated/translated TRS rendering");
    mesh = triangle;
    mesh.indices[2] = 8;
    Require(!preview::PickMesh(ray, mesh, pose), "invalid triangle index was dereferenced");
    std::cout << "Native mesh preview geometry contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
