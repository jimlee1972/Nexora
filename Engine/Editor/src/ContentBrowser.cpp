#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Foundation/Types.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace nexora::editor {
namespace {
std::string PathUtf8(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return {text.begin(), text.end()};
}
bool WithinMeshBudget(std::span<const ContentItem> items, runtime::AssetUuid replacement = {},
                      const std::shared_ptr<const MeshGeometry> &mesh = {}) {
  std::size_t remaining = kMaximumWorkspaceMeshBytes;
  for (const auto &item : items) {
    const auto &geometry = item.id == replacement && mesh ? mesh : item.mesh;
    if (!geometry)
      continue;
    if (geometry->vertices.capacity() > remaining / sizeof(MeshVertex))
      return false;
    remaining -= geometry->vertices.capacity() * sizeof(MeshVertex);
    if (geometry->indices.capacity() > remaining / sizeof(std::uint16_t))
      return false;
    remaining -= geometry->indices.capacity() * sizeof(std::uint16_t);
  }
  return true;
}
bool WithinMaterialBudget(std::span<const ContentItem> items, runtime::AssetUuid replacement = {},
                          const std::shared_ptr<const MaterialAsset> &material = {}) {
  std::size_t count{};
  for (const auto &item : items) {
    const auto &value = item.id == replacement && material ? material : item.material;
    if (value && ++count > kMaximumWorkspaceMaterials)
      return false;
  }
  return true;
}
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
  if (!WithinMeshBudget(items) || !WithinMaterialBudget(items))
    return false;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> ids;
  std::unordered_set<std::string> paths;
  for (const auto &item : items)
    if (item.id == runtime::AssetUuid{} || !SafeRelative(item.path) ||
        !ids.insert(item.id).second || !paths.insert(PathUtf8(item.path)).second)
      return false;
  items_.assign(items.begin(), items.end());
  std::ranges::sort(items_, {}, [](const ContentItem &item) { return PathUtf8(item.path); });
  selection_.clear();
  undo_.clear();
  undo_selection_.clear();
  generation_ = project_generation;
  ++revision_;
  return true;
}
bool ContentBrowserModel::Discover(ContentItem item) {
  if (item.id == runtime::AssetUuid{} || !SafeRelative(item.path) || Find(item.id) ||
      !ValidDestination(item.path))
    return false;
  auto next = items_;
  next.push_back(item);
  if (!WithinMeshBudget(next) || !WithinMaterialBudget(next))
    return false;
  // Discovery is not an authoring command. Preserve the earlier command's Undo snapshot too.
  if (!undo_.empty()) {
    if (std::ranges::any_of(undo_, [&](const ContentItem &old) {
          return old.id == item.id || old.path == item.path;
        }))
      return false;
    auto previous = undo_;
    previous.push_back(item);
    if (!WithinMeshBudget(previous) || !WithinMaterialBudget(previous))
      return false;
    std::ranges::sort(previous, {}, [](const ContentItem &value) { return PathUtf8(value.path); });
    undo_ = std::move(previous);
  }
  std::ranges::sort(next, {}, [](const ContentItem &value) { return PathUtf8(value.path); });
  items_ = std::move(next);
  ++revision_;
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
    breadcrumbs_.push_back({PathUtf8(part), current});
  }
  return true;
}
void ContentBrowserModel::SetFilter(std::string query, std::string type) {
  query_ = Lower(query);
  type_ = Lower(type);
}
bool ContentBrowserModel::MatchesVisible(const ContentItem &item) const {
  return item.path.parent_path() == folder_ &&
         (query_.empty() ||
          Lower(PathUtf8(item.path.filename())).find(query_) != std::string::npos) &&
         (type_.empty() || Lower(item.type) == type_);
}
void ContentBrowserModel::SelectVisible() {
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> selected;
  for (const auto &item : items_)
    if (MatchesVisible(item))
      selected.insert(item.id);
  selection_ = std::move(selected);
}
bool ContentBrowserModel::SelectVisibleRange(runtime::AssetUuid first, runtime::AssetUuid last) {
  const auto visible = Visible(0, items_.size());
  auto begin = std::ranges::find(visible, first, [](const ContentItem *item) { return item->id; });
  auto end = std::ranges::find(visible, last, [](const ContentItem *item) { return item->id; });
  if (begin == visible.end() || end == visible.end())
    return false;
  if (begin > end)
    std::swap(begin, end);
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> selected;
  selected.reserve(static_cast<std::size_t>(end - begin) + 1);
  for (auto row = begin; row != end + 1; ++row)
    selected.insert((*row)->id);
  selection_ = std::move(selected);
  return true;
}
std::vector<const ContentItem *> ContentBrowserModel::Visible(std::size_t offset,
                                                              std::size_t count) const {
  std::vector<const ContentItem *> matches;
  for (const auto &item : items_) {
    if (!MatchesVisible(item))
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
std::size_t ContentBrowserModel::VisibleCount() const {
  std::size_t count = 0;
  for (const auto &item : items_) {
    if (MatchesVisible(item))
      ++count;
  }
  return count;
}
std::vector<Breadcrumb> ContentBrowserModel::ChildFolders() const {
  std::vector<Breadcrumb> folders;
  std::unordered_set<std::string> seen;
  for (const auto &item : items_) {
    const auto relative = item.path.lexically_relative(folder_);
    if (relative.empty() || relative == ".")
      continue;
    auto part = relative.begin();
    const auto name = *part;
    const auto label = PathUtf8(name);
    if (++part == relative.end())
      continue;
    const auto path = (folder_ / name).lexically_normal();
    if (seen.insert(PathUtf8(path)).second)
      folders.push_back({label, path});
  }
  std::ranges::sort(folders, {}, &Breadcrumb::label);
  return folders;
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
  ++revision_;
  undo_ = items_;
  undo_selection_ = selection_;
  items_ = std::move(next);
  std::ranges::sort(items_, {}, [](const ContentItem &item) { return PathUtf8(item.path); });
  if (error)
    error->clear();
  return true;
}
bool ContentBrowserModel::Rename(runtime::AssetUuid id, std::string_view filename,
                                 std::string *error) {
  auto next = items_;
  auto found = std::ranges::find(next, id, &ContentItem::id);
  if (!foundation::IsValidUtf8(filename))
    return false;
  const std::filesystem::path name(std::u8string(filename.begin(), filename.end()));
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
    if (!ValidDestination(destination, id) || !destinations.insert(PathUtf8(destination)).second) {
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
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> requested;
  for (const auto id : ids)
    if (!requested.insert(id).second)
      return false;
  std::vector<ContentItem> next;
  next.reserve(items_.size());
  std::size_t found = 0;
  for (const auto &item : items_) {
    if (requested.contains(item.id))
      ++found;
    else
      next.push_back(item);
  }
  if (found != requested.size())
    return false;
  if (!Commit(std::move(next), error))
    return false;
  for (const auto id : ids)
    selection_.erase(id);
  return true;
}
bool ContentBrowserModel::Undo() {
  if (undo_.empty())
    return false;
  ++revision_;
  items_.swap(undo_);
  undo_.clear();
  selection_.swap(undo_selection_);
  undo_selection_.clear();
  return true;
}
bool ContentBrowserModel::PublishArtifact(runtime::AssetUuid id, std::string artifact_hash,
                                          ThumbnailState thumbnail, std::string *error,
                                          std::shared_ptr<const MeshGeometry> mesh,
                                          std::shared_ptr<const MaterialAsset> material) {
  const auto found = std::ranges::find(items_, id, &ContentItem::id);
  if (found == items_.end() || artifact_hash.empty()) {
    if (error)
      *error = "asset or artifact hash is invalid";
    return false;
  }
  if (!WithinMeshBudget(items_, id, mesh)) {
    if (error)
      *error = "Live mesh geometry exceeds the 128 MiB workspace budget.";
    return false;
  }
  if (!WithinMaterialBudget(items_, id, material) || !WithinMaterialBudget(undo_, id, material)) {
    if (error)
      *error = "Live or retained Undo scalar PBR materials exceed the 4096 asset budget.";
    return false;
  }
  ++revision_;
  if (mesh)
    found->mesh = std::move(mesh);
  if (material)
    found->material = std::move(material);
  found->artifact_hash = std::move(artifact_hash);
  found->thumbnail = thumbnail;
  const auto undo = std::ranges::find(undo_, id, &ContentItem::id);
  if (undo != undo_.end()) {
    undo->artifact_hash = found->artifact_hash;
    undo->thumbnail = thumbnail;
    undo->mesh = found->mesh;
    undo->material = found->material;
  }
  if (error)
    error->clear();
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
    pending_[PathUtf8(event.path.lexically_normal())] = std::move(event);
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
