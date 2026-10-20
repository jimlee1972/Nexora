#include "Nexora/Editor/ContentBrowser.h"
#include "ContentDirectoryIndexTestAccess.h"

#include "ContentPathSortTestAccess.h"
#include "Nexora/Foundation/Types.h"
#if defined(NEXORA_HAS_2D)
#include "Nexora/Editor/TwoDAssetImport.h"
#endif

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <new>

#include <type_traits>

namespace nexora::editor {
namespace {
thread_local std::size_t fail_directory_after = std::numeric_limits<std::size_t>::max();
thread_local std::size_t fail_sort_after = std::numeric_limits<std::size_t>::max();
thread_local bool last_sort_accelerated{};
std::string PathUtf8(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return {text.begin(), text.end()};
}
void SortItems(std::vector<ContentItem> &items) {
  static_assert(std::is_nothrow_swappable_v<ContentItem>);
  const bool accelerated = [&] {
    if (items.size() < 2)
      return true;
    if (items.size() > ContentPathSortTestAccess::kMaximumItems)
      return false;
    try {
      struct Key final {
        std::string bytes;
        std::size_t source;
      };
      std::vector<Key> keys;
      keys.reserve(items.size());
      std::size_t remaining = ContentPathSortTestAccess::kMaximumKeyBytes;
      for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].path.native().size() > remaining)
          return false;
        auto bytes = PathUtf8(items[i].path);
        if (bytes.size() > remaining)
          return false;
        remaining -= bytes.size();
        if (keys.size() == fail_sort_after) {
          fail_sort_after = std::numeric_limits<std::size_t>::max();
          throw std::bad_alloc{};
        }
        keys.push_back({std::move(bytes), i});
      }
      std::ranges::sort(keys, {}, &Key::bytes);
      std::vector<std::size_t> destination(items.size());
      for (std::size_t i = 0; i < keys.size(); ++i)
        destination[keys[i].source] = i;
      // All storage/conversion is staged before item mutation. A source-to-destination
      // permutation then moves complete owning records using only noexcept swaps.
      for (std::size_t i = 0; i < items.size(); ++i)
        while (destination[i] != i) {
          const auto target = destination[i];
          std::swap(items[i], items[target]);
          std::swap(destination[i], destination[target]);
        }
      return true;
    } catch (const std::bad_alloc &) {
      return false;
    }
  }();
  last_sort_accelerated = accelerated;
  if (!accelerated)
    std::ranges::sort(items, {}, [](const ContentItem &item) { return PathUtf8(item.path); });
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
bool WithinTwoDBudget(std::span<const ContentItem> items, runtime::AssetUuid replacement = {},
                      const std::shared_ptr<const TwoDAssetPayload> &payload = {}) {
#if defined(NEXORA_HAS_2D)
  std::size_t remaining = kMaximumWorkspaceTwoDBytes, count{};
#endif
  for (const auto &item : items) {
    const auto &value = item.id == replacement && payload ? payload : item.twod;
    if (!value)
      continue;
#if defined(NEXORA_HAS_2D)
    if (value->Id() != item.id || value->Type() != item.type ||
        ++count > kMaximumWorkspaceTwoDAssets || value->RetainedBytes() > remaining)
      return false;
    remaining -= value->RetainedBytes();
#else
    return false;
#endif
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
bool ValidDiagnostic(const ContentItem &item) {
  return item.import_diagnostic.size() <= ContentItem::kMaximumImportDiagnosticBytes &&
         item.import_diagnostic.find('\0') == std::string::npos &&
         foundation::IsValidUtf8(item.import_diagnostic);
}
} // namespace

struct ContentBrowserModel::DirectoryIndex final {
  struct Folder final {
    std::vector<std::size_t> items;
    std::vector<Breadcrumb> children;
  };
  std::unordered_map<std::filesystem::path, Folder> folders;
};
void ContentDirectoryIndexTestAccess::FailAfterFolders(std::size_t count) noexcept {
  fail_directory_after = count;
}
bool ContentDirectoryIndexTestAccess::IsIndexed(const ContentBrowserModel &model) noexcept {
  return static_cast<bool>(model.directory_index_);
}
const ContentBrowserModel::DirectoryIndex *ContentBrowserModel::DirectoryIndexView() const {
  if (directory_index_attempted_)
    return directory_index_.get();
  directory_index_attempted_ = true;
  if (items_.size() > kMaximumDirectoryIndexItems)
    return nullptr;
  try {
    auto candidate = std::make_shared<DirectoryIndex>();
    std::size_t remaining = kMaximumDirectoryIndexPathWorkBytes;
    for (std::size_t slot = 0; slot < items_.size(); ++slot) {
      const auto &path = items_[slot].path;
      // Drive-relative/native-root peculiarities retain the existing filesystem semantics.
      if (path.has_root_path() || path.native().size() > remaining)
        return nullptr;
      const auto source_bytes = PathUtf8(path).size();
      if (source_bytes > remaining)
        return nullptr;
      remaining -= source_bytes;
      const auto parent = path.parent_path();
      if (parent.empty())
        continue; // No accepted nonempty folder can directly contain a root-level filename.
      std::size_t depth{};
      for (const auto &component : parent) {
        static_cast<void>(component);
        if (++depth > kMaximumDirectoryIndexDepth)
          return nullptr;
      }
      std::vector<std::filesystem::path> missing;
      for (auto current = parent; !current.empty() && !candidate->folders.contains(current);
           current = current.parent_path()) {
        if (missing.size() == kMaximumDirectoryIndexDepth ||
            missing.size() >= kMaximumDirectoryIndexFolders - candidate->folders.size())
          return nullptr;
        const auto bytes = PathUtf8(current).size();
        // Account source traversal plus key/child-path/label UTF-8 work. This is not total RSS.
        if (bytes > remaining / 3)
          return nullptr;
        remaining -= bytes * 3;
        missing.push_back(current);
      }
      // Parents first: every directory is linked exactly once, without per-item child
      // deduplication.
      for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
        if (candidate->folders.size() == fail_directory_after) {
          fail_directory_after = std::numeric_limits<std::size_t>::max();
          throw std::bad_alloc{};
        }
        candidate->folders.try_emplace(*it);
        const auto ancestor = candidate->folders.find(it->parent_path());
        if (ancestor != candidate->folders.end())
          ancestor->second.children.push_back({PathUtf8(it->filename()), it->lexically_normal()});
      }
      candidate->folders.at(parent).items.push_back(slot);
    }
    for (auto &[path, folder] : candidate->folders)
      std::ranges::sort(folder.children, {}, &Breadcrumb::label);
    directory_index_ = std::move(candidate);
    return directory_index_.get();
  } catch (const std::bad_alloc &) {
    // Never publish a partial index. Cache the fallback decision until source/order publication.
    return nullptr;
  }
}

void ContentPathSortTestAccess::FailAfterKeys(std::size_t count) noexcept {
  fail_sort_after = count;
}
bool ContentPathSortTestAccess::LastSortAccelerated() noexcept { return last_sort_accelerated; }

ContentBrowserModel::ContentBrowserModel(std::uint64_t project_generation)
    : generation_(project_generation) {
  SetFolder("Content");
}
bool ContentBrowserModel::Reset(std::span<const ContentItem> items,
                                std::uint64_t project_generation) {
  if (!WithinMeshBudget(items) || !WithinMaterialBudget(items) || !WithinTwoDBudget(items))
    return false;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> ids;
  std::unordered_set<std::string> paths;
  for (const auto &item : items)
    if (item.id == runtime::AssetUuid{} || !SafeRelative(item.path) || !ValidDiagnostic(item) ||
        !ids.insert(item.id).second || !paths.insert(PathUtf8(item.path)).second)
      return false;
  child_folders_.reset();
  visible_indices_.reset();
  InvalidateDirectoryIndex();
  items_.assign(items.begin(), items.end());
  SortItems(items_);
  selection_.clear();
  undo_.clear();
  undo_selection_.clear();
  generation_ = project_generation;
  ++revision_;
  return true;
}
bool ContentBrowserModel::Discover(ContentItem item) {
  if (item.id == runtime::AssetUuid{} || !SafeRelative(item.path) || !ValidDiagnostic(item) ||
      Find(item.id) || !ValidDestination(item.path))
    return false;
  auto next = items_;
  next.push_back(item);
  if (!WithinMeshBudget(next) || !WithinMaterialBudget(next) || !WithinTwoDBudget(next))
    return false;
  // Discovery is not an authoring command. Preserve the earlier command's Undo snapshot too.
  if (!undo_.empty()) {
    if (std::ranges::any_of(undo_, [&](const ContentItem &old) {
          return old.id == item.id || old.path == item.path;
        }))
      return false;
    auto previous = undo_;
    previous.push_back(item);
    if (!WithinMeshBudget(previous) || !WithinMaterialBudget(previous) ||
        !WithinTwoDBudget(previous))
      return false;
    SortItems(previous);
    undo_ = std::move(previous);
  }
  SortItems(next);
  child_folders_.reset();
  visible_indices_.reset();
  InvalidateDirectoryIndex();
  items_ = std::move(next);
  ++revision_;
  return true;
}
bool ContentBrowserModel::SetFolder(const std::filesystem::path &folder) {
  if (!SafeRelative(folder))
    return false;
  const auto normalized = folder.lexically_normal();
  if (folder_ != normalized) {
    child_folders_.reset();
    visible_indices_.reset();
  }
  folder_ = normalized;
  breadcrumbs_.clear();
  std::filesystem::path current;
  for (const auto &part : folder_) {
    current /= part;
    breadcrumbs_.push_back({PathUtf8(part), current});
  }
  return true;
}
bool ContentBrowserModel::RevealAsset(runtime::AssetUuid id) {
  const auto *item = Find(id);
  if (!item || !SafeRelative(item->path.parent_path()))
    return false;
  auto folder = item->path.parent_path();
  std::vector<Breadcrumb> breadcrumbs;
  std::filesystem::path current;
  for (const auto &part : folder) {
    current /= part;
    breadcrumbs.push_back({PathUtf8(part), current});
  }
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> selection{id};
  if (folder_ != folder)
    child_folders_.reset();
  if (folder_ != folder || !query_.empty() || !type_.empty())
    visible_indices_.reset();
  folder_.swap(folder);
  breadcrumbs_.swap(breadcrumbs);
  selection_.swap(selection);
  query_.clear();
  type_.clear();
  return true;
}
void ContentBrowserModel::SetFilter(std::string query, std::string type) {
  auto next_query = Lower(query), next_type = Lower(type);
  if (query_ != next_query || type_ != next_type) {
    visible_indices_.reset();
    query_.swap(next_query);
    type_.swap(next_type);
  }
}
bool ContentBrowserModel::MatchesVisible(const ContentItem &item) const {
  return item.path.parent_path() == folder_ &&
         (query_.empty() ||
          Lower(PathUtf8(item.path.filename())).find(query_) != std::string::npos) &&
         (type_.empty() || Lower(item.type) == type_);
}
void ContentBrowserModel::SelectVisible() {
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> selected;
  const auto &indices = VisibleIndices();
  selected.reserve(indices.size());
  for (const auto index : indices)
    selected.insert(items_[index].id);
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
const std::vector<std::size_t> &ContentBrowserModel::VisibleIndices() const {
  if (!visible_indices_) {
    std::vector<std::size_t> indices;
    if (const auto *directory = DirectoryIndexView()) {
      const auto folder = directory->folders.find(folder_);
      if (folder != directory->folders.end())
        for (const auto slot : folder->second.items)
          if (MatchesVisible(items_[slot]))
            indices.push_back(slot);
    } else {
      for (std::size_t index = 0; index < items_.size(); ++index)
        if (MatchesVisible(items_[index]))
          indices.push_back(index);
    }
    visible_indices_ = std::make_shared<const std::vector<std::size_t>>(std::move(indices));
  }
  return *visible_indices_;
}
std::vector<const ContentItem *> ContentBrowserModel::Visible(std::size_t offset,
                                                              std::size_t count) const {
  std::vector<const ContentItem *> matches;
  const auto &indices = VisibleIndices();
  if (offset >= indices.size())
    return matches;
  count = std::min(count, indices.size() - offset);
  matches.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    matches.push_back(&items_[indices[offset + i]]);
  return matches;
}
std::size_t ContentBrowserModel::VisibleCount() const { return VisibleIndices().size(); }
std::vector<Breadcrumb> ContentBrowserModel::ChildFolders() const { return *ChildFolderSnapshot(); }
std::shared_ptr<const std::vector<Breadcrumb>> ContentBrowserModel::ChildFolderSnapshot() const {
  if (child_folders_)
    return child_folders_;
  if (const auto *directory = DirectoryIndexView()) {
    const auto folder = directory->folders.find(folder_);
    // Retained public snapshots own just these children, not the whole project index.
    child_folders_ = folder == directory->folders.end()
                         ? std::make_shared<const std::vector<Breadcrumb>>()
                         : std::make_shared<const std::vector<Breadcrumb>>(folder->second.children);
    return child_folders_;
  }
  std::vector<Breadcrumb> folders;
  std::unordered_set<std::string> seen;
  for (const auto &item : items_) {
    const auto relative = item.path.lexically_relative(folder_);
    if (relative.empty() || relative == ".")
      continue;
    auto part = relative.begin();
    const auto name = *part;
    if (name == "..")
      continue;
    const auto label = PathUtf8(name);
    if (++part == relative.end())
      continue;
    const auto path = (folder_ / name).lexically_normal();
    if (seen.insert(PathUtf8(path)).second)
      folders.push_back({label, path});
  }
  std::ranges::sort(folders, {}, &Breadcrumb::label);
  child_folders_ = std::make_shared<const std::vector<Breadcrumb>>(std::move(folders));
  return child_folders_;
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
  child_folders_.reset();
  visible_indices_.reset();
  InvalidateDirectoryIndex();
  items_ = std::move(next);
  SortItems(items_);
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
  child_folders_.reset();
  visible_indices_.reset();
  InvalidateDirectoryIndex();
  items_.swap(undo_);
  undo_.clear();
  selection_.swap(undo_selection_);
  undo_selection_.clear();
  return true;
}
bool ContentBrowserModel::PublishArtifact(runtime::AssetUuid id, std::string artifact_hash,
                                          ThumbnailState thumbnail, std::string *error,
                                          std::shared_ptr<const MeshGeometry> mesh,
                                          std::shared_ptr<const MaterialAsset> material,
                                          std::shared_ptr<const TwoDAssetPayload> twod) {
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
  if (!WithinTwoDBudget(items_, id, twod) || !WithinTwoDBudget(undo_, id, twod)) {
    if (error)
      *error = "Live or retained Undo 2D payloads exceed their budget or identity/type contract.";
    return false;
  }
  ++revision_;
  if (mesh)
    found->mesh = std::move(mesh);
  if (material)
    found->material = std::move(material);
  if (twod)
    found->twod = std::move(twod);
  found->artifact_hash = std::move(artifact_hash);
  found->thumbnail = thumbnail;
  if (thumbnail == ThumbnailState::Ready)
    found->import_diagnostic.clear();
  const auto undo = std::ranges::find(undo_, id, &ContentItem::id);
  if (undo != undo_.end()) {
    undo->artifact_hash = found->artifact_hash;
    undo->thumbnail = thumbnail;
    undo->import_diagnostic = found->import_diagnostic;
    undo->mesh = found->mesh;
    undo->material = found->material;
    undo->twod = found->twod;
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
