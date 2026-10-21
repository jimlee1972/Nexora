#pragma once
#include "Nexora/Editor/Api.h"
#include <cstddef>

namespace nexora::editor {
class ContentBrowserModel;
// Private, owner-thread one-shot allocation fault. No callback/allocator is retained.
class NEXORA_EDITOR_API ContentDirectoryIndexTestAccess final {
public:
  static void FailAfterFolders(std::size_t count) noexcept;
  [[nodiscard]] static bool IsIndexed(const ContentBrowserModel &) noexcept;
};
} // namespace nexora::editor
