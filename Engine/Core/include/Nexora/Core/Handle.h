#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace nexora::core {

template <typename Tag> struct Handle final {
  std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t generation{0};

  [[nodiscard]] constexpr bool IsValid() const noexcept { return generation != 0; }
  friend constexpr bool operator==(Handle, Handle) noexcept = default;
};

template <typename Tag> class HandlePool final {
public:
  using HandleType = Handle<Tag>;

  [[nodiscard]] HandleType Create() {
    while (!free_.empty()) {
      const auto index = free_.back();
      free_.pop_back();
      auto &slot = slots_[index];
      if (!slot.retired) {
        slot.alive = true;
        return {index, slot.generation};
      }
    }
    slots_.push_back({1, true, false});
    return {static_cast<std::uint32_t>(slots_.size() - 1), 1};
  }

  [[nodiscard]] bool Destroy(HandleType handle) {
    if (!Contains(handle))
      return false;
    auto &slot = slots_[handle.index];
    slot.alive = false;
    if (slot.generation == std::numeric_limits<std::uint32_t>::max()) {
      slot.retired = true;
    } else {
      ++slot.generation;
      free_.push_back(handle.index);
    }
    return true;
  }

  [[nodiscard]] bool Contains(HandleType handle) const noexcept {
    return handle.IsValid() && handle.index < slots_.size() && slots_[handle.index].alive &&
           slots_[handle.index].generation == handle.generation;
  }

private:
  struct Slot final {
    std::uint32_t generation;
    bool alive;
    bool retired;
  };
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
};

} // namespace nexora::core
