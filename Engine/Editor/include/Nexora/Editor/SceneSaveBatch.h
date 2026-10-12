#pragma once

#include "Nexora/Editor/SceneFiles.h"

#include <memory>

namespace nexora::editor {

enum class SceneSaveBatchStatus {
  Rejected,
  Published,
  RecoveryRequired,
  PublishedRecoveryRequired
};
struct SceneSaveBatchResult final {
  SceneSaveBatchStatus status{SceneSaveBatchStatus::Rejected};
  std::string message;
  [[nodiscard]] bool Published() const noexcept {
    return status == SceneSaveBatchStatus::Published ||
           status == SceneSaveBatchStatus::PublishedRecoveryRequired;
  }
};

// Borrowed workspace/sessions/documents must outlive a prepared batch. Calls are serialized on
// the authoring thread; drain background readers before publishing. Captured payloads own bytes.
// Every destination is staged and checked before sequential native replacements. Recovery gates
// cooperative readers; this is not a filesystem-wide atomic rename for independent readers.
class NEXORA_EDITOR_API SceneSaveBatch final {
public:
  static constexpr std::size_t kMaximumDocuments = 16;
  static constexpr std::size_t kMaximumPayloadBytes = 128 * 1024 * 1024;
  explicit SceneSaveBatch(const ProjectWorkspace &workspace);
  ~SceneSaveBatch();
  SceneSaveBatch(const SceneSaveBatch &) = delete;
  SceneSaveBatch &operator=(const SceneSaveBatch &) = delete;
  bool Prepare(std::span<SceneFileSession *const> sessions, std::string *error = nullptr);
  [[nodiscard]] SceneSaveBatchResult Publish();
  void Cancel() noexcept;
  // Restore an interrupted uncommitted batch or finish cleanup of a fully committed batch.
  // Before restoration, validate every original/output copy and current destination. Completed
  // phases verify destinations and safely finish cleanup even if earlier cleanup removed copies;
  // conflicting external changes reject and retain recovery data for inspection.
  [[nodiscard]] static bool Recover(const ProjectWorkspace &workspace,
                                    std::string *error = nullptr);
  [[nodiscard]] static bool HasRecovery(const ProjectWorkspace &workspace);

private:
  friend class SceneSaveBatchTestAccess;
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace nexora::editor
