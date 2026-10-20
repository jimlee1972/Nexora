#pragma once
#include "Nexora/Editor/GitStatusProvider.h"

namespace nexora::editor {
// Private, serialized owner-thread fault seam. One-shot failures exercise atomic publication;
// no callback or allocator pointer is retained. This is not an extension capability.
enum class GitStatusAllocationPoint : std::uint8_t { None, Refresh, Outcome, Capture };
class NEXORA_EDITOR_API GitStatusProviderTestAccess final {
public:
  static void FailNext(GitStatusProvider &, GitStatusAllocationPoint);
  // One shot on this thread, after the specified number of admitted records. No effect on
  // empty/invalid input that performs no candidate-storage allocation.
  static void FailDecodeAllocationAfter(std::size_t records);
};
} // namespace nexora::editor
