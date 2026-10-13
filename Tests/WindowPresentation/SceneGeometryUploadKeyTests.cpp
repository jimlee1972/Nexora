#include "SceneGeometryUploadKey.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    using namespace Nexora::Presentation;
    using Key = detail::SceneGeometryUploadKey;
    std::array vertices{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    std::array indices{std::byte{5}, std::byte{6}};
    auto candidate = Key::Capture(vertices, indices, 256);
    vertices[0] = std::byte{9};
    Require(candidate && candidate.payload[0] == std::byte{1},
            "geometry key borrowed mutable input instead of owning the admitted bytes");
    vertices[0] = std::byte{1};
    Key key;
    SceneGeometryUploadCacheDiagnostics stats;
    key.Commit(std::move(candidate), stats);
    Require(key.Matches(vertices, indices, 256) && !key.Matches(vertices, indices, 768) &&
                stats.available && stats.uploads == 1 && stats.retainedCpuBytes == 6,
            "geometry key lost exact payload, destination identity or retained accounting");
    std::array combined{std::byte{1}, std::byte{2}, std::byte{3},
                        std::byte{4}, std::byte{5}, std::byte{6}};
    Require(!key.Matches(std::span(combined).first(3), std::span(combined).subspan(3), 256),
            "identical concatenated bytes with a different vertex/index boundary reused");
    vertices[3] = std::byte{7};
    Require(!key.Matches(vertices, indices, 256), "changed vertex bits reused stale geometry");
    vertices[3] = std::byte{4};
    indices[0] = std::byte{7};
    Require(!key.Matches(vertices, indices, 256), "changed index bits reused stale geometry");
    indices[0] = std::byte{5};
    Key::RecordHit(stats);
    Require(stats.hits == 1 && stats.uploads == 1 && stats.retainedCpuBytes == 6,
            "reuse changed owned payload accounting or recorded a geometry write");
    auto replacement = Key::Capture(vertices, indices, 768);
    key.Clear(stats); // Adapter retires the old GPU allocation before publishing the staged key.
    Require(!key.Matches(vertices, indices, 256) && stats.retainedCpuBytes == 0,
            "retired allocation retained its geometry key");
    key.Commit(std::move(replacement), stats);
    Require(key.Matches(vertices, indices, 768) && !key.Matches(vertices, indices, 256) &&
                stats.uploads == 2 && stats.retainedCpuBytes == 6,
            "destination replacement lost its staged key or preserved the old destination");
    auto empty = Key::Capture({}, indices);
    key.Commit(std::move(empty), stats);
    Require(key.Matches(vertices, indices, 768) && stats.uploads == 2,
            "failed/empty key candidate consumed retained identity");
    key.Clear(stats);
    key.Clear(stats);
    Require(stats.retainedCpuBytes == 0 && stats.uploads == 2 && stats.hits == 1,
            "idempotent teardown lost cumulative counters or retained CPU payload");
    std::cout << "Exact geometry upload key contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
