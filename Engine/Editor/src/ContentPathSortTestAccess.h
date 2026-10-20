#pragma once
#include "Nexora/Editor/Api.h"
#include <cstddef>

namespace nexora::editor {
// Private owner-thread probes, not a public SDK/callback contract.
class NEXORA_EDITOR_API ContentPathSortTestAccess final {
public:
  static constexpr std::size_t kMaximumItems = 262144;
  static constexpr std::size_t kMaximumKeyBytes = 16 * 1024 * 1024;
  static void FailAfterKeys(std::size_t count) noexcept;
  [[nodiscard]] static bool LastSortAccelerated() noexcept;
};
} // namespace nexora::editor
