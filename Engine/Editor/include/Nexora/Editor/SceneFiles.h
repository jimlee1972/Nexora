#pragma once

#include "Nexora/Editor/EditorWorkspace.h"

namespace nexora::editor {

struct SceneFileToken final {
  foundation::Uuid project;
  std::uint64_t document_generation{};
  bool operator==(const SceneFileToken &) const = default;
};

enum class SceneFileStatus { Applied, NeedsPath, NeedsUnsavedChoice, NeedsOverwrite, Rejected };
struct SceneFileResult final {
  SceneFileStatus status{SceneFileStatus::Rejected};
  std::string message;
  [[nodiscard]] bool Applied() const noexcept { return status == SceneFileStatus::Applied; }
};

// Borrows one workspace and document, both of which must outlive this session. All calls are
// serialized on the authoring thread. Paths/results/tokens returned here own their values.
class NEXORA_EDITOR_API SceneFileSession final {
public:
  SceneFileSession(const ProjectWorkspace &workspace, SceneDocument &document);
  [[nodiscard]] SceneFileToken Token() const noexcept;
  [[nodiscard]] std::optional<std::filesystem::path> CurrentPath() const;
  [[nodiscard]] bool SaveBlocked() const noexcept { return save_blocked_; }
  // Bootstrapping only: associates an already loaded/new document with a managed path. A failed
  // load can protect this destination from ordinary Save until another document is opened/new.
  bool BindCurrent(std::filesystem::path relative_path, bool save_blocked = false);
  SceneFileResult New(SceneFileToken token, bool discard_unsaved = false);
  SceneFileResult Open(SceneFileToken token, const std::filesystem::path &relative_path,
                       bool discard_unsaved = false);
  SceneFileResult Save(SceneFileToken token);
  SceneFileResult SaveAs(SceneFileToken token, const std::filesystem::path &relative_path,
                         bool replace_existing = false);

private:
  [[nodiscard]] bool Live(SceneFileToken token) const noexcept;
  [[nodiscard]] std::optional<std::filesystem::path>
  Resolve(const std::filesystem::path &relative_path) const;
  const ProjectWorkspace &workspace_;
  SceneDocument &document_;
  std::filesystem::path root_;
  foundation::Uuid project_;
  std::uint64_t generation_{};
  std::optional<std::filesystem::path> current_;
  bool save_blocked_{};
};

} // namespace nexora::editor
