#include "Nexora/Runtime/LargeWorld.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
  Require(!s.AddCell({12, 12, 212, Box(200, 0, 300, 100), 1, 1, 1, 1}) &&
              !s.AddCell({12, 110, 212, Box(200, 0, 300, 100), 1, 1, 1, 1}),
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
void V2Foundation() {
  const std::vector<lw::SpatialItem> items{
      {3, Box(140, 10, 150, 20)}, {1, Box(10, 10, 20, 20)}, {2, Box(20, 10, 30, 20)}};
  lw::AdaptivePartitionBuilder builder{100};
  const auto first = builder.Build(items);
  const std::vector<lw::SpatialItem> reordered{items.rbegin(), items.rend()};
  const auto second = builder.Build(reordered);
  Require(first && second && first->build_hash == second->build_hash &&
              first->cells.size() == second->cells.size() &&
              first->cells.front().id == second->cells.front().id &&
              first->cells.front().content == second->cells.front().content,
          "V2 partition build is not deterministic");
  const std::array<lw::Id, 1> changed{3};
  const auto incremental = builder.Rebuild(*first, reordered, changed);
  Require(incremental && incremental->build_hash == first->build_hash,
          "V2 unchanged incremental build diverged");

  lw::WorldOrigin origin{1000, 256};
  Require(!origin.Update({999, 0, 0}), "origin rebased before threshold");
  const auto rebase = origin.Update({1300, 0, -20});
  Require(rebase && rebase->current_origin.x == 1280 && rebase->sequence == 1 &&
              origin.ToRenderRelative({1301, 0, -20}).x == 21,
          "origin rebase was not quantized or identity-neutral");

  lw::PersistentDeltaStore deltas;
  Require(deltas.Apply({7, 20, 1, false, "open"}) && deltas.Apply({7, 10, 2, true, {}}) &&
              !deltas.Apply({7, 20, 1, false, "stale"}),
          "persistent delta revision contract failed");
  const auto loaded = deltas.Load(7);
  Require(loaded.size() == 2 && loaded[0].object == 10 && loaded[1].payload == "open" &&
              deltas.Digest(7) != 0,
          "persistent delta reload is not deterministic");
}
void V2PartitionAndHlod() {
  const std::vector<lw::SpatialItem> dense{
      {1, Box(1, 1, 2, 2)}, {2, Box(20, 1, 21, 2)}, {3, Box(130, 1, 131, 2)}};
  lw::AdaptivePartitionBuilder adaptive{16, 1, 4};
  const auto partition = adaptive.Build(dense);
  Require(partition && partition->cells.size() == 3 &&
              std::ranges::any_of(partition->cells,
                                  [](const auto &cell) { return cell.coordinate.level > 0; }),
          "adaptive quadtree did not preserve sparse coarse cells");
  const auto groups = lw::CellGroupBuilder{2}.Build(*partition);
  Require(groups.size() == 2 && groups[0].cells.size() == 2, "hierarchical groups failed");

  const std::vector<lw::SpatialItem> vertical{{10, {{1, 1, 1}, {2, 2, 2}}},
                                              {11, {{1, 65, 1}, {2, 66, 2}}}};
  const auto volume =
      lw::VolumePartitionBuilder{32, lw::VolumePartitionMode::Grid3D}.Build(vertical);
  Require(volume && volume->cells.size() == 2 &&
              volume->cells[0].coordinate.y != volume->cells[1].coordinate.y,
          "3D volume partition collapsed vertical cells");

  lw::HlodV2 hlod;
  Require(hlod.AddTier({100, groups[0].id, lw::HlodRepresentation::MergedMesh, 100, 1}) &&
              hlod.AddTier({101, groups[0].id, lw::HlodRepresentation::Impostor, 500, 2}) &&
              hlod.Select(groups[0].id, 600).show_full_content,
          "HLOD hid full content before proxy readiness");
  Require(hlod.SetReady(100, true) && hlod.Select(groups[0].id, 200).visible_tier == 100 &&
              !hlod.Select(groups[0].id, 200).show_full_content,
          "HLOD ready switch failed");
  Require(hlod.SetReady(101, true) && hlod.Select(groups[0].id, 600).visible_tier == 101,
          "HLOD multi-tier selection failed");
  const std::array<lw::Id, 2> assets{9, 4};
  const auto impostor_a = lw::ImpostorBuilder{}.Build(groups[0].id, assets, 8, 512);
  const std::array<lw::Id, 2> reversed{4, 9};
  const auto impostor_b = lw::ImpostorBuilder{}.Build(groups[0].id, reversed, 8, 512);
  Require(impostor_a && impostor_b && impostor_a->content_hash == impostor_b->content_hash,
          "impostor build was not deterministic");
}
void V2ResidencyAndPersistence() {
  lw::StreamingManager streaming{{1000, 1000}, 0};
  Require(streaming.AddCell({1, 101, 201, Box(0, 0, 100, 100), 10, 10, 1, 1}) &&
              streaming.AddCell({2, 102, 202, Box(100, 0, 200, 100), 10, 10, 1, 1}) &&
              streaming.SetOccupiedFootprint(77, Box(99, 20, 101, 21)) && streaming.Update() &&
              streaming.Status(1)->residency == lw::Residency::Full &&
              streaming.Status(2)->residency == lw::Residency::Full,
          "adaptive/3D boundary collision cells were not pinned");
  Require(streaming.ClearOccupiedFootprint(77) && streaming.Update() &&
              streaming.Status(1)->residency == lw::Residency::Unloaded,
          "occupied footprint did not release cells");

  lw::PersistentDeltaStore deltas;
  Require(deltas.Apply({9, 1, 1, false, "opened"}) && deltas.Apply({9, 2, 1, true, {}}),
          "persistent capture failed");
  const std::unordered_map<lw::Id, std::string> defaults{{1, "closed"}, {2, "alive"}, {3, "idle"}};
  const auto loaded = deltas.Materialize(9, defaults);
  Require(loaded.at(1) == "opened" && !loaded.contains(2) && loaded.at(3) == "idle",
          "persistent unload/reload materialization failed");
}
} // namespace
int main() {
  try {
    Spatial();
    Streaming();
    Content();
    Baseline();
    V2Foundation();
    V2PartitionAndHlod();
    V2ResidencyAndPersistence();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
