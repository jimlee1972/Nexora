#pragma once
#include "Nexora/Editor/GitDiffProvider.h"

namespace nexora::editor {
enum class GitDiffAllocationPoint : std::uint8_t { None, Intake, Outcome, Capture };
// Private one-shot owner-thread fault seam; retains no callback/allocator pointer.
class NEXORA_EDITOR_API GitDiffProviderTestAccess final {
public:
  static void FailNext(GitDiffProvider &, GitDiffAllocationPoint);
};
} // namespace nexora::editor
