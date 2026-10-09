#pragma once
#include "Nexora/Editor/AdditiveSceneComposition.h"

namespace nexora::editor {
// Private serialized test seam after candidate admission, before source/metadata revalidation.
// Fixture hooks may change disk data, but must not reenter the session or destroy its borrowers.
class NEXORA_EDITOR_API SceneCompositionTestAccess final {
public:
  static void BeforeRestorePublish(AdditiveSceneComposition &, std::function<void()> hook);
};
} // namespace nexora::editor
