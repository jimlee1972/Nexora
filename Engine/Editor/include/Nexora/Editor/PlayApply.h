#pragma once

#include "Nexora/Editor/EditorWorkspace.h"

namespace nexora::editor {

// Owns the exact supported transform diff reviewed by the user, including session/document/entity
// generations. No World/SceneDocument borrow survives capture; unsupported-scene rows conflict.
struct PlayTransformReview final {
  std::uint64_t play_generation{};
  std::uint64_t document_generation{};
  std::vector<runtime::TransformApplyDiff> diffs;
  std::vector<SceneDocument::NodeKey> keys;
};
enum class PlayTransformApplyStatus { Applied, Conflict, Failed };

[[nodiscard]] NEXORA_EDITOR_API PlayTransformReview
CapturePlayTransformReview(const SceneDocument &document, const runtime::PlaySession &play);
// Caller pauses before confirmation. Fresh generations, keys, and diffs must match every reviewed
// row. A successful apply is one document Undo step, leaves Play paused, and changes transforms
// only. The embedding app unloads the module and discards the clone after successful application.
[[nodiscard]] NEXORA_EDITOR_API PlayTransformApplyStatus
ApplyReviewedPlayTransforms(SceneDocument &document, runtime::PlaySession &play,
                            const PlayTransformReview &review, std::string *error = nullptr);

} // namespace nexora::editor
