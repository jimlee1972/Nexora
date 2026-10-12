#include "Nexora/Editor/PrefabRebase.h"
#include "Nexora/Editor/PrefabComparison.h"
#include <algorithm>
#include <array>
#include <map>
namespace nexora::editor {
namespace {
using GroupKey = std::array<std::uint64_t, 4>;
GroupKey Key(foundation::Uuid node, foundation::Uuid field) {
  return {node.high, node.low, field.high, field.low};
}
struct Group final {
  bool local_changed{}, source_changed{}, equal{true};
};
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
std::optional<GroupKey> Field(std::string_view path) {
  if (!path.starts_with("nodes/") || path.size() <= 87 || path.substr(42, 8) != "/fields/")
    return {};
  const auto node = foundation::Uuid::Parse(path.substr(6, 36));
  const auto field = foundation::Uuid::Parse(path.substr(50, 36));
  if (!node || !field || path[86] != '/')
    return {};
  return Key(node.Value(), field.Value());
}
} // namespace
std::optional<PrefabRebasePlan>
BuildPrefabRebasePlan(const PrefabAsset &base, const PrefabAsset &local, const PrefabAsset &source,
                      std::span<const PrefabRebaseChoice> choices, std::string *error) {
  if (error)
    error->clear();
  if (base.id != source.id || source.revision <= base.revision || local.id == source.id ||
      choices.size() > PrefabAssets::kMaximumProperties || base.nested != source.nested ||
      local.nested != base.nested || !BuildPrefabPropertySnapshot(local, base) ||
      !BuildPrefabPropertySnapshot(local, source)) {
    Fail(error, "Rebase requires newer same-source revisions and compatible stable structure.");
    return {};
  }
  // Field additions/removals need persistent identity reconciliation, handled separately.
  for (const auto &original : base.nodes) {
    const auto signature = [](const PrefabNodeIdentity &node) {
      std::map<std::string, std::array<std::uint64_t, 2>> fields;
      for (const auto &field : node.properties)
        fields.emplace(field.field, std::array{field.id.high, field.id.low});
      return fields;
    };
    const auto expected = signature(original);
    for (const auto *asset : {&local, &source}) {
      const auto found = std::ranges::find(asset->nodes, original.id, &PrefabNodeIdentity::id);
      if (found == asset->nodes.end() || signature(*found) != expected) {
        Fail(error, "Rebase cannot add, remove or replace stable field identities.");
        return {};
      }
    }
  }
  auto comparison = ComparePrefabRevisions(&base, &local, &source, error);
  if (!comparison)
    return {};
  std::map<GroupKey, Group> groups;
  for (const auto &row : comparison->rows) {
    if (row.stable_path == "prefab/base")
      continue;
    const auto field = Field(row.stable_path);
    if (!field) {
      Fail(error, "Rebase cannot reconcile changed scene, node or nested metadata.");
      return {};
    }
    auto &group = groups[*field];
    group.local_changed |= row.local != row.base;
    group.source_changed |= row.remote != row.base;
    group.equal &= row.local == row.remote;
  }
  std::map<GroupKey, PrefabRebaseDecision> decisions;
  for (const auto &choice : choices) {
    const auto key = Key(choice.property.node, choice.property.field);
    const auto found = groups.find(key);
    if (found == groups.end() || !found->second.local_changed || !found->second.source_changed ||
        found->second.equal ||
        (choice.decision != PrefabRebaseDecision::KeepLocal &&
         choice.decision != PrefabRebaseDecision::TakeSource) ||
        !decisions.emplace(key, choice.decision).second) {
      Fail(error, "Choose each actual conflicting complete field group at most once.");
      return {};
    }
  }
  PrefabRebasePlan plan{std::move(*comparison)};
  std::vector<PrefabPropertySelection> selected;
  for (const auto &[key, group] : groups) {
    const PrefabPropertySelection property{{key[0], key[1]}, {key[2], key[3]}};
    if (group.local_changed && group.source_changed && !group.equal) {
      plan.conflicts.push_back(property);
      const auto choice = decisions.find(key);
      if (choice == decisions.end())
        ++plan.unresolved;
      else if (choice->second == PrefabRebaseDecision::TakeSource)
        selected.push_back(property);
    } else if (group.source_changed && !group.local_changed)
      selected.push_back(property);
  }
  if (!plan.unresolved) {
    plan.candidate = BuildPrefabPropertySnapshot(local, source, selected);
    if (!plan.candidate) {
      Fail(error, "Chosen groups form an incompatible property hierarchy.");
      return {};
    }
  }
  return plan;
}
} // namespace nexora::editor
