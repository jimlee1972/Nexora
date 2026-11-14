#pragma once

#include "Nexora/Editor/SpriteSourceCatalog.h"

namespace nexora::editor {

// Owning GUI values, not write authority. The source ID is assigned by the file creator.
struct SpriteSourceCreationRequest final {
  foundation::Uuid project;
  std::filesystem::path root, folder;
  SpriteSourceContentVersion version;
  std::string name;
  twod::SpriteAsset source;
};
enum class SpriteSourceCreationState { Rejected, Created, Uncertain };
enum class SpriteSourceCreationError {
  None,
  Denied,
  Stale,
  Name,
  Destination,
  Atlas,
  Geometry,
  Capacity,
  Content,
  Exists,
  Storage,
  Publication
};
struct SpriteSourceCreationResult final {
  SpriteSourceCreationState state{SpriteSourceCreationState::Rejected};
  SpriteSourceCreationError error{SpriteSourceCreationError::None};
  std::filesystem::path path;
  runtime::AssetUuid asset;
};
[[nodiscard]] NEXORA_EDITOR_API bool ValidSpriteSourceName(std::string_view) noexcept;
[[nodiscard]] NEXORA_EDITOR_API std::string_view
    SpriteSourceCreationMessage(SpriteSourceCreationError) noexcept;
// Serialized authoring-owner call outside GUI rendering. The workspace/content borrows last
// only for this call. Creates one new 96-byte source without replacing any destination/sidecar;
// the owner subsequently imports it through AssetWorkspace's saved-source path.
[[nodiscard]] NEXORA_EDITOR_API SpriteSourceCreationResult
CreateSpriteSourceFile(const ProjectWorkspace &, const ProjectContentSession &,
                       const SpriteSourceCreationRequest &, bool authorized) noexcept;

} // namespace nexora::editor
