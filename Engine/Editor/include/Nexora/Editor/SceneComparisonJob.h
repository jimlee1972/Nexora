#pragma once

#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/SceneComparison.h"
#include "Nexora/Editor/SceneFiles.h"

namespace nexora::editor {
enum class SceneComparisonPhase : std::uint8_t {
  Idle,
  Queued,
  Reading,
  Comparing,
  AwaitingPublish,
  Ready,
  Cancelled,
  Stale,
  Failed
};
struct SceneComparisonSnapshot final {
  std::uint64_t operation{};
  SceneComparisonPhase phase{SceneComparisonPhase::Idle};
  SceneFileToken token;
  std::filesystem::path path;
  std::string message;
  std::shared_ptr<const SceneComparison> result;
};
// One retained bounded inspection. Start/Poll/Cancel/Snapshot/Shutdown are serialized owner calls.
// Start owns base/local sources; the worker borrows no document, workspace or file session.
// Poll rejects changed scope/content before exposing output. Result describes captured disk bytes,
// never grants overwrite authority. Read-only/reference inspection performs no source writes.
// The JobSystem must outlive this owner; Shutdown cancels and drains its job.
class NEXORA_EDITOR_API SceneComparisonJob final {
public:
  explicit SceneComparisonJob(core::JobSystem &jobs);
  ~SceneComparisonJob();
  SceneComparisonJob(const SceneComparisonJob &) = delete;
  SceneComparisonJob &operator=(const SceneComparisonJob &) = delete;
  bool Start(const SceneFileSession &files, std::string *error = nullptr);
  bool Poll(const SceneFileSession &files);
  bool Cancel() noexcept;
  [[nodiscard]] bool Busy() const;
  [[nodiscard]] SceneComparisonSnapshot Snapshot() const;
  void Shutdown() noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
