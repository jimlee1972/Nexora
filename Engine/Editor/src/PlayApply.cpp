#include "Nexora/Editor/PlayApply.h"

namespace nexora::editor {
PlayTransformReview CapturePlayTransformReview(const SceneDocument &document,
                                               const runtime::PlaySession &play) {
  PlayTransformReview review{
      play.Generation(), document.Generation(), play.PreviewTransformApplyBack(), {}};
  review.keys.reserve(review.diffs.size());
  for (auto &diff : review.diffs) {
    const auto key = document.Key(diff.entity);
    review.keys.push_back(key.value_or(SceneDocument::NodeKey{}));
    if (!key)
      diff.conflict = true;
  }
  return review;
}
PlayTransformApplyStatus ApplyReviewedPlayTransforms(SceneDocument &document,
                                                     runtime::PlaySession &play,
                                                     const PlayTransformReview &review,
                                                     std::string *error) {
  if (error)
    error->clear();
  const auto conflict = [&] {
    if (error)
      *error = "Play transform review is stale or conflicts with the Editor. No changes applied.";
    return PlayTransformApplyStatus::Conflict;
  };
  if (play.State() != runtime::PlayState::Paused || review.diffs.empty()) {
    if (error)
      *error = "Pause Play and review at least one changed transform before applying.";
    return PlayTransformApplyStatus::Failed;
  }
  const auto current = CapturePlayTransformReview(document, play);
  if (review.play_generation != current.play_generation ||
      review.document_generation != current.document_generation || review.keys != current.keys ||
      review.diffs.size() != current.diffs.size())
    return conflict();
  std::vector<runtime::Transform> transforms;
  transforms.reserve(current.diffs.size());
  for (std::size_t index = 0; index < current.diffs.size(); ++index) {
    const auto &old = review.diffs[index];
    const auto &next = current.diffs[index];
    if (old.conflict || next.conflict || old.entity != next.entity ||
        old.original != next.original || old.editor != next.editor || old.runtime != next.runtime ||
        old.editor_exists != next.editor_exists)
      return conflict();
    transforms.push_back(next.runtime);
  }
  if (!document.SetTransforms(current.keys, transforms)) {
    if (error)
      *error = "The Editor rejected the transform transaction. No changes applied.";
    return PlayTransformApplyStatus::Failed;
  }
  return PlayTransformApplyStatus::Applied;
}
} // namespace nexora::editor
