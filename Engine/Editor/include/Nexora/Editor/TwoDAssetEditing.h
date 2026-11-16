#pragma once
#include "Nexora/Editor/SpriteSourceCatalog.h"

namespace nexora::editor {
// Owning canonical imported baseline and proposed Atlas/Clip bytes; no borrowed model or authority.
struct TwoDAssetEditingRequest final {
  foundation::Uuid project;
  std::filesystem::path root, path;
  SpriteSourceContentVersion version;
  runtime::AssetUuid id;
  std::string type, artifact;
  twod::Bytes baseline, proposed;
};
enum class TwoDAssetEditingState { Rejected, Saved, Uncertain };
enum class TwoDAssetEditingError {
  None,
  Denied,
  Stale,
  Path,
  Identity,
  Closure,
  Data,
  Unchanged,
  Content,
  Storage,
  Publication
};
struct TwoDAssetEditingResult final {
  TwoDAssetEditingState state{TwoDAssetEditingState::Rejected};
  TwoDAssetEditingError error{TwoDAssetEditingError::None};
};
[[nodiscard]] NEXORA_EDITOR_API std::string_view
    TwoDAssetEditingMessage(TwoDAssetEditingError) noexcept;
// Owner-thread only. Prepares bounded imported owners/dependencies and Content Undo before the
// atomic source replacement. Identity sidecar is unchanged. Cooperative filesystem, not a CAS.
[[nodiscard]] NEXORA_EDITOR_API TwoDAssetEditingResult
SaveTwoDAssetFile(const ProjectWorkspace &, ProjectContentSession &,
                  const TwoDAssetEditingRequest &, bool authorized) noexcept;
} // namespace nexora::editor
