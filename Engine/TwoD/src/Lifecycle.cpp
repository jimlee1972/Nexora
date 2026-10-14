#include "Nexora/TwoD/Lifecycle.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace nexora::twod {
namespace {
std::uint64_t Owner() {
  static std::atomic<std::uint64_t> next{1};
  auto value = next.load();
  for (;;) {
    if (value == UINT64_MAX)
      throw std::overflow_error("2D pool identity exhausted");
    if (next.compare_exchange_weak(value, value + 1))
      return value;
  }
}
} // namespace
InstancePool::InstancePool(std::size_t capacity) : owner_(Owner()) {
  if (capacity == 0 || capacity > 100000)
    throw std::invalid_argument("2D pool capacity");
  slots_.resize(capacity);
  free_.reserve(capacity);
  for (std::size_t i = capacity; i > 0; --i)
    free_.push_back(static_cast<std::uint32_t>(i - 1));
}
Instance *InstancePool::Find(InstanceHandle h) noexcept {
  if (h.owner != owner_ || h.index >= slots_.size())
    return nullptr;
  auto &s = slots_[h.index];
  return s.live && s.generation == h.generation ? &s.value : nullptr;
}
std::optional<std::vector<InstanceHandle>> InstancePool::Create(std::span<const Instance> input) {
  if (updating_ || input.size() > free_.size())
    return std::nullopt;
  for (const auto &v : input)
    if (!runtime::IsValidTransform(v.transform) || !ValidateSprite(v.sprite))
      return std::nullopt;
  std::vector<InstanceHandle> handles;
  handles.reserve(input.size());
  // Allocate all results before publishing any slot.
  for (std::size_t i = 0; i < input.size(); ++i) {
    const auto index = free_[free_.size() - 1 - i];
    handles.push_back({owner_, index, slots_[index].generation});
  }
  for (std::size_t i = 0; i < input.size(); ++i) {
    auto &s = slots_[handles[i].index];
    s.value = input[i];
    s.value.transform = *runtime::NormalizedTransform(s.value.transform);
    s.live = true;
  }
  free_.resize(free_.size() - input.size());
  count_ += input.size();
  return handles;
}
bool InstancePool::Destroy(std::span<const InstanceHandle> handles) {
  if (updating_)
    return false;
  std::set<std::uint32_t> unique;
  for (const auto &h : handles)
    if (!Find(h) || !unique.insert(h.index).second)
      return false;
  for (const auto &h : handles) {
    auto &s = slots_[h.index];
    s.live = false;
    --count_;
    if (s.generation < UINT32_MAX) {
      ++s.generation;
      free_.push_back(h.index);
    }
  }
  return true;
}
void InstancePool::ForEach(const std::function<void(Instance &)> &update) {
  if (updating_ || !update)
    throw std::logic_error("2D pool update reentry");
  updating_ = true;
  try {
    for (auto &s : slots_)
      if (s.live)
        update(s.value);
  } catch (...) {
    updating_ = false;
    throw;
  }
  updating_ = false;
}
FixedStepper::FixedStepper(double step, std::uint32_t maximum) : step_(step), maximum_(maximum) {
  if (!std::isfinite(step) || step < 0.000001 || step > 3600 || maximum == 0 || maximum > 100000)
    throw std::invalid_argument("2D fixed step");
}
std::optional<StepResult> FixedStepper::Advance(double delta) {
  if (!std::isfinite(delta) || delta < 0 || !std::isfinite(delta + remainder_))
    return std::nullopt;
  const auto total = delta + remainder_;
  // A small relative tolerance removes rounding-only missed steps across equivalent partitions.
  const auto count = std::floor(total / step_ + 1e-10);
  if (count > maximum_ || count > static_cast<double>(UINT64_MAX - next_))
    return std::nullopt;
  const auto steps = static_cast<std::uint64_t>(count);
  const auto remainder = std::max(0.0, total - static_cast<double>(steps) * step_);
  StepResult result{next_, steps, remainder};
  next_ += steps;
  remainder_ = remainder;
  return result;
}
} // namespace nexora::twod
