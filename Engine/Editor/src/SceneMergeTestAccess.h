#pragma once
#include "Nexora/Editor/Api.h"
namespace nexora::editor::detail {
// Private deterministic allocation failure while the isolated candidate World is still alive.
struct NEXORA_EDITOR_API SceneMergeTestAccess final {
  static void FailNextAfterLoad() noexcept;
};
} // namespace nexora::editor::detail
