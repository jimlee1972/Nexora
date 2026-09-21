#include "Nexora/Runtime/LargeWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace nexora::runtime::large_world {
namespace {
bool Finite(Vec3d value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool Fits(MemoryUsage usage, MemoryUsage additional, StreamingBudget budget) {
  return usage.ram <= budget.ram && additional.ram <= budget.ram - usage.ram &&
         usage.vram <= budget.vram && additional.vram <= budget.vram - usage.vram;
}
MemoryUsage Add(MemoryUsage a, MemoryUsage b) { return {a.ram + b.ram, a.vram + b.vram}; }
MemoryUsage Cost(const CellDescriptor &cell, Residency state) {
  if (state == Residency::Full)
    return {cell.full_ram, cell.full_vram};
  if (state == Residency::Hlod)
    return {cell.hlod_ram, cell.hlod_vram};
  return {};
}
} // namespace

bool Bounds::Valid() const noexcept {
  return Finite(minimum) && Finite(maximum) && minimum.x <= maximum.x && minimum.y <= maximum.y &&
         minimum.z <= maximum.z;
}
bool Bounds::Intersects(const Bounds &other) const noexcept {
  return Valid() && other.Valid() && minimum.x <= other.maximum.x && maximum.x >= other.minimum.x &&
         minimum.y <= other.maximum.y && maximum.y >= other.minimum.y &&
         minimum.z <= other.maximum.z && maximum.z >= other.minimum.z;
}
double Bounds::DistanceSquared(Vec3d point) const noexcept {
  if (!Valid() || !Finite(point))
    return std::numeric_limits<double>::infinity();
  const auto axis = [](double value, double low, double high) {
    return value < low ? low - value : (value > high ? value - high : 0.0);
  };
  const double dx = axis(point.x, minimum.x, maximum.x), dy = axis(point.y, minimum.y, maximum.y),
               dz = axis(point.z, minimum.z, maximum.z);
  return dx * dx + dy * dy + dz * dz;
}

FixedGrid::FixedGrid(double cell_size) : cell_size_(cell_size) {}
std::optional<GridCoordinate> FixedGrid::Coordinate(Vec3d position) const noexcept {
  if (!Finite(position) || !std::isfinite(cell_size_) || cell_size_ <= 0.0)
    return std::nullopt;
  return GridCoordinate{static_cast<std::int64_t>(std::floor(position.x / cell_size_)),
                        static_cast<std::int64_t>(std::floor(position.z / cell_size_))};
}
std::optional<Bounds> FixedGrid::CellBounds(GridCoordinate coordinate) const noexcept {
  if (!std::isfinite(cell_size_) || cell_size_ <= 0.0)
    return std::nullopt;
  const double x = static_cast<double>(coordinate.x) * cell_size_,
               z = static_cast<double>(coordinate.z) * cell_size_;
  return Bounds{{x, -std::numeric_limits<double>::max(), z},
                {x + cell_size_, std::numeric_limits<double>::max(), z + cell_size_}};
}

LooseQuadtree::LooseQuadtree(Bounds bounds, std::size_t capacity, std::size_t max_depth,
                             double looseness)
    : world_bounds_(bounds), capacity_(capacity), max_depth_(max_depth), looseness_(looseness) {}
bool LooseQuadtree::Insert(SpatialItem item) {
  if (item.id == 0 || !item.bounds.Valid() || !world_bounds_.Valid() ||
      !world_bounds_.Intersects(item.bounds) || capacity_ == 0 || max_depth_ == 0 ||
      !std::isfinite(looseness_) || looseness_ < 1.0 || items_.contains(item.id))
    return false;
  return items_.emplace(item.id, item.bounds).second;
}
bool LooseQuadtree::Remove(Id id) { return items_.erase(id) == 1; }
std::vector<Id> LooseQuadtree::Query(const Bounds &bounds) const {
  std::vector<Id> result;
  if (!bounds.Valid())
    return result;
  for (const auto &[id, item] : items_)
    if (item.Intersects(bounds))
      result.push_back(id);
  std::ranges::sort(result);
  return result;
}

StreamingManager::StreamingManager(StreamingBudget budget, double unload_hysteresis)
    : budget_(budget),
      hysteresis_(std::isfinite(unload_hysteresis) && unload_hysteresis >= 0.0
                      ? unload_hysteresis
                      : 0.0) {}
bool StreamingManager::AddCell(CellDescriptor cell) {
  if (cell.cell_id == 0 || cell.full_bundle_id == 0 || cell.hlod_bundle_id == 0 ||
      cell.cell_id == cell.full_bundle_id || cell.cell_id == cell.hlod_bundle_id ||
      cell.full_bundle_id == cell.hlod_bundle_id || !cell.bounds.Valid())
    return false;
  for (const auto &[existing_id, existing] : cells_)
    if (cell.cell_id == existing_id || cell.cell_id == existing.descriptor.full_bundle_id ||
        cell.cell_id == existing.descriptor.hlod_bundle_id ||
        cell.full_bundle_id == existing_id ||
        cell.full_bundle_id == existing.descriptor.full_bundle_id ||
        cell.full_bundle_id == existing.descriptor.hlod_bundle_id ||
        cell.hlod_bundle_id == existing_id ||
        cell.hlod_bundle_id == existing.descriptor.full_bundle_id ||
        cell.hlod_bundle_id == existing.descriptor.hlod_bundle_id)
      return false;
  return cells_.emplace(cell.cell_id, CellRecord{cell, {}}).second;
}
bool StreamingManager::AddOrUpdateSource(StreamingSource source) {
  if (source.id == 0 || !Finite(source.position) || !std::isfinite(source.full_radius) ||
      !std::isfinite(source.prefetch_radius) || source.full_radius < 0.0 ||
      source.prefetch_radius < source.full_radius)
    return false;
  sources_.insert_or_assign(source.id, source);
  return true;
}
bool StreamingManager::RemoveSource(Id source) { return sources_.erase(source) == 1; }
bool StreamingManager::AddPortal(Id from, Id to) {
  if (from == to || !cells_.contains(from) || !cells_.contains(to))
    return false;
  return portals_[from].insert(to).second;
}
bool StreamingManager::SetOccupied(Id cell, bool occupied) {
  const auto found = cells_.find(cell);
  if (found == cells_.end())
    return false;
  found->second.status.occupied = occupied;
  return true;
}
MemoryUsage StreamingManager::Usage() const noexcept {
  MemoryUsage result{};
  for (const auto &[id, cell] : cells_)
    result = Add(result, Cost(cell.descriptor, cell.status.residency));
  return result;
}
bool StreamingManager::Update() {
  struct Request {
    Id id{};
    Residency desired{};
    int priority{};
    double distance{};
    bool portal{};
  };
  std::vector<Request> requests;
  for (auto &[id, cell] : cells_) {
    double nearest = std::numeric_limits<double>::infinity();
    int priority = std::numeric_limits<int>::min();
    Residency desired = Residency::Unloaded;
    for (const auto &[source_id, source] : sources_) {
      const double distance = std::sqrt(cell.descriptor.bounds.DistanceSquared(source.position));
      const double full_limit =
          source.full_radius + (cell.status.residency == Residency::Full ? hysteresis_ : 0.0);
      const double prefetch_limit =
          source.prefetch_radius +
          (cell.status.residency != Residency::Unloaded ? hysteresis_ : 0.0);
      if (distance <= prefetch_limit) {
        desired = std::max(desired, distance <= full_limit ? Residency::Full : Residency::Hlod);
        priority = std::max(priority, source.priority);
        nearest = std::min(nearest, distance);
      }
    }
    if (cell.status.occupied) {
      desired = Residency::Full;
      priority = std::numeric_limits<int>::max();
      nearest = 0.0;
    }
    requests.push_back({id, desired, priority, nearest, false});
  }
  for (const auto &request : requests)
    if (request.desired == Residency::Full)
      if (const auto links = portals_.find(request.id); links != portals_.end())
        for (Id target : links->second) {
          auto found = std::ranges::find(requests, target, &Request::id);
          if (found != requests.end() && found->desired == Residency::Unloaded) {
            found->desired = Residency::Hlod;
            found->priority = request.priority;
            found->portal = true;
          }
        }
  std::ranges::sort(requests, [](const Request &a, const Request &b) {
    if (a.desired != b.desired)
      return a.desired > b.desired;
    if (a.priority != b.priority)
      return a.priority > b.priority;
    if (a.distance != b.distance)
      return a.distance < b.distance;
    return a.id < b.id;
  });
  for (auto &[id, cell] : cells_) {
    cell.status.residency = Residency::Unloaded;
    cell.status.demanded = cell.status.prefetched = false;
    cell.status.priority = 0;
  }
  MemoryUsage usage = Usage();
  bool all_satisfied = true;
  for (const auto &request : requests) {
    auto &cell = cells_.at(request.id);
    if (request.desired == Residency::Unloaded)
      continue;
    const auto cost = Cost(cell.descriptor, request.desired);
    const auto candidate = Add(usage, cost);
    if (!Fits(usage, cost, budget_) && !cell.status.occupied) {
      ++rejected_;
      all_satisfied = false;
      continue;
    }
    cell.status.residency = request.desired;
    cell.status.demanded = request.desired == Residency::Full;
    cell.status.prefetched = request.desired == Residency::Hlod;
    cell.status.priority = request.priority;
    usage = candidate;
  }
  return all_satisfied;
}
const CellDescriptor *StreamingManager::FindCell(Id id) const {
  const auto found = cells_.find(id);
  return found == cells_.end() ? nullptr : &found->second.descriptor;
}
std::optional<CellStatus> StreamingManager::Status(Id id) const {
  const auto found = cells_.find(id);
  return found == cells_.end() ? std::nullopt : std::optional<CellStatus>{found->second.status};
}
StreamingMetrics StreamingManager::Metrics() const noexcept {
  StreamingMetrics result;
  result.usage = Usage();
  result.rejected_transitions = rejected_;
  for (const auto &[id, cell] : cells_) {
    result.full_cells += cell.status.residency == Residency::Full;
    result.hlod_cells += cell.status.residency == Residency::Hlod;
    result.pinned_cells += cell.status.occupied;
  }
  return result;
}

bool OfflineHlodSet::Add(HlodCluster cluster) {
  if (cluster.id == 0 || cluster.source_cell == 0 || cluster.mesh == 0 || cluster.material == 0 ||
      !cluster.bounds.Valid() ||
      std::ranges::find(clusters_, cluster.id, &HlodCluster::id) != clusters_.end())
    return false;
  clusters_.push_back(cluster);
  return true;
}
bool TerrainClipmap::Build(std::uint32_t side, double patch_size, std::uint32_t lod_levels) {
  if (side == 0 || !std::isfinite(patch_size) || patch_size <= 0.0 || lod_levels == 0)
    return false;
  std::vector<TerrainPatch> next;
  next.reserve(static_cast<std::size_t>(side) * side);
  const auto half = static_cast<std::int64_t>(side / 2);
  for (std::uint32_t z = 0; z < side; ++z)
    for (std::uint32_t x = 0; x < side; ++x) {
      GridCoordinate c{static_cast<std::int64_t>(x) - half, static_cast<std::int64_t>(z) - half};
      const double px = c.x * patch_size, pz = c.z * patch_size;
      next.push_back({c, {{px, -10000.0, pz}, {px + patch_size, 10000.0, pz + patch_size}}, 0});
    }
  patches_ = std::move(next);
  lod_levels_ = lod_levels;
  return true;
}
std::vector<TerrainPatch> TerrainClipmap::Cull(const Bounds &view, Vec3d observer) const {
  std::vector<TerrainPatch> result;
  if (!view.Valid() || !Finite(observer))
    return result;
  for (auto patch : patches_)
    if (patch.bounds.Intersects(view)) {
      patch.lod = std::min(lod_levels_ - 1,
                           static_cast<std::uint32_t>(
                               std::sqrt(patch.bounds.DistanceSquared(observer)) /
                               std::max(1.0, patch.bounds.maximum.x - patch.bounds.minimum.x)));
      result.push_back(patch);
    }
  return result;
}
bool VegetationField::SetSpecies(Id species, std::vector<VegetationInstance> instances) {
  if (species == 0 || std::ranges::any_of(instances, [](const auto &i) {
        return !Finite(i.position) || !std::isfinite(i.scale) || i.scale <= 0.0F;
      }))
    return false;
  species_.insert_or_assign(species, std::move(instances));
  return true;
}
std::vector<VegetationBatch> VegetationField::Cull(const Bounds &view, Vec3d observer,
                                                   double lod_distance) const {
  std::vector<VegetationBatch> result;
  if (!view.Valid() || !Finite(observer) || !std::isfinite(lod_distance) || lod_distance <= 0.0)
    return result;
  for (const auto &[id, instances] : species_) {
    std::size_t near = 0, far = 0;
    for (const auto &instance : instances) {
      Bounds point{instance.position, instance.position};
      if (!view.Intersects(point))
        continue;
      const double distance = std::sqrt(point.DistanceSquared(observer));
      (distance < lod_distance ? near : far)++;
    }
    if (near)
      result.push_back({id, 0, 0, near});
    if (far)
      result.push_back({id, 1, near, far});
  }
  std::ranges::sort(result, {}, &VegetationBatch::species);
  return result;
}
std::size_t VegetationField::InstanceCount() const noexcept {
  std::size_t count = 0;
  for (const auto &[id, instances] : species_)
    count += instances.size();
  return count;
}

} // namespace nexora::runtime::large_world
