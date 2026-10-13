#include "StagedUploadAllocation.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int live{}, released{};
struct Allocation {
  int id;
  explicit Allocation(int value) : id(value) { ++live; }
  ~Allocation() {
    --live;
    ++released;
  }
};
struct Resource {
  std::unique_ptr<Allocation> value;
  Allocation *Get() const noexcept { return value.get(); }
};
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    using Nexora::Presentation::detail::EnsureUploadAllocation;
    Resource resource{std::make_unique<Allocation>(1)};
    std::size_t capacity = 4096, calls{};
    const auto retained = resource.Get();
    const auto reject = [&](Resource &, std::size_t) {
      ++calls;
      return false;
    };
    Require(EnsureUploadAllocation(resource, capacity, 1024, reject) && calls == 0 &&
                resource.Get() == retained && capacity == 4096,
            "retained allocation was replaced for a smaller upload");
    Require(!EnsureUploadAllocation(resource, capacity, 8193,
                                    [&](Resource &candidate, std::size_t size) {
                                      ++calls;
                                      Require(size == 16384,
                                              "growth was not geometrically bounded");
                                      candidate.value = std::make_unique<Allocation>(2);
                                      return false;
                                    }) &&
                resource.Get() == retained && capacity == 4096 && live == 1 && released == 1,
            "failed staged growth lost retained storage/capacity or leaked its candidate");
    Require(EnsureUploadAllocation(resource, capacity, 512, reject) && calls == 1 &&
                resource.Get()->id == 1 && capacity == 4096,
            "smaller retry after failure attempted allocation or lost the retained payload");
    Require(EnsureUploadAllocation(resource, capacity, 8193,
                                   [&](Resource &candidate, std::size_t) {
                                     ++calls;
                                     candidate.value = std::make_unique<Allocation>(3);
                                     return true;
                                   }) &&
                resource.Get()->id == 3 && capacity == 16384 && live == 1 && released == 2,
            "successful staged growth did not commit ownership and capacity exactly once");
    const auto grown = resource.Get();
    Require(!EnsureUploadAllocation(resource, capacity, 32768,
                                    [&](Resource &, std::size_t) {
                                      ++calls;
                                      return true;
                                    }) &&
                resource.Get() == grown && capacity == 16384,
            "empty successful candidate consumed retained storage or inflated capacity");
    Require(!EnsureUploadAllocation(resource, capacity, std::numeric_limits<std::size_t>::max(),
                                    reject) &&
                calls == 3 && resource.Get() == grown && capacity == 16384,
            "overflow called the allocator or changed retained state");
    resource.value.reset();
    Require(live == 0 && released == 3, "staged upload ownership leaked at teardown");
    // An empty slot must allocate even if a previous failed implementation left stale capacity.
    capacity = 65536;
    Require(EnsureUploadAllocation(resource, capacity, 512,
                                   [](Resource &candidate, std::size_t size) {
                                     Require(size == 4096,
                                             "empty-slot recovery retained false capacity");
                                     candidate.value = std::make_unique<Allocation>(4);
                                     return true;
                                   }) &&
                resource.Get()->id == 4 && capacity == 4096,
            "empty resource with stale capacity was incorrectly treated as reusable");
    resource.value.reset();
    Require(live == 0 && released == 4, "empty-slot recovery leaked its owned allocation");
    std::cout << "Staged upload allocation contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
