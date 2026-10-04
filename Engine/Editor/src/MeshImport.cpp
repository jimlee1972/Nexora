#include "Nexora/Editor/MeshImport.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace nexora::editor {
namespace {

std::string_view Token(std::string_view &line) {
  const auto start = line.find_first_not_of(" \t\r");
  if (start == std::string_view::npos) {
    line = {};
    return {};
  }
  line.remove_prefix(start);
  const auto end = line.find_first_of(" \t\r");
  const auto token = line.substr(0, end);
  line.remove_prefix(token.size());
  return token;
}

bool Number(std::string_view text, float &value) {
  if (text.starts_with('+'))
    text.remove_prefix(1);
  if (text.empty())
    return false;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
         std::isfinite(value);
}

bool Index(std::string_view text, std::size_t count, std::size_t &index) {
  std::int64_t value{};
  if (text.starts_with('+'))
    text.remove_prefix(1);
  if (text.empty())
    return false;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
      value == 0 || value > static_cast<std::int64_t>(count) ||
      value < -static_cast<std::int64_t>(count))
    return false;
  index =
      static_cast<std::size_t>(value > 0 ? value - 1 : static_cast<std::int64_t>(count) + value);
  return true;
}

bool Normalize(std::array<float, 3> &normal) {
  const auto length = std::hypot(static_cast<double>(normal[0]), static_cast<double>(normal[1]),
                                 static_cast<double>(normal[2]));
  if (!std::isfinite(length) || length == 0)
    return false;
  for (auto &value : normal)
    value = static_cast<float>(value / length);
  return true;
}

} // namespace

MeshImportResult ImportObjMesh(std::string_view source, const std::function<bool()> &cancelled) {
  if (source.size() > kMaximumObjSourceBytes)
    return {.error = "OBJ source exceeds the 16 MiB limit."};
  if (source.find('\0') != std::string_view::npos)
    return {.error = "OBJ source contains a NUL byte."};
  constexpr std::size_t maximum_vertices = 65535;
  constexpr std::size_t maximum_indices = 1048576;
  std::vector<std::array<float, 3>> positions, normals;
  std::vector<std::array<float, 2>> coordinates;
  MeshGeometry geometry;
  // Explicit normals can share vertices. Generated flat normals belong to a face and must not
  // merge across adjacent faces even when those faces share the same position/UV records.
  using Corner = std::array<std::size_t, 3>;
  std::map<Corner, std::uint16_t> vertices;
  const auto missing = std::numeric_limits<std::size_t>::max();
  std::size_t line_number{};
  const auto fail = [&](std::string message) -> MeshImportResult {
    return {.error = std::move(message), .line = line_number};
  };
  while (!source.empty()) {
    ++line_number;
    if (cancelled && cancelled())
      return {.line = line_number, .cancelled = true};
    const auto newline = source.find('\n');
    auto line = source.substr(0, newline);
    source.remove_prefix(line.size() + static_cast<std::size_t>(newline != std::string_view::npos));
    line = line.substr(0, line.find('#'));
    const auto kind = Token(line);
    if (kind.empty())
      continue;
    if (kind == "v" || kind == "vn") {
      std::array<float, 3> value{};
      for (auto &component : value)
        if (!Number(Token(line), component))
          return fail("Position and normal records require three finite numbers.");
      if (!Token(line).empty())
        return fail("Position and normal records must have exactly three coordinates.");
      if (kind == "vn" && !Normalize(value))
        return fail("An OBJ normal must have nonzero length.");
      auto &records = kind == "v" ? positions : normals;
      if (records.size() == maximum_vertices)
        return fail("OBJ coordinate records exceed the 65,535-record limit.");
      records.push_back(value);
    } else if (kind == "vt") {
      std::array<float, 2> value{};
      for (auto &component : value)
        if (!Number(Token(line), component))
          return fail("Texture coordinates require two finite numbers.");
      if (!Token(line).empty())
        return fail("Texture coordinates must have exactly two coordinates.");
      if (coordinates.size() == maximum_vertices)
        return fail("OBJ texture records exceed the 65,535-record limit.");
      coordinates.push_back(value);
    } else if (kind == "f") {
      std::array<Corner, 3> corners{};
      for (auto &corner : corners) {
        corner = {missing, missing, missing};
        auto text = Token(line);
        const auto slash = text.find('/');
        if (!Index(text.substr(0, slash), positions.size(), corner[0]))
          return fail("Face position index is missing or outside the preceding records.");
        if (slash != std::string_view::npos) {
          text.remove_prefix(slash + 1);
          const auto second_slash = text.find('/');
          const auto uv = text.substr(0, second_slash);
          if (!uv.empty() && !Index(uv, coordinates.size(), corner[1]))
            return fail("Face texture index is outside the preceding records.");
          if (second_slash != std::string_view::npos) {
            if (!Index(text.substr(second_slash + 1), normals.size(), corner[2]))
              return fail("Face normal index is missing or outside the preceding records.");
          } else if (uv.empty()) {
            return fail("Face texture index is missing after '/'.");
          }
        }
      }
      if (!Token(line).empty())
        return fail("Only triangulated OBJ faces are supported; triangulate the source mesh.");
      std::array<double, 3> a{}, b{}, cross{};
      for (std::size_t axis = 0; axis < 3; ++axis) {
        a[axis] =
            static_cast<double>(positions[corners[1][0]][axis]) - positions[corners[0][0]][axis];
        b[axis] =
            static_cast<double>(positions[corners[2][0]][axis]) - positions[corners[0][0]][axis];
      }
      for (std::size_t axis = 0; axis < 3; ++axis)
        cross[axis] = a[(axis + 1) % 3] * b[(axis + 2) % 3] - a[(axis + 2) % 3] * b[(axis + 1) % 3];
      const auto length = std::hypot(cross[0], cross[1], cross[2]);
      if (!std::isfinite(length) || length == 0)
        return fail("OBJ face is degenerate.");
      std::array<float, 3> face_normal{};
      for (std::size_t axis = 0; axis < 3; ++axis)
        face_normal[axis] = static_cast<float>(cross[axis] / length);
      if (geometry.indices.size() > maximum_indices - 3)
        return fail("OBJ triangles exceed the 1,048,576-index limit.");
      for (const auto &corner : corners) {
        const auto existing = vertices.find(corner);
        if (corner[2] != missing && existing != vertices.end()) {
          geometry.indices.push_back(existing->second);
          continue;
        }
        if (geometry.vertices.size() == maximum_vertices)
          return fail("Expanded OBJ geometry exceeds the 65,535-vertex limit.");
        MeshVertex vertex;
        vertex.position = positions[corner[0]];
        vertex.normal = corner[2] == missing ? face_normal : normals[corner[2]];
        if (corner[1] != missing)
          vertex.uv = coordinates[corner[1]];
        const auto index = static_cast<std::uint16_t>(geometry.vertices.size());
        if (corner[2] != missing)
          vertices.emplace(corner, index);
        geometry.vertices.push_back(vertex);
        geometry.indices.push_back(index);
      }
    } else if (kind != "o" && kind != "g" && kind != "s" && kind != "usemtl" && kind != "mtllib") {
      return fail("Unsupported OBJ record: " + std::string(kind));
    }
  }
  if (cancelled && cancelled())
    return {.line = line_number, .cancelled = true};
  if (geometry.indices.empty())
    return fail("OBJ source contains no triangles.");
  geometry.minimum = geometry.maximum = geometry.vertices.front().position;
  for (const auto &vertex : geometry.vertices)
    for (std::size_t axis = 0; axis < 3; ++axis) {
      geometry.minimum[axis] = std::min(geometry.minimum[axis], vertex.position[axis]);
      geometry.maximum[axis] = std::max(geometry.maximum[axis], vertex.position[axis]);
    }
  return {.geometry = std::move(geometry)};
}

} // namespace nexora::editor
