#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Revision(std::string bytes, foundation::Uuid instance, foundation::Uuid source,
                     std::uint64_t from, std::uint64_t to) {
  const auto prefix = "prefab-placement " + instance.ToString() + ' ' + source.ToString() + ' ';
  const auto old = prefix + std::to_string(from) + '\n';
  const auto position = bytes.find(old);
  Require(position != bytes.npos, "Binding revision fixture missing");
  bytes.replace(position, old.size(), prefix + std::to_string(to) + '\n');
  return bytes;
}
std::map<std::filesystem::path, std::string> Sources(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root / ".nexora/prefabs"))
    if (entry.is_regular_file()) {
      std::ifstream input(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string{std::istreambuf_iterator<char>(input), {}});
    }
  return result;
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-placement-snapshot-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Placement snapshot"), "Workspace failed");
  runtime::World source_world, nested_world, world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  const auto source_root = source.Create("Source root"),
             child = source.Create("Child", source_root);
  Require(source.SetOpaqueComponent(*source.Key(child), {91, "Absent.Provider", {0, 255, 27}}),
          "Unknown source failed");
  std::uint64_t serial{};
  const auto factory = [&] { return foundation::Uuid{8910, ++serial}; };
  editor::SceneDocument nested(nested_world, nested_world.LoadScene("Nested"));
  const auto nested_node = nested.Create("Nested source");
  Require(nested.SetOpaqueComponent(*nested.Key(nested_node), {93, "Nested.Missing", {3, 255, 11}}),
          "Nested unknown fixture failed");
  const auto nested_asset = editor::PrefabAssets::Capture({8911, 2}, nested, factory);
  auto asset = editor::PrefabAssets::Capture({8911, 1}, source, factory);
  Require(nested_asset && asset && editor::PrefabAssets::Publish(workspace, *nested_asset),
          "Nested publication failed");
  const foundation::Uuid nested_instance{8913, 1};
  const auto attachment =
      std::ranges::find(asset->nodes, source_root, &editor::PrefabNodeIdentity::serialized_node);
  Require(attachment != asset->nodes.end(), "Nested attachment identity absent");
  asset->nested = {{nested_instance, attachment->id, {nested_asset->id, 1}}};
  Require(editor::PrefabAssets::Publish(workspace, *asset), "Source publish failed");
  const foundation::Uuid first{8912, 1}, second{8912, 2};
  for (const auto instance : {first, second}) {
    const auto review = editor::ProjectPrefabPlacement::Prepare(workspace, asset->id, scene);
    Require(review && editor::ProjectPrefabPlacement::Instantiate(workspace, scene, *review,
                                                                  instance, true)
                          .has_value(),
            "Two-placement fixture failed");
  }
  Require(scene.PrefabPlacements()[0].nodes.size() == 3 &&
              std::ranges::any_of(scene.PrefabPlacements()[0].nodes,
                                  [](const auto &node) { return !node.scope.empty(); }),
          "Actual scoped nested binding fixture absent");
  const auto selected = *scene.Key(scene.PrefabPlacements()[0].nodes.front().target);
  const auto other = *scene.Key(scene.PrefabPlacements()[1].nodes.front().target);
  const auto free = scene.Create("Unbound");
  const auto generation = scene.Generation();
  Require(scene.SetEulerField(std::array{*scene.Key(free)}, 1, 720) &&
              scene.SetTransform(free, {4, 5, 6, 0, 0, std::sqrt(.5), std::sqrt(.5)}),
          "Unbound latent hint fixture failed");

  Require(scene.Select(std::array{selected}) && scene.CopySelection() &&
              scene.Save(root / "baseline.scene"),
          "Baseline/clipboard failed");
  const auto before = *scene.PrepareSave();
  Require(source.Rename(*source.Key(source_root), "Revision two"), "Advance source failed");
  const auto advanced = editor::PrefabAssets::Capture(asset->id, source, factory, &*asset);
  Require(advanced && editor::PrefabAssets::Publish(workspace, *advanced, &*asset),
          "Revision two publish failed");
  const auto sources = Sources(root);
  runtime::World staged_world;
  const auto staged_scene = staged_world.LoadScene("Staged");
  editor::SceneDocument staged(staged_world, staged_scene);
  Require(staged.ReloadBytes(before.Bytes()) &&
              staged.Rename(*staged.Key(selected.id), "Revision two") &&
              staged.SetTransform(selected.id, {2.75, -3.25, 7.125}) &&
              staged.SetEulerField(std::array{*staged.Key(selected.id)}, 1, 720) &&
              staged.SetOpaqueComponent(*staged.Key(selected.id), {92, "Missing", {0, 255, 28}}),
          "Property stage failed");
  auto *stored = staged_world.FindEntity(selected.id);
  Require(stored && !stored->camera && !stored->light && !stored->mesh_renderer,
          "Dormant fixture failed");
  std::istringstream input(*staged_world.SaveScene(staged_scene));
  std::string line, snapshot;
  Require(static_cast<bool>(std::getline(input, line)), "Runtime fixture header absent");
  snapshot = line + '\n';
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::vector<std::string> fields;
    for (std::string field; row >> field;)
      fields.push_back(std::move(field));
    Require(fields.size() == 21, "Canonical runtime fixture schema changed");
    if (fields[0] == std::to_string(selected.id)) {
      fields[15] = "91";
      fields[16] = "0.2";
      fields[17] = "950";
      fields[18] = "3.5";
      fields[19] = "123456789012345";
      fields[20] = "987654321098765";
    }
    for (std::size_t i = 0; i < fields.size(); ++i)
      snapshot += (i ? " " : "") + fields[i];
    snapshot += '\n';
  }
  Require(staged_world.ReplaceSceneSnapshot(staged_scene, snapshot),
          "Canonical dormant fixture rejected");
  const auto candidate = Revision(staged.PrepareSave()->Bytes(), first, asset->id, 1, 2);
  const auto sentinel = scene.Create("Redo sentinel");
  Require(sentinel && scene.Undo() && scene.MatchesPreparedSave(before) &&
              scene.ApplyPrefabPlacementSnapshot(before, before.Bytes(), first, true) &&
              scene.Redo() && scene.Undo(),
          "No-op consumed pending Redo");
  const auto reject = [&](std::string_view bytes, foundation::Uuid instance, bool authorized) {
    Require(!scene.ApplyPrefabPlacementSnapshot(before, bytes, instance, authorized) &&
                scene.MatchesPreparedSave(before),
            "Rejected snapshot mutated document");
  };
  Require(!scene.ApplyPropertySnapshot(before, candidate, true) &&
              scene.MatchesPreparedSave(before),
          "Ordinary property transaction accepted changed placement binding");
  reject(candidate, first, false);
  reject(candidate, {}, true);
  reject(candidate, {9999, 1}, true);
  reject(staged.PrepareSave()->Bytes(), first, true);
  reject(Revision(candidate, second, asset->id, 1, 2), first, true);
  reject(Revision(candidate, first, asset->id, 2, 0), first, true);
  Require(staged.Rename(*staged.Key(free), "Foreign"), "Unbound name fixture failed");
  reject(Revision(staged.PrepareSave()->Bytes(), first, asset->id, 1, 2), first, true);
  Require(staged.Undo() && staged.SetTransform(other.id, {9, 8, 7}),
          "Other-placement fixture failed");
  reject(Revision(staged.PrepareSave()->Bytes(), first, asset->id, 1, 2), first, true);
  Require(staged.Undo() && staged.Move(selected.id, free, 0), "Hierarchy fixture failed");
  reject(Revision(staged.PrepareSave()->Bytes(), first, asset->id, 1, 2), first, true);
  Require(staged.Undo(), "Hierarchy fixture Undo failed");
  auto changed_mapping = candidate;
  const auto mapped = scene.PrefabPlacements()[0].nodes.front().source_node.ToString();
  const auto mapping_position = changed_mapping.find(mapped);
  Require(mapping_position != changed_mapping.npos, "Source mapping fixture absent");
  changed_mapping.replace(mapping_position, mapped.size(), foundation::Uuid{9998, 1}.ToString());
  reject(changed_mapping, first, true);
  auto changed_scope = candidate;
  const auto scope_position = changed_scope.find(nested_instance.ToString());
  Require(scope_position != changed_scope.npos, "Nested scoped mapping fixture absent");
  changed_scope.replace(scope_position, nested_instance.ToString().size(),
                        foundation::Uuid{9996, 1}.ToString());
  reject(changed_scope, first, true);
  auto changed_source = candidate;
  const auto source_position = changed_source.find(asset->id.ToString());
  Require(source_position != changed_source.npos, "Source asset fixture absent");
  changed_source.replace(source_position, asset->id.ToString().size(),
                         foundation::Uuid{9997, 1}.ToString());
  reject(changed_source, first, true);
  Require(scene.ApplyPrefabPlacementSnapshot(before, candidate, first, true),
          "Owning property/revision transaction failed");
  const auto after = *scene.PrepareSave();
  Require(after.Bytes() == candidate && scene.PrefabPlacements()[0].revision == 2 &&
              scene.PrefabPlacements()[1].revision == 1 && scene.Key(selected.id) == selected &&
              scene.Key(other.id) == other && scene.Generation() == generation && scene.Dirty() &&
              scene.Selection().size() == 1 && scene.Selection().front() == selected.id,
          "Transaction changed unrelated identity/selection/baseline");
  Require(Sources(root) == sources &&
              !scene.ApplyPrefabPlacementSnapshot(before, candidate, first, true) &&
              scene.MatchesPreparedSave(after),
          "Stale transaction wrote source or changed history");
  for (int i = 0; i < 40; ++i) {
    Require(scene.Undo(), "Revision transaction Undo rejected");
    Require(scene.MatchesPreparedSave(before),
            "Revision Undo did not restore exact original metadata/runtime");
    Require(!scene.Dirty(), "Revision Undo changed saved baseline");
    Require(scene.PrefabPlacements()[0].revision == 1, "Revision Undo lost original binding");
    Require(scene.Redo(), "Revision transaction Redo rejected");
    Require(scene.MatchesPreparedSave(after),
            "Revision Redo did not restore exact advanced metadata/runtime");
    Require(scene.PrefabPlacements()[0].revision == 2, "Revision Redo lost advanced binding");
    Require(world.FindEntity(selected.id)->camera_data.vertical_field_of_view == 91 &&
                world.FindEntity(selected.id)->mesh_data.mesh == 123456789012345ULL,
            "Revision Redo lost dormant stored fields");
  }
  Require(scene.SetTransform(free, {4, 5, 6}) && scene.EulerAngles(free)->at(1) == 720 &&
              scene.Undo() && scene.MatchesPreparedSave(after),
          "Unbound latent authored hint was normalized away by revision transaction");
  const auto unchanged = *scene.Transform(other.id);
  runtime::WorldCommandBuffer foreign;
  foreign.SetTransform(other.id, runtime::WithPosition(unchanged, 77, 8, 9));
  Require(foreign.Apply(world), "Foreign replay fixture failed");
  const auto foreign_bytes = scene.PrepareSave()->Bytes();
  Require(!scene.Undo() && scene.PrepareSave()->Bytes() == foreign_bytes,
          "Stale replay consumed cursor or overwrote unrelated entity");
  foreign.SetTransform(other.id, unchanged);
  Require(foreign.Apply(world) && scene.Undo() && scene.MatchesPreparedSave(before) &&
              scene.Redo() && scene.MatchesPreparedSave(after) && scene.Paste() && scene.Undo() &&
              scene.MatchesPreparedSave(after) && scene.Save(root / "reopened.scene"),
          "Rejected replay lost cursor, clipboard or save");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
  Require(reopened.Reload(root / "reopened.scene") &&
              reopened.PrepareSave()->Bytes() == after.Bytes() &&
              reopened.PrefabPlacements()[0].revision == 2 && scene.Undo() &&
              scene.MatchesPreparedSave(before) && Sources(root) == sources,
          "Save/reopen or baseline-independent Undo changed state/source files");
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
