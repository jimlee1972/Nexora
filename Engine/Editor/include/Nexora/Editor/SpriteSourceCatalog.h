#pragma once

#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/TwoDAssetImport.h"

namespace nexora::editor {
class ProjectContentSession;

struct SpriteSourceContentVersion final {
  std::uint64_t project_generation{}, content_revision{};
  friend bool operator==(const SpriteSourceContentVersion &,
                         const SpriteSourceContentVersion &) = default;
};

struct SpriteSourceSnapshot final {
  std::shared_ptr<const twod::SpriteAsset> source;
  std::shared_ptr<const twod::Atlas> atlas;
};

// Authoring-thread owning metadata catalog. No IO, World changes or native resources.
// ImportTwoDAsset has already validated each immutable payload; publication checks closure.
class NEXORA_EDITOR_API SpriteSourceCatalog final {
public:
  bool PublishContent(const ContentBrowserModel &, std::string *error = nullptr);
  void Clear() noexcept;
  [[nodiscard]] SpriteSourceContentVersion Version() const noexcept { return version_; }
  [[nodiscard]] std::optional<SpriteSourceSnapshot> ResolveAsset(runtime::AssetUuid,
                                                                 SpriteSourceContentVersion) const;

private:
  SpriteSourceContentVersion version_;
  std::unordered_map<runtime::AssetUuid, SpriteSourceSnapshot, runtime::AssetUuidHash> sources_;
};

// Version 1: one version byte, then UUID high/low little-endian (17 bytes total).
// This Editor-owned reference is not a runtime Sprite, ND2D catalog or export instruction.
inline constexpr runtime::TypeId kSpriteSourceReferenceType = 0x4544535052540001ULL;
inline constexpr std::string_view kSpriteSourceReferenceName = "editor.sprite.source";
[[nodiscard]] NEXORA_EDITOR_API OpaqueComponent SpriteSourceReference(runtime::AssetUuid);
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::AssetUuid>
ReadSpriteSourceReference(const OpaqueComponentInfo &);
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::AssetUuid>
ReadSpriteSourceReference(const SceneDocument &, SceneDocument::NodeKey);

// Complete current selection only, including live NodeKeys and current generation/revision.
// Caller supplies current writer/Play/recovery authority through editable. One metadata Undo;
// unsupported reserved components reject the whole request without replacing their bytes.
NEXORA_EDITOR_API bool AssignSpriteSources(SceneDocument &, std::span<const SceneDocument::NodeKey>,
                                           runtime::AssetUuid, SpriteSourceContentVersion,
                                           const ProjectContentSession &,
                                           const SpriteSourceCatalog &, bool editable);
// Missing source/Atlas assets do not prevent clearing a supported reference.
NEXORA_EDITOR_API bool ClearSpriteSources(SceneDocument &, std::span<const SceneDocument::NodeKey>,
                                          SpriteSourceContentVersion, const ProjectContentSession &,
                                          bool editable);
} // namespace nexora::editor
