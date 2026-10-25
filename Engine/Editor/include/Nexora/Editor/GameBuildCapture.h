#pragma once
#include "Nexora/Editor/GameProjectExport.h"

namespace nexora::editor {
// Exact saved bytes (or observed absence), never a document/workspace borrow.
struct GameBuildSource final {
  std::filesystem::path relative;
  std::optional<runtime::ByteBuffer> bytes;
};
struct GameBuildCapture final {
  GameProjectExportInput project;
  std::vector<GameBuildSource> sources;
};
inline constexpr std::size_t kMaximumGameBuildSourceBytes = 256 * 1024 * 1024;
inline constexpr std::size_t kMaximumGameBuildSourceFiles = 16384;
// Owner-thread saved-scene capture plus exact settings/descriptor/scene/source/identity baselines.
// Rejects unreimported sources, mismatched catalogs and changed sidecar identities. No writes.
[[nodiscard]] NEXORA_EDITOR_API std::optional<GameBuildCapture>
CaptureGameBuild(const ProjectWorkspace &, const ContentBrowserModel &, const MeshAssetCatalog &,
                 const MaterialAssetCatalog &, std::string *error = nullptr);
// Shared bounded regular-file reader and final source guard for CLI and Editor build owners.
[[nodiscard]] NEXORA_EDITOR_API std::optional<GameBuildSource>
ReadGameBuildSource(const std::filesystem::path &root, const std::filesystem::path &relative,
                    std::size_t maximum_bytes, bool allow_missing = false,
                    std::string *error = nullptr);
[[nodiscard]] NEXORA_EDITOR_API bool GameBuildSourcesCurrent(const std::filesystem::path &root,
                                                             std::span<const GameBuildSource>,
                                                             std::string *error = nullptr);
} // namespace nexora::editor
