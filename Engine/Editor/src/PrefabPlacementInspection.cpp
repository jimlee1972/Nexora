#include "Nexora/Editor/PrefabPlacementInspection.h"
#include <algorithm>
#include <map>

namespace nexora::editor {
namespace {
bool Allowed(const ProjectWorkspace &workspace) {
  return !workspace.Root().empty() && !workspace.Project().id.IsNil() &&
         !workspace.HasRecoveryJournal() && !workspace.HasExternalChange();
}
auto Path(std::span<const foundation::Uuid> scope, foundation::Uuid node) {
  std::vector<std::uint64_t> result;
  result.reserve(scope.size() * 2 + 2);
  for (const auto id : scope) {
    result.push_back(id.high);
    result.push_back(id.low);
  }
  result.push_back(node.high);
  result.push_back(node.low);
  return result;
}
std::optional<PrefabRevisionReference>
ValidateMapping(const SceneDocument::PrefabPlacement &placement,
                const SceneDocument::PrefabPlacementNode &selected,
                const ResolvedPrefabGraph &graph) {
  if (placement.nodes.size() != graph.expanded_nodes)
    return {};
  std::map<std::vector<std::uint64_t>, PrefabRevisionReference> expected;
  for (const auto &instance : graph.instances) {
    const auto asset = std::ranges::find_if(graph.assets, [&](const auto &value) {
      return value.id == instance.source.asset && value.revision == instance.source.revision;
    });
    if (asset == graph.assets.end())
      return {};
    for (const auto &node : asset->nodes)
      if (!expected.emplace(Path(instance.scope, node.id), instance.source).second)
        return {};
  }
  std::optional<PrefabRevisionReference> result;
  for (const auto &node : placement.nodes) {
    const auto found = expected.find(Path(node.scope, node.source_node));
    if (found == expected.end())
      return {};
    if (node == selected)
      result = found->second;
    expected.erase(found);
  }
  return expected.empty() ? result : std::nullopt;
}
} // namespace

std::optional<PrefabPlacementInspection>
PrefabPlacementInspector::Inspect(const ProjectWorkspace &workspace, const SceneDocument &target,
                                  SceneDocument::NodeKey key) {
  if (!Allowed(workspace) || target.Key(key.id) != key)
    return {};
  const auto placements = target.PrefabPlacements();
  const auto placement = std::ranges::find_if(placements, [&](const auto &value) {
    return std::ranges::any_of(value.nodes,
                               [&](const auto &node) { return node.target == key.id; });
  });
  if (placement == placements.end())
    return {};
  const auto node =
      std::ranges::find(placement->nodes, key.id, &SceneDocument::PrefabPlacementNode::target);
  auto expected = target.PrepareSave();
  if (!expected)
    return {};
  const auto root = workspace.Root();
  const auto project = workspace.Project().id;
  auto owned_placement = *placement;
  auto owned_node = *node;
  auto current = PrefabAssets::Load(workspace, placement->source);
  auto graph = PrefabAssets::ResolveProject(workspace, {placement->source, placement->revision});
  auto scoped = graph ? ValidateMapping(owned_placement, owned_node, *graph) : std::nullopt;
  if (!scoped)
    graph.reset();
  PrefabPlacementInspection result{root,
                                   project,
                                   key,
                                   std::move(owned_placement),
                                   std::move(owned_node),
                                   std::move(*expected),
                                   std::move(current),
                                   std::move(graph),
                                   scoped};
  if (!Matches(workspace, target, result))
    return {};
  return result;
}
bool PrefabPlacementInspector::Matches(const ProjectWorkspace &workspace,
                                       const SceneDocument &target,
                                       const PrefabPlacementInspection &inspection) {
  if (!Allowed(workspace) || workspace.Root() != inspection.root_ ||
      workspace.Project().id != inspection.project_ ||
      target.Key(inspection.target_.id) != inspection.target_ ||
      !target.MatchesPreparedSave(inspection.expected_) ||
      PrefabAssets::Load(workspace, inspection.placement_.source) != inspection.current_)
    return false;
  if (inspection.graph_) {
    for (const auto &asset : inspection.graph_->assets) {
      const auto actual = PrefabAssets::LoadRevision(workspace, {asset.id, asset.revision});
      if (!actual || *actual != asset)
        return false;
    }
  } else {
    const auto graph = PrefabAssets::ResolveProject(workspace, inspection.Source());
    if (graph && ValidateMapping(inspection.placement_, inspection.node_, *graph))
      return false;
  }
  return Allowed(workspace) && workspace.Root() == inspection.root_ &&
         workspace.Project().id == inspection.project_ &&
         target.MatchesPreparedSave(inspection.expected_);
}
} // namespace nexora::editor
