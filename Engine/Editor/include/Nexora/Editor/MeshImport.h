#pragma once

#include "Nexora/Editor/Api.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor {

struct MeshVertex final {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> uv{};
};

struct MeshGeometry final {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::array<float, 3> minimum{};
  std::array<float, 3> maximum{};
};

struct MeshImportResult final {
  std::optional<MeshGeometry> geometry{};
  std::string error{};
  std::size_t line{};
  bool cancelled{};
};

// Geometry-only, triangulated Wavefront OBJ: v, vt, vn and f with positive/relative indices.
// Missing normals become flat face normals; material/group declarations do not load files.
// Owns every returned value. No I/O, asset publication or GPU work occurs here.
inline constexpr std::size_t kMaximumObjSourceBytes = 16 * 1024 * 1024;
inline constexpr std::size_t kMaximumWorkspaceMeshBytes = 128 * 1024 * 1024;
[[nodiscard]] NEXORA_EDITOR_API MeshImportResult
ImportObjMesh(std::string_view source, const std::function<bool()> &cancelled = {});

} // namespace nexora::editor
