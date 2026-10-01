#include "Nexora/Editor/ProjectContent.h"

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

bool ProjectContentSession::Open(const ProjectWorkspace &workspace, const AssetWorkspace &assets,
                                 std::uint64_t project_generation, bool writable,
                                 std::string *error) {
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
    const auto path = std::filesystem::path("Content") / entry.relative_path;
    if (!ContentPath(path) || entry.id == runtime::AssetUuid{} || !dependencies.Set(entry.id, {}))
      return Fail("asset index contains an invalid entry", error);
    items.push_back({entry.id, path.lexically_normal(), entry.type, entry.artifact_hash,
                     ThumbnailFor(entry.state)});
  }

  ContentBrowserModel browser(project_generation);
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
  std::unordered_set<std::string> destinations;
  for (const auto &[source_relative, destination_relative] : moves) {
    std::string path_error;
    const auto source = ExistingPath(source_relative, &path_error);
    const auto destination =
        DestinationPath(destination_relative, create_destination_directories, &path_error);
    std::error_code ec;
    if (source.empty() || destination.empty() || std::filesystem::exists(destination, ec) || ec ||
        !destinations.insert(destination.generic_string()).second)
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
  std::ifstream input(source, std::ios::binary);
  std::ostringstream bytes;
  bytes << input.rdbuf();
  if (!input.good() && !input.eof())
    return Fail("asset source could not be read", error);

  const auto source_hash = Hex(Hash(bytes.str(), 1469598103934665603ULL));
  const auto artifact_hash = Hex(Hash(bytes.str(), Hash(asset.ToString(), 1469598103934665603ULL)));
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
  if (!browser_.PublishArtifact(asset, std::string(transaction.Artifact()), ThumbnailState::Ready,
                                error))
    return Fail(error && !error->empty() ? *error : "reimport publication failed", error);
  ClearError(error);
  return true;
}

} // namespace nexora::editor
