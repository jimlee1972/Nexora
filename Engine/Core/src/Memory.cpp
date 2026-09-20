#include "Nexora/Core/Memory.h"

#include <algorithm>
#include <new>
#include <stdexcept>

#if defined(NEXORA_USE_MIMALLOC)
#include <mimalloc.h>
#endif

namespace nexora::core {
namespace {
void *AlignedAllocate(std::size_t size, std::size_t alignment) {
#if defined(NEXORA_USE_MIMALLOC)
  void *memory = mi_malloc_aligned(size, alignment);
  if (memory == nullptr)
    throw std::bad_alloc{};
  return memory;
#else
  return ::operator new(size, std::align_val_t{alignment});
#endif
}

void AlignedDeallocate(void *memory, std::size_t alignment) noexcept {
#if defined(NEXORA_USE_MIMALLOC)
  (void)alignment;
  mi_free(memory);
#else
  ::operator delete(memory, std::align_val_t{alignment});
#endif
}
} // namespace

void *TrackingAllocator::Allocate(std::size_t size, std::size_t alignment, MemoryTag tag) {
  if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
    throw std::invalid_argument("allocation size and alignment must be valid");
  }
  void *memory = AlignedAllocate(size, alignment);
  std::lock_guard lock{mutex_};
  allocations_.emplace(memory, Allocation{size, tag, alignment});
  statistics_.live_bytes += size;
  statistics_.peak_bytes = std::max(statistics_.peak_bytes, statistics_.live_bytes);
  ++statistics_.allocation_count;
  bytes_by_tag_[tag] += size;
  return memory;
}

void TrackingAllocator::Deallocate(void *memory) noexcept {
  if (memory == nullptr)
    return;
  std::size_t alignment = alignof(std::max_align_t);
  {
    std::lock_guard lock{mutex_};
    const auto found = allocations_.find(memory);
    if (found == allocations_.end())
      return;
    alignment = found->second.alignment;
    statistics_.live_bytes -= found->second.size;
    ++statistics_.free_count;
    bytes_by_tag_[found->second.tag] -= found->second.size;
    allocations_.erase(found);
  }
  AlignedDeallocate(memory, alignment);
}

MemoryStatistics TrackingAllocator::Statistics() const noexcept {
  std::lock_guard lock{mutex_};
  return statistics_;
}

std::size_t TrackingAllocator::BytesForTag(MemoryTag tag) const noexcept {
  std::lock_guard lock{mutex_};
  const auto found = bytes_by_tag_.find(tag);
  return found == bytes_by_tag_.end() ? 0 : found->second;
}

FrameArena::FrameArena(std::size_t capacity)
    : storage_(std::make_unique<std::byte[]>(capacity)), capacity_(capacity) {
  if (capacity == 0)
    throw std::invalid_argument("frame arena capacity must be non-zero");
}

// Size/alignment ordering mirrors the standard aligned-allocation APIs.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void *FrameArena::Allocate(std::size_t size, std::size_t alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    throw std::invalid_argument("alignment must be a power of two");
  }
  const auto aligned = (offset_ + alignment - 1) & ~(alignment - 1);
  if (size > capacity_ - std::min(aligned, capacity_))
    throw std::bad_alloc{};
  auto *result = storage_.get() + aligned;
  offset_ = aligned + size;
  return result;
}

void FrameArena::Reset() noexcept {
  offset_ = 0;
  ++generation_;
  if (generation_ == 0)
    generation_ = 1;
}

} // namespace nexora::core
