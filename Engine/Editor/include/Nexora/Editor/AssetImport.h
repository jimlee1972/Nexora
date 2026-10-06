#pragma once

#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/Api.h"
#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nexora::editor {

using ImportOperationId = std::uint64_t;

enum class ImportOperationKind : std::uint8_t { Workspace, Reimport };
enum class ImportOperationState : std::uint8_t {
  Queued,
  Running,
  Cancelling,
  AwaitingPublish,
  Succeeded,
  Cancelled,
  Failed,
  Stale
};
enum class ImportStage : std::uint8_t {
  Queued,
  Enumerating,
  Reading,
  Staging,
  Publishing,
  Finished
};
enum class ImportDiagnosticSeverity : std::uint8_t { Info, Warning, Error };

struct ImportProgressEvent final {
  ImportOperationId operation{};
  ImportStage stage{ImportStage::Queued};
  std::size_t completed{};
  std::size_t total{};
};

struct ImportDiagnostic final {
  ImportOperationId operation{};
  ImportDiagnosticSeverity severity{ImportDiagnosticSeverity::Info};
  std::string code;
  std::string message;
  std::filesystem::path path;
  runtime::AssetUuid asset;
};

struct ImportOperationSnapshot final {
  ImportOperationId operation{};
  ImportOperationKind kind{ImportOperationKind::Workspace};
  ImportOperationState state{ImportOperationState::Queued};
  std::uint64_t project_generation{};
  std::size_t progress_capacity{};
  std::size_t diagnostic_capacity{};
  std::vector<ImportProgressEvent> progress;
  std::vector<ImportDiagnostic> diagnostics;
  std::size_t dropped_progress{};
  std::size_t dropped_diagnostics{};
};

struct WorkspaceImportRequest final {
  std::uint64_t project_generation{};
  std::filesystem::path content_root;
  AssetIdentityMode identity_mode{AssetIdentityMode::DerivedFromPath};
};

struct ReimportJobRequest final {
  std::uint64_t project_generation{};
  runtime::AssetUuid asset;
  std::filesystem::path source;
  std::string previous_artifact;
  std::string settings_hash{"default-v1"};
  std::vector<runtime::AssetUuid> dependencies;
  // Persistent importer type; empty derives it from the source extension.
  std::string type{};
};

struct ImportOperationResult final {
  ImportOperationSnapshot snapshot;
  std::optional<AssetWorkspace> workspace;
  std::optional<ReimportResult> reimport;
};

// Multi-operation worker queue. Workers may read source files and build staging results, but only
// the authoring thread may TakeResult() and publish them into a live workspace/content model.
// Shutdown() cancels and joins every submitted job; the referenced JobSystem must outlive the
// queue.
// Intake retains at most 64 operations by default; the four-argument constructor sets a custom
// limit (zero becomes one). Completed, failed and cancelled operations retain their slot until
// TakeResult() consumes them. Full intake returns zero plus a retryable error without submitting
// work or evicting an existing result. Start/TakeResult/Shutdown run on the authoring thread.
class NEXORA_EDITOR_API AssetImportQueue final {
public:
  explicit AssetImportQueue(core::JobSystem &jobs, std::size_t progress_capacity = 16,
                            std::size_t diagnostic_capacity = 32);
  AssetImportQueue(core::JobSystem &jobs, std::size_t progress_capacity,
                   std::size_t diagnostic_capacity, std::size_t operation_capacity);
  ~AssetImportQueue();
  AssetImportQueue(const AssetImportQueue &) = delete;
  AssetImportQueue &operator=(const AssetImportQueue &) = delete;
  AssetImportQueue(AssetImportQueue &&) = delete;
  AssetImportQueue &operator=(AssetImportQueue &&) = delete;

  [[nodiscard]] ImportOperationId Start(WorkspaceImportRequest request,
                                        std::string *error = nullptr);
  [[nodiscard]] ImportOperationId Start(ReimportJobRequest request, std::string *error = nullptr);
  bool Cancel(ImportOperationId operation) noexcept;
  [[nodiscard]] std::optional<ImportOperationSnapshot> Snapshot(ImportOperationId operation) const;
  [[nodiscard]] std::optional<ImportOperationResult> TakeResult(ImportOperationId operation);
  void Shutdown() noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};

} // namespace nexora::editor
