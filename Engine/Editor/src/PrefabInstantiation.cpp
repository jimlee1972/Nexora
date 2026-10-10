#include "Nexora/Editor/PrefabAssets.h"
#include <algorithm>
#include <map>

namespace nexora::editor {
std::optional<std::vector<InstantiatedPrefabNode>>
PrefabAssets::Instantiate(PrefabRevisionReference root, std::span<const PrefabAsset> sources,
                          SceneDocument &target, const SceneDocument::PreparedSave &expected,
                          bool authorized) {
  if (!authorized || !target.MatchesPreparedSave(expected))
    return {};
  const auto graph = Resolve(root, sources);
  if (!graph || !graph->expanded_nodes)
    return {};
  runtime::World staging_world;
  SceneDocument staging(staging_world, staging_world.LoadScene("Prefab materialization"));
  std::vector<InstantiatedPrefabNode> result;
  result.reserve(graph->expanded_nodes);
  std::map<runtime::Id, std::size_t> positions;
  for (const auto &instance : graph->instances) {
    const auto asset = std::ranges::find_if(graph->assets, [&](const PrefabAsset &value) {
      return value.id == instance.source.asset && value.revision == instance.source.revision;
    });
    if (asset == graph->assets.end())
      return {};
    runtime::World source_world;
    SceneDocument source(source_world, source_world.LoadScene("Prefab source"));
    if (!source.ReloadBytes(asset->scene_bytes, kMaximumNodes))
      return {};
    runtime::Id attachment{};
    if (instance.attachment) {
      if (instance.scope.empty())
        return {};
      auto parent_scope = instance.scope;
      parent_scope.pop_back();
      const auto parent = std::ranges::find_if(result, [&](const InstantiatedPrefabNode &entry) {
        return entry.scope == parent_scope && entry.node == *instance.attachment;
      });
      if (parent == result.end())
        return {};
      attachment = parent->target.id;
    }
    const auto prepared = staging.PrepareSave();
    if (!prepared || prepared->Bytes().size() > kMaximumSceneBytes)
      return {};
    const auto imported = staging.ImportForestBytes(*prepared, asset->scene_bytes, true);
    if (!imported || imported->size() != asset->nodes.size())
      return {};
    runtime::WorldCommandBuffer attachments;
    for (const auto &entry : *imported) {
      const auto identity =
          std::ranges::find(asset->nodes, entry.source, &PrefabNodeIdentity::serialized_node);
      if (identity == asset->nodes.end() ||
          !positions.emplace(entry.target.id, result.size()).second)
        return {};
      result.push_back({instance.scope, identity->id, entry.target});
      const auto parent = source.Parent(entry.source);
      if (!parent)
        return {};
      if (attachment && !*parent)
        attachments.SetParent(entry.target.id, attachment, false);
    }
    // Only the staging World changes here. Keeping local pose retains authored TRS/Euler lanes.
    if (attachments.Size() && !attachments.Apply(staging_world))
      return {};
  }
  const auto prepared = staging.PrepareSave();
  if (!prepared || prepared->Bytes().size() > kMaximumSceneBytes ||
      result.size() != graph->expanded_nodes || !target.MatchesPreparedSave(expected))
    return {};
  const auto imported = target.ImportForestBytes(expected, prepared->Bytes(), authorized);
  if (!imported)
    return {};
  // ImportForestBytes guarantees one complete source-to-target map. All result storage and
  // lookup nodes already exist before publication; translating keys cannot allocate.
  for (const auto &entry : *imported)
    result[positions.at(entry.source)].target = entry.target;
  return result;
}
} // namespace nexora::editor
