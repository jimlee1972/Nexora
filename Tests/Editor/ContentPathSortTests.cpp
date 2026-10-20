#include "../../Engine/Editor/src/ContentPathSortTestAccess.h"
#include "Nexora/Editor/ContentBrowser.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Item = editor::ContentItem;
using Model = editor::ContentBrowserModel;
using Access = editor::ContentPathSortTestAccess;
using Id = runtime::AssetUuid;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path &path) {
  const auto bytes = path.generic_u8string();
  return {bytes.begin(), bytes.end()};
}
Item Asset(std::uint64_t id, std::filesystem::path path) {
  Item item;
  item.id = {44, id};
  item.path = std::move(path);
  item.type = "Unknown.Plugin.Binary";
  item.artifact_hash = "opaque-artifact-" + std::to_string(id);
  item.thumbnail = static_cast<editor::ThumbnailState>(id % 3);
  item.import_diagnostic = "Opaque %s metadata\n" + std::to_string(id);
  return item;
}
void Same(const Item &actual, const Item &expected) {
  Require(actual.id == expected.id && actual.path.native() == expected.path.native() &&
              actual.type == expected.type && actual.artifact_hash == expected.artifact_hash &&
              actual.thumbnail == expected.thumbnail && actual.mesh == expected.mesh &&
              actual.material == expected.material && actual.twod == expected.twod &&
              actual.import_diagnostic == expected.import_diagnostic,
          "Sorting changed an owning source record, payload identity or raw metadata");
}
void Check(const Model &model, std::span<const Item> source) {
  // The original UTF-8 projection comparator is the semantic reference, independent of the
  // new cached-key permutation. Include the complete payload/metadata, not just row names.
  std::vector<Item> expected(source.begin(), source.end());
  std::ranges::sort(expected, {}, [](const Item &item) { return Utf8(item.path); });
  Require(model.Items().size() == expected.size(), "Source record count changed");
  for (std::size_t i = 0; i < expected.size(); ++i)
    Same(model.Items()[i], expected[i]);
}
std::vector<Item> Sources(std::size_t count) {
  std::vector<Item> source;
  for (std::size_t i = 0; i < count; ++i)
    source.push_back(Asset(i + 1, std::filesystem::path(u8"Content/編輯 µ") /
                                      ("Group" + std::to_string(i % 11)) /
                                      ("asset-" + std::to_string((i * 701) % count) + ".bin")));
  source.push_back(Asset(count + 1, std::filesystem::path(u8"Content/e\u0301.obj")));
  source.push_back(Asset(count + 2, std::filesystem::path(u8"Content/é.obj")));
  source.push_back(Asset(count + 3, "Content/A.obj"));
  source.push_back(Asset(count + 4, "Content/a.obj"));
  source.push_back(Asset(count + 5, std::filesystem::path(u8"Content/\U0001F600.obj")));
  source.push_back(Asset(count + 6, std::filesystem::path(u8"Content/\uE000.obj")));
#if !defined(_WIN32)
  source.push_back(Asset(count + 7, "Content/raw-" + std::string(1, static_cast<char>(0xff))));
#endif
  auto mesh = std::make_shared<editor::MeshGeometry>();
  mesh->vertices.resize(3);
  mesh->vertices[1].position = {1, 0, 0};
  mesh->vertices[2].position = {0, 1, 0};
  for (auto &vertex : mesh->vertices)
    vertex.normal = {0, 0, 1};
  mesh->indices = {0, 1, 2};
  mesh->maximum = {1, 1, 0};
  source[0].mesh = std::move(mesh);
  source[0].type = "Mesh";
  source[0].path.replace_extension(".obj");
  auto material = editor::ImportMaterial(
      "NEXORA_MATERIAL 1 base_color 1 0 0 metallic 0 roughness 0.5 occlusion 1 emission 0 0 0");
  Require(material.material.has_value(), "Actual canonical material fixture failed");
  source[1].material = std::make_shared<const editor::MaterialAsset>(std::move(*material.material));
  source[1].type = "Material";
  source[1].path.replace_extension(".nmaterial");
  std::ranges::reverse(source);
  return source;
}
void MutationsAndFaults() {
  const auto source = Sources(2048);
  Model model;
  Require(model.Reset(source, 17) && Access::LastSortAccelerated(),
          "Normal key cache not admitted");
  Check(model, source);
  Access::FailAfterKeys(4);
  Require(model.Reset(source, 18) && !Access::LastSortAccelerated(),
          "Partial key allocation failure did not take the complete fallback");
  Check(model, source);
  Require(model.Reset(source, 19) && Access::LastSortAccelerated(), "Sort allocation retry failed");
  Check(model, source);
  Require(model.Select({44, 1}), "Selection setup failed");
  auto renamed = source;
  const auto target = std::ranges::find(renamed, Id{44, 1}, &Item::id);
  target->path = target->path.parent_path() / "0-renamed.obj";
  Require(model.Rename({44, 1}, "0-renamed.obj") && Access::LastSortAccelerated() &&
              model.Selection() == std::vector{Id{44, 1}},
          "Rename sort changed identity/selection");
  Check(model, renamed);
  const auto discovered = Asset(9999, "Content/New/discovered.asset");
  Require(model.Discover(discovered) && Access::LastSortAccelerated(), "Discovery sort failed");
  renamed.push_back(discovered);
  Check(model, renamed);
  Require(model.Undo(), "Earlier Rename Undo was lost by discovery");
  auto expected = source;
  expected.push_back(discovered);
  Check(model, expected);
  const std::array moved{Id{44, 1}, Id{44, 2}};
  Require(model.Move(moved, "Content/Moved") && Access::LastSortAccelerated(), "Move sort failed");
  auto moved_source = expected;
  for (auto &item : moved_source)
    if (std::ranges::find(moved, item.id) != moved.end())
      item.path = std::filesystem::path("Content/Moved") / item.path.filename();
  Check(model, moved_source);
  Require(model.Undo(), "Move Undo failed");
  Check(model, expected);
  const std::array removed{Id{44, 1}, Id{44, 2}};
  Require(model.Delete(removed) && Access::LastSortAccelerated(), "Delete sort failed");
  auto remaining = expected;
  std::erase_if(remaining, [&](const Item &item) {
    return std::ranges::find(removed, item.id) != removed.end();
  });
  Check(model, remaining);
  Require(model.Undo(), "Delete Undo failed");
  Check(model, expected);
  const auto generation = model.ProjectGeneration(), revision = model.Revision();
  auto duplicate = source;
  duplicate.push_back(source.front());
  Require(!model.Reset(duplicate, 99) && !model.Discover(source.front()) &&
              model.ProjectGeneration() == generation && model.Revision() == revision,
          "Rejected source admission changed data or authority");
  Check(model, expected);
}
void KeyByteLimit() {
  const std::string prefix = "Content/";
  const auto half = Access::kMaximumKeyBytes / 2;
  const std::string name(half - prefix.size() - 1, 'a');
  std::array source{Asset(1, prefix + name + '1'), Asset(2, prefix + name + '0')};
  Model model;
  Require(model.Reset(source, 1) && Access::LastSortAccelerated(),
          "Exact key-byte budget rejected");
  Check(model, source);
  source[1].path += "x";
  Require(model.Reset(source, 2) && !Access::LastSortAccelerated(),
          "Over-key-budget data did not use the full fallback");
  Check(model, source);
}
void ItemLimit() {
  std::vector<Item> source;
  source.reserve(Access::kMaximumItems + 1);
  for (std::size_t i = 0; i <= Access::kMaximumItems; ++i)
    source.push_back(Asset(i + 1, std::filesystem::path("Content") / std::to_string(i)));
  Model model;
  const auto check = [&](std::size_t count) {
    Require(model.Items().size() == count, "Budget fallback lost source records");
    std::vector<bool> seen(count + 1);
    std::string previous;
    for (const auto &item : model.Items()) {
      Require(item.id.high == 44 && item.id.low > 0 && item.id.low <= count && !seen[item.id.low],
              "Budget sort duplicated/replaced a source identity");
      seen[item.id.low] = true;
      Same(item, source[item.id.low - 1]);
      const auto path = Utf8(item.path);
      Require(previous <= path, "Budget sort changed UTF-8 byte ordering");
      previous = path;
    }
  };
  Require(model.Reset(std::span(source).first(source.size() - 1), 1) &&
              Access::LastSortAccelerated(),
          "Maximum cached item count rejected");
  check(Access::kMaximumItems);
  Require(model.Reset(source, 2) && !Access::LastSortAccelerated(),
          "Over-item-budget sort did not fall back");
  check(source.size());
}
} // namespace
int main() {
  try {
    MutationsAndFaults();
    KeyByteLimit();
    ItemLimit();
    std::cout << "UTF-8 path ordering, full source records, Undo and bounded allocation fallback "
                 "passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
