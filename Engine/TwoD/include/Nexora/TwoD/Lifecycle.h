#pragma once
#include "Nexora/TwoD/Scene.h"
#include <memory>
#include <span>

namespace nexora::twod {
struct InstanceHandle final {
  std::uint64_t owner{};
  std::uint32_t index{}, generation{};
  friend bool operator==(const InstanceHandle &, const InstanceHandle &) = default;
};
struct Instance final {
  runtime::Transform transform;
  Sprite sprite;
};
class NEXORA_2D_API InstancePool final {
public:
  explicit InstancePool(std::size_t capacity);
  InstancePool(const InstancePool &) = delete;
  InstancePool &operator=(const InstancePool &) = delete;
  InstancePool(InstancePool &&) = delete;
  InstancePool &operator=(InstancePool &&) = delete;
  [[nodiscard]] std::optional<std::vector<InstanceHandle>>
  Create(std::span<const Instance> instances);
  bool Destroy(std::span<const InstanceHandle> handles);
  [[nodiscard]] Instance *Find(InstanceHandle handle) noexcept;
  void ForEach(const std::function<void(Instance &)> &update);
  [[nodiscard]] std::size_t Size() const noexcept { return count_; }
  // Owner-thread reload branch: copies slots/free-list/generations without changing handle
  // identity. Allocation can throw; reentrant structural access returns null. A branch must
  // stay unpublished until its embedding transaction commits; ordinary pools remain distinct.
  [[nodiscard]] std::unique_ptr<InstancePool> ForkForReload() const;
  // Infallible state exchange for branches of this owner, preserving the pool object's address.
  // Rejects unrelated owners and either pool's ForEach. Find pointers expire on success.
  [[nodiscard]] bool SwapReloadState(InstancePool &branch) noexcept;

private:
  friend class GameplayBridge;
  struct ReloadCopy final {};
  InstancePool(const InstancePool &source, ReloadCopy);
  struct Slot {
    Instance value;
    std::uint32_t generation{1};
    bool live{};
  };
  std::uint64_t owner_;
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::size_t count_{};
  bool updating_{};
};
struct StepResult final {
  std::uint64_t first{}, steps{};
  double remainder{};
};
class NEXORA_2D_API FixedStepper final {
public:
  explicit FixedStepper(double step = 1.0 / 60.0, std::uint32_t maximumSteps = 240);
  // Excess backlog rejects without advancing; time is never silently dropped.
  [[nodiscard]] std::optional<StepResult> Advance(double frameDelta);
  [[nodiscard]] double StepSeconds() const noexcept { return step_; }

private:
  double step_, remainder_{};
  std::uint32_t maximum_;
  std::uint64_t next_{};
};
} // namespace nexora::twod
