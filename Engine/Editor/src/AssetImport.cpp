#include "Nexora/Editor/AssetImport.h"

#include "Nexora/Core/Cancellation.h"
#include "ReimportSource.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <ranges>
#include <unordered_map>
#include <utility>

namespace nexora::editor {
namespace {

template <typename Value>
void PushBounded(std::deque<Value> &values, Value value, std::size_t capacity,
                 std::size_t &dropped) {
  if (values.size() == capacity) {
    values.pop_front();
    ++dropped;
  }
  values.push_back(std::move(value));
}

} // namespace

struct AssetImportQueue::Implementation final {
  struct Operation final {
    mutable std::mutex mutex;
    ImportOperationId id{};
    ImportOperationKind kind{ImportOperationKind::Workspace};
    ImportOperationState state{ImportOperationState::Queued};
    std::uint64_t project_generation{};
    core::CancellationSource cancellation;
    core::JobHandle job;
    std::deque<ImportProgressEvent> progress;
    std::deque<ImportDiagnostic> diagnostics;
    std::size_t dropped_progress{};
    std::size_t dropped_diagnostics{};
    std::optional<AssetWorkspace> workspace;
    std::optional<ReimportResult> reimport;
  };

  Implementation(core::JobSystem &job_system, std::size_t progress_limit,
                 std::size_t diagnostic_limit, std::size_t operation_limit)
      : jobs(job_system), progress_capacity(std::max<std::size_t>(progress_limit, 1)),
        diagnostic_capacity(std::max<std::size_t>(diagnostic_limit, 1)),
        operation_capacity(std::max<std::size_t>(operation_limit, 1)) {}

  void Progress(const std::shared_ptr<Operation> &operation, ImportStage stage,
                std::size_t completed, std::size_t total) const {
    std::lock_guard lock{operation->mutex};
    if (operation->state == ImportOperationState::Queued)
      operation->state = ImportOperationState::Running;
    PushBounded(operation->progress, {operation->id, stage, completed, total}, progress_capacity,
                operation->dropped_progress);
  }

  void Diagnose(const std::shared_ptr<Operation> &operation, ImportDiagnosticSeverity severity,
                std::string code, std::string message, std::filesystem::path path = {},
                runtime::AssetUuid asset = {}) const {
    std::lock_guard lock{operation->mutex};
    PushBounded(
        operation->diagnostics,
        {operation->id, severity, std::move(code), std::move(message), std::move(path), asset},
        diagnostic_capacity, operation->dropped_diagnostics);
  }

  void FinishCancelled(const std::shared_ptr<Operation> &operation) const {
    Diagnose(operation, ImportDiagnosticSeverity::Info, "import.cancelled",
             "Import operation was cancelled.");
    std::lock_guard lock{operation->mutex};
    operation->workspace.reset();
    operation->reimport.reset();
    operation->state = ImportOperationState::Cancelled;
    PushBounded(operation->progress, {operation->id, ImportStage::Finished, 0, 0},
                progress_capacity, operation->dropped_progress);
  }

  void FinishFailed(const std::shared_ptr<Operation> &operation, std::string code,
                    std::string message, const std::filesystem::path &path = {},
                    runtime::AssetUuid asset = {}) const {
    Diagnose(operation, ImportDiagnosticSeverity::Error, std::move(code), std::move(message), path,
             asset);
    std::lock_guard lock{operation->mutex};
    operation->workspace.reset();
    operation->reimport.reset();
    operation->state = ImportOperationState::Failed;
    PushBounded(operation->progress, {operation->id, ImportStage::Finished, 0, 0},
                progress_capacity, operation->dropped_progress);
  }

  [[nodiscard]] ImportOperationSnapshot SnapshotLocked(const Operation &operation) const {
    return {operation.id,
            operation.kind,
            operation.state,
            operation.project_generation,
            progress_capacity,
            diagnostic_capacity,
            {operation.progress.begin(), operation.progress.end()},
            {operation.diagnostics.begin(), operation.diagnostics.end()},
            operation.dropped_progress,
            operation.dropped_diagnostics};
  }

  core::JobSystem &jobs;
  const std::size_t progress_capacity;
  const std::size_t diagnostic_capacity;
  const std::size_t operation_capacity;
  mutable std::mutex mutex;
  std::unordered_map<ImportOperationId, std::shared_ptr<Operation>> operations;
  ImportOperationId next_operation{1};
  bool accepting{true};
};

AssetImportQueue::AssetImportQueue(core::JobSystem &jobs, std::size_t progress_capacity,
                                   std::size_t diagnostic_capacity)
    : AssetImportQueue(jobs, progress_capacity, diagnostic_capacity, 64) {}

AssetImportQueue::AssetImportQueue(core::JobSystem &jobs, std::size_t progress_capacity,
                                   std::size_t diagnostic_capacity, std::size_t operation_capacity)
    : implementation_(std::make_unique<Implementation>(jobs, progress_capacity, diagnostic_capacity,
                                                       operation_capacity)) {}

AssetImportQueue::~AssetImportQueue() { Shutdown(); }

ImportOperationId AssetImportQueue::Start(WorkspaceImportRequest request, std::string *error) {
  std::error_code ec;
  const auto content_root = std::filesystem::canonical(request.content_root, ec);
  if (request.project_generation == 0 || ec || !std::filesystem::is_directory(content_root, ec)) {
    if (error)
      *error = "workspace import request has an invalid generation or content root";
    return 0;
  }

  auto operation = std::make_shared<Implementation::Operation>();
  {
    std::lock_guard lock{implementation_->mutex};
    if (!implementation_->accepting) {
      if (error)
        *error = "asset import queue is shutting down";
      return 0;
    }
    if (implementation_->operations.size() >= implementation_->operation_capacity) {
      if (error)
        *error = "asset import queue is full; consume a completed result before retrying";
      return 0;
    }
    operation->id = implementation_->next_operation++;
    operation->kind = ImportOperationKind::Workspace;
    operation->project_generation = request.project_generation;
    implementation_->operations.emplace(operation->id, operation);
  }
  implementation_->Progress(operation, ImportStage::Queued, 0, 0);
  const auto identity_mode = request.identity_mode;
  auto *const implementation = implementation_.get();
  try {
    operation->job = implementation_->jobs.Submit(
        {[implementation, operation, content_root,
          identity_mode](const core::CancellationToken &token) {
           if (token.IsCancellationRequested()) {
             implementation->FinishCancelled(operation);
             return;
           }
           implementation->Progress(operation, ImportStage::Enumerating, 0, 0);
           AssetWorkspace workspace;
           std::string import_error;
           const bool imported = workspace.ImportTree(
               content_root, [&token] { return token.IsCancellationRequested(); },
               [implementation, operation](std::size_t completed, std::size_t total) {
                 implementation->Progress(operation, ImportStage::Reading, completed, total);
               },
               identity_mode, &import_error);
           if (token.IsCancellationRequested()) {
             implementation->FinishCancelled(operation);
             return;
           }
           if (!imported) {
             implementation->FinishFailed(operation, "workspace.import_failed",
                                          import_error.empty() ? "Content import failed."
                                                               : std::move(import_error),
                                          content_root);
             return;
           }
           for (const auto &entry : workspace.Entries())
             if (entry.state == ImportState::Failed)
               implementation->Diagnose(
                   operation, ImportDiagnosticSeverity::Error, "asset.import_failed", entry.error,
                   content_root / std::filesystem::path(std::u8string(entry.relative_path.begin(),
                                                                      entry.relative_path.end())),
                   entry.id);
           implementation->Progress(operation, ImportStage::Staging, workspace.Entries().size(),
                                    workspace.Entries().size());
           std::lock_guard lock{operation->mutex};
           if (token.IsCancellationRequested()) {
             operation->state = ImportOperationState::Cancelled;
             return;
           }
           operation->workspace = std::move(workspace);
           operation->state = ImportOperationState::AwaitingPublish;
         },
         core::JobPriority::Normal, operation->cancellation.Token(), "Editor workspace import"});
  } catch (const std::exception &exception) {
    implementation_->FinishFailed(operation, "import.submit_failed", exception.what(),
                                  content_root);
  }
  if (error)
    error->clear();
  return operation->id;
}

ImportOperationId AssetImportQueue::Start(ReimportJobRequest request, std::string *error) {
  std::error_code ec;
  const auto source = std::filesystem::canonical(request.source, ec);
  if (request.project_generation == 0 || request.asset == runtime::AssetUuid{} ||
      request.previous_artifact.empty() || request.settings_hash.empty() || ec ||
      !std::filesystem::is_regular_file(source, ec)) {
    if (error)
      *error = "reimport request has an invalid generation, asset, source, or hash";
    return 0;
  }

  auto operation = std::make_shared<Implementation::Operation>();
  {
    std::lock_guard lock{implementation_->mutex};
    if (!implementation_->accepting) {
      if (error)
        *error = "asset import queue is shutting down";
      return 0;
    }
    if (implementation_->operations.size() >= implementation_->operation_capacity) {
      if (error)
        *error = "asset import queue is full; consume a completed result before retrying";
      return 0;
    }
    operation->id = implementation_->next_operation++;
    operation->kind = ImportOperationKind::Reimport;
    operation->project_generation = request.project_generation;
    implementation_->operations.emplace(operation->id, operation);
  }
  implementation_->Progress(operation, ImportStage::Queued, 0, 4);
  auto *const implementation = implementation_.get();
  try {
    operation->job = implementation_->jobs.Submit(
        {[implementation, operation, request = std::move(request),
          source](const core::CancellationToken &token) mutable {
           if (token.IsCancellationRequested()) {
             implementation->FinishCancelled(operation);
             return;
           }
           implementation->Progress(operation, ImportStage::Reading, 1, 4);
           auto imported =
               detail::ReadReimportSource(source, request.type, request.asset,
                                          [&token] { return token.IsCancellationRequested(); });
           if (imported.cancelled || token.IsCancellationRequested()) {
             implementation->FinishCancelled(operation);
             return;
           }
           if (!imported.error.empty()) {
             implementation->FinishFailed(operation,
                                          imported.read_failed ? "reimport.read_failed"
                                          : request.type == ".nmaterial"
                                              ? "reimport.material_failed"
                                              : "reimport.mesh_failed",
                                          imported.error, source, request.asset);
             return;
           }
           implementation->Progress(operation, ImportStage::Staging, 2, 4);
           ReimportResult result{request.project_generation,
                                 request.asset,
                                 std::move(imported.source_hash),
                                 std::move(request.settings_hash),
                                 std::move(imported.artifact_hash),
                                 std::move(request.dependencies),
                                 {},
                                 false,
                                 std::move(imported.mesh),
                                 std::move(imported.material)};
           if (token.IsCancellationRequested()) {
             implementation->FinishCancelled(operation);
             return;
           }
           implementation->Progress(operation, ImportStage::Publishing, 3, 4);
           std::lock_guard lock{operation->mutex};
           if (token.IsCancellationRequested()) {
             operation->state = ImportOperationState::Cancelled;
             return;
           }
           operation->reimport = std::move(result);
           operation->state = ImportOperationState::AwaitingPublish;
         },
         core::JobPriority::Normal, operation->cancellation.Token(), "Editor asset reimport"});
  } catch (const std::exception &exception) {
    implementation_->FinishFailed(operation, "import.submit_failed", exception.what(), source,
                                  request.asset);
  }
  if (error)
    error->clear();
  return operation->id;
}

bool AssetImportQueue::Cancel(ImportOperationId operation_id) noexcept {
  std::shared_ptr<Implementation::Operation> operation;
  {
    std::lock_guard lock{implementation_->mutex};
    const auto found = implementation_->operations.find(operation_id);
    if (found == implementation_->operations.end())
      return false;
    operation = found->second;
  }
  std::lock_guard lock{operation->mutex};
  if (operation->state == ImportOperationState::Succeeded ||
      operation->state == ImportOperationState::Cancelled ||
      operation->state == ImportOperationState::Failed ||
      operation->state == ImportOperationState::Stale)
    return false;
  operation->cancellation.Cancel();
  operation->workspace.reset();
  operation->reimport.reset();
  operation->state = ImportOperationState::Cancelling;
  return true;
}

std::optional<ImportOperationSnapshot>
AssetImportQueue::Snapshot(ImportOperationId operation_id) const {
  std::shared_ptr<Implementation::Operation> operation;
  {
    std::lock_guard lock{implementation_->mutex};
    const auto found = implementation_->operations.find(operation_id);
    if (found == implementation_->operations.end())
      return std::nullopt;
    operation = found->second;
  }
  std::lock_guard lock{operation->mutex};
  return implementation_->SnapshotLocked(*operation);
}

std::optional<ImportOperationResult> AssetImportQueue::TakeResult(ImportOperationId operation_id) {
  std::shared_ptr<Implementation::Operation> operation;
  {
    std::lock_guard lock{implementation_->mutex};
    const auto found = implementation_->operations.find(operation_id);
    if (found == implementation_->operations.end())
      return std::nullopt;
    operation = found->second;
  }
  ImportOperationResult result;
  {
    std::lock_guard lock{operation->mutex};
    if (operation->job.IsValid()) {
      const auto job_status = operation->job.Status();
      if (job_status == core::JobStatus::Queued || job_status == core::JobStatus::Running)
        return std::nullopt;
      if ((job_status == core::JobStatus::Completed || job_status == core::JobStatus::Cancelled) &&
          operation->state == ImportOperationState::Cancelling) {
        operation->state = ImportOperationState::Cancelled;
        PushBounded(operation->diagnostics,
                    {operation->id,
                     ImportDiagnosticSeverity::Info,
                     "import.cancelled",
                     "Import operation was cancelled.",
                     {},
                     {}},
                    implementation_->diagnostic_capacity, operation->dropped_diagnostics);
      } else if ((job_status == core::JobStatus::Completed ||
                  job_status == core::JobStatus::Failed) &&
                 operation->state != ImportOperationState::AwaitingPublish &&
                 operation->state != ImportOperationState::Cancelled &&
                 operation->state != ImportOperationState::Failed) {
        operation->state = ImportOperationState::Failed;
        PushBounded(operation->diagnostics,
                    {operation->id,
                     ImportDiagnosticSeverity::Error,
                     "import.worker_terminated",
                     "Import worker terminated without a staged result.",
                     {},
                     {}},
                    implementation_->diagnostic_capacity, operation->dropped_diagnostics);
      }
    }
    if (operation->state != ImportOperationState::AwaitingPublish &&
        operation->state != ImportOperationState::Cancelled &&
        operation->state != ImportOperationState::Failed)
      return std::nullopt;
    if (operation->state == ImportOperationState::Cancelled &&
        std::ranges::none_of(operation->diagnostics, [](const ImportDiagnostic &diagnostic) {
          return diagnostic.code == "import.cancelled";
        })) {
      PushBounded(operation->diagnostics,
                  {operation->id,
                   ImportDiagnosticSeverity::Info,
                   "import.cancelled",
                   "Import operation was cancelled.",
                   {},
                   {}},
                  implementation_->diagnostic_capacity, operation->dropped_diagnostics);
      PushBounded(operation->progress, {operation->id, ImportStage::Finished, 0, 0},
                  implementation_->progress_capacity, operation->dropped_progress);
    }
    if (operation->state == ImportOperationState::AwaitingPublish &&
        operation->cancellation.Token().IsCancellationRequested()) {
      operation->workspace.reset();
      operation->reimport.reset();
      operation->state = ImportOperationState::Cancelled;
      PushBounded(operation->diagnostics,
                  {operation->id,
                   ImportDiagnosticSeverity::Info,
                   "import.cancelled",
                   "Import operation was cancelled before publication.",
                   {},
                   {}},
                  implementation_->diagnostic_capacity, operation->dropped_diagnostics);
    }
    result.snapshot = implementation_->SnapshotLocked(*operation);
    result.workspace = std::move(operation->workspace);
    result.reimport = std::move(operation->reimport);
  }
  {
    std::lock_guard lock{implementation_->mutex};
    implementation_->operations.erase(operation_id);
  }
  return result;
}

void AssetImportQueue::Shutdown() noexcept {
  std::vector<std::shared_ptr<Implementation::Operation>> operations;
  {
    std::lock_guard lock{implementation_->mutex};
    if (!implementation_->accepting && implementation_->operations.empty())
      return;
    implementation_->accepting = false;
    for (const auto &[unused, operation] : implementation_->operations)
      operations.push_back(operation);
  }
  for (const auto &operation : operations)
    operation->cancellation.Cancel();
  for (const auto &operation : operations) {
    if (!operation->job.IsValid())
      continue;
    try {
      implementation_->jobs.Wait(operation->job);
    } catch (...) {
    }
  }
  std::lock_guard lock{implementation_->mutex};
  implementation_->operations.clear();
}

} // namespace nexora::editor
