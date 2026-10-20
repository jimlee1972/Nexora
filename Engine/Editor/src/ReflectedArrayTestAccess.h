#pragma once
#include "Nexora/Editor/ReflectedInspector.h"

namespace nexora::editor {
// Private serialized thread-local one-shot failures; no allocator/callback is retained.
class NEXORA_EDITOR_API ReflectedArrayTestAccess final {
public:
  static void FailAfterReplacements(std::size_t count) noexcept;
  static void FailHistoryAdmission() noexcept;
};
} // namespace nexora::editor
