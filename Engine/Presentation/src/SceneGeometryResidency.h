#pragma once

#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <bitset>
#include <cstring>
#include <new>

namespace Nexora::Presentation::detail {
// Canonical content of one validated batch: its referenced contiguous vertex interval and
// locally rebased indices. Combined-upload offsets, materials and instances are not identity.
struct MeshGeometryView final {
  std::span<const SceneVertex> vertices;
  std::span<const std::uint16_t> indices;
  std::uint16_t base{};
  std::uint64_t fingerprint{};

  static std::optional<MeshGeometryView> From(const SceneDrawData &data,
                                              const SceneMeshBatch &batch) noexcept {
    if (data.vertices.empty() || data.vertices.size() > 65535 || !batch.indexCount ||
        batch.indexCount > 1048576 || batch.firstIndex > data.indices.size() ||
        batch.indexCount > data.indices.size() - batch.firstIndex)
      return std::nullopt;
    const auto indices = data.indices.subspan(batch.firstIndex, batch.indexCount);
    const auto [low, high] = std::minmax_element(indices.begin(), indices.end());
    if (*high >= data.vertices.size())
      return std::nullopt;
    MeshGeometryView view{data.vertices.subspan(*low, *high - *low + 1), indices, *low,
                          14695981039346656037ULL};
    const auto hash = [&](unsigned char byte) {
      view.fingerprint = (view.fingerprint ^ byte) * 1099511628211ULL;
    };
    for (const auto byte : std::as_bytes(view.vertices))
      hash(std::to_integer<unsigned char>(byte));
    for (const auto index : indices) {
      const auto local = static_cast<std::uint16_t>(index - view.base);
      hash(static_cast<unsigned char>(local));
      hash(static_cast<unsigned char>(local >> 8));
    }
    return view;
  }
  std::size_t VertexBytes() const noexcept { return vertices.size_bytes(); }
  std::size_t Bytes() const noexcept { return VertexBytes() + indices.size_bytes(); }
};

class MeshGeometryKey final {
public:
  static MeshGeometryKey Capture(const MeshGeometryView &view) noexcept {
    MeshGeometryKey key;
    if (view.vertices.empty() || view.indices.empty() || view.vertices.size() > 65535 ||
        view.indices.size() > 1048576)
      return key;
    key.payload_.reset(new (std::nothrow) std::byte[view.Bytes()]);
    if (!key.payload_)
      return key;
    key.vertexBytes_ = view.VertexBytes();
    key.bytes_ = view.Bytes();
    key.fingerprint_ = view.fingerprint;
    std::memcpy(key.payload_.get(), view.vertices.data(), key.vertexBytes_);
    for (std::size_t i = 0; i < view.indices.size(); ++i) {
      const auto local = static_cast<std::uint16_t>(view.indices[i] - view.base);
      std::memcpy(key.payload_.get() + key.vertexBytes_ + i * sizeof(local), &local, sizeof(local));
    }
    return key;
  }
  bool Matches(const MeshGeometryView &view) const noexcept {
    // The fingerprint only rejects candidates; exact bytes always authorize a cache hit.
    if (!payload_ || fingerprint_ != view.fingerprint || bytes_ != view.Bytes() ||
        vertexBytes_ != view.VertexBytes() ||
        std::memcmp(payload_.get(), view.vertices.data(), vertexBytes_) != 0)
      return false;
    for (std::size_t i = 0; i < view.indices.size(); ++i) {
      std::uint16_t local{};
      std::memcpy(&local, payload_.get() + vertexBytes_ + i * sizeof(local), sizeof(local));
      if (local != view.indices[i] - view.base)
        return false;
    }
    return true;
  }
  explicit operator bool() const noexcept { return payload_ != nullptr; }
  const std::byte *Data() const noexcept { return payload_.get(); }
  std::size_t Bytes() const noexcept { return bytes_; }
  std::size_t VertexBytes() const noexcept { return vertexBytes_; }

private:
  std::unique_ptr<std::byte[]> payload_;
  std::size_t bytes_{}, vertexBytes_{};
  std::uint64_t fingerprint_{};
};

// Resource is an owning movable native allocation with a boolean validity check. Native
// adapters stage allocation/copy before Commit and complete prior GPU work before any eviction.
template <class Resource> class SceneGeometryResidency final {
public:
  static constexpr std::size_t kEntries = 64;
  static constexpr std::size_t kBudget = 32 * 1024 * 1024;
  static constexpr std::size_t kCandidateBudget = 8 * 1024 * 1024;
  struct Entry final {
    MeshGeometryKey key;
    Resource resource;
    std::size_t allocationBytes{};
    std::uint64_t age{};
    bool leased{};
  };
  struct Admission final {
    std::size_t slot{};
    std::bitset<kEntries> retire;
  };
  void BeginFrame() noexcept {
    for (auto &entry : entries_)
      entry.leased = false;
  }
  std::optional<std::size_t> Find(const MeshGeometryView &view,
                                  SceneGeometryResidencyDiagnostics &stats) noexcept {
    for (std::size_t slot = 0; slot < kEntries; ++slot) {
      auto &entry = entries_[slot];
      if (entry.resource && entry.key.Matches(view)) {
        entry.leased = true;
        entry.age = NextAge();
        stats.available = true;
        ++stats.hits;
        return slot;
      }
    }
    return std::nullopt;
  }
  std::optional<Admission> Plan(std::size_t allocationBytes) const noexcept {
    if (!allocationBytes || allocationBytes > kCandidateBudget)
      return std::nullopt;
    Admission plan;
    std::size_t resident = 0;
    std::optional<std::size_t> empty;
    for (std::size_t slot = 0; slot < kEntries; ++slot) {
      resident += entries_[slot].allocationBytes;
      if (!entries_[slot].resource && !empty)
        empty = slot;
    }
    while (!empty || resident > kBudget - allocationBytes) {
      std::optional<std::size_t> oldest;
      for (std::size_t slot = 0; slot < kEntries; ++slot) {
        const auto &entry = entries_[slot];
        if (entry.resource && !entry.leased && !plan.retire[slot] &&
            (!oldest || entry.age < entries_[*oldest].age))
          oldest = slot;
      }
      if (!oldest)
        return std::nullopt;
      plan.retire.set(*oldest);
      resident -= entries_[*oldest].allocationBytes;
      if (!empty)
        empty = *oldest;
    }
    plan.slot = *empty;
    return plan;
  }
  // Caller holds an unchanged plan, a complete immutable candidate, and (when retire.any())
  // proof that prior submitted work is complete. Leases exclude the current unsubmitted draw.
  void Commit(const Admission &plan, MeshGeometryKey key, Resource resource,
              std::size_t allocationBytes, SceneGeometryResidencyDiagnostics &stats) noexcept {
    for (std::size_t slot = 0; slot < kEntries; ++slot)
      if (plan.retire[slot]) {
        stats.retainedCpuBytes -= entries_[slot].key.Bytes();
        stats.nativeAllocationBytes -= entries_[slot].allocationBytes;
        entries_[slot] = {};
        ++stats.evictions;
      }
    auto &entry = entries_[plan.slot];
    entry.key = std::move(key);
    entry.resource = std::move(resource);
    entry.allocationBytes = allocationBytes;
    entry.age = NextAge();
    entry.leased = true;
    stats.available = true;
    ++stats.uploads;
    stats.retainedCpuBytes += entry.key.Bytes();
    stats.nativeAllocationBytes += allocationBytes;
  }
  const Entry &At(std::size_t slot) const noexcept { return entries_[slot]; }
  void Clear(SceneGeometryResidencyDiagnostics &stats) noexcept {
    for (auto &entry : entries_) {
      stats.retainedCpuBytes -= entry.key.Bytes();
      stats.nativeAllocationBytes -= entry.allocationBytes;
      entry = {};
    }
  }

private:
  std::uint64_t NextAge() noexcept {
    if (age_ == UINT64_MAX) {
      for (auto &entry : entries_)
        entry.age = 0;
      age_ = 0;
    }
    return ++age_;
  }
  std::array<Entry, kEntries> entries_{};
  std::uint64_t age_{};
};
} // namespace Nexora::Presentation::detail
