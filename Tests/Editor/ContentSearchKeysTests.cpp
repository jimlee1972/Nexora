#include "../../Engine/Editor/src/ContentDirectoryIndexTestAccess.h"
#include "../../Engine/Editor/src/ContentSearchKeysTestAccess.h"
#include "Nexora/Editor/ContentBrowser.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <clocale>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Model = editor::ContentBrowserModel;
using Access = editor::ContentSearchKeysTestAccess;
using Item = editor::ContentItem;
using Id = runtime::AssetUuid;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path &path) {
  const auto bytes = path.generic_u8string();
  return {bytes.begin(), bytes.end()};
}
std::string Lower(std::string value) {
  for (auto &c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
Item Asset(std::uint64_t id, const std::filesystem::path &folder) {
  Item item;
  item.id = {19, id};
  item.path = folder / ("Asset_" + std::to_string(id) + "_Tag" + std::to_string(id % 7) + ".OBJ");
  item.type = id % 2 ? ".Mesh" : "TEXTURE";
  item.artifact_hash = "preserved-" + std::to_string(id);
  item.import_diagnostic = "owned source observation";
  return item;
}
std::vector<Item> Sources() {
  std::vector<Item> items;
  for (std::size_t i = 0; i < 3072; ++i)
    items.push_back(Asset(i + 1, i < 2048 ? "Content/A" : "Content/B"));
  items[0].path = std::filesystem::path(u8"Content/A/À-編輯%s.OBJ");
#if !defined(_WIN32)
  items[1].path = "Content/A/raw-" + std::string(1, static_cast<char>(0xff)) + ".OBJ";
#endif
  for (std::size_t i = 0; i < 8; ++i)
    items.push_back(Asset(4000 + i, "Content"));
  std::ranges::reverse(items);
  return items;
}
std::vector<Id> Reference(const Model &model, const std::filesystem::path &folder,
                          std::string query, std::string type) {
  query = Lower(std::move(query));
  type = Lower(std::move(type));
  std::vector<Id> result;
  for (const auto &item : model.Items()) {
    if (item.path.parent_path() == folder &&
        (query.empty() || Lower(Utf8(item.path.filename())).find(query) != std::string::npos) &&
        (type.empty() || Lower(item.type) == type))
      result.push_back(item.id);
  }
  return result;
}
void Check(Model &model, const std::filesystem::path &folder, std::string query,
           std::string type = {}) {
  Require(model.SetFolder(folder), "Folder admission failed");
  const auto expected = Reference(model, folder, query, type);
  model.SetFilter(std::move(query), std::move(type));
  const auto revision = model.Revision();
  const auto selected = model.Selection();
  Require(model.VisibleCount() == expected.size(), "Complete filter count changed");
  const auto rows = model.Visible(0, model.Items().size());
  Require(rows.size() == expected.size(), "Complete filter lost source rows");
  for (std::size_t i = 0; i < rows.size(); ++i)
    Require(rows[i]->id == expected[i], "Filter order/identity differs from legacy oracle");
  if (expected.size() > 5) {
    const auto clipped = model.Visible(2, 3);
    Require(clipped.size() == 3, "Clip range changed");
    for (std::size_t i = 0; i < clipped.size(); ++i)
      Require(clipped[i]->id == expected[i + 2], "Clipped identity/order changed");
  }
  Require(model.Revision() == revision && model.Selection() == selected,
          "Read-only projection changed source revision or selection");
}
void Semantics() {
  Model model;
  const auto items = Sources();
  Require(model.Reset(items, 71), "Reset failed");
  for (const auto *query : {"TAG0", ".obj", "%s", "absent", ""})
    for (const auto *type : {"", ".MESH", "texture", "absent"})
      Check(model, "Content/A", query, type);
  Require(Access::IsCached(model) && Access::Rows(model) == 2048 &&
              Access::LogicalBytes(model) <= Access::MaximumBytes(),
          "Cache admission/budget invalid");
  Check(model, "Content/A", "TAG1");
  Access::FailAfterKeys(0);
  Check(model, "Content/A", "TAG2"); // Reuse must not consume the new-build fault.
  Require(Access::IsCached(model), "Changing only a filter rebuilt the source keys");
  Check(model, "Content/B", "TAG2");
  Require(!Access::IsCached(model) && Access::Rows(model) == 0,
          "Folder transition published partial or stale keys");
  Access::FailAfterKeys(std::numeric_limits<std::size_t>::max());
  Check(model, "Content/B", "TAG3");
  Require(!Access::IsCached(model), "Failed admission retried on every keystroke");
  Check(model, "Content", "asset");
  Require(!Access::IsCached(model), "Small folder allocated a full key cache");
  Check(model, "Content/B", "TAG0");
  Require(Access::IsCached(model), "Fresh scope did not recover optional acceleration");
  const auto id = model.Visible(0, 1).front()->id;
  model.SelectVisible();
  Require(model.Selection().size() == model.VisibleCount(), "Select All lost filtered rows");
  Require(model.Select(id), "Selection failed");
  const auto range = model.Visible(0, 8);
  Require(model.SelectVisibleRange(range[2]->id, range[5]->id) && model.Selection().size() == 4,
          "Filtered range selection lost complete ordering");
  editor::AssetDragPayload drag;
  drag.project_generation = model.ProjectGeneration();
  drag.asset = id;
  Require(editor::ValidateDrag(drag, model, "Content/A", true) == editor::DragValidation::Valid &&
              editor::ValidateDrag(drag, model, "Content/A", false) ==
                  editor::DragValidation::ReadOnly,
          "Cache changed drag identity or read-only authority");
  --drag.project_generation;
  Require(editor::ValidateDrag(drag, model, "Content/A", true) ==
              editor::DragValidation::StaleProject,
          "Cache accepted a stale project drag");
  Require(model.ProjectGeneration() == 71, "Filtering changed project generation");
}
void LifetimeAndTransactions() {
  Model survivor;
  {
    Model original;
    Require(original.Reset(Sources(), 72), "Reset failed");
    Check(original, "Content/A", "asset");
    survivor = original;
  }
  Check(survivor, "Content/A", "tag1");
  Require(Access::IsCached(survivor), "Copied owning cache did not survive original teardown");
  const Id id{19, 3};
  const auto original_path = survivor.Find(id)->path;
  Require(survivor.Select(id), "Select failed");
  Require(survivor.Rename(id, "renamed.obj"), "Rename failed");
  Require(!Access::IsCached(survivor), "Rename retained stale interpretation");
  Check(survivor, "Content/A", "RENAMED");
  auto discovered = Asset(9999, "Content/A");
  Require(survivor.Discover(discovered), "Discovery failed");
  Check(survivor, "Content/A", "renamed");
  Require(survivor.PublishArtifact(id, "current-reimport", editor::ThumbnailState::Ready),
          "Artifact publication failed");
  Check(survivor, "Content/A", "renamed");
  Require(Access::IsCached(survivor) &&
              survivor.Visible(0, 1).front()->artifact_hash == "current-reimport",
          "Source-only cache retained a stale artifact");
  Require(survivor.Undo() && survivor.Find(id)->path == original_path &&
              survivor.Find(id)->artifact_hash == "current-reimport" &&
              survivor.Find(discovered.id),
          "Undo lost original identity, discovery or current reimport");
  Check(survivor, "Content/A", "tag3");
  const std::array moved{id};
  Require(survivor.Move(moved, "Content/B"), "Move failed");
  Check(survivor, "Content/A", "asset");
  Check(survivor, "Content/B", "asset");
  Require(survivor.Undo(), "Move Undo failed");
  Check(survivor, "Content/A", "asset");
  Require(survivor.Delete(moved), "Delete failed");
  Check(survivor, "Content/A", "asset");
  Require(survivor.Undo() && survivor.Find(id), "Delete Undo lost source");
  Require(survivor.Reset(Sources(), 73), "Replacement Reset failed");
  Check(survivor, "Content/B", "tag2");
  Require(survivor.ProjectGeneration() == 73, "Replacement generation changed");
}
void Fallback() {
  for (const auto fail : {std::size_t{0}, std::size_t{1}, Access::MinimumRows() - 1}) {
    Model model;
    Require(model.Reset(Sources(), 81), "Reset failed");
    Access::FailAfterKeys(fail);
    Check(model, "Content/A", "asset", ".mesh");
    Require(!Access::IsCached(model) && Access::LogicalBytes(model) == 0,
            "Allocation failure published a partial cache");
  }
  Access::FailAfterKeys(std::numeric_limits<std::size_t>::max());
  Model linear;
  Require(linear.Reset(Sources(), 82), "Reset failed");
  editor::ContentDirectoryIndexTestAccess::FailAfterFolders(0);
  Check(linear, "Content/A", "tag2");
  Require(!Access::IsCached(linear), "Unavailable directory index fabricated membership");
  auto oversized = Sources();
  const std::string type(Access::MaximumBytes() / 2048 + 8, 'T');
  for (auto &item : oversized)
    item.type = type;
  Model budget;
  Require(budget.Reset(oversized, 83), "Budget source Reset failed");
  Check(budget, "Content/A", "asset");
  Require(!Access::IsCached(budget) && budget.Items().size() == oversized.size(),
          "Budget fallback truncated source rows");
}
void Locale() {
  const auto *current = std::setlocale(LC_CTYPE, nullptr);
  const std::string saved = current ? current : "C";
  struct Restore {
    std::string name;
    ~Restore() { std::setlocale(LC_CTYPE, name.c_str()); }
  } restore{saved};
  Require(std::setlocale(LC_CTYPE, "C"), "C locale unavailable");
  Model model;
  Require(model.Reset(Sources(), 91), "Reset failed");
  const auto query = Utf8(std::filesystem::path(u8"À"));
  Check(model, "Content/A", query);
  for (const auto *locale :
       {"en_US.ISO8859-1", "English_United States.1252", "French_France.1252", "C.UTF-8"})
    if (std::setlocale(LC_CTYPE, locale))
      Check(model, "Content/A", query);
}
} // namespace
int main() {
  Semantics();
  LifetimeAndTransactions();
  Fallback();
  Locale();
  std::cout << "Content search keys: complete legacy semantics, owning copies, transactions and "
               "fallback passed\n";
}
