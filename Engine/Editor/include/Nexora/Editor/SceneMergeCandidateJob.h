#pragma once
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/SceneMerge.h"

namespace nexora::editor {
enum class SceneMergeCandidatePhase : std::uint8_t {
  Idle,
  Queued,
  Resolving,
  AwaitingPublish,
  Ready,
  Cancelled,
  Stale,
  Failed
};
struct SceneMergeCandidateSnapshot final {
  std::uint64_t operation{}, review{};
  SceneMergeCandidatePhase phase{SceneMergeCandidatePhase::Idle};
  std::string message;
  std::shared_ptr<const SceneMergeCandidate> result;
};
// Serialized owner calls. The worker owns an immutable review and bounded copied choices;
// it borrows no authoring/project/plugin object and performs no IO. Poll publishes data only
// when the caller's current review identity still matches. This grants no write authority.
// Cancellation is checked around bounded synchronous parsing. Jobs and EditorCore outlive
// Shutdown's drain and every retained result/review. Allocation failures may propagate at Start.
// Stop/join JobSystem before unloading EditorCore: descriptor cleanup can follow job completion.
class NEXORA_EDITOR_API SceneMergeCandidateJob final {
public:
  explicit SceneMergeCandidateJob(core::JobSystem &);
  ~SceneMergeCandidateJob();
  SceneMergeCandidateJob(const SceneMergeCandidateJob &) = delete;
  SceneMergeCandidateJob &operator=(const SceneMergeCandidateJob &) = delete;
  bool Start(const SceneMergeReview &, const SceneMergeRequest &, std::string *error = nullptr);
  bool Poll(std::uint64_t current_review);
  bool Cancel() noexcept;
  [[nodiscard]] bool Busy() const noexcept;
  [[nodiscard]] SceneMergeCandidateSnapshot Snapshot() const;
  void Shutdown() noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
