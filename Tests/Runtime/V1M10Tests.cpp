#include "Nexora/Runtime/LargeWorld.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace lw = nexora::runtime::large_world;
namespace {
void Require(bool c, const char *m) {
  if (!c)
    throw std::runtime_error(m);
}
lw::Bounds Box(double x0, double z0, double x1, double z1) { return {{x0, -10, z0}, {x1, 10, z1}}; }
void Spatial() {
  lw::FixedGrid grid{100};
  Require(grid.Coordinate({-0.1, 0, 100}) == lw::GridCoordinate{-1, 1},
          "fixed grid boundary failed");
  lw::LooseQuadtree tree{Box(-1000, -1000, 1000, 1000), 4, 8, 1.5};
  Require(tree.Insert({2, Box(10, 10, 20, 20)}) && tree.Insert({1, Box(-20, -20, -10, -10)}),
          "spatial insert failed");
  Require(tree.Query(Box(-25, -25, 0, 0)) == std::vector<lw::Id>{1}, "spatial query failed");
  Require(!tree.Insert({1, Box(0, 0, 1, 1)}) && tree.Remove(1), "spatial failure contract failed");
}
void Streaming() {
  lw::StreamingManager s{{1000, 1000}, 20};
  Require(s.AddCell({10, 110, 210, Box(0, 0, 100, 100), 400, 300, 40, 30}) &&
              s.AddCell({11, 111, 211, Box(100, 0, 200, 100), 400, 300, 40, 30}),
          "cell add failed");
  Require(!s.AddCell({12, 12, 212, Box(200, 0, 300, 100), 1, 1, 1, 1}),
          "cell/bundle identity accepted");
  Require(s.AddPortal(10, 11) && s.AddOrUpdateSource({1, {50, 0, 50}, 25, 60, 5}) && s.Update(),
          "streaming update failed");
  Require(s.Status(10)->residency == lw::Residency::Full &&
              s.Status(11)->residency == lw::Residency::Hlod && s.Status(11)->prefetched,
          "portal prefetch failed");
  Require(s.Metrics().usage.ram == 440 && s.Metrics().usage.vram == 330, "budget metrics failed");
  Require(s.SetOccupied(10, true) && s.AddOrUpdateSource({1, {1000, 0, 1000}, 1, 1, 5}) &&
              s.Update(),
          "occupied update failed");
  Require(s.Status(10)->residency == lw::Residency::Full, "occupied collision cell unloaded");
  Require(s.SetOccupied(10, false) && s.Update() &&
              s.Status(10)->residency == lw::Residency::Unloaded,
          "cell unpin failed");
  lw::StreamingManager tight{{50, 50}, 0};
  Require(tight.AddCell({20, 120, 220, Box(0, 0, 1, 1), 100, 100, 10, 10}) &&
              tight.AddOrUpdateSource({1, {0, 0, 0}, 10, 20, 1}) && !tight.Update(),
          "budget rejection failed");
  Require(tight.Metrics().rejected_transitions == 1, "rejection metric failed");
}
void Content() {
  lw::OfflineHlodSet h;
  Require(h.Add({1, 10, 100, 200, Box(0, 0, 100, 100)}) &&
              !h.Add({1, 10, 100, 200, Box(0, 0, 100, 100)}),
          "HLOD validation failed");
  lw::TerrainClipmap t;
  Require(t.Build(32, 64, 4) && t.PatchCount() == 1024 &&
              !t.Cull(Box(-128, -128, 128, 128), {0, 0, 0}).empty(),
          "terrain failed");
  lw::VegetationField v;
  std::vector<lw::VegetationInstance> trees;
  for (int i = 0; i < 10000; ++i)
    trees.push_back({{double(i % 100), 0, double(i / 100)}, 1});
  Require(v.SetSpecies(7, std::move(trees)) && v.SpeciesCount() == 1 && v.InstanceCount() == 10000,
          "vegetation storage failed");
  auto batches = v.Cull(Box(0, 0, 99, 99), {0, 0, 0}, 50);
  Require(batches.size() == 2 && batches[0].count + batches[1].count == 10000,
          "vegetation LOD failed");
}
void Baseline() {
  lw::LooseQuadtree tree{Box(0, 0, 10000, 10000), 8, 10, 1.5};
  auto begin = std::chrono::steady_clock::now();
  for (std::uint64_t i = 1; i <= 10000; ++i) {
    double x = double(i % 100), z = double(i / 100);
    Require(tree.Insert({i, Box(x, z, x + .5, z + .5)}), "baseline insert failed");
  }
  Require(!tree.Query(Box(0, 0, 50, 50)).empty() &&
              std::chrono::steady_clock::now() - begin < std::chrono::seconds(5),
          "spatial baseline failed");
}
} // namespace
int main() {
  try {
    Spatial();
    Streaming();
    Content();
    Baseline();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
