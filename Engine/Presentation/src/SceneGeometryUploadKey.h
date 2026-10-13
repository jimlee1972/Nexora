#pragma once

#include "Nexora/Presentation/Surface.h"
#include <cstring>
#include <new>

namespace Nexora::Presentation::detail {
// Exact owning prefix key. The adapter separately ensures the same live GPU allocation and
// enough capacity, and clears this key when retiring/replacing that allocation.
class SceneGeometryUploadKey final {
public:
  struct Candidate {
    std::unique_ptr<std::byte[]> payload;
    std::size_t vertexBytes{}, bytes{}, destination{};
    explicit operator bool() const noexcept { return payload != nullptr; }
  };
  static Candidate Capture(std::span<const std::byte> vertices, std::span<const std::byte> indices,
                           std::size_t destination = 0) noexcept {
    if (vertices.empty() || indices.empty() || vertices.size() > 65535 * sizeof(SceneVertex) ||
        indices.size() > 1048576 * sizeof(std::uint16_t))
      return {};
    Candidate candidate;
    candidate.vertexBytes = vertices.size();
    candidate.bytes = vertices.size() + indices.size();
    candidate.destination = destination;
    candidate.payload.reset(new (std::nothrow) std::byte[candidate.bytes]);
    if (candidate) {
      std::memcpy(candidate.payload.get(), vertices.data(), vertices.size());
      std::memcpy(candidate.payload.get() + vertices.size(), indices.data(), indices.size());
    }
    return candidate;
  }
  bool Matches(std::span<const std::byte> vertices, std::span<const std::byte> indices,
               std::size_t destination = 0) const noexcept {
    return payload_ && vertexBytes_ == vertices.size() && bytes_ - vertexBytes_ == indices.size() &&
           destination_ == destination &&
           std::memcmp(payload_.get(), vertices.data(), vertices.size()) == 0 &&
           std::memcmp(payload_.get() + vertexBytes_, indices.data(), indices.size()) == 0;
  }
  void Commit(Candidate candidate, SceneGeometryUploadCacheDiagnostics &stats) noexcept {
    if (!candidate)
      return;
    stats.available = true;
    ++stats.uploads;
    stats.retainedCpuBytes = stats.retainedCpuBytes - bytes_ + candidate.bytes;
    payload_ = std::move(candidate.payload);
    vertexBytes_ = candidate.vertexBytes;
    bytes_ = candidate.bytes;
    destination_ = candidate.destination;
  }
  void Clear(SceneGeometryUploadCacheDiagnostics &stats) noexcept {
    stats.retainedCpuBytes -= bytes_;
    payload_.reset();
    vertexBytes_ = bytes_ = destination_ = 0;
  }
  static void RecordHit(SceneGeometryUploadCacheDiagnostics &stats) noexcept {
    stats.available = true;
    ++stats.hits;
  }

private:
  std::unique_ptr<std::byte[]> payload_;
  std::size_t vertexBytes_{}, bytes_{}, destination_{};
};
} // namespace Nexora::Presentation::detail
