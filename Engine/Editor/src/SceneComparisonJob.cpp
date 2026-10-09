#include "Nexora/Editor/SceneComparisonJob.h"

#include <array>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace nexora::editor {
struct SceneComparisonJob::Implementation final {
  struct Operation final {
    std::mutex mutex;
    SceneComparisonSnapshot snapshot;
    core::CancellationSource cancellation;
    core::JobHandle job;
    std::filesystem::path root, source;
    std::uint64_t file_session{};
    std::optional<std::string> base;
    std::optional<SceneDocument::PreparedSave> local;
    std::shared_ptr<const SceneComparison> pending;
    bool consumed{};
    void Set(SceneComparisonPhase phase, std::string message) {
      std::lock_guard lock{mutex};
      snapshot.phase = phase;
      snapshot.message = std::move(message);
    }
    void Run(const core::CancellationToken &token) {
      const auto cancelled = [&] {
        if (!token.IsCancellationRequested())
          return false;
        Set(SceneComparisonPhase::Cancelled, "Scene comparison cancelled.");
        return true;
      };
      if (cancelled())
        return;
      Set(SceneComparisonPhase::Reading, "Reading a captured disk revision.");
      std::error_code ec;
      const auto status = std::filesystem::symlink_status(source, ec);
      const bool absent = ec == std::errc::no_such_file_or_directory ||
                          (!ec && status.type() == std::filesystem::file_type::not_found);
      std::optional<std::string> remote;
      if (!absent) {
        if (ec || !std::filesystem::is_regular_file(status))
          throw std::runtime_error("Scene comparison source is not a regular file.");
        const auto size = std::filesystem::file_size(source, ec);
        if (ec || size > SceneComparison::kMaximumSourceBytes)
          throw std::runtime_error("Scene comparison disk revision exceeds 8 MiB.");
        std::ifstream input(source, std::ios::binary);
        if (!input)
          throw std::runtime_error("Scene comparison could not open the disk revision.");
        remote.emplace();
        std::array<char, 64 * 1024> buffer;
        while (input) {
          if (cancelled())
            return;
          input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
          const auto count = static_cast<std::size_t>(input.gcount());
          if (count > SceneComparison::kMaximumSourceBytes - remote->size())
            throw std::runtime_error("Scene comparison disk revision grew beyond 8 MiB.");
          remote->append(buffer.data(), count);
        }
        if (!input.eof())
          throw std::runtime_error("Scene comparison disk revision could not be read completely.");
      }
      if (cancelled())
        return;
      Set(SceneComparisonPhase::Comparing, "Comparing bounded captured scene revisions.");
      std::string error;
      auto compared = CompareSceneRevisions(
          base ? std::optional<std::string_view>(*base) : std::nullopt, local->Bytes(),
          remote ? std::optional<std::string_view>(*remote) : std::nullopt, &error);
      if (cancelled())
        return;
      if (!compared)
        throw std::runtime_error(error);
      pending = std::make_shared<const SceneComparison>(std::move(*compared));
      Set(SceneComparisonPhase::AwaitingPublish,
          "Captured comparison awaits current scene checks.");
    }
  };
  core::JobSystem &jobs;
  std::shared_ptr<Operation> operation;
  std::uint64_t next{1};
  bool stopped{};
  explicit Implementation(core::JobSystem &value) : jobs(value) {}
};
SceneComparisonJob::SceneComparisonJob(core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(jobs)) {}
SceneComparisonJob::~SceneComparisonJob() { Shutdown(); }
bool SceneComparisonJob::Start(const SceneFileSession &files, std::string *error) {
  auto &impl = *implementation_;
  const auto reject = [&](const char *message) {
    if (error)
      *error = message;
    if (!impl.operation || impl.operation->consumed) {
      auto observation = std::make_shared<Implementation::Operation>();
      observation->consumed = true;
      observation->snapshot.phase = SceneComparisonPhase::Failed;
      observation->snapshot.message = message;
      impl.operation = std::move(observation);
    }
    return false;
  };
  if (impl.stopped || (impl.operation && !impl.operation->consumed) ||
      impl.next == std::numeric_limits<std::uint64_t>::max())
    return reject("Scene comparison is busy or stopped.");
  if (!files.Live(files.Token()) || !files.current_ || !files.disk_baseline_)
    return reject("Scene comparison requires a current named scene with a known baseline.");
  const auto source = files.Resolve(*files.current_);
  if (!source || files.disk_baseline_->bytes.size() > SceneComparison::kMaximumSourceBytes)
    return reject("Scene comparison baseline is unsafe or exceeds 8 MiB.");
  auto local = files.document_.PrepareSave();
  if (!local || local->Bytes().size() > SceneComparison::kMaximumSourceBytes)
    return reject("Scene comparison local revision is unavailable or exceeds 8 MiB.");
  auto op = std::make_shared<Implementation::Operation>();
  op->snapshot = {impl.next++,     SceneComparisonPhase::Queued, files.Token(),
                  *files.current_, "Scene comparison queued.",   {}};
  op->root = files.root_;
  op->file_session = files.session_id_;
  op->source = *source;
  if (files.disk_baseline_->exists)
    op->base = files.disk_baseline_->bytes;
  op->local = std::move(local);
  try {
    op->job = impl.jobs.Submit(
        {[op](const core::CancellationToken &token) {
           try {
             op->Run(token);
           } catch (const std::exception &exception) {
             op->Set(SceneComparisonPhase::Failed, exception.what());
           } catch (...) {
             op->Set(SceneComparisonPhase::Failed, "Scene comparison worker failed.");
           }
         },
         core::JobPriority::Low, op->cancellation.Token(), "Scene semantic comparison"});
  } catch (...) {
    return reject("Scene comparison could not submit its worker.");
  }
  impl.operation = std::move(op);
  if (error)
    error->clear();
  return true;
}
bool SceneComparisonJob::Poll(const SceneFileSession &files) {
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
    op->Set(SceneComparisonPhase::Failed, "Scene comparison worker failed.");
  }
  if (op->cancellation.Token().IsCancellationRequested() || status == core::JobStatus::Cancelled)
    op->Set(SceneComparisonPhase::Cancelled, "Scene comparison cancelled.");
  else if (Snapshot().phase == SceneComparisonPhase::AwaitingPublish) {
    if (!files.Live(op->snapshot.token) || files.root_ != op->root ||
        files.session_id_ != op->file_session || !files.disk_baseline_ ||
        files.disk_baseline_->exists != op->base.has_value() ||
        (op->base && files.disk_baseline_->bytes != *op->base) ||
        files.CurrentPath() != op->snapshot.path ||
        files.Resolve(op->snapshot.path) != op->source ||
        !files.document_.MatchesPreparedSave(*op->local))
      op->Set(SceneComparisonPhase::Stale,
              "Scene comparison discarded after scene scope/content changed.");
    else {
      std::lock_guard lock{op->mutex};
      op->snapshot.result = std::move(op->pending);
      op->snapshot.phase = SceneComparisonPhase::Ready;
      op->snapshot.message =
          "Captured base/local/disk comparison. Disk may change; Save rechecks separately.";
    }
  }
  op->base.reset();
  op->local.reset();
  op->pending.reset();
  op->consumed = true;
  return true;
}
bool SceneComparisonJob::Cancel() noexcept {
  const auto op = implementation_->operation;
  if (!op || op->consumed)
    return false;
  op->cancellation.Cancel();
  return true;
}
bool SceneComparisonJob::Busy() const {
  const auto op = implementation_->operation;
  return op && !op->consumed;
}
SceneComparisonSnapshot SceneComparisonJob::Snapshot() const {
  const auto op = implementation_->operation;
  if (!op)
    return {};
  std::lock_guard lock{op->mutex};
  return op->snapshot;
}
void SceneComparisonJob::Shutdown() noexcept {
  auto &impl = *implementation_;
  impl.stopped = true;
  const auto op = impl.operation;
  if (!op || op->consumed)
    return;
  op->cancellation.Cancel();
  try {
    impl.jobs.Wait(op->job);
  } catch (...) {
  }
  op->base.reset();
  op->local.reset();
  op->pending.reset();
  op->consumed = true;
  try {
    op->Set(SceneComparisonPhase::Cancelled, "Scene comparison stopped.");
  } catch (...) {
  }
}
} // namespace nexora::editor
