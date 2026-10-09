#pragma once

#include "Nexora/Editor/EditorWorkspace.h"

namespace nexora::editor {
class ProjectContentSession;

struct SceneFileToken final {
  foundation::Uuid project;
  std::uint64_t document_generation{};
  bool operator==(const SceneFileToken &) const = default;
};

enum class SceneFileStatus { Applied, NeedsPath, NeedsUnsavedChoice, NeedsOverwrite, Rejected };
struct SceneOverwriteToken final {
  std::uint64_t session{}, revision{};
  bool operator==(const SceneOverwriteToken &) const = default;
};
struct SceneFileResult final {
  SceneFileStatus status{SceneFileStatus::Rejected};
  std::string message;
  std::optional<SceneOverwriteToken> overwrite_token{};
  [[nodiscard]] bool Applied() const noexcept { return status == SceneFileStatus::Applied; }
};

// Borrows one workspace and document, both of which must outlive this session. All calls are
// serialized on the authoring thread. Paths/results/tokens returned here own their values.
class NEXORA_EDITOR_API SceneFileSession final {
public:
  static constexpr std::size_t kMaximumDiskBaselineBytes = 64 * 1024 * 1024;
  SceneFileSession(const ProjectWorkspace &workspace, SceneDocument &document);
  SceneFileSession(const SceneFileSession &) = delete;
  SceneFileSession &operator=(const SceneFileSession &) = delete;
  [[nodiscard]] SceneFileToken Token() const noexcept;
  [[nodiscard]] std::optional<std::filesystem::path> CurrentPath() const;
  [[nodiscard]] bool SaveBlocked() const noexcept { return save_blocked_ || content_blocked_; }
  // Bootstrapping only: associates an already loaded/new document with a managed path. A failed
  // load can protect this destination from ordinary Save until another document is opened/new.
  bool BindCurrent(std::filesystem::path relative_path, bool save_blocked = false);
  SceneFileResult New(SceneFileToken token, bool discard_unsaved = false);
  SceneFileResult Open(SceneFileToken token, const std::filesystem::path &relative_path,
                       bool discard_unsaved = false);
  SceneFileResult Save(SceneFileToken token,
                       std::optional<SceneOverwriteToken> overwrite_token = std::nullopt);
  // replace_existing without an overwrite token is an explicit caller-owned replacement policy.
  // Interactive callers must pass the NeedsOverwrite token to recheck the confirmed bytes.
  SceneFileResult SaveAs(SceneFileToken token, const std::filesystem::path &relative_path,
                         bool replace_existing = false,
                         std::optional<SceneOverwriteToken> overwrite_token = std::nullopt);
  // Associate a loaded Content scene before browser mutations, then call after mutations. Tracks
  // its stable asset UUID through rename/move/Undo without changing the document or its history.
  // Missing, stale or unsafe tracked assets block ordinary Save until restored or explicitly
  // replaced with New/Open/Save As. The content session is borrowed only for this call.
  SceneFileResult SynchronizeContent(SceneFileToken token, const ProjectContentSession &content);
  // Bootstrap first, before RememberCurrent. Missing settings return NeedsPath; rejected settings
  // or source files stay protected for this session. Restore never discards a dirty document.
  SceneFileResult RestoreStartup(SceneFileToken token);
  // Remembers a clean associated scene, or a committed relocation of its existing Content asset
  // even while the document is dirty. This metadata commit never saves document/history; failure
  // must not turn a successful scene save into failure.
  SceneFileResult RememberCurrent(SceneFileToken token);

private:
  friend class SceneComparisonJob;
  friend class SceneSaveBatch;
  friend class AdditiveSceneSession;
  friend class AdditiveSceneComposition;
  struct DiskSnapshot final {
    bool exists{};
    std::string bytes;
    bool operator==(const DiskSnapshot &) const = default;
  };
  struct PendingOverwrite final {
    SceneFileToken document;
    std::filesystem::path path;
    SceneOverwriteToken token;
    DiskSnapshot snapshot;
  };
  [[nodiscard]] static std::optional<DiskSnapshot> ReadDisk(const std::filesystem::path &path);
  SceneFileResult RequireOverwrite(SceneFileToken token, const std::filesystem::path &path,
                                   DiskSnapshot snapshot, std::string message);
  [[nodiscard]] bool Live(SceneFileToken token) const noexcept;
  [[nodiscard]] std::optional<std::filesystem::path>
  Resolve(const std::filesystem::path &relative_path) const;
  [[nodiscard]] std::optional<std::filesystem::path> StartupMetadataPath() const;
  [[nodiscard]] SceneFileResult ReadStartup(std::optional<std::filesystem::path> &relative) const;
  const ProjectWorkspace &workspace_;
  SceneDocument &document_;
  std::filesystem::path root_;
  foundation::Uuid project_;
  std::uint64_t generation_{};
  std::optional<std::filesystem::path> current_;
  bool save_blocked_{};
  std::optional<runtime::AssetUuid> content_asset_;
  std::uint64_t content_generation_{};
  bool content_blocked_{}, content_relocated_{};
  bool startup_checked_{}, startup_blocked_{};
  std::optional<DiskSnapshot> disk_baseline_;
  std::optional<PendingOverwrite> pending_overwrite_;
  std::uint64_t session_id_{}, overwrite_revision_{};
};

} // namespace nexora::editor
