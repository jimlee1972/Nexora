#pragma once

#include "Nexora/Editor/AdditiveSceneSession.h"

namespace nexora::editor {
struct SceneCompositionEntry final {
  std::filesystem::path path;
  bool owned{};
  std::vector<std::size_t> dependencies;
};
enum class SceneCompositionStatus : unsigned char { Missing, Restored, Rejected };

// Borrows a session and its workspace, which must outlive this serialized authoring-thread owner.
// Metadata owns at most sixteen named paths/roles/dependency indexes and active selection, not
// document content/history. Stop Play/drain readers before Restore. Save never saves scene sources.
// Named source payloads are limited to 128 MiB in aggregate (logical bytes, not total RSS).
// Actual loaded baselines are counted after admission and before publication, not just file_size.
// Save/Restore revalidate existing distinct source destinations; aliases/absent sources reject.
// Unnamed or unresolved compositions reject Save and preserve the previous metadata. Exact-byte
// external-change checks protect the observed baseline; no fsync/power-loss guarantee is made.
class NEXORA_EDITOR_API AdditiveSceneComposition final {
public:
  explicit AdditiveSceneComposition(AdditiveSceneSession &);
  ~AdditiveSceneComposition();
  AdditiveSceneComposition(const AdditiveSceneComposition &) = delete;
  AdditiveSceneComposition &operator=(const AdditiveSceneComposition &) = delete;
  [[nodiscard]] static std::optional<std::filesystem::path>
  BootstrapScene(const ProjectWorkspace &, std::string *error = nullptr);
  // Restore requires one clean borrowed primary, already opened from BootstrapScene(). Candidate
  // admissions/dependencies all succeed before membership changes. Failure preserves the primary.
  SceneCompositionStatus Restore(std::string *error = nullptr);
  bool Save(std::string *error = nullptr);

private:
  friend class SceneCompositionTestAccess;
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::editor
