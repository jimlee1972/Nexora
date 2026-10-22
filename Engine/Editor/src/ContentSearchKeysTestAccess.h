#pragma once
#include "Nexora/Editor/Api.h"
#include <cstddef>
namespace nexora::editor {
class ContentBrowserModel;
// Private owner-thread observations/fault injection; never retains a callback or allocator.
class NEXORA_EDITOR_API ContentSearchKeysTestAccess final {
public:
  static void FailAfterKeys(std::size_t count) noexcept;
  [[nodiscard]] static bool IsCached(const ContentBrowserModel &) noexcept;
  [[nodiscard]] static std::size_t Rows(const ContentBrowserModel &) noexcept;
  [[nodiscard]] static std::size_t LogicalBytes(const ContentBrowserModel &) noexcept;
  [[nodiscard]] static std::size_t MaximumBytes() noexcept;
  [[nodiscard]] static std::size_t MinimumRows() noexcept;
};
} // namespace nexora::editor
