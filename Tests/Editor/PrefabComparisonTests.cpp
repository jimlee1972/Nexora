#include "Nexora/Editor/PrefabComparison.h"
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
  std::uint64_t sequence = 100;
  const auto identity = [&] { return foundation::Uuid{810, ++sequence}; };
  runtime::World world, renumbered_world;
  editor::SceneDocument original(world, world.LoadScene("Stable source"));
  // Different scene allocation changes every serialized entity ID while keeping authored content.
  static_cast<void>(renumbered_world.LoadScene("Unrelated scene"));
  editor::SceneDocument renumbered(renumbered_world, renumbered_world.LoadScene("Stable source"));
  const auto make = [&](editor::SceneDocument &document) {
    const auto parent = document.Create("Parent"), child = document.Create("Child", parent);
    const std::array keys{*document.Key(parent)};
    Require(
        document.SetTransform(parent, {1, 2, 3}) && document.SetEulerField(keys, 1, 720) &&
            document.SetOpaqueComponent(*document.Key(child), {99, "Unavailable", {0, 255, 27}}),
        "Comparison fixture failed");
    return std::array{parent, child};
  };
  const auto ids = make(original), changed_ids = make(renumbered);
  const auto base = Assets::Capture({811, 1}, original, identity);
  auto shifted = Assets::Capture({811, 1}, renumbered, identity);
  Require(base && shifted && ids != changed_ids, "Runtime ID renumbering fixture failed");
  for (std::size_t i = 0; i < shifted->nodes.size(); ++i) {
    const auto serialized = shifted->nodes[i].serialized_node;
    shifted->nodes[i] = base->nodes[i];
    shifted->nodes[i].serialized_node = serialized;
  }
  shifted->revision = 2;
  Require(Assets::Validate(*shifted), "Remapped stable identity fixture failed");
  std::string error;
  const auto equivalent = editor::ComparePrefabRevisions(&*base, &*shifted, &*base, &error);
  Require(equivalent && equivalent->rows.empty() && error.empty(),
          "Renumbered Runtime IDs fabricated prefab changes");
  Require(renumbered.Rename(*renumbered.Key(changed_ids[1]), "Local child") &&
              renumbered.SetTransform(
                  changed_ids[0],
                  runtime::WithPosition(*renumbered.Transform(changed_ids[0]), 4, 2, 3)) &&
              renumbered.SetOpaqueComponent(*renumbered.Key(changed_ids[1]),
                                            {99, "Unavailable", {0, 255, 28}}),
          "Local edits failed");
  auto local = Assets::Capture(base->id, renumbered, identity, &*shifted);
  Require(local.has_value(), "Local capture failed");
  Require(original.Rename(*original.Key(ids[1]), "Remote child"), "Remote edit failed");
  const auto remote = Assets::Capture(base->id, original, identity, &*base);
  Require(remote.has_value(), "Remote capture failed");
  const auto comparison = editor::ComparePrefabRevisions(&*base, &*local, &*remote);
  Require(comparison && comparison->conflicts == 1 && comparison->rows.size() == 3,
          "Actual field changes/conflict were not independently aligned");
  const auto child_uuid = base->nodes[1].id.ToString();
  const auto name = std::ranges::find_if(
      comparison->rows, [](const auto &row) { return row.stable_path.ends_with("/name"); });
  Require(name != comparison->rows.end() &&
              name->stable_path.find(child_uuid) != std::string::npos && name->base == "Child" &&
              name->local == "Local child" && name->remote == "Remote child" &&
              name->choice == editor::SceneComparisonChoice::Unresolved,
          "Stable name path/conflict payload failed");
  const auto opaque = std::ranges::find_if(
      comparison->rows, [](const auto &row) { return row.stable_path.ends_with("/payload_hex"); });
  Require(opaque != comparison->rows.end() && opaque->base == "00ff1b" &&
              opaque->local == "00ff1c" && opaque->choice == editor::SceneComparisonChoice::Local,
          "Exact unknown payload comparison failed");
  Require(renumbered.Reparent(changed_ids[1], 0), "Parent edit failed");
  const auto reparented = Assets::Capture(base->id, renumbered, identity, &*local);
  Require(reparented.has_value(), "Reparented capture failed");
  const auto parent_diff = editor::ComparePrefabRevisions(&*base, &*reparented, &*base);
  Require(parent_diff.has_value(), "Parent comparison failed");
  const auto parent_row = std::ranges::find_if(
      parent_diff->rows, [](const auto &row) { return row.stable_path.ends_with("/parent"); });
  Require(parent_row != parent_diff->rows.end() &&
              parent_row->base == base->nodes[0].id.ToString() && parent_row->local == "root",
          "Parent values exposed serialized Runtime IDs instead of stable identity");
  auto nested = *base;
  nested.nested = {{{812, 1}, nested.nodes.front().id, {{813, 1}, 5}}};
  const auto metadata = editor::ComparePrefabRevisions(&*base, &nested, &*base);
  Require(metadata && metadata->rows.size() == 4 && metadata->conflicts == 0 &&
              std::ranges::all_of(
                  metadata->rows,
                  [](const auto &row) { return row.stable_path.starts_with("nested/"); }),
          "Nested reference metadata comparison failed");
  auto bad = *base;
  bad.nodes[1].id = bad.nodes[0].id;
  Require(!editor::ComparePrefabRevisions(&bad, &*base, &*base, &error) && !error.empty(),
          "Duplicate stable identity source compared");
  bad = *base;
  bad.scene_bytes = "corrupt";
  Require(!editor::ComparePrefabRevisions(&*base, &bad, nullptr, &error) && !error.empty(),
          "Corrupt present source compared");
  Require(renumbered.Rename(*renumbered.Key(changed_ids[1]), "Unsafe\tlabel"),
          "Unsafe label fixture failed");
  const auto unsafe = Assets::Capture(base->id, renumbered, identity, &*reparented);
  Require(unsafe && !editor::ComparePrefabRevisions(&*base, &*unsafe, &*base),
          "Unsafe comparison display text accepted");
  const auto deleted = editor::ComparePrefabRevisions(&*base, nullptr, &*base);
  Require(deleted && !deleted->rows.empty() && deleted->conflicts == 0 &&
              std::ranges::all_of(deleted->rows, [](const auto &row) { return !row.local; }),
          "Absent source did not retain deletion semantics");
  Require(editor::ComparePrefabRevisions(nullptr, nullptr, nullptr)->rows.empty(),
          "Empty absent comparison failed");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Stable prefab comparisons passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
