#include "SceneMeshPreview.h"

#include <iostream>
#include <limits>
#include <numbers>
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
    nexora::runtime::World world;
    const auto scene = world.LoadScene("Exact authored mesh");
    const auto root = world.CreateEntity(scene).id;
    const auto parent = world.CreateEntity(scene).id;
    const auto leaf = world.CreateEntity(scene).id;
    nexora::runtime::Transform stretch;
    stretch.sx = -2;
    stretch.sy = 3;
    nexora::runtime::Transform turn;
    turn.qz = std::sin(std::numbers::pi / 8);
    turn.qw = std::cos(std::numbers::pi / 8);
    nexora::runtime::WorldCommandBuffer ancestry;
    ancestry.SetTransform(root, stretch);
    ancestry.SetTransform(parent, turn);
    ancestry.SetTransform(leaf, {1, 0, 2});
    ancestry.SetParent(parent, root, false);
    ancestry.SetParent(leaf, parent, false);
    Require(ancestry.Apply(world), "authored affine hierarchy fixture failed");
    const auto matrix = *world.WorldMatrix(leaf);
    const auto instance = preview::AffineInstance(matrix);
    Require(instance && instance->model_transform &&
                Nexora::Presentation::ValidateSceneInstance(*instance),
            "exact Runtime matrix did not reach the affine Presentation boundary");
    const double c = std::sqrt(0.5);
    const double x = -3.4 * c, y = 5.7 * c;
    const auto &m = *instance->model_transform;
    Require(std::abs(m[0] * 0.8 + m[1] * 0.1 + m[3] - x) < 1e-6 &&
                std::abs(m[4] * 0.8 + m[5] * 0.1 + m[7] - y) < 1e-6 && m[11] == 2,
            "column-major Runtime matrix was not explicitly transposed for Presentation");
    const ViewportRay affine_ray{{x, y, 7}, {0, 0, -1}};
    const auto affine_hit = preview::PickMesh(affine_ray, triangle, matrix);
    Require(affine_hit && std::abs(*affine_hit - 5) < 1e-10 &&
                !preview::PickMesh(affine_ray, triangle, *world.WorldTransform(leaf)) &&
                preview::PickMesh({{x, y, -3}, {0, 0, 1}}, triangle, matrix) &&
                !preview::PickMesh(affine_ray, triangle, matrix, 4.99),
            "affine silhouette picking fell back to lossy TRS or rejected the mirrored back face");
    auto invalid_matrix = matrix;
    invalid_matrix[3] = 0.1;
    Require(!preview::AffineInstance(invalid_matrix) &&
                !preview::PickMesh(affine_ray, triangle, invalid_matrix),
            "nonaffine matrix reached drawing or picking");
    invalid_matrix = matrix;
    invalid_matrix[0] = std::numeric_limits<double>::max();
    Require(!preview::AffineInstance(invalid_matrix), "double-to-float overflow reached drawing");
    invalid_matrix = matrix;
    invalid_matrix[0] = invalid_matrix[1] = invalid_matrix[2] = 0;
    Require(!preview::AffineInstance(invalid_matrix), "singular authored matrix reached drawing");
    stretch.x = 100;
    nexora::runtime::WorldCommandBuffer move_root;
    move_root.SetTransform(root, stretch);
    Require(move_root.Apply(world) && std::abs(instance->model_transform->at(3) + 2 * c) < 1e-6 &&
                preview::PickMesh(affine_ray, triangle, matrix) &&
                !preview::PickMesh(affine_ray, triangle, *world.WorldMatrix(leaf)),
            "owning native matrix borrowed mutable Runtime storage");
    std::cout << "Native mesh preview geometry contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
