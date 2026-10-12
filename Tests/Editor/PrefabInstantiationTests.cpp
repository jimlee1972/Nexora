#include "Nexora/Editor/PrefabAssets.h"
#include "Nexora/Editor/SceneSaveBatch.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(bool bound) {
  std::uint64_t next = 100;
  const auto identity = [&] { return foundation::Uuid{700, ++next}; };
  runtime::World root_world, child_world, target_world;
  editor::SceneDocument root(root_world, root_world.LoadScene("Root source"));
  editor::SceneDocument child(child_world, child_world.LoadScene("Child source"));
  editor::SceneDocument target(target_world, target_world.LoadScene("Target"));
  const auto attachment = root.Create("Attachment"),
             root_child = root.Create("Root child", attachment);
  const auto child_root = child.Create("Nested root"),
             leaf = child.Create("Nested leaf", child_root);
  const std::array child_keys{*child.Key(child_root)};
  Require(
      root.SetTransform(attachment, {10, 20, 30}) && child.SetTransform(child_root, {1, 2, 3}) &&
          child.SetEulerField(child_keys, 1, 720) &&
          child.SetOpaqueComponent(*child.Key(leaf), {91, "Unavailable.Provider", {0, 255, 27}}),
      "Materialization fixture failed");
  auto container = Assets::Capture({701, 1}, root, identity);
  const auto nested = Assets::Capture({701, 2}, child, identity);
  Require(container && nested, "Asset capture failed");
  const auto attach_id =
      std::ranges::find(container->nodes, attachment, &editor::PrefabNodeIdentity::serialized_node)
          ->id;
  const foundation::Uuid first{702, 1}, second{702, 2};
  Require(child.Rename(*child.Key(child_root), "Newer nested root"), "Revision edit failed");
  const auto newer = Assets::Capture(nested->id, child, identity, &*nested);
  Require(newer && newer->revision == 2, "Second exact revision capture failed");
  container->nested = {{first, attach_id, {nested->id, 1}}, {second, attach_id, {nested->id, 2}}};
  const auto variant = Assets::Capture({701, 3}, root, identity, &*container);
  Require(variant.has_value(), "Variant capture failed");
  const std::array sources{*container, *nested, *variant, *newer};
  const auto seed = target.Create("Seed");
  const auto seed_key = *target.Key(seed);
  const std::array seed_ids{seed};
  Require(target.Select(seed_ids) && target.CopySelection() && target.Rename(seed_key, "Changed") &&
              target.Undo(),
          "Clipboard/Redo fixture failed");
  const auto expected = *target.PrepareSave();
  Require(!Assets::Instantiate({variant->id, 1}, sources, target, expected, false) &&
              !Assets::Instantiate({variant->id, 2}, sources, target, expected, true) &&
              target.PrepareSave()->Bytes() == expected.Bytes() && target.Redo() && target.Undo(),
          "Rejected materialization changed bytes or history");
  auto corrupted = sources;
  corrupted[1].scene_bytes = "corrupt";
  Require(!Assets::Instantiate({variant->id, 1}, corrupted, target, expected, true),
          "Corrupt nested source materialized");
  corrupted = sources;
  corrupted[1].nested = {{{703, 1}, nested->nodes.front().id, {container->id, 1}}};
  Require(!Assets::Instantiate({variant->id, 1}, corrupted, target, expected, true),
          "Cyclic nested graph materialized");
  Require(target.Rename(seed_key, "Stale") &&
              !Assets::Instantiate({variant->id, 1}, sources, target, expected, true) &&
              target.Undo(),
          "Stale document observation authorized materialization");
  const foundation::Uuid placement_id{710, 1};
  const auto instantiated =
      bound ? Assets::InstantiateBound(placement_id, {variant->id, 1}, sources, target, expected,
                                       true)
            : Assets::Instantiate({variant->id, 1}, sources, target, expected, true);
  Require(instantiated && instantiated->size() == 6 && target.Nodes().size() == 7,
          "Variant duplicated its base or lost nested placements");
  const auto mapped = [&](std::vector<foundation::Uuid> scope, foundation::Uuid node) {
    const auto found = std::ranges::find_if(*instantiated, [&](const auto &entry) {
      return entry.scope == scope && entry.node == node;
    });
    Require(found != instantiated->end() && target.Key(found->target.id) == found->target,
            "Scoped stable identity mapping failed");
    return found->target;
  };
  const auto root_key = mapped({}, attach_id);
  const auto root_child_id =
      std::ranges::find(container->nodes, root_child, &editor::PrefabNodeIdentity::serialized_node)
          ->id;
  Require(target.Parent(mapped({}, root_child_id).id) == root_key.id &&
              target.Transform(root_key.id) == root.Transform(attachment),
          "Root instance hierarchy/transform changed");
  const auto child_id =
      std::ranges::find(nested->nodes, child_root, &editor::PrefabNodeIdentity::serialized_node)
          ->id;
  const auto leaf_id =
      std::ranges::find(nested->nodes, leaf, &editor::PrefabNodeIdentity::serialized_node)->id;
  const auto a = mapped({first}, child_id), b = mapped({second}, child_id);
  Require(a.id != b.id, "Repeated placement reused live identity");
  for (const auto scope : {first, second}) {
    const auto parent = mapped({scope}, child_id), child_key = mapped({scope}, leaf_id);
    Require(target.Parent(parent.id) == root_key.id && target.Parent(child_key.id) == parent.id &&
                target.Transform(parent.id) == child.Transform(child_root) &&
                target.EulerAngles(parent.id) == child.EulerAngles(child_root) &&
                target.Name(parent.id) == (scope == first ? "Nested root" : "Newer nested root") &&
                target.OpaqueComponents(child_key) == child.OpaqueComponents(*child.Key(leaf)),
            "Nested attachment lost local pose, authored Euler, name or opaque bytes");
  }
  const std::string published = target.PrepareSave()->Bytes();
  Require(target.Undo() && target.PrepareSave()->Bytes() == expected.Bytes() &&
              target.Selection().size() == 1 && target.Selection().front() == seed &&
              !target.Key(a.id) && target.Redo() && target.PrepareSave()->Bytes() == published &&
              target.Key(a.id) == a && target.Paste() &&
              target.Name(target.Selection().front()) == "Seed Copy" && target.Undo() &&
              target.PrepareSave()->Bytes() == published,
          "Materialization was not one Undo/Redo or replaced user clipboard");
  if (!bound) {
    Require(target.PrefabPlacements().empty(), "Detached import acquired placement authority");
    return;
  }
  const auto bindings =
      std::vector(target.PrefabPlacements().begin(), target.PrefabPlacements().end());
  Require(bindings.size() == 1 && bindings.front().instance == placement_id &&
              bindings.front().source == variant->id && bindings.front().revision == 1 &&
              bindings.front().nodes.size() == 6 &&
              published.starts_with("NEXORA_EDITOR_SCENE 4\n") && target.Key(seed) == seed_key &&
              target.Generation() == seed_key.document_generation,
          "Bound import lost exact source/scope metadata or stable existing keys");
  for (const auto &entry : *instantiated)
    Require(std::ranges::any_of(bindings.front().nodes,
                                [&](const auto &mapping) {
                                  return mapping.scope == entry.scope &&
                                         mapping.source_node == entry.node &&
                                         mapping.target == entry.target.id;
                                }),
            "Persistent nested mapping differs from actual materialization");
  Require(target.Rename(seed_key, "Ordinary edit") &&
              std::ranges::equal(target.PrefabPlacements(), bindings) && target.Undo() &&
              target.PrepareSave()->Bytes() == published,
          "Ordinary rename/Undo changed placement bindings");
  const auto current = *target.PrepareSave();
  Require(
      !Assets::InstantiateBound({}, {variant->id, 1}, sources, target, current, true) &&
          !Assets::InstantiateBound(placement_id, {variant->id, 1}, sources, target, current,
                                    true) &&
          !Assets::InstantiateBound({710, 2}, {variant->id, 1}, sources, target, current, false) &&
          target.MatchesPreparedSave(current),
      "Nil/duplicate/unauthorized placement changed the live document");
  runtime::World property_world;
  editor::SceneDocument property(property_world, property_world.LoadScene("Property probe"));
  Require(property.ReloadBytes(published) &&
              property.Rename(*property.Key(root_key.id), "Property replacement") &&
              target.ApplyPropertySnapshot(current, property.PrepareSave()->Bytes(), true) &&
              std::ranges::equal(target.PrefabPlacements(), bindings) && target.Undo() &&
              target.PrepareSave()->Bytes() == published && target.Redo() && target.Undo(),
          "Generic property snapshot/Undo/Redo lost bound metadata");
  auto different_binding = published;
  const auto source_record =
      "prefab-placement " + placement_id.ToString() + ' ' + variant->id.ToString() + " 1\n";
  const auto start = different_binding.find(source_record);
  Require(start != std::string::npos, "Placement source record absent");
  different_binding.replace(start, source_record.size(),
                            "prefab-placement " + placement_id.ToString() + ' ' +
                                variant->id.ToString() + " 2\n");
  Require(!target.ApplyPropertySnapshot(current, different_binding, true) &&
              target.MatchesPreparedSave(current),
          "Property-only replacement changed placement source context");
  const auto capture = target.CaptureRuntimeScene();
  Require(capture && capture->runtime_snapshot.find("prefab-") == std::string::npos &&
              !Assets::Capture({710, 9}, target, identity),
          "Editor bindings leaked to Runtime or were silently captured as unlinked assets");
  const auto second_placement =
      Assets::InstantiateBound({710, 2}, {variant->id, 1}, sources, target, current, true);
  Require(second_placement && target.PrefabPlacements().size() == 2,
          "Repeated bound placement failed");
  const auto repeated = *target.PrepareSave();
  const auto repeated_bindings =
      std::vector(target.PrefabPlacements().begin(), target.PrefabPlacements().end());
  Require(target.Select(std::array{a}) && target.DeleteSelection() &&
              target.PrefabPlacements().size() == 1 &&
              target.PrefabPlacements().front().instance == foundation::Uuid{710, 2} &&
              target.PrepareSave().has_value() && target.Undo() &&
              target.MatchesPreparedSave(repeated) &&
              std::ranges::equal(target.PrefabPlacements(), repeated_bindings) && target.Redo() &&
              target.PrefabPlacements().size() == 1 && target.Undo(),
          "Structural deletion retained a dangling placement or lost binding Undo/Redo");
  const auto temporary =
      std::filesystem::temp_directory_path() /
      ("nexora-bound-prefab-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{temporary};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(temporary, "Bound prefab"), "Bound save workspace failed");
  editor::SceneFileSession files(workspace, target);
  Require(files.SaveAs(files.Token(), "Content/Bound.scene").Applied() && !target.Dirty(),
          "Scene file publication rejected owning bindings");
  std::ifstream file(temporary / "Content/Bound.scene", std::ios::binary);
  const std::string saved{std::istreambuf_iterator<char>(file), {}};
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopen"));
  Require(reopened.ReloadBytes(saved) && !reopened.Dirty() &&
              std::ranges::equal(reopened.PrefabPlacements(), repeated_bindings) &&
              reopened.PrepareSave()->Bytes() == saved && reopened.Key(a.id) != a,
          "Scene reopen lost stable scoped bindings or retained stale generation keys");
  const auto reopened_before = *reopened.PrepareSave();
  Require(!reopened.ImportForestBytes(reopened_before, saved, true) &&
              reopened.MatchesPreparedSave(reopened_before),
          "Ordinary forest import silently dropped persistent source bindings");
  const auto reject = [&](std::string bad) {
    Require(!reopened.ReloadBytes(bad) && reopened.MatchesPreparedSave(reopened_before) &&
                !reopened.Dirty(),
            "Corrupt placement reload changed live content/baseline");
  };
  auto bad = saved;
  bad.replace(0, std::string("NEXORA_EDITOR_SCENE 4").size(), "NEXORA_EDITOR_SCENE 3");
  reject(bad);
  bad = saved;
  const auto record = saved.substr(saved.find("prefab-placement "),
                                   saved.find('\n', saved.find("prefab-placement ")) -
                                       saved.find("prefab-placement ") + 1);
  bad.insert(bad.find(record), record);
  reject(bad);
  bad = saved;
  const auto node_start = bad.find("prefab-node "), node_end = bad.find('\n', node_start);
  const auto node_record = bad.substr(node_start, node_end - node_start + 1);
  bad.insert(node_start, node_record);
  reject(bad);
  bad = saved;
  const auto revision = bad.find(source_record) + source_record.size() - 2;
  bad.replace(revision, 1, "-1");
  reject(bad);
  bad = saved;
  const auto target_start = bad.rfind(' ', node_end);
  bad.replace(target_start + 1, node_end - target_start - 1, "18446744073709551615");
  reject(bad);
  bad = saved;
  bad.replace(bad.find(placement_id.ToString()), 36, foundation::Uuid{}.ToString());
  reject(bad);
  Require(target.Rename(seed_key, "Batch edit"), "Bound batch edit failed");
  editor::SceneSaveBatch batch(workspace);
  const std::array sessions{&files};
  Require(batch.Prepare(sessions) && batch.Publish().Published() && !target.Dirty() &&
              std::ranges::equal(target.PrefabPlacements(), repeated_bindings) && target.Undo() &&
              target.PrepareSave()->Bytes() == saved,
          "Coordinated Scene Save All lost bindings or consumed Undo");
  Require(reopened.NewScene() && reopened.PrefabPlacements().empty() &&
              reopened.ReloadBytes(saved) &&
              std::ranges::equal(reopened.PrefabPlacements(), repeated_bindings),
          "NewScene/reload retained stale or lost reopened placement metadata");
  runtime::World many_world;
  editor::SceneDocument many(many_world, many_world.LoadScene("Placement budget"));
  for (std::size_t i = 0; i < editor::SceneDocument::kMaximumPrefabPlacements; ++i) {
    const auto observation = *many.PrepareSave();
    Require(
        Assets::InstantiateBound({711, i + 1}, {variant->id, 1}, sources, many, observation, true)
            .has_value(),
        "Valid repeated placement budget rejected early");
  }
  const auto limit = *many.PrepareSave();
  Require(!Assets::InstantiateBound({711, 129}, {variant->id, 1}, sources, many, limit, true) &&
              many.MatchesPreparedSave(limit) && many.Undo() && many.Redo() &&
              many.MatchesPreparedSave(limit),
          "Placement limit mutated content or broke complete history");
}
} // namespace
int main() {
  try {
    Run(false);
    Run(true);
    std::cout << "Nested prefab materialization passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
