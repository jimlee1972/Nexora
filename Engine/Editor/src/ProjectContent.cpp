#include "Nexora/Editor/ProjectContent.h"
#include "ReimportSource.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <unordered_set>

namespace nexora::editor {
namespace {

bool SafeRelative(const std::filesystem::path &path) {
  if (path.empty() || path.is_absolute() || path.has_root_name())
    return false;
  return std::ranges::none_of(path, [](const auto &part) { return part == ".." || part == "."; });
}

bool ContentPath(const std::filesystem::path &path) {
  if (!SafeRelative(path))
    return false;
  const auto first = path.begin();
  return first != path.end() && *first == "Content";
}

std::uint64_t Hash(std::string_view bytes, std::uint64_t seed) {
  auto value = seed;
  for (const unsigned char byte : bytes) {
    value ^= byte;
    value *= 1099511628211ULL;
  }
  return value;
}

std::string Hex(std::uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}

ThumbnailState ThumbnailFor(ImportState state) {
  switch (state) {
  case ImportState::Imported:
    return ThumbnailState::Ready;
  case ImportState::Failed:
    return ThumbnailState::Failed;
  case ImportState::Pending:
  case ImportState::Cancelled:
    return ThumbnailState::Loading;
  }
  return ThumbnailState::Failed;
}

} // namespace

struct ProjectContentSession::PendingReimport final {
  PendingReimport(AssetImportQueue &import_queue, ImportOperationId import_operation,
                  std::uint64_t generation, runtime::AssetUuid target,
                  std::filesystem::path asset_path, std::string artifact, std::string settings,
                  std::filesystem::file_time_type source_write, std::uintmax_t source_bytes,
                  std::vector<runtime::AssetUuid> asset_dependencies)
      : queue(&import_queue), operation(import_operation), project_generation(generation),
        asset(target), path(std::move(asset_path)), previous_artifact(std::move(artifact)),
        settings_hash(std::move(settings)), source_write_time(source_write),
        source_size(source_bytes), dependencies(std::move(asset_dependencies)) {}

  AssetImportQueue *queue{};
  ImportOperationId operation{};
  std::uint64_t project_generation{};
  runtime::AssetUuid asset;
  std::filesystem::path path;
  std::string previous_artifact;
  std::string settings_hash;
  std::filesystem::file_time_type source_write_time;
  std::uintmax_t source_size{};
  std::vector<runtime::AssetUuid> dependencies;

  ~PendingReimport() {
    if (queue != nullptr && operation != 0)
      static_cast<void>(queue->Cancel(operation));
  }
};

ProjectContentSession::ProjectContentSession() = default;
ProjectContentSession::~ProjectContentSession() = default;
ProjectContentSession::ProjectContentSession(ProjectContentSession &&) noexcept = default;
ProjectContentSession &
ProjectContentSession::operator=(ProjectContentSession &&) noexcept = default;

bool ProjectContentSession::Open(const ProjectWorkspace &workspace, const AssetWorkspace &assets,
                                 std::uint64_t project_generation, bool writable,
                                 std::string *error) {
  if (pending_reimport_)
    return Fail("project content cannot reopen while a reimport is active", error);
  if (writable && !workspace.Writable())
    return Fail("a read-only project workspace cannot open writable content", error);
  std::error_code ec;
  const auto root = std::filesystem::canonical(workspace.Root(), ec);
  if (ec || !std::filesystem::is_directory(root, ec))
    return Fail("project root is unavailable", error);
  const bool persistent_identities = assets.PersistentIdentities();
  if (persistent_identities) {
    const auto expected_content = std::filesystem::canonical(root / "Content", ec);
    if (ec || assets.ContentRoot() != expected_content)
      return Fail("asset identity index belongs to a different content root", error);
    if (writable && !assets.WritableIdentities())
      return Fail("a read-only asset identity index cannot open a writable content session", error);
  }

  std::vector<ContentItem> items;
  items.reserve(assets.Entries().size());
  AssetDependencyGraph dependencies;
  for (const auto &entry : assets.Entries()) {
    const auto path = std::filesystem::path("Content") /
                      std::filesystem::path(
                          std::u8string(entry.relative_path.begin(), entry.relative_path.end()));
    if (!ContentPath(path) || entry.id == runtime::AssetUuid{} || !dependencies.Set(entry.id, {}))
      return Fail("asset index contains an invalid entry", error);
    items.push_back({entry.id, path.lexically_normal(), entry.type, entry.artifact_hash,
                     ThumbnailFor(entry.state), entry.mesh});
  }

  auto browser = browser_;
  browser.SetFilter({}, {});
  static_cast<void>(browser.SetFolder("Content"));
  if (!browser.Reset(items, project_generation))
    return Fail("asset index could not initialize the content browser", error);

  root_ = root;
  browser_ = std::move(browser);
  dependencies_ = std::move(dependencies);
  conflicts_ = {};
  undo_moves_.clear();
  operation_ = 0;
  writable_ = writable;
  persistent_identities_ = persistent_identities;
  last_reimport_.reset();
  ClearError(error);
  return true;
}

bool ProjectContentSession::Fail(std::string message, std::string *error) {
  last_error_ = std::move(message);
  if (error)
    *error = last_error_;
  return false;
}

void ProjectContentSession::ClearError(std::string *error) {
  last_error_.clear();
  if (error)
    error->clear();
}

void ProjectContentSession::AppendAssetMove(std::vector<FileMove> &moves,
                                            const std::filesystem::path &source,
                                            const std::filesystem::path &destination) const {
  moves.emplace_back(source, destination);
  if (persistent_identities_)
    moves.emplace_back(AssetWorkspace::IdentitySidecar(source),
                       AssetWorkspace::IdentitySidecar(destination));
}

std::filesystem::path ProjectContentSession::ExistingPath(const std::filesystem::path &relative,
                                                          std::string *error) const {
  if (root_.empty() || !SafeRelative(relative)) {
    if (error)
      *error = "asset path is unsafe";
    return {};
  }
  std::error_code ec;
  const auto joined = root_ / relative.lexically_normal();
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(joined, ec)) || ec ||
      !std::filesystem::is_regular_file(joined, ec) || ec) {
    if (error)
      *error = "asset source is unavailable or is a symlink";
    return {};
  }
  const auto canonical = std::filesystem::canonical(joined, ec);
  const auto within = std::filesystem::relative(canonical, root_, ec);
  if (ec || !SafeRelative(within)) {
    if (error)
      *error = "asset source escapes the project root";
    return {};
  }
  return canonical;
}

std::filesystem::path ProjectContentSession::DestinationPath(const std::filesystem::path &relative,
                                                             bool create_parent,
                                                             std::string *error) const {
  if (root_.empty() || !SafeRelative(relative)) {
    if (error)
      *error = "asset destination is unsafe";
    return {};
  }
  const auto joined = root_ / relative.lexically_normal();
  std::error_code ec;
  if (create_parent)
    std::filesystem::create_directories(joined.parent_path(), ec);
  if (ec || !std::filesystem::is_directory(joined.parent_path(), ec) || ec) {
    if (error)
      *error = "asset destination directory is unavailable";
    return {};
  }
  const auto parent = std::filesystem::canonical(joined.parent_path(), ec);
  const auto within = std::filesystem::relative(parent, root_, ec);
  if (ec || (!within.empty() && within != "." && !SafeRelative(within))) {
    if (error)
      *error = "asset destination escapes the project root";
    return {};
  }
  return parent / joined.filename();
}

bool ProjectContentSession::CommitMoves(ContentBrowserModel candidate, std::vector<FileMove> moves,
                                        bool create_destination_directories, std::string *error) {
  if (!writable_)
    return Fail("project content is read-only", error);
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>> resolved;
  resolved.reserve(moves.size());
  std::unordered_set<std::filesystem::path> destinations;
  for (const auto &[source_relative, destination_relative] : moves) {
    std::string path_error;
    const auto source = ExistingPath(source_relative, &path_error);
    const auto destination =
        DestinationPath(destination_relative, create_destination_directories, &path_error);
    std::error_code ec;
    if (source.empty() || destination.empty() || std::filesystem::exists(destination, ec) || ec ||
        !destinations.insert(destination).second)
      return Fail(path_error.empty() ? "asset destination already exists" : path_error, error);
    resolved.emplace_back(source, destination);
  }

  std::size_t completed = 0;
  for (; completed < resolved.size(); ++completed) {
    std::error_code ec;
    std::filesystem::rename(resolved[completed].first, resolved[completed].second, ec);
    if (!ec)
      continue;
    while (completed > 0) {
      --completed;
      std::error_code rollback_error;
      std::filesystem::rename(resolved[completed].second, resolved[completed].first,
                              rollback_error);
    }
    return Fail("asset filesystem transaction failed: " + ec.message(), error);
  }

  browser_ = std::move(candidate);
  undo_moves_ = std::move(moves);
  ClearError(error);
  return true;
}

bool ProjectContentSession::Rename(runtime::AssetUuid asset, std::string_view filename,
                                   std::string *error) {
  const auto *current = browser_.Find(asset);
  if (current == nullptr)
    return Fail("asset does not exist", error);
  const auto source = current->path;
  if (source.filename().generic_u8string() == std::u8string(filename.begin(), filename.end())) {
    if (!writable_)
      return Fail("project content is read-only", error);
    std::string path_error;
    if (ExistingPath(source, &path_error).empty() ||
        (persistent_identities_ &&
         ExistingPath(AssetWorkspace::IdentitySidecar(source), &path_error).empty()))
      return Fail(path_error, error);
    ClearError(error);
    return true;
  }
  auto candidate = browser_;
  std::string model_error;
  if (!candidate.Rename(asset, filename, &model_error))
    return Fail(model_error.empty() ? "asset rename was rejected" : model_error, error);
  const auto destination = candidate.Find(asset)->path;
  std::vector<FileMove> moves;
  AppendAssetMove(moves, source, destination);
  return CommitMoves(std::move(candidate), std::move(moves), false, error);
}

bool ProjectContentSession::Move(std::span<const runtime::AssetUuid> assets,
                                 const std::filesystem::path &folder, std::string *error) {
  if (!ContentPath(folder))
    return Fail("asset destination must be inside Content", error);
  std::vector<FileMove> moves;
  moves.reserve(assets.size());
  for (const auto asset : assets) {
    const auto *current = browser_.Find(asset);
    if (current == nullptr)
      return Fail("asset does not exist", error);
    AppendAssetMove(moves, current->path, folder / current->path.filename());
  }
  auto candidate = browser_;
  std::string model_error;
  if (!candidate.Move(assets, folder, &model_error))
    return Fail(model_error.empty() ? "asset move was rejected" : model_error, error);
  return CommitMoves(std::move(candidate), std::move(moves), false, error);
}

bool ProjectContentSession::Move(const AssetDragPayload &payload,
                                 const std::filesystem::path &folder, std::string *error) {
  const auto validation = ValidateDrag(payload, browser_, folder, writable_);
  if (validation != DragValidation::Valid)
    return Fail("asset drag payload or destination is invalid", error);
  const std::array assets{payload.asset};
  return Move(assets, folder, error);
}

bool ProjectContentSession::Delete(std::span<const runtime::AssetUuid> assets, std::string *error) {
  if (assets.empty())
    return Fail("no assets were selected", error);
  auto candidate = browser_;
  std::string model_error;
  if (!candidate.Delete(assets, &model_error))
    return Fail(model_error.empty() ? "asset delete was rejected" : model_error, error);

  std::error_code ec;
  do {
    ++operation_;
  } while (std::filesystem::exists(root_ / ".nexora" / "trash" / std::to_string(operation_), ec) &&
           !ec);
  if (ec)
    return Fail("project trash could not be inspected: " + ec.message(), error);
  const auto operation = operation_;
  std::vector<FileMove> moves;
  moves.reserve(assets.size());
  for (const auto asset : assets) {
    const auto *current = browser_.Find(asset);
    if (current == nullptr)
      return Fail("asset does not exist", error);
    AppendAssetMove(moves, current->path,
                    std::filesystem::path(".nexora") / "trash" / std::to_string(operation) /
                        current->path);
  }
  return CommitMoves(std::move(candidate), std::move(moves), true, error);
}

bool ProjectContentSession::Undo(std::string *error) {
  if (!writable_)
    return Fail("project content is read-only", error);
  if (undo_moves_.empty())
    return Fail("there is no content mutation to undo", error);

  std::vector<std::pair<std::filesystem::path, std::filesystem::path>> resolved;
  resolved.reserve(undo_moves_.size());
  for (auto it = undo_moves_.rbegin(); it != undo_moves_.rend(); ++it) {
    std::string path_error;
    const auto source = ExistingPath(it->second, &path_error);
    const auto destination = DestinationPath(it->first, true, &path_error);
    std::error_code ec;
    if (source.empty() || destination.empty() || std::filesystem::exists(destination, ec) || ec)
      return Fail(path_error.empty() ? "undo destination already exists" : path_error, error);
    resolved.emplace_back(source, destination);
  }

  std::size_t completed = 0;
  for (; completed < resolved.size(); ++completed) {
    std::error_code ec;
    std::filesystem::rename(resolved[completed].first, resolved[completed].second, ec);
    if (!ec)
      continue;
    while (completed > 0) {
      --completed;
      std::error_code rollback_error;
      std::filesystem::rename(resolved[completed].second, resolved[completed].first,
                              rollback_error);
    }
    return Fail("content undo failed: " + ec.message(), error);
  }

  if (!browser_.Undo()) {
    for (auto it = resolved.rbegin(); it != resolved.rend(); ++it) {
      std::error_code rollback_error;
      std::filesystem::rename(it->second, it->first, rollback_error);
    }
    return Fail("content model rejected filesystem undo", error);
  }
  undo_moves_.clear();
  ClearError(error);
  return true;
}

bool ProjectContentSession::Reimport(runtime::AssetUuid asset, std::string *error) {
  if (!writable_)
    return Fail("project content is read-only", error);
  const auto *item = browser_.Find(asset);
  if (item == nullptr)
    return Fail("asset does not exist", error);

  std::string path_error;
  const auto source = ExistingPath(item->path, &path_error);
  if (source.empty())
    return Fail(path_error, error);
  auto imported = detail::ReadReimportSource(source, item->type);
  if (!imported.error.empty())
    return Fail(imported.error, error);
  const auto source_hash = Hex(Hash(imported.bytes, 1469598103934665603ULL));
  const auto artifact_hash =
      Hex(Hash(imported.bytes, Hash(asset.ToString(), 1469598103934665603ULL)));
  auto candidate = browser_;
  if (!candidate.PublishArtifact(asset, artifact_hash, ThumbnailState::Ready, error, imported.mesh))
    return Fail(error && !error->empty() ? *error : "reimport geometry publication failed", error);
  ReimportTransaction transaction(browser_.ProjectGeneration(), asset, item->artifact_hash);
  const auto dependencies = dependencies_.Forward(asset);
  if (!transaction.Stage({browser_.ProjectGeneration(),
                          asset,
                          source_hash,
                          "default-v1",
                          artifact_hash,
                          dependencies,
                          {},
                          false}) ||
      !transaction.Commit(browser_.ProjectGeneration(), dependencies_))
    return Fail(std::string(transaction.Diagnostic()), error);
  browser_ = std::move(candidate);
  ClearError(error);
  return true;
}

bool ProjectContentSession::BeginReimport(AssetImportQueue &imports, runtime::AssetUuid asset,
                                          std::string *error) {
  if (!writable_)
    return Fail("project content is read-only", error);
  if (pending_reimport_)
    return Fail("another reimport operation is already active", error);
  const auto *item = browser_.Find(asset);
  if (item == nullptr)
    return Fail("asset does not exist", error);

  std::string path_error;
  const auto source = ExistingPath(item->path, &path_error);
  if (source.empty())
    return Fail(path_error, error);
  std::error_code revision_error;
  const auto source_write_time = std::filesystem::last_write_time(source, revision_error);
  if (revision_error)
    return Fail("asset source revision could not be inspected: " + revision_error.message(), error);
  const auto source_size = std::filesystem::file_size(source, revision_error);
  if (revision_error)
    return Fail("asset source revision could not be inspected: " + revision_error.message(), error);
  ReimportJobRequest request{
      browser_.ProjectGeneration(), asset,     source, item->artifact_hash, "default-v1",
      dependencies_.Forward(asset), item->type};
  std::string start_error;
  const auto operation = imports.Start(request, &start_error);
  if (operation == 0)
    return Fail(start_error.empty() ? "reimport operation could not start" : start_error, error);
  pending_reimport_ = std::make_unique<PendingReimport>(
      imports, operation, request.project_generation, asset, item->path, item->artifact_hash,
      request.settings_hash, source_write_time, source_size, request.dependencies);
  last_reimport_.reset();
  ClearError(error);
  return true;
}

bool ProjectContentSession::PollReimport(std::string *error) {
  if (!pending_reimport_)
    return false;
  auto result = pending_reimport_->queue->TakeResult(pending_reimport_->operation);
  if (!result)
    return false;

  last_reimport_ = std::move(result->snapshot);
  pending_reimport_->operation = 0;
  const auto finish = [this](ImportOperationState state, ImportDiagnosticSeverity severity,
                             std::string code, std::string message, std::string *out_error) {
    last_reimport_->state = state;
    if (last_reimport_->diagnostics.size() >= last_reimport_->diagnostic_capacity) {
      last_reimport_->diagnostics.erase(last_reimport_->diagnostics.begin());
      ++last_reimport_->dropped_diagnostics;
    }
    last_reimport_->diagnostics.push_back(
        {last_reimport_->operation, severity, std::move(code), message, {}, {}});
    last_error_ = state == ImportOperationState::Succeeded ? std::string{} : std::move(message);
    if (out_error)
      *out_error = last_error_;
    pending_reimport_.reset();
    return true;
  };

  if (last_reimport_->state == ImportOperationState::Cancelled) {
    last_error_.clear();
    if (error)
      error->clear();
    pending_reimport_.reset();
    return true;
  }
  if (last_reimport_->state == ImportOperationState::Failed || !result->reimport) {
    const auto message = !last_reimport_->diagnostics.empty()
                             ? last_reimport_->diagnostics.back().message
                             : "reimport worker produced no staged result";
    return finish(ImportOperationState::Failed, ImportDiagnosticSeverity::Error,
                  "reimport.worker_failed", message, error);
  }

  const auto *current = browser_.Find(pending_reimport_->asset);
  std::string revision_path_error;
  const auto source = ExistingPath(pending_reimport_->path, &revision_path_error);
  std::error_code revision_error;
  auto source_write_time = std::filesystem::file_time_type{};
  std::uintmax_t source_size = 0;
  bool source_revision_failed = source.empty();
  if (!source_revision_failed) {
    source_write_time = std::filesystem::last_write_time(source, revision_error);
    source_revision_failed = static_cast<bool>(revision_error);
    revision_error.clear();
    source_size = std::filesystem::file_size(source, revision_error);
    source_revision_failed = source_revision_failed || static_cast<bool>(revision_error);
  }
  if (browser_.ProjectGeneration() != pending_reimport_->project_generation || current == nullptr ||
      current->path != pending_reimport_->path ||
      current->artifact_hash != pending_reimport_->previous_artifact || source_revision_failed ||
      source_write_time != pending_reimport_->source_write_time ||
      source_size != pending_reimport_->source_size ||
      result->reimport->settings_hash != pending_reimport_->settings_hash ||
      dependencies_.Forward(pending_reimport_->asset) != pending_reimport_->dependencies) {
    return finish(ImportOperationState::Stale, ImportDiagnosticSeverity::Warning, "reimport.stale",
                  "Reimport completion was discarded because its project or asset revision is "
                  "stale.",
                  error);
  }

  auto candidate = browser_;
  std::string publish_error;
  if (!candidate.PublishArtifact(pending_reimport_->asset, result->reimport->artifact_hash,
                                 ThumbnailState::Ready, &publish_error, result->reimport->mesh))
    return finish(ImportOperationState::Failed, ImportDiagnosticSeverity::Error,
                  "reimport.publish_failed", publish_error, error);
  ReimportTransaction transaction(pending_reimport_->project_generation, pending_reimport_->asset,
                                  pending_reimport_->previous_artifact);
  if (!transaction.Stage(std::move(*result->reimport)) ||
      !transaction.Commit(browser_.ProjectGeneration(), dependencies_)) {
    return finish(ImportOperationState::Failed, ImportDiagnosticSeverity::Error,
                  "reimport.publish_failed", std::string(transaction.Diagnostic()), error);
  }
  browser_ = std::move(candidate);
  if (last_reimport_->progress.size() >= last_reimport_->progress_capacity) {
    last_reimport_->progress.erase(last_reimport_->progress.begin());
    ++last_reimport_->dropped_progress;
  }
  last_reimport_->progress.push_back({last_reimport_->operation, ImportStage::Finished, 4, 4});
  return finish(ImportOperationState::Succeeded, ImportDiagnosticSeverity::Info,
                "reimport.succeeded", "Reimport artifact was published.", error);
}

bool ProjectContentSession::CancelReimport() noexcept {
  return pending_reimport_ && pending_reimport_->queue->Cancel(pending_reimport_->operation);
}

bool ProjectContentSession::ReimportBusy() const noexcept { return pending_reimport_ != nullptr; }

std::optional<ImportOperationSnapshot> ProjectContentSession::ReimportStatus() const {
  if (pending_reimport_)
    return pending_reimport_->queue->Snapshot(pending_reimport_->operation);
  return last_reimport_;
}

} // namespace nexora::editor
