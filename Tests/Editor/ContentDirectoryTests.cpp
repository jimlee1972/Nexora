#include "../../Engine/Editor/src/ContentDirectoryIndexTestAccess.h"
#include "Nexora/Editor/ContentBrowser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Model = editor::ContentBrowserModel;
using Access = editor::ContentDirectoryIndexTestAccess;
using Item = editor::ContentItem;
using Id = runtime::AssetUuid;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return {value.begin(), value.end()};
}
std::string Lower(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value;
}
Item Asset(std::uint64_t id, std::filesystem::path path, std::string type = "Mesh") {
  Item result;
  result.id = {1, id};
  result.path = std::move(path);
  result.type = std::move(type);
  result.artifact_hash = "original";
  return result;
}
std::vector<Item> Sources() {
  std::vector<Item> result{Asset(1, "Content/Root.obj"),
                           Asset(2, "Content/A/a.obj"),
                           Asset(3, std::filesystem::path(u8"Content/A/子/編輯.png"), "Texture"),
                           Asset(4, "Content/Aardvark/x.obj"),
                           Asset(5, "Content/B/z.obj"),
                           Asset(6, "Other/T/o.obj"),
                           Asset(7, std::filesystem::path(u8"Content/A/百分%/x.obj")),
                           Asset(8, std::filesystem::path(u8"Content/百分%s.png"), "Texture"),
                           Asset(9, "Content/Folderfile", "Binary"),
                           Asset(10, "Content/Folderfile/nested.obj"),
                           Asset(11, "loose.obj"),
                           Asset(12, "Content//repeat/r.obj"),
                           Asset(14, "Content/A/b.obj"),
                           Asset(15, "Content/A/c.png", "Texture")};
#if !defined(_WIN32)
  result.push_back(Asset(13, "Content/raw-" + std::string(1, static_cast<char>(0xff)) + "/x.obj"));
#endif
  std::ranges::reverse(result);
  return result;
}
// The original complete linear semantics are an independent oracle for acceleration.
std::vector<editor::Breadcrumb> ReferenceChildren(const Model &model,
                                                  const std::filesystem::path &folder) {
  std::vector<editor::Breadcrumb> result;
  std::unordered_set<std::string> seen;
  for (const auto &item : model.Items()) {
    const auto relative = item.path.lexically_relative(folder);
    if (relative.empty() || relative == ".")
      continue;
    auto part = relative.begin();
    const auto name = *part;
    if (name == ".." || ++part == relative.end())
      continue;
    const auto path = (folder / name).lexically_normal();
    if (seen.insert(Utf8(path)).second)
      result.push_back({Utf8(name), path});
  }
  std::ranges::sort(result, {}, &editor::Breadcrumb::label);
  return result;
}
void SameChildren(std::span<const editor::Breadcrumb> actual,
                  std::span<const editor::Breadcrumb> expected) {
  Require(actual.size() == expected.size(), "Immediate directory count differs from full source");
  for (std::size_t i = 0; i < actual.size(); ++i)
    Require(actual[i].label == expected[i].label &&
                actual[i].path.native() == expected[i].path.native(),
            "Directory label/native path/order changed");
}
void Check(Model &model, const std::filesystem::path &folder, std::string query = {},
           std::string type = {}) {
  Require(model.SetFolder(folder), "Folder query rejected");
  model.SetFilter(query, type);
  const auto normalized = folder.lexically_normal();
  const auto children = model.ChildFolderSnapshot();
  SameChildren(*children, ReferenceChildren(model, normalized));
  query = Lower(std::move(query));
  type = Lower(std::move(type));
  std::vector<const Item *> expected;
  for (const auto &item : model.Items())
    if (item.path.parent_path() == normalized &&
        (query.empty() || Lower(Utf8(item.path.filename())).find(query) != std::string::npos) &&
        (type.empty() || Lower(item.type) == type))
      expected.push_back(&item);
  Require(model.VisibleCount() == expected.size(), "Visible count omitted/mixed source assets");
  for (const auto offset :
       {std::size_t{0}, std::size_t{1}, std::numeric_limits<std::size_t>::max()})
    for (const auto count :
         {std::size_t{0}, std::size_t{1}, std::numeric_limits<std::size_t>::max()}) {
      const auto actual = model.Visible(offset, count);
      const auto size = offset >= expected.size() ? 0 : std::min(count, expected.size() - offset);
      Require(actual.size() == size, "Clipped page range lost bounds");
      for (std::size_t i = 0; i < size; ++i)
        Require(actual[i] == expected[offset + i],
                "Page order or model-local item ownership changed");
    }
  const auto revision = model.Revision();
  model.SelectVisible();
  std::vector<Id> ids;
  for (const auto *item : expected)
    ids.push_back(item->id);
  std::ranges::sort(ids, {}, &Id::low);
  Require(model.Selection() == ids && model.Revision() == revision &&
              model.ChildFolderSnapshot() == children,
          "Full selection changed source revision or filter-independent folder ownership");
}
void QueriesAndLifetime() {
  Model model;
  const auto source = Sources();
  Require(model.Reset(source, 41), "Source admission failed");
  for (const auto *folder : {"Content", "Content/A", "Content/Aardvark", "Content/B", "Other",
                             "Other/T", "Missing/nowhere", "Content/Folderfile", "Content/repeat"})
    for (const auto *query : {"", "A", ".obj", "not-found", "%"})
      for (const auto *type : {"", "MESH", "Texture"})
        Check(model, folder, query, type);
  Check(model, std::filesystem::path(u8"Content/A/子"), "編");
  Require(Access::IsIndexed(model) && !model.Undo(), "Normal directory queries lost index/history");
  Check(model, "Content/A");
  const auto visible = model.Visible(0, 100);
  Require(visible.size() == 3 &&
              model.SelectVisibleRange(visible.back()->id, visible.front()->id) &&
              model.Selection().size() == 3,
          "Indexed reverse range selection lost unclipped rows");
  Model copy = model;
  Require(copy.Visible(0, 100).front() != model.Visible(0, 100).front(),
          "Copied directory index retained source item pointers");
  const auto retained = model.ChildFolderSnapshot();
  const auto old_children = *retained;
  Require(model.Rename({1, 2}, "renamed.obj"), "Rename failed");
  Check(model, "Content/A");
  Check(copy, "Content/A");
  Require(copy.Find({1, 2})->path.filename() == "a.obj", "Copy shared mutable source paths");
  Require(model.Discover(Asset(16, "Content/A/discovered.obj")) && model.Undo() &&
              model.Find({1, 2})->path.filename() == "a.obj" && model.Find({1, 16}),
          "Index invalidation damaged discovery or earlier authoring Undo");
  Check(model, "Content/A");
  const std::array moved{Id{1, 2}};
  Require(model.Move(moved, "Content/B"), "Move failed");
  Check(model, "Content/A");
  Check(model, "Content/B");
  Require(model.Undo(), "Move Undo failed");
  Check(model, "Content/A");
  const std::array removed{Id{1, 4}};
  Require(model.Delete(removed), "Delete failed");
  Check(model, "Content");
  Require(model.Undo(), "Delete Undo failed");
  Check(model, "Content");
  const auto artifact_folders = model.ChildFolderSnapshot();
  Require(model.PublishArtifact({1, 1}, "fresh", editor::ThumbnailState::Ready) &&
              model.ChildFolderSnapshot() == artifact_folders && Access::IsIndexed(model),
          "Artifact-only publication revoked path indexing");
  const auto fresh_rows = model.Visible(0, 100);
  const auto fresh =
      std::ranges::find_if(fresh_rows, [](const Item *item) { return item->id == Id{1, 1}; });
  Require(fresh != fresh_rows.end() && (*fresh)->artifact_hash == "fresh" &&
              *fresh == model.Find({1, 1}),
          "Indexed rows did not resolve current artifact data");
  const auto payload =
      editor::AssetDragPayload{std::string(editor::AssetDragPayload::kType), 41, {1, 1}};
  auto replacement = Sources();
  for (auto &item : replacement)
    item.path = std::filesystem::path("Content/Replaced") / std::to_string(item.id.low);
  Require(model.Reset(replacement, 42), "Same-count replacement failed");
  Check(model, "Content");
  Check(model, "Content/Replaced");
  Require(editor::ValidateDrag(payload, model, "Content", true) ==
              editor::DragValidation::StaleProject,
          "Indexed source replacement revived stale drag authority");
  SameChildren(*retained, old_children);
  std::shared_ptr<const std::vector<editor::Breadcrumb>> orphan;
  {
    Model transient = copy;
    transient.SetFolder("Content");
    orphan = transient.ChildFolderSnapshot();
  }
  const auto orphan_children = *orphan;
  copy = Model{};
  SameChildren(*orphan, orphan_children);
}
void AllocationFallback() {
  Model model;
  Require(model.Reset(Sources(), 7), "Fault fixture admission failed");
  Access::FailAfterFolders(1);
  Check(model, "Content");
  Check(model, "Content/A");
  Require(!Access::IsIndexed(model), "Partial allocation-failed index was published");
  Access::FailAfterFolders(std::numeric_limits<std::size_t>::max());
  Require(model.Discover(Asset(99, "Content/New/asset.obj")), "Post-failure discovery failed");
  Check(model, "Content");
  Check(model, "Content/New");
  Require(Access::IsIndexed(model),
          "Fresh source did not recover indexing after allocation failure");
}
void DepthAndWorkLimits() {
  std::filesystem::path parent = "Content";
  for (std::size_t i = 1; i < Model::kMaximumDirectoryIndexDepth; ++i)
    parent /= "d";
  Model model;
  auto source = Asset(1, parent / "a.obj");
  Require(model.Reset(std::array{source}, 1), "Depth-boundary source rejected");
  Check(model, parent);
  Require(Access::IsIndexed(model), "Maximum directory depth was not admitted");
  parent /= "d";
  source.path = parent / "a.obj";
  Require(model.Reset(std::array{source}, 2), "Over-depth source was removed");
  Check(model, parent);
  Require(!Access::IsIndexed(model), "Over-depth index was admitted");
  source.path = std::filesystem::path("Content") /
                std::string(Model::kMaximumDirectoryIndexPathWorkBytes, 'x');
  Require(model.Reset(std::array{source}, 3), "Oversized metadata path was dropped");
  Require(model.VisibleCount() == 0, "Old deep folder unexpectedly contained the replacement");
  model.SetFolder("Content");
  Require(model.VisibleCount() == 1 && !Access::IsIndexed(model),
          "Path-work fallback omitted source or exceeded admission");
#if defined(_WIN32)
  source.path = "C:Content/a.obj";
  Require(model.Reset(std::array{source}, 4) && model.SetFolder("C:Content") &&
              model.VisibleCount() == 1 && !Access::IsIndexed(model),
          "Drive-relative fallback changed native source semantics");
#endif
}
void CountLimits() {
  {
    std::vector<Item> source;
    source.reserve(Model::kMaximumDirectoryIndexItems + 1);
    for (std::size_t i = 0; i <= Model::kMaximumDirectoryIndexItems; ++i)
      source.push_back(Asset(i + 1, std::filesystem::path("Content") / std::to_string(i)));
    Model model;
    Require(model.Reset(std::span(source).first(source.size() - 1), 1),
            "Item limit source rejected");
    Require(model.VisibleCount() == Model::kMaximumDirectoryIndexItems && Access::IsIndexed(model),
            "Maximum indexed item count rejected");
    Require(model.Reset(source, 2) && model.VisibleCount() == source.size() &&
                !Access::IsIndexed(model),
            "Over-item-budget fallback omitted source or retained an index");
  }
  {
    std::vector<Item> source;
    for (std::size_t i = 0; i < Model::kMaximumDirectoryIndexFolders; ++i)
      source.push_back(
          Asset(i + 1, std::filesystem::path("Content") / ("D" + std::to_string(i)) / "a.obj"));
    Model model;
    Require(model.Reset(std::span(source).first(source.size() - 1), 3),
            "Folder limit source rejected");
    Require(model.ChildFolderSnapshot()->size() == source.size() - 1 && Access::IsIndexed(model),
            "Maximum directory count rejected");
    Require(model.Reset(source, 4) && model.ChildFolderSnapshot()->size() == source.size() &&
                !Access::IsIndexed(model),
            "Over-directory-budget fallback omitted navigation or retained a partial index");
  }
}
} // namespace
int main() {
  try {
    QueriesAndLifetime();
    AllocationFallback();
    DepthAndWorkLimits();
    CountLimits();
    std::cout
        << "Complete directory queries, ownership, source mutations and bounded fallback passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
