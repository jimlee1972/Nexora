#pragma once

#include "Nexora/Editor/SpriteSourceCatalog.h"

namespace nexora::editor {

// Owning imported baseline and proposed values. Serialized owner-thread calls only;
// this request carries no authority and retains no Content or document borrow.
struct SpriteSourceEditingRequest final {
  foundation::Uuid project;
  std::filesystem::path root, path;
  SpriteSourceContentVersion version;
  std::string artifact;
  twod::SpriteAsset baseline, source;
};
enum class SpriteSourceEditingState { Rejected, Saved, Uncertain };
enum class SpriteSourceEditingError {
  None,
  Denied,
  Stale,
  Path,
  Identity,
  Atlas,
  Geometry,
  Unchanged,
  Content,
  Storage,
  Publication
};
struct SpriteSourceEditingResult final {
  SpriteSourceEditingState state{SpriteSourceEditingState::Rejected};
  SpriteSourceEditingError error{SpriteSourceEditingError::None};
};
[[nodiscard]] NEXORA_EDITOR_API std::string_view
    SpriteSourceEditingMessage(SpriteSourceEditingError) noexcept;
// Prepares normal typed import, browser/Undo and dependency publication before replacing
// the sole ordinary source file. Its mandatory sidecar and UUID are preserved. Disk baseline
// checks assume a cooperative local filesystem; they are not an adversarial compare-and-swap.
[[nodiscard]] NEXORA_EDITOR_API SpriteSourceEditingResult
SaveSpriteSourceFile(const ProjectWorkspace &, ProjectContentSession &,
                     const SpriteSourceEditingRequest &, bool authorized) noexcept;

} // namespace nexora::editor
