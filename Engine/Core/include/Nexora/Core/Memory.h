#pragma once

#include "Nexora/Core/Api.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <unordered_map>

namespace nexora::core {

using MemoryTag = std::uint32_t;
inline constexpr MemoryTag kMemoryTagUnknown = 0;

struct MemoryStatistics final {
  std::size_t live_bytes{};
  std::size_t peak_bytes{};
  std::size_t allocation_count{};
  std::size_t free_count{};
};

class NEXORA_CORE_API TrackingAllocator final {
public:
  [[nodiscard]] void *Allocate(std::size_t size, std::size_t alignment, MemoryTag tag);
  void Deallocate(void *memory) noexcept;
  [[nodiscard]] MemoryStatistics Statistics() const noexcept;
  [[nodiscard]] std::size_t BytesForTag(MemoryTag tag) const noexcept;

private:
  struct Allocation final {
    std::size_t size;
    MemoryTag tag;
    std::size_t alignment;
  };
  mutable std::mutex mutex_;
  std::unordered_map<void *, Allocation> allocations_;
  std::unordered_map<MemoryTag, std::size_t> bytes_by_tag_;
  MemoryStatistics statistics_;
};

class NEXORA_CORE_API FrameArena final {
public:
  explicit FrameArena(std::size_t capacity);
  [[nodiscard]] void *Allocate(std::size_t size, std::size_t alignment);
  void Reset() noexcept;
  [[nodiscard]] std::size_t Used() const noexcept { return offset_; }
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  std::unique_ptr<std::byte[]> storage_;
  std::size_t capacity_{};
  std::size_t offset_{};
  std::uint64_t generation_{1};
};

} // namespace nexora::core
