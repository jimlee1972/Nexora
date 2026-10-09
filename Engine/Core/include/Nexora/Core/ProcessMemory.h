#pragma once

#include "Nexora/Core/Api.h"

#include <cstdint>
#include <optional>

namespace nexora::core {
// Current process resident set / working set bytes, including shared resident pages.
// This is neither GPU memory, allocator accounting nor the operating-system lifetime peak.
// A failed or unsupported host observation returns unavailable, never a fabricated zero.
[[nodiscard]] NEXORA_CORE_API std::optional<std::uint64_t> CurrentProcessResidentBytes() noexcept;
} // namespace nexora::core
