#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabAssets.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Prefab source"));
  const auto parent = scene.Create("Parent"), child = scene.Create("Child", parent);
  const auto parent_key = *scene.Key(parent), child_key = *scene.Key(child);
  Require(scene.SetOpaqueComponent(child_key, {99, "Unavailable.Provider", {0, 255, 27, 127}}),
          "Opaque fixture failed");
  const auto original = scene.PrepareSave()->Bytes();
  std::uint64_t identity = 10;
  const auto create = [&]() -> foundation::Uuid { return {0x505245464142ULL, ++identity}; };
  auto base = Assets::Capture({1, 1}, scene, create);
  Require(base && base->revision == 1 && base->nodes.size() == 2 && Assets::Validate(*base) &&
              base->scene_bytes == original && scene.PrepareSave()->Bytes() == original,
          "Capture changed source or lost exact unknown data");
  const auto encoded = Assets::Encode(*base);
  Require(encoded && Assets::Decode(*encoded) == base, "Exact identity/scene round trip failed");
  for (std::size_t count = 0; count < encoded->size(); ++count)
    Require(!Assets::Decode(std::span(*encoded).first(count)), "Truncated asset accepted");
  auto invalid_bytes = *encoded;
  invalid_bytes.push_back(std::byte{});
  Require(!Assets::Decode(invalid_bytes), "Trailing asset bytes accepted");
  invalid_bytes = *encoded;
  invalid_bytes[6] = std::byte{'9'};
  Require(!Assets::Decode(invalid_bytes), "Unknown schema accepted");
  invalid_bytes = *encoded;
  std::fill(invalid_bytes.begin() + 56, invalid_bytes.begin() + 64, std::byte{255});
  Require(!Assets::Decode(invalid_bytes), "Overflow node/instance count accepted");
  auto bad = *base;
  bad.nodes[1].id = bad.nodes[0].id;
  Require(!Assets::Encode(bad), "Duplicate stable node ID accepted");
  bad = *base;
  bad.nodes[1].properties[0].id = bad.nodes[0].properties[0].id;
  Require(!Assets::Validate(bad), "Duplicate stable property ID accepted");
  bad = *base;
  bad.nodes[0].serialized_node = std::numeric_limits<runtime::Id>::max();
  Require(!Assets::Validate(bad), "Foreign serialized node mapped");
  bad = *base;
  bad.nodes[0].properties[0].field = "missing";
  Require(!Assets::Validate(bad), "Missing property field mapped");
  bad = *base;
  bad.scene_bytes = "corrupt";
  Require(!Assets::Validate(bad), "Corrupt scene snapshot accepted");
  Require(!Assets::Capture({1, 2}, scene, [] { return foundation::Uuid{}; }),
          "Nil factory IDs accepted");
  Require(!Assets::Capture({1, 2}, scene, [] { return foundation::Uuid{1, 2}; }),
          "Aliased factory IDs accepted");

  Require(scene.Rename(parent_key, "Renamed parent"), "Rename fixture failed");
  auto revision = Assets::Capture(base->id, scene, create, &*base);
  Require(revision && revision->revision == 2 && revision->nodes == base->nodes &&
              revision->scene_bytes != base->scene_bytes && base->scene_bytes == original,
          "Rename changed stable node/property IDs or mutated immutable base");
  Require(scene.Undo() && scene.PrepareSave()->Bytes() == original,
          "Capture inserted source Undo history");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
  Require(reopened.ReloadBytes(base->scene_bytes), "Actual source reopen failed");
  auto reopened_asset = Assets::Capture(base->id, reopened, create, &*base);
  Require(reopened_asset && reopened_asset->nodes == base->nodes,
          "Reopen lost persistent identities");
  Require(reopened.OpaqueComponents(*reopened.Key(child))->front().data ==
              std::vector<std::uint8_t>({0, 255, 27, 127}),
          "Reopen lost unavailable provider bytes");
  Require(reopened.SetOpaqueComponent(*reopened.Key(child), {100, "New.Provider", {8}}),
          "New identity callback fixture failed");
  auto callback_previous = *base;
  const auto callback_capture = Assets::Capture(
      base->id, reopened,
      [&]() {
        callback_previous.nodes.clear();
        callback_previous.nodes.shrink_to_fit();
        callback_previous.scene_bytes.clear();
        return create();
      },
      &callback_previous);
  Require(callback_capture && callback_capture->nodes[0] == base->nodes[0] &&
              callback_capture->nodes[1].id == base->nodes[1].id &&
              callback_capture->nodes[1].properties.size() == base->nodes[1].properties.size() + 1,
          "Caller identity callback invalidated retained previous identities");
  auto exhausted = *base;
  exhausted.revision = std::numeric_limits<std::uint64_t>::max();
  Require(!Assets::Capture(base->id, scene, create, &exhausted), "Revision wrapped");
  auto variant = Assets::Capture({2, 2}, scene, create, &*base);
  Require(variant && variant->base == editor::PrefabRevisionReference{base->id, 1} &&
              variant->nodes == base->nodes && variant->revision == 1,
          "Variant lost base/stable identity");
  auto nested = Assets::Capture({3, 3}, scene, create);
  Require(nested.has_value(), "Nested source failed");
  variant->nested = {{{4, 1}, variant->nodes[0].id, {nested->id, 1}},
                     {{4, 2}, variant->nodes[1].id, {nested->id, 1}}};
  const std::array sources{*base, *variant, *nested};
  auto graph = Assets::Resolve({variant->id, 1}, sources);
  Require(graph && graph->assets.size() == 3 && graph->instances.size() == 3 &&
              graph->expanded_nodes == 6 && graph->instances[1].scope != graph->instances[2].scope,
          "Repeated nested instances aliased or base counted as an instance");
  auto copied = graph->assets;
  copied[0].scene_bytes.clear();
  Require(!graph->assets[0].scene_bytes.empty(), "Resolved archive retained caller aliases");
  auto stale = sources;
  stale[0].revision = 2;
  Require(!Assets::Resolve({variant->id, 1}, stale), "Stale base revision resolved");
  Require(!Assets::Resolve({variant->id, 1}, std::span(sources).first(2)),
          "Missing nested source resolved");
  auto cyclic = sources;
  cyclic[2].nested = {{{9, 9}, cyclic[2].nodes[0].id, {variant->id, 1}}};
  Require(!Assets::Resolve({variant->id, 1}, cyclic), "Nested dependency cycle resolved");
  cyclic = sources;
  cyclic[0].base = editor::PrefabRevisionReference{variant->id, 1};
  Require(!Assets::Resolve({variant->id, 1}, cyclic), "Variant base cycle resolved");
  auto duplicates = sources;
  duplicates[2].id = duplicates[0].id;
  Require(!Assets::Resolve({variant->id, 1}, duplicates), "Duplicate source identity resolved");
  bad = *variant;
  bad.nested[1].instance = bad.nested[0].instance;
  Require(!Assets::Validate(bad), "Nested instance namespace alias accepted");
  bad = *variant;
  bad.nested[0].attachment = {999, 999};
  Require(!Assets::Validate(bad), "Foreign attachment node accepted");

  auto expanded = *variant;
  expanded.nested.clear();
  for (std::uint64_t i = 0; i < Assets::kMaximumInstances; ++i)
    expanded.nested.push_back({{8, i + 1}, expanded.nodes[0].id, {nested->id, 1}});
  const std::array too_many{*base, expanded, *nested};
  Require(Assets::Validate(expanded) && !Assets::Resolve({expanded.id, 1}, too_many),
          "Expanded instance limit ignored");
  std::vector<editor::PrefabAsset> chain;
  for (std::uint64_t i = 0; i < Assets::kMaximumDepth + 2; ++i) {
    auto item = *base;
    item.id = {100, i + 1};
    item.base = i + 1 < Assets::kMaximumDepth + 2
                    ? std::optional(editor::PrefabRevisionReference{{100, i + 2}, 1})
                    : std::nullopt;
    chain.push_back(std::move(item));
  }
  Require(!Assets::Resolve({chain[0].id, 1}, chain), "Overdeep base chain resolved");
  chain.pop_back();
  chain.back().base.reset();
  Require(Assets::Resolve({chain[0].id, 1}, chain).has_value(),
          "Supported maximum-depth base chain rejected");
  // Shared base closures must stay bounded instead of revisiting exponentially many paths.
  for (std::size_t i = 1; i + 2 < chain.size(); ++i)
    chain[i].nested = {{{200, i}, chain[i].nodes[0].id, {chain[i + 2].id, 1}}};
  const auto diamond = Assets::Resolve({chain[0].id, 1}, chain);
  Require(diamond && diamond->instances.size() == 1 && diamond->assets.size() == chain.size(),
          "Shared base closure was re-expanded as live instances");
  runtime::World large_world;
  const auto large_scene = large_world.LoadScene("Large prefab");
  runtime::SceneEditor large_editor(large_world);
  std::vector<runtime::Entity> prototypes(2050);
  for (std::size_t i = 0; i < prototypes.size(); ++i)
    prototypes[i].id = i + 1;
  const auto created = large_editor.CloneEntityForest(large_scene, prototypes);
  Require(created.size() == prototypes.size(), "Large source fixture failed");
  std::ostringstream large_bytes;
  large_bytes << "NEXORA_EDITOR_SCENE 3\n";
  for (const auto id : created)
    large_bytes << "node " << id << " 0 Generated\n";
  large_bytes << "world\n" << *large_world.SaveScene(large_scene);
  editor::SceneDocument large_document(large_world, large_scene);
  Require(large_document.ReloadBytes(large_bytes.str()), "Large authoring fixture failed");
  const auto large_asset = Assets::Capture({300, 1}, large_document, create);
  Require(large_asset && large_asset->nodes.size() == 2050, "Valid large source capture failed");
  auto large_parent = *base;
  large_parent.nested = {{{301, 1}, large_parent.nodes[0].id, {large_asset->id, 1}},
                         {{301, 2}, large_parent.nodes[1].id, {large_asset->id, 1}}};
  const std::array large_sources{large_parent, *large_asset};
  Require(!Assets::Resolve({large_parent.id, 1}, large_sources), "Expanded node budget ignored");

  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-prefab-codec-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  const auto path = root / "template.nxprefab";
  {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(encoded->data()),
                 static_cast<std::streamsize>(encoded->size()));
    Require(static_cast<bool>(output), "Actual prefab file write failed");
  }
  std::ifstream input(path, std::ios::binary);
  const std::string persisted{std::istreambuf_iterator<char>{input}, {}};
  const auto *first = reinterpret_cast<const std::byte *>(persisted.data());
  const auto loaded = Assets::Decode(std::span(first, persisted.size()));
  Require(loaded == base && loaded->scene_bytes == original,
          "Actual prefab save/reopen lost exact data");

  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root / "Project", "Prefab persistence", &error),
          "Actual project creation failed");
  Require(Assets::Publish(workspace, *base, nullptr, &error) &&
              Assets::Load(workspace, base->id) == base,
          "Project-owned prefab publication/reopen failed");
  const auto destination =
      workspace.Root() / ".nexora/prefabs" / (base->id.ToString() + ".nxprefab");
  auto stage = destination;
  stage += ".tmp";
  std::filesystem::create_directory(stage);
  Require(!Assets::Publish(workspace, *revision, &*base, &error) && !error.empty() &&
              std::filesystem::is_directory(stage) && Assets::Load(workspace, base->id) == base,
          "Foreign staging was consumed or good revision overwritten");
  std::filesystem::remove(stage);
  Require(!Assets::Publish(workspace, *base, nullptr, &error),
          "Existing prefab identity replaced without expected revision");
  Require(Assets::Publish(workspace, *revision, &*base, &error) &&
              Assets::Load(workspace, base->id) == revision &&
              !Assets::Publish(workspace, *revision, &*base, &error),
          "Expected source revision check failed");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly, &error) &&
                Assets::Load(observer, base->id) == revision &&
                !Assets::Publish(observer, *base, nullptr, &error),
            "Read-only persistence protection failed");
  }
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(!Assets::Publish(workspace, *variant, nullptr, &error) &&
              !Assets::Load(workspace, base->id) && std::filesystem::is_directory(recovery),
          "Pending recovery did not freeze prefab reads/publication");
  std::filesystem::remove(recovery);
  Require(workspace.SaveWorkspace({}, &error), "Workspace baseline failed");
  const auto workspace_path = workspace.Root() / ".nexora/workspace";
  const auto workspace_time = std::filesystem::last_write_time(workspace_path);
  std::ifstream workspace_input(workspace_path, std::ios::binary);
  const std::string workspace_bytes{std::istreambuf_iterator<char>{workspace_input}, {}};
  workspace_input.close();
  {
    std::ofstream output(workspace_path, std::ios::binary | std::ios::trunc);
    output << "external workspace bytes";
  }
  std::filesystem::last_write_time(workspace_path, workspace_time + std::chrono::seconds(2));
  Require(workspace.HasExternalChange() && !Assets::Publish(workspace, *variant, nullptr, &error) &&
              !Assets::Load(workspace, base->id),
          "External workspace change did not freeze prefab persistence");
  {
    std::ofstream output(workspace_path, std::ios::binary | std::ios::trunc);
    output << workspace_bytes;
  }
  std::filesystem::last_write_time(workspace_path, workspace_time);
  Require(!workspace.HasExternalChange() && Assets::Load(workspace, base->id) == revision,
          "Exact workspace resolution did not restore prefab inspection");
#if !defined(_WIN32)
  const auto alias = root / "aliased-prefab";
  std::filesystem::create_hard_link(destination, alias);
  auto revision3 = *revision;
  revision3.revision = 3;
  Require(!Assets::Load(workspace, base->id) &&
              !Assets::Publish(workspace, revision3, &*revision, &error) &&
              std::filesystem::exists(alias),
          "Aliased source was accepted or replaced");
  std::filesystem::remove(alias);
#endif
  {
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    output << "external prefab bytes";
  }
  auto final_revision = *revision;
  final_revision.revision = 3;
  Require(!Assets::Publish(workspace, final_revision, &*revision, &error) &&
              !Assets::Load(workspace, base->id),
          "External prefab bytes overwritten or accepted");
  std::ifstream changed(destination, std::ios::binary);
  Require(std::string(std::istreambuf_iterator<char>{changed}, {}) == "external prefab bytes",
          "Failed revision compare changed external source");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
