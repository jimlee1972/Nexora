#pragma once

#include <cstddef>
#include <limits>
#include <utility>

namespace Nexora::Presentation::detail {
// Caller has waited the owning slot's GPU completion. ComPtr-like ownership and capacity are
// published together; a failed or empty candidate never releases the live retained allocation.
template <class Resource, class Allocate>
bool EnsureUploadAllocation(Resource &resource, std::size_t &capacity, std::size_t required,
                            Allocate &&allocate) {
  if (!required)
    return false;
  if (resource.Get() && capacity >= required)
    return true;
  std::size_t candidateCapacity = 4096;
  while (candidateCapacity < required) {
    if (candidateCapacity > std::numeric_limits<std::size_t>::max() / 2)
      return false;
    candidateCapacity *= 2;
  }
  Resource candidate;
  if (!std::forward<Allocate>(allocate)(candidate, candidateCapacity) || !candidate.Get())
    return false;
  resource = std::move(candidate);
  capacity = candidateCapacity;
  return true;
}
} // namespace Nexora::Presentation::detail
