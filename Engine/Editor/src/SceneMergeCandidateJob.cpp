#include "Nexora/Editor/SceneMergeCandidateJob.h"
#include <limits>
#include <mutex>
#include <stdexcept>

namespace nexora::editor {
struct SceneMergeCandidateJob::Implementation final {
  struct Operation final {
    std::mutex mutex;
    SceneMergeCandidateSnapshot snapshot;
    core::CancellationSource cancellation;
    core::JobHandle job;
    std::optional<SceneMergeReview> review;
    SceneMergeRequest request;
    std::shared_ptr<const SceneMergeCandidate> pending;
    bool consumed{};
    void Set(SceneMergeCandidatePhase phase, std::string message) {
      std::lock_guard lock{mutex};
      snapshot.message = std::move(message);
      snapshot.phase = phase;
    }
    void Run(const core::CancellationToken &token) {
      const auto cancelled = [&] {
        if (!token.IsCancellationRequested())
          return false;
        Set(SceneMergeCandidatePhase::Cancelled, "Candidate validation cancelled.");
        return true;
      };
      if (cancelled())
        return;
      Set(SceneMergeCandidatePhase::Resolving, "Validating complete captured scene candidate.");
      std::string error;
      auto candidate = SceneMerge::Resolve(*review, request, &error);
      if (cancelled())
        return;
      if (!candidate)
        throw std::runtime_error(error);
      pending = std::make_shared<const SceneMergeCandidate>(std::move(*candidate));
      Set(SceneMergeCandidatePhase::AwaitingPublish, "Candidate awaits current review checks.");
    }
  };
  core::JobSystem &jobs;
  std::shared_ptr<Operation> operation;
  std::uint64_t next{1};
  bool stopped{};
  explicit Implementation(core::JobSystem &value) : jobs(value) {}
};
SceneMergeCandidateJob::SceneMergeCandidateJob(core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(jobs)) {}
SceneMergeCandidateJob::~SceneMergeCandidateJob() { Shutdown(); }
bool SceneMergeCandidateJob::Start(const SceneMergeReview &review, const SceneMergeRequest &request,
                                   std::string *error) {
  auto &impl = *implementation_;
  const auto reject = [&](const char *message) {
    if (error)
      *error = message;
    if (!impl.operation || impl.operation->consumed) {
      auto failed = std::make_shared<Implementation::Operation>();
      failed->consumed = true;
      failed->snapshot.review = review.Identity();
      failed->snapshot.phase = SceneMergeCandidatePhase::Failed;
      failed->snapshot.message = message;
      impl.operation = std::move(failed);
    }
    return false;
  };
  if (impl.stopped || Busy() || impl.next == std::numeric_limits<std::uint64_t>::max())
    return reject("Candidate validation is busy or stopped.");
  if (!review.Identity() || request.schema != SceneMerge::kSchemaVersion ||
      request.review != review.Identity() ||
      request.choices.size() > review.Comparison().rows.size())
    return reject("Candidate request schema, review or choice capacity is invalid.");
  auto op = std::make_shared<Implementation::Operation>();
  op->review = review;
  op->request = request;
  op->snapshot = {impl.next,
                  review.Identity(),
                  SceneMergeCandidatePhase::Queued,
                  "Candidate validation queued.",
                  {}};
  try {
    op->job = impl.jobs.Submit(
        {[op](const core::CancellationToken &token) {
           try {
             op->Run(token);
           } catch (const std::exception &exception) {
             op->Set(SceneMergeCandidatePhase::Failed, exception.what());
           } catch (...) {
             op->Set(SceneMergeCandidatePhase::Failed, "Candidate validation failed.");
           }
         },
         core::JobPriority::Low, op->cancellation.Token(), "Scene merge candidate validation"});
  } catch (...) {
    return reject("Candidate validation could not submit its worker.");
  }
  ++impl.next;
  impl.operation = std::move(op);
  if (error)
    error->clear();
  return true;
}
bool SceneMergeCandidateJob::Poll(std::uint64_t current_review) {
  auto &impl = *implementation_;
  const auto op = impl.operation;
  if (!op || op->consumed)
    return false;
  const auto status = op->job.Status();
  if (status == core::JobStatus::Queued || status == core::JobStatus::Running)
    return false;
  try {
    impl.jobs.Wait(op->job);
  } catch (...) {
    op->Set(SceneMergeCandidatePhase::Failed, "Candidate worker failed.");
  }
  if (op->cancellation.Token().IsCancellationRequested() || status == core::JobStatus::Cancelled)
    op->Set(SceneMergeCandidatePhase::Cancelled, "Candidate validation cancelled.");
  else if (Snapshot().phase == SceneMergeCandidatePhase::AwaitingPublish) {
    if (!current_review || current_review != op->snapshot.review)
      op->Set(SceneMergeCandidatePhase::Stale, "Candidate discarded after its review changed.");
    else {
      std::lock_guard lock{op->mutex};
      op->snapshot.message =
          "Complete captured candidate validated. Source publication is separate.";
      op->snapshot.result = std::move(op->pending);
      op->snapshot.phase = SceneMergeCandidatePhase::Ready;
    }
  }
  op->review.reset();
  op->request.choices.clear();
  op->pending.reset();
  op->consumed = true;
  return true;
}
bool SceneMergeCandidateJob::Cancel() noexcept {
  const auto op = implementation_->operation;
  if (!op || op->consumed)
    return false;
  op->cancellation.Cancel();
  return true;
}
bool SceneMergeCandidateJob::Busy() const noexcept {
  const auto op = implementation_->operation;
  return op && !op->consumed;
}
SceneMergeCandidateSnapshot SceneMergeCandidateJob::Snapshot() const {
  const auto op = implementation_->operation;
  if (!op)
    return {};
  std::lock_guard lock{op->mutex};
  return op->snapshot;
}
void SceneMergeCandidateJob::Shutdown() noexcept {
  auto &impl = *implementation_;
  impl.stopped = true;
  const auto op = impl.operation;
  if (!op)
    return;
  if (!op->consumed) {
    op->cancellation.Cancel();
    try {
      impl.jobs.Wait(op->job);
    } catch (...) {
    }
  }
  op->review.reset();
  op->request.choices.clear();
  op->pending.reset();
  op->consumed = true;
  try {
    std::lock_guard lock{op->mutex};
    op->snapshot.result.reset();
    op->snapshot.phase = SceneMergeCandidatePhase::Cancelled;
    op->snapshot.message = "Candidate validation stopped.";
  } catch (...) {
  }
}
} // namespace nexora::editor
