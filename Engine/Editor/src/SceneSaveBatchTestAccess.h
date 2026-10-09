#pragma once

#include "Nexora/Editor/SceneSaveBatch.h"

#include <functional>

namespace nexora::editor {
// Private test seam. Hook executes on the authoring thread before an indexed publication;
// it may alter a fixture source/stage/document but must not reenter the batch itself.
class NEXORA_EDITOR_API SceneSaveBatchTestAccess final {
public:
  static void BeforePublish(SceneSaveBatch &batch, std::function<void(std::size_t)> hook);
};
} // namespace nexora::editor
