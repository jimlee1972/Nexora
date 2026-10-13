#include "SceneMeshPreview.h"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
// This executable alone replaces ordinary allocation. Fixtures and assertions run unarmed;
// only the real frame builder's vector growth is faulted, on the calling thread.
thread_local int allocationsBeforeFailure = -1;
thread_local int allocationAttempts = 0;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool SameVertex(const Nexora::Presentation::SceneVertex &a,
                const Nexora::Presentation::SceneVertex &b) {
  return std::ranges::equal(a.position, b.position) && std::ranges::equal(a.normal, b.normal) &&
         std::ranges::equal(a.uv, b.uv) && std::ranges::equal(a.tangent, b.tangent);
}

void CheckFailure(int successfulAllocations) {
  using namespace nexora::editor;
  MeshGeometry triangle;
  triangle.vertices.resize(3);
  triangle.vertices[1].position[0] = 1;
  triangle.vertices[2].position[1] = 1;
  triangle.indices = {0, 1, 2};
  preview::Geometry geometry;
  const auto first = geometry.Append(triangle);
  Require(first.has_value(), "initial mesh did not publish");
  const auto oldVertices = geometry.vertices;
  const auto oldIndices = geometry.indices;
  MeshGeometry addition;
  // Force both growth points without assuming any implementation's vector capacity policy.
  addition.vertices.resize(geometry.vertices.capacity() + 1);
  for (std::size_t i = 0; i < addition.vertices.size(); ++i) {
    addition.vertices[i].position = {static_cast<float>(i), 2, 3};
    addition.vertices[i].normal = {0, 1, 0};
    addition.vertices[i].uv = {0.25F, 0.75F};
  }
  addition.indices.resize((geometry.indices.capacity() / 3 + 1) * 3);
  for (std::size_t i = 0; i < addition.indices.size(); ++i)
    addition.indices[i] = static_cast<std::uint16_t>(i % 3);

  std::optional<preview::MeshRange> rejected;
  bool escaped = false;
  allocationAttempts = 0;
  allocationsBeforeFailure = successfulAllocations;
  try {
    rejected = geometry.Append(addition);
  } catch (const std::bad_alloc &) {
    escaped = true;
  }
  allocationsBeforeFailure = -1;
  Require(allocationAttempts == successfulAllocations + 1, "intended allocation was not faulted");
  Require(!escaped && !rejected, "allocation failure escaped the unavailable-mesh fallback");
  Require(geometry.indices == oldIndices &&
              std::ranges::equal(geometry.vertices, oldVertices, SameVertex) &&
              first->firstIndex == 0 && first->indexCount == oldIndices.size(),
          "allocation failure mutated an existing geometry range");
  const auto retry = geometry.Append(addition);
  Require(retry && retry->firstIndex == oldIndices.size() &&
              retry->indexCount == addition.indices.size() &&
              geometry.vertices.size() == oldVertices.size() + addition.vertices.size() &&
              geometry.indices.size() == oldIndices.size() + addition.indices.size(),
          "successful retry did not publish exactly one complete mesh");
  Require(std::ranges::equal(oldVertices, std::span(geometry.vertices).first(oldVertices.size()),
                             SameVertex),
          "retry changed the previous mesh");
  for (std::size_t i = 0; i < addition.vertices.size(); ++i) {
    const auto &vertex = geometry.vertices[oldVertices.size() + i];
    Require(std::ranges::equal(vertex.position, addition.vertices[i].position) &&
                std::ranges::equal(vertex.normal, addition.vertices[i].normal) &&
                std::ranges::equal(vertex.uv, addition.vertices[i].uv),
            "retry lost converted attributes");
  }
  for (std::size_t i = 0; i < addition.indices.size(); ++i)
    Require(geometry.indices[retry->firstIndex + i] == oldVertices.size() + addition.indices[i],
            "retry rebased indices against a partial append");

  // Existing capacity must not grow on every append, and validation must precede allocation.
  geometry.vertices.reserve(geometry.vertices.size() + triangle.vertices.size());
  geometry.indices.reserve(geometry.indices.size() + triangle.indices.size());
  allocationAttempts = 0;
  allocationsBeforeFailure = 0;
  const auto fitting = geometry.Append(triangle);
  triangle.indices[2] = 99;
  const auto invalid = geometry.Append(triangle);
  allocationsBeforeFailure = -1;
  Require(fitting && !invalid && allocationAttempts == 0,
          "fitting or invalid append attempted an unnecessary allocation");
}
} // namespace

void *operator new(std::size_t bytes) {
  if (allocationsBeforeFailure >= 0) {
    ++allocationAttempts;
    if (allocationsBeforeFailure-- == 0)
      throw std::bad_alloc{};
  }
  if (void *memory = std::malloc(bytes ? bytes : 1))
    return memory;
  throw std::bad_alloc{};
}
void *operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }

int main() {
  try {
    CheckFailure(0);
    CheckFailure(1);
    std::cout << "Preview geometry allocation failure and retry contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    allocationsBeforeFailure = -1;
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
