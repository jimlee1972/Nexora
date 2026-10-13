#include "SceneGeometryResidency.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
using namespace Nexora::Presentation;
using namespace Nexora::Presentation::detail;
struct Destroy {
  static inline unsigned released{};
  void operator()(int *resource) const noexcept {
    ++released;
    delete resource;
  }
};
using Resource = std::unique_ptr<int, Destroy>;
using Cache = SceneGeometryResidency<Resource>;
void Run() {
  std::array<SceneVertex, 6> vertices{};
  vertices[1].position[0] = vertices[4].position[0] = 1;
  vertices[2].position[1] = vertices[5].position[1] = 1;
  std::array<std::uint16_t, 6> indices{0, 1, 2, 3, 4, 5};
  SceneDrawData draw{vertices, indices};
  const auto first = MeshGeometryView::From(draw, {0, 3, 0, 1});
  const auto shifted = MeshGeometryView::From(draw, {3, 3, 0, 1});
  Require(first && shifted, "canonical fixture rejected");
  auto key = MeshGeometryKey::Capture(*first);
  Require(key && key.Matches(*shifted) && key.VertexBytes() == 3 * sizeof(SceneVertex),
          "combined-layout offset became mesh identity");
  vertices[3].uv[0] = 0.25F;
  auto changed = *MeshGeometryView::From(draw, {3, 3, 0, 1});
  changed.fingerprint = first->fingerprint;
  Require(!key.Matches(changed), "fingerprint collision authorized changed vertex content");
  vertices[3].uv[0] = 0;
  indices[4] = 5;
  changed = *MeshGeometryView::From(draw, {3, 3, 0, 1});
  changed.fingerprint = first->fingerprint;
  Require(!key.Matches(changed), "fingerprint collision authorized changed index content");
  indices[4] = 4;
  vertices[3].uv[0] = 0;
  vertices[0].normal[0] = 1;
  Require(key.Matches(*shifted), "owning key borrowed subsequently modified source data");
  indices[5] = 99;
  Require(!MeshGeometryView::From(draw, {3, 3, 0, 1}) &&
              !MeshGeometryView::From(draw, {6, 3, 0, 1}) && !MeshGeometryKey::Capture({}),
          "invalid range or unbounded/empty content accepted");
  indices[5] = 5;
  vertices[3].uv[0] = 0;

  Cache cache;
  SceneGeometryResidencyDiagnostics stats;
  const auto admit = [&](unsigned id, std::size_t bytes) {
    vertices[0].uv[0] = static_cast<float>(id);
    const auto view = *MeshGeometryView::From(draw, {0, 3, 0, 1});
    auto candidate = MeshGeometryKey::Capture(view);
    const auto plan = cache.Plan(bytes);
    Require(plan.has_value(), "available cache admission failed");
    cache.Commit(*plan, std::move(candidate), Resource(new int(id)), bytes, stats);
    return plan->slot;
  };
  constexpr auto large = Cache::kCandidateBudget;
  std::array<std::size_t, 4> slots{};
  for (unsigned i = 0; i < slots.size(); ++i)
    slots[i] = admit(i, large);
  const auto ownedBytes = stats.retainedCpuBytes;
  Require(stats.nativeAllocationBytes == Cache::kBudget && !cache.Plan(large) &&
              !cache.Plan(large + 1) && !cache.Plan(0),
          "admission exceeded budget or evicted a current unsubmitted lease");
  cache.BeginFrame();
  vertices[0].uv[0] = 0;
  const auto view = *MeshGeometryView::From(draw, {0, 3, 0, 1});
  Require(cache.Find(view, stats) == slots[0], "retained content was not reusable next frame");
  const auto plan = cache.Plan(large);
  Require(plan && plan->retire.count() == 1 && plan->retire[slots[1]] && !plan->retire[slots[0]] &&
              Destroy::released == 0,
          "LRU planning retired resources before completion or ignored a new lease");
  // Abandoning a staged allocation/failing completion must not publish the plan.
  Require(cache.At(slots[1]).resource && stats.evictions == 0 &&
              stats.retainedCpuBytes == ownedBytes && stats.nativeAllocationBytes == Cache::kBudget,
          "abandoned plan mutated resident ownership/accounting");
  admit(4, large);
  vertices[0].uv[0] = 0;
  Require(Destroy::released == 1 && stats.evictions == 1 &&
              stats.nativeAllocationBytes == Cache::kBudget &&
              stats.retainedCpuBytes == ownedBytes && cache.Find(view, stats) == slots[0],
          "completed eviction lost protected content or inflated accounting");
  cache.Clear(stats);
  Require(Destroy::released == 5 && stats.retainedCpuBytes == 0 &&
              stats.nativeAllocationBytes == 0 && stats.uploads == 5,
          "teardown retained resource/key ownership or reset cumulative counters");
  cache.Clear(stats);
  Require(Destroy::released == 5, "idempotent teardown released resources twice");

  for (unsigned i = 0; i < Cache::kEntries; ++i)
    admit(i, 4096);
  Require(!cache.Plan(4096), "entry cap evicted a current draw lease");
  cache.BeginFrame();
  Require(cache.Plan(4096).has_value(), "entry cap never became reusable after completion");
  cache.Clear(stats);
  Require(Destroy::released == 69 && stats.retainedCpuBytes == 0 &&
              stats.nativeAllocationBytes == 0,
          "entry-cap teardown retained ownership");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Canonical mesh residency, budget and lease contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
