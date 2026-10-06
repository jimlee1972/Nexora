#pragma once

#include "Nexora/Editor/MeshImport.h"
#include "Nexora/Runtime/AssetPipeline.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace nexora::editor::detail {

struct ReimportSource final {
  std::string source_hash;
  std::string artifact_hash;
  std::shared_ptr<const MeshGeometry> mesh;
  std::string error;
  bool cancelled{};
  bool read_failed{};
};

inline std::uint64_t HashSourceChunk(std::string_view bytes, std::uint64_t seed) {
  for (const unsigned char byte : bytes) {
    seed ^= byte;
    seed *= 1099511628211ULL;
  }
  return seed;
}

inline std::string SourceHashHex(std::uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}

// Shared staging path for indexing, synchronous authoring requests and background jobs. Ordinary
// assets retain only a fixed read chunk and incremental hashes; OBJ parsing retains bounded source
// bytes. Publication is a separate revision-checked authoring-thread transaction. Partial hashes
// never escape a failed or cancelled read.
inline ReimportSource ReadReimportSource(const std::filesystem::path &path, std::string type,
                                         runtime::AssetUuid asset,
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
  if (cancelled && cancelled()) {
    result.cancelled = true;
    return result;
  }
  // Reject known oversized OBJ sources before reading/allocating their payload.
  // The streaming guard below still handles growth and unavailable size metadata.
  if (type == ".obj") {
    std::error_code size_error;
    const auto source_size = std::filesystem::file_size(path, size_error);
    if (!size_error && source_size > kMaximumObjSourceBytes) {
      result.error = "OBJ source exceeds the 16 MiB limit.";
      return result;
    }
  }
  auto source_hash = std::uint64_t{1469598103934665603ULL};
  auto artifact_hash = HashSourceChunk(asset.ToString(), source_hash);
  std::string obj_bytes;
  std::array<char, 8192> chunk{};
  while (input) {
    if (cancelled && cancelled()) {
      result.cancelled = true;
      return result;
    }
    input.read(chunk.data(), chunk.size());
    const auto count = static_cast<std::size_t>(input.gcount());
    if (type == ".obj" && count > kMaximumObjSourceBytes - obj_bytes.size()) {
      result.error = "OBJ source exceeds the 16 MiB limit.";
      return result;
    }
    const std::string_view bytes{chunk.data(), count};
    source_hash = HashSourceChunk(bytes, source_hash);
    artifact_hash = HashSourceChunk(bytes, artifact_hash);
    if (type == ".obj")
      obj_bytes.append(bytes);
  }
  if (input.bad() || !input.eof()) {
    result.error = "Asset source could not be read.";
    result.read_failed = true;
    return result;
  }
  if (cancelled && cancelled()) {
    result.cancelled = true;
    return result;
  }
  if (type == ".obj") {
    auto parsed = ImportObjMesh(obj_bytes, cancelled);
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
  result.source_hash = SourceHashHex(source_hash);
  result.artifact_hash = SourceHashHex(artifact_hash);
  return result;
}

} // namespace nexora::editor::detail
