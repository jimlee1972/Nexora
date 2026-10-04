#pragma once

#include "Nexora/Editor/MeshImport.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>

namespace nexora::editor::detail {

struct ReimportSource final {
  std::string bytes;
  std::shared_ptr<const MeshGeometry> mesh;
  std::string error;
  bool cancelled{};
  bool read_failed{};
};

// Shared staging path for synchronous authoring requests and background jobs. A worker owns the
// returned geometry; publication is a separate revision-checked authoring-thread transaction.
inline ReimportSource ReadReimportSource(const std::filesystem::path &path, std::string type,
                                         const std::function<bool()> &cancelled = {}) {
  if (type.empty())
    type = path.extension().string();
  for (auto &character : type)
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  ReimportSource result;
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    result.error = "Asset source could not be opened.";
    result.read_failed = true;
    return result;
  }
  std::array<char, 8192> chunk{};
  while (input) {
    if (cancelled && cancelled()) {
      result.cancelled = true;
      return result;
    }
    input.read(chunk.data(), chunk.size());
    const auto count = static_cast<std::size_t>(input.gcount());
    if (type == ".obj" && count > kMaximumObjSourceBytes - result.bytes.size()) {
      result.error = "OBJ source exceeds the 16 MiB limit.";
      return result;
    }
    result.bytes.append(chunk.data(), count);
  }
  if (input.bad() || !input.eof()) {
    result.error = "Asset source could not be read.";
    result.read_failed = true;
    return result;
  }
  if (type == ".obj") {
    auto parsed = ImportObjMesh(result.bytes, cancelled);
    if (parsed.cancelled) {
      result.cancelled = true;
      return result;
    }
    if (!parsed.geometry) {
      result.error = "OBJ line " + std::to_string(parsed.line) + ": " + parsed.error;
      return result;
    }
    result.mesh = std::make_shared<const MeshGeometry>(std::move(*parsed.geometry));
  }
  return result;
}

} // namespace nexora::editor::detail
