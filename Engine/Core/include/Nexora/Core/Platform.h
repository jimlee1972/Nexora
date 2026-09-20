#pragma once

#include <cstddef>
#include <string_view>

#include "Nexora/Core/Api.h"

namespace nexora::core::platform {

// Number of hardware threads to size worker pools against; never zero even
// when the OS cannot report a concurrency hint.
[[nodiscard]] NEXORA_CORE_API std::size_t HardwareConcurrency() noexcept;

// Best-effort OS thread name for profilers and debuggers. Silently
// truncates or no-ops where the platform cannot honor the request; naming
// is a diagnostic aid, never a correctness dependency.
NEXORA_CORE_API void SetCurrentThreadName(std::string_view name) noexcept;

} // namespace nexora::core::platform
