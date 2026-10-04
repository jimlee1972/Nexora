#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nexora::editor {

// Authoring-thread owner for the live project Content Browser. It binds the deterministic asset
// index to a generation-scoped model and makes filesystem mutations recoverable. The UI borrows
// this session; it never owns project files or retains ContentItem pointers across calls.
class NEXORA_EDITOR_API ProjectContentSession final {
public:
  ProjectContentSession();
  ~ProjectContentSession();
  ProjectContentSession(ProjectContentSession &&) noexcept;
  ProjectContentSession &operator=(ProjectContentSession &&) noexcept;
  ProjectContentSession(const ProjectContentSession &) = delete;
  ProjectContentSession &operator=(const ProjectContentSession &) = delete;

  bool Open(const ProjectWorkspace &workspace, const AssetWorkspace &assets,
            std::uint64_t project_generation, bool writable = true, std::string *error = nullptr);

  [[nodiscard]] ContentBrowserModel &Browser() noexcept { return browser_; }
  [[nodiscard]] const ContentBrowserModel &Browser() const noexcept { return browser_; }
  [[nodiscard]] AssetDependencyGraph &Dependencies() noexcept { return dependencies_; }
  [[nodiscard]] const AssetDependencyGraph &Dependencies() const noexcept { return dependencies_; }
  [[nodiscard]] DirtyConflictModel &Conflicts() noexcept { return conflicts_; }
  [[nodiscard]] const DirtyConflictModel &Conflicts() const noexcept { return conflicts_; }
  [[nodiscard]] bool Writable() const noexcept { return writable_; }
  [[nodiscard]] bool CanUndo() const noexcept { return !undo_moves_.empty(); }
  [[nodiscard]] std::string_view LastError() const noexcept { return last_error_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return root_; }

  bool Rename(runtime::AssetUuid asset, std::string_view filename, std::string *error = nullptr);
  bool Move(std::span<const runtime::AssetUuid> assets, const std::filesystem::path &folder,
            std::string *error = nullptr);
  bool Move(const AssetDragPayload &payload, const std::filesystem::path &folder,
            std::string *error = nullptr);
  bool Delete(std::span<const runtime::AssetUuid> assets, std::string *error = nullptr);
  bool Undo(std::string *error = nullptr);
  bool Reimport(runtime::AssetUuid asset, std::string *error = nullptr);
  // The borrowed queue must outlive the session while a reimport is pending, including during
  // exception unwinding. PollReimport publishes the result; cancellation alone retains the borrow.
  bool BeginReimport(AssetImportQueue &imports, runtime::AssetUuid asset,
                     std::string *error = nullptr);
  bool PollReimport(std::string *error = nullptr);
  bool CancelReimport() noexcept;
  [[nodiscard]] bool ReimportBusy() const noexcept;
  [[nodiscard]] std::optional<ImportOperationSnapshot> ReimportStatus() const;

private:
  struct PendingReimport;
  using FileMove = std::pair<std::filesystem::path, std::filesystem::path>;

  bool CommitMoves(ContentBrowserModel candidate, std::vector<FileMove> moves,
                   bool create_destination_directories, std::string *error);
  void AppendAssetMove(std::vector<FileMove> &moves, const std::filesystem::path &source,
                       const std::filesystem::path &destination) const;
  [[nodiscard]] std::filesystem::path ExistingPath(const std::filesystem::path &relative,
                                                   std::string *error) const;
  [[nodiscard]] std::filesystem::path DestinationPath(const std::filesystem::path &relative,
                                                      bool create_parent, std::string *error) const;
  bool Fail(std::string message, std::string *error);
  void ClearError(std::string *error);

  std::filesystem::path root_;
  ContentBrowserModel browser_;
  AssetDependencyGraph dependencies_;
  DirtyConflictModel conflicts_;
  std::vector<FileMove> undo_moves_;
  std::uint64_t operation_{};
  bool writable_{};
  bool persistent_identities_{};
  std::string last_error_;
  std::unique_ptr<PendingReimport> pending_reimport_;
  std::optional<ImportOperationSnapshot> last_reimport_;
};

} // namespace nexora::editor
