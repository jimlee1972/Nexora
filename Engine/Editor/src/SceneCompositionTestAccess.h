#pragma once
#include "Nexora/Editor/AdditiveSceneComposition.h"

namespace nexora::editor {
// Private serialized fixture seams at size preflight and candidate/source publication boundaries.
// Fixture hooks may change disk data, but must not reenter the session or destroy its borrowers.
class NEXORA_EDITOR_API SceneCompositionTestAccess final {
public:
  static void BeforeRestorePublish(AdditiveSceneComposition &, std::function<void()> hook);
  static void AfterSourceSizePreflight(AdditiveSceneComposition &, std::function<void()> hook);
};
} // namespace nexora::editor
