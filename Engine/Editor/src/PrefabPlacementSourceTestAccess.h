#pragma once
#include "Nexora/Editor/Api.h"
namespace nexora::editor {
// Private thread-local one-shot seam after complete candidate staging, before source IO.
class NEXORA_EDITOR_API PrefabPlacementSourceTestAccess final {
public:
  static void FailNextBeforePublish() noexcept;
};
} // namespace nexora::editor
