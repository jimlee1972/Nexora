#include "Nexora/Editor/PrefabAssets.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
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
  const auto instantiated = Assets::Instantiate({variant->id, 1}, sources, target, expected, true);
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
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Nested prefab materialization passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
