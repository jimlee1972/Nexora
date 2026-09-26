#include "Nexora/Editor/ContentBrowser.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace nexora::editor {
namespace {
std::string Lower(std::string_view value) {
  std::string result(value);
  std::ranges::transform(result, result.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}
bool SafeRelative(const std::filesystem::path &path) {
  if (path.empty() || path.is_absolute())
    return false;
  for (const auto &part : path)
    if (part == ".." || part == ".")
      return false;
  return true;
}
} // namespace

ContentBrowserModel::ContentBrowserModel(std::uint64_t project_generation)
    : generation_(project_generation) {
  SetFolder("Content");
}
bool ContentBrowserModel::Reset(std::span<const ContentItem> items,
                                std::uint64_t project_generation) {
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> ids;
  std::unordered_set<std::string> paths;
  for (const auto &item : items)
    if (item.id == runtime::AssetUuid{} || !SafeRelative(item.path) ||
        !ids.insert(item.id).second || !paths.insert(item.path.generic_string()).second)
      return false;
  items_.assign(items.begin(), items.end());
  std::ranges::sort(items_, {}, [](const ContentItem &item) { return item.path.generic_string(); });
  selection_.clear();
  undo_.clear();
  generation_ = project_generation;
  return true;
}
bool ContentBrowserModel::SetFolder(const std::filesystem::path &folder) {
  if (!SafeRelative(folder))
    return false;
  folder_ = folder.lexically_normal();
  breadcrumbs_.clear();
  std::filesystem::path current;
  for (const auto &part : folder_) {
    current /= part;
    breadcrumbs_.push_back({part.string(), current});
  }
  return true;
}
void ContentBrowserModel::SetFilter(std::string query, std::string type) {
  query_ = Lower(query);
  type_ = Lower(type);
}
std::vector<const ContentItem *> ContentBrowserModel::Visible(std::size_t offset,
                                                              std::size_t count) const {
  std::vector<const ContentItem *> matches;
  for (const auto &item : items_) {
    const auto parent = item.path.parent_path();
    if (parent != folder_ ||
        (!query_.empty() &&
         Lower(item.path.filename().string()).find(query_) == std::string::npos) ||
        (!type_.empty() && Lower(item.type) != type_))
      continue;
    if (offset != 0) {
      --offset;
      continue;
    }
    if (matches.size() == count)
      break;
    matches.push_back(&item);
  }
  return matches;
}
const ContentItem *ContentBrowserModel::Find(runtime::AssetUuid id) const {
  const auto found = std::ranges::find(items_, id, &ContentItem::id);
  return found == items_.end() ? nullptr : &*found;
}
bool ContentBrowserModel::Select(runtime::AssetUuid id, bool additive) {
  if (!Find(id))
    return false;
  if (!additive)
    selection_.clear();
  selection_.insert(id);
  return true;
}
bool ContentBrowserModel::Toggle(runtime::AssetUuid id) {
  if (!Find(id))
    return false;
  if (selection_.erase(id) == 0)
    selection_.insert(id);
  return true;
}
bool ContentBrowserModel::IsSelected(runtime::AssetUuid id) const {
  return selection_.contains(id);
}
std::vector<runtime::AssetUuid> ContentBrowserModel::Selection() const {
  std::vector<runtime::AssetUuid> result(selection_.begin(), selection_.end());
  std::ranges::sort(result, [](const auto &a, const auto &b) {
    return a.high < b.high || (a.high == b.high && a.low < b.low);
  });
  return result;
}
bool ContentBrowserModel::ValidDestination(const std::filesystem::path &path,
                                           runtime::AssetUuid except) const {
  return SafeRelative(path) && std::ranges::none_of(items_, [&](const ContentItem &item) {
           return item.id != except && item.path == path;
         });
}
bool ContentBrowserModel::Commit(std::vector<ContentItem> next, std::string *error) {
  undo_ = items_;
  items_ = std::move(next);
  std::ranges::sort(items_, {}, [](const ContentItem &item) { return item.path.generic_string(); });
  if (error)
    error->clear();
  return true;
}
bool ContentBrowserModel::Rename(runtime::AssetUuid id, std::string_view filename,
                                 std::string *error) {
  auto next = items_;
  auto found = std::ranges::find(next, id, &ContentItem::id);
  const std::filesystem::path name(filename);
  if (found == next.end() || name.filename() != name || !SafeRelative(name)) {
    if (error)
      *error = "asset or filename is invalid";
    return false;
  }
  const auto destination = found->path.parent_path() / name;
  if (!ValidDestination(destination, id)) {
    if (error)
      *error = "destination already exists or is unsafe";
    return false;
  }
  found->path = destination;
  return Commit(std::move(next), error);
}
bool ContentBrowserModel::Move(std::span<const runtime::AssetUuid> ids,
                               const std::filesystem::path &folder, std::string *error) {
  if (ids.empty() || !SafeRelative(folder))
    return false;
  auto next = items_;
  std::unordered_set<std::string> destinations;
  for (const auto id : ids) {
    auto found = std::ranges::find(next, id, &ContentItem::id);
    if (found == next.end())
      return false;
    const auto destination = folder / found->path.filename();
    if (!ValidDestination(destination, id) ||
        !destinations.insert(destination.generic_string()).second) {
      if (error)
        *error = "move destination conflicts";
      return false;
    }
    found->path = destination;
  }
  return Commit(std::move(next), error);
}
bool ContentBrowserModel::Delete(std::span<const runtime::AssetUuid> ids, std::string *error) {
  if (ids.empty())
    return false;
  auto next = items_;
  for (const auto id : ids) {
    const auto found = std::ranges::find(next, id, &ContentItem::id);
    if (found == next.end())
      return false;
    next.erase(found);
  }
  for (const auto id : ids)
    selection_.erase(id);
  return Commit(std::move(next), error);
}
bool ContentBrowserModel::Undo() {
  if (undo_.empty())
    return false;
  items_.swap(undo_);
  undo_.clear();
  return true;
}

DragValidation ValidateDrag(const AssetDragPayload &payload, const ContentBrowserModel &model,
                            const std::filesystem::path &target, bool writable) noexcept {
  if (payload.type != AssetDragPayload::kType)
    return DragValidation::WrongType;
  if (payload.project_generation != model.ProjectGeneration())
    return DragValidation::StaleProject;
  if (!model.Find(payload.asset))
    return DragValidation::MissingAsset;
  if (!SafeRelative(target))
    return DragValidation::InvalidTarget;
  return writable ? DragValidation::Valid : DragValidation::ReadOnly;
}

bool AssetDependencyGraph::Set(runtime::AssetUuid asset,
                               std::span<const runtime::AssetUuid> dependencies) {
  if (asset == runtime::AssetUuid{} || std::ranges::find(dependencies, asset) != dependencies.end())
    return false;
  auto copy = std::vector(dependencies.begin(), dependencies.end());
  std::ranges::sort(copy, [](const auto &a, const auto &b) {
    return a.high < b.high || (a.high == b.high && a.low < b.low);
  });
  if (std::ranges::adjacent_find(copy) != copy.end())
    return false;
  edges_[asset] = std::move(copy);
  return true;
}
std::vector<runtime::AssetUuid> AssetDependencyGraph::Forward(runtime::AssetUuid asset) const {
  const auto found = edges_.find(asset);
  return found == edges_.end() ? std::vector<runtime::AssetUuid>{} : found->second;
}
std::vector<runtime::AssetUuid> AssetDependencyGraph::Reverse(runtime::AssetUuid asset) const {
  std::vector<runtime::AssetUuid> result;
  for (const auto &[source, dependencies] : edges_)
    if (std::ranges::find(dependencies, asset) != dependencies.end())
      result.push_back(source);
  return result;
}
std::vector<runtime::AssetUuid> AssetDependencyGraph::FindCycle() const {
  std::unordered_map<runtime::AssetUuid, int, runtime::AssetUuidHash> state;
  std::vector<runtime::AssetUuid> stack, cycle;
  std::function<bool(runtime::AssetUuid)> visit = [&](runtime::AssetUuid node) {
    state[node] = 1;
    stack.push_back(node);
    for (const auto dependency : Forward(node)) {
      if (state[dependency] == 1) {
        const auto start = std::ranges::find(stack, dependency);
        cycle.assign(start, stack.end());
        cycle.push_back(dependency);
        return true;
      }
      if (state[dependency] == 0 && visit(dependency))
        return true;
    }
    stack.pop_back();
    state[node] = 2;
    return false;
  };
  for (const auto &[node, unused] : edges_)
    if (state[node] == 0 && visit(node))
      break;
  return cycle;
}

ReimportTransaction::ReimportTransaction(std::uint64_t generation, runtime::AssetUuid asset,
                                         std::string previous_artifact)
    : generation_(generation), asset_(asset), artifact_(std::move(previous_artifact)) {}
bool ReimportTransaction::Stage(ReimportResult result) {
  if (result.project_generation != generation_ || result.asset != asset_ || result.cancelled ||
      result.source_hash.empty() || result.settings_hash.empty() || result.artifact_hash.empty()) {
    diagnostic_ = result.cancelled ? "reimport cancelled" : "invalid or stale reimport result";
    return false;
  }
  staged_ = std::move(result);
  return true;
}
bool ReimportTransaction::Commit(std::uint64_t current_generation, AssetDependencyGraph &graph) {
  if (!staged_ || current_generation != generation_) {
    diagnostic_ = "reimport completion is stale";
    return false;
  }
  auto candidate = graph;
  if (!candidate.Set(asset_, staged_->dependencies) || !candidate.FindCycle().empty()) {
    diagnostic_ = "asset dependency cycle";
    return false;
  }
  graph = std::move(candidate);
  artifact_ = staged_->artifact_hash;
  diagnostic_ = staged_->diagnostic;
  staged_.reset();
  return true;
}

void WatcherDebouncer::Push(FileEvent event) {
  if (!event.self_write)
    pending_[event.path.lexically_normal().generic_string()] = std::move(event);
}
std::vector<std::filesystem::path>
WatcherDebouncer::Flush(std::chrono::steady_clock::time_point now) {
  std::vector<std::filesystem::path> ready;
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (now - it->second.observed < delay_) {
      ++it;
      continue;
    }
    ready.push_back(it->second.path.lexically_normal());
    it = pending_.erase(it);
  }
  std::ranges::sort(ready);
  return ready;
}

bool DirtyConflictModel::Detect(runtime::AssetUuid asset, std::string editor_hash,
                                std::string disk_hash, bool dirty) {
  if (!dirty || editor_hash == disk_hash)
    return false;
  const auto found = std::ranges::find(conflicts_, asset, &DirtyConflict::asset);
  if (found == conflicts_.end())
    conflicts_.push_back({asset, std::move(editor_hash), std::move(disk_hash)});
  else
    *found = {asset, std::move(editor_hash), std::move(disk_hash)};
  return true;
}
bool DirtyConflictModel::Resolve(runtime::AssetUuid asset, DirtyConflictChoice choice) {
  const auto found = std::ranges::find(conflicts_, asset, &DirtyConflict::asset);
  if (found == conflicts_.end() || choice == DirtyConflictChoice::Pending)
    return false;
  found->choice = choice;
  return true;
}
const DirtyConflict *DirtyConflictModel::Find(runtime::AssetUuid asset) const {
  const auto found = std::ranges::find(conflicts_, asset, &DirtyConflict::asset);
  return found == conflicts_.end() ? nullptr : &*found;
}

} // namespace nexora::editor
