#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexora::runtime::large_world {

using Id = std::uint64_t;

struct Vec3d final {
  double x{}, y{}, z{};
};
struct Bounds final {
  Vec3d minimum{}, maximum{};
  [[nodiscard]] bool Valid() const noexcept;
  [[nodiscard]] bool Intersects(const Bounds &other) const noexcept;
  [[nodiscard]] double DistanceSquared(Vec3d point) const noexcept;
};
struct GridCoordinate final {
  std::int64_t x{}, z{};
  friend bool operator==(const GridCoordinate &, const GridCoordinate &) = default;
};

class NEXORA_RUNTIME_API FixedGrid final {
public:
  explicit FixedGrid(double cell_size);
  [[nodiscard]] std::optional<GridCoordinate> Coordinate(Vec3d position) const noexcept;
  [[nodiscard]] std::optional<Bounds> CellBounds(GridCoordinate coordinate) const noexcept;

private:
  double cell_size_{};
};

struct SpatialItem final {
  Id id{};
  Bounds bounds{};
};
class NEXORA_RUNTIME_API LooseQuadtree final {
public:
  LooseQuadtree(Bounds world_bounds, std::size_t capacity = 8, std::size_t max_depth = 8,
                double looseness = 1.5);
  bool Insert(SpatialItem item);
  bool Remove(Id id);
  [[nodiscard]] std::vector<Id> Query(const Bounds &bounds) const;
  [[nodiscard]] std::size_t Size() const noexcept { return items_.size(); }

private:
  Bounds world_bounds_{};
  std::size_t capacity_{}, max_depth_{};
  double looseness_{};
  std::unordered_map<Id, Bounds> items_;
};

enum class Residency { Unloaded, Hlod, Full };
struct CellDescriptor final {
  Id cell_id{}, full_bundle_id{}, hlod_bundle_id{};
  Bounds bounds{};
  std::size_t full_ram{}, full_vram{}, hlod_ram{}, hlod_vram{};
};
struct StreamingSource final {
  Id id{};
  Vec3d position{};
  double full_radius{}, prefetch_radius{};
  int priority{};
};
struct MemoryUsage final {
  std::size_t ram{}, vram{};
};
struct StreamingBudget final {
  std::size_t ram{}, vram{};
};
struct CellStatus final {
  Residency residency{Residency::Unloaded};
  bool demanded{}, prefetched{}, occupied{};
  int priority{};
};
struct StreamingMetrics final {
  MemoryUsage usage{};
  std::size_t full_cells{}, hlod_cells{}, pinned_cells{}, rejected_transitions{};
};

class NEXORA_RUNTIME_API StreamingManager final {
public:
  StreamingManager(StreamingBudget budget, double unload_hysteresis);
  bool AddCell(CellDescriptor cell);
  bool AddOrUpdateSource(StreamingSource source);
  bool RemoveSource(Id source);
  bool AddPortal(Id from_cell, Id to_cell);
  bool SetOccupied(Id cell, bool occupied);
  // Replaces one character's occupied set atomically. Every intersected collision cell is pinned.
  bool SetOccupiedFootprint(Id character, const Bounds &footprint);
  bool ClearOccupiedFootprint(Id character);
  bool Update();
  [[nodiscard]] const CellDescriptor *FindCell(Id cell) const;
  [[nodiscard]] std::optional<CellStatus> Status(Id cell) const;
  [[nodiscard]] StreamingMetrics Metrics() const noexcept;

private:
  struct CellRecord final {
    CellDescriptor descriptor;
    CellStatus status;
  };
  [[nodiscard]] MemoryUsage Usage() const noexcept;
  StreamingBudget budget_{};
  double hysteresis_{};
  std::unordered_map<Id, CellRecord> cells_;
  std::unordered_map<Id, StreamingSource> sources_;
  std::unordered_map<Id, std::unordered_set<Id>> portals_;
  std::unordered_set<Id> manually_occupied_;
  std::unordered_map<Id, std::unordered_set<Id>> occupied_by_character_;
  std::size_t rejected_{};
};

struct HlodCluster final {
  Id id{}, source_cell{}, mesh{}, material{};
  Bounds bounds{};
};
class NEXORA_RUNTIME_API OfflineHlodSet final {
public:
  bool Add(HlodCluster cluster);
  [[nodiscard]] std::span<const HlodCluster> Clusters() const noexcept { return clusters_; }

private:
  std::vector<HlodCluster> clusters_;
};

struct TerrainPatch final {
  GridCoordinate coordinate{};
  Bounds bounds{};
  std::uint32_t lod{};
};
class NEXORA_RUNTIME_API TerrainClipmap final {
public:
  bool Build(std::uint32_t side, double patch_size, std::uint32_t lod_levels);
  [[nodiscard]] std::vector<TerrainPatch> Cull(const Bounds &view, Vec3d observer) const;
  [[nodiscard]] std::size_t PatchCount() const noexcept { return patches_.size(); }

private:
  std::uint32_t lod_levels_{};
  std::vector<TerrainPatch> patches_;
};

struct VegetationInstance final {
  Vec3d position{};
  float scale{1.0F};
};
struct VegetationBatch final {
  Id species{};
  std::uint32_t lod{};
  std::size_t first{}, count{};
};
class NEXORA_RUNTIME_API VegetationField final {
public:
  bool SetSpecies(Id species, std::vector<VegetationInstance> instances);
  [[nodiscard]] std::vector<VegetationBatch> Cull(const Bounds &view, Vec3d observer,
                                                  double lod_distance) const;
  [[nodiscard]] std::size_t InstanceCount() const noexcept;
  [[nodiscard]] std::size_t SpeciesCount() const noexcept { return species_.size(); }

private:
  std::unordered_map<Id, std::vector<VegetationInstance>> species_;
};

// V2 partition identities are derived from integer coordinates rather than floating-point
// positions, making build output stable across hosts and repeated incremental builds.
struct PartitionCoordinate final {
  std::int64_t x{}, y{}, z{};
  std::uint8_t level{};
  friend bool operator==(const PartitionCoordinate &, const PartitionCoordinate &) = default;
};
struct PartitionCell final {
  Id id{};
  PartitionCoordinate coordinate{};
  Bounds bounds{};
  std::vector<Id> content;
  std::uint64_t content_hash{};
};
struct PartitionBuild final {
  std::vector<PartitionCell> cells;
  std::uint64_t build_hash{};
};
struct CellGroup final {
  Id id{};
  std::uint8_t level{};
  Bounds bounds{};
  std::vector<Id> cells;
  std::uint64_t content_hash{};
};
class NEXORA_RUNTIME_API CellGroupBuilder final {
public:
  explicit CellGroupBuilder(std::size_t fanout = 4);
  [[nodiscard]] std::vector<CellGroup> Build(const PartitionBuild &partition) const;

private:
  std::size_t fanout_{};
};
class NEXORA_RUNTIME_API AdaptivePartitionBuilder final {
public:
  explicit AdaptivePartitionBuilder(double leaf_size, std::size_t split_threshold = 8,
                                    std::uint8_t max_level = 8);
  [[nodiscard]] std::optional<PartitionBuild> Build(std::span<const SpatialItem> items) const;
  [[nodiscard]] std::optional<PartitionBuild> Rebuild(const PartitionBuild &previous,
                                                      std::span<const SpatialItem> items,
                                                      std::span<const Id> changed_items) const;

private:
  double leaf_size_{};
  std::size_t split_threshold_{};
  std::uint8_t max_level_{};
};

enum class VolumePartitionMode { Grid3D, Explicit };
struct ExplicitVolume final {
  Id id{};
  Bounds bounds{};
};
class NEXORA_RUNTIME_API VolumePartitionBuilder final {
public:
  VolumePartitionBuilder(double cell_size, VolumePartitionMode mode);
  [[nodiscard]] std::optional<PartitionBuild>
  Build(std::span<const SpatialItem> items, std::span<const ExplicitVolume> volumes = {}) const;

private:
  double cell_size_{};
  VolumePartitionMode mode_{};
};

struct OriginRebase final {
  Vec3d previous_origin{}, current_origin{}, render_delta{};
  std::uint64_t sequence{};
};
class NEXORA_RUNTIME_API WorldOrigin final {
public:
  explicit WorldOrigin(double threshold, double quantum);
  [[nodiscard]] std::optional<OriginRebase> Update(Vec3d observer);
  [[nodiscard]] Vec3d ToRenderRelative(Vec3d absolute) const noexcept;
  [[nodiscard]] Vec3d Origin() const noexcept { return origin_; }

private:
  double threshold_{}, quantum_{};
  Vec3d origin_{};
  std::uint64_t sequence_{};
};

enum class HlodRepresentation { Full, MergedMesh, Impostor };
struct HlodTier final {
  Id id{};
  Id group{};
  HlodRepresentation representation{HlodRepresentation::Full};
  double minimum_distance{};
  std::uint64_t artifact_hash{};
};
struct HlodSelection final {
  Id visible_tier{};
  bool show_full_content{true};
};
class NEXORA_RUNTIME_API HlodV2 final {
public:
  bool AddTier(HlodTier tier);
  bool SetReady(Id tier, bool ready);
  [[nodiscard]] HlodSelection Select(Id group, double distance) const;

private:
  std::vector<HlodTier> tiers_;
  std::unordered_set<Id> ready_;
};
struct ImpostorArtifact final {
  Id id{};
  Id group{};
  std::uint32_t views{};
  std::uint32_t resolution{};
  std::uint64_t content_hash{};
};
class NEXORA_RUNTIME_API ImpostorBuilder final {
public:
  [[nodiscard]] std::optional<ImpostorArtifact> Build(Id group, std::span<const Id> source_assets,
                                                      std::uint32_t views,
                                                      std::uint32_t resolution) const;
};

struct PersistentCellDelta final {
  Id cell{}, object{};
  std::uint64_t revision{};
  bool removed{};
  std::string payload;
};
class NEXORA_RUNTIME_API PersistentDeltaStore final {
public:
  bool Apply(PersistentCellDelta delta);
  [[nodiscard]] std::vector<PersistentCellDelta> Load(Id cell) const;
  [[nodiscard]] std::uint64_t Digest(Id cell) const noexcept;
  [[nodiscard]] std::unordered_map<Id, std::string>
  Materialize(Id cell, const std::unordered_map<Id, std::string> &defaults) const;

private:
  std::unordered_map<Id, std::unordered_map<Id, PersistentCellDelta>> cells_;
};

} // namespace nexora::runtime::large_world
