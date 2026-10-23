#pragma once
#include <array>
#include <cstdint>
namespace nexora::runtime::detail {
// A failed native read releases controls every poll, but reports one lifecycle edge per outage.
inline bool SetGamepadPresence(uint32_t &present, std::array<uint64_t, 4> &disconnected,
                               uint32_t slot, bool connected) noexcept {
  if (slot >= disconnected.size())
    return false;
  const auto bit = 1u << slot;
  if (connected)
    present |= bit;
  else if (present & bit) {
    present &= ~bit;
    ++disconnected[slot];
  }
  return true;
}
} // namespace nexora::runtime::detail
