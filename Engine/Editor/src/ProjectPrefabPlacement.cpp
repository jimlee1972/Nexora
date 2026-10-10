#include "Nexora/Editor/ProjectPrefabPlacement.h"
namespace nexora::editor {
namespace {
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
bool Allowed(const ProjectWorkspace &workspace, bool write) {
  return !workspace.Root().empty() && !workspace.Project().id.IsNil() &&
         !workspace.HasRecoveryJournal() && !workspace.HasExternalChange() &&
         (!write || workspace.Writable());
}
} // namespace
bool ProjectPrefabPlacement::Matches(const ProjectWorkspace &workspace, const SceneDocument &target,
                                     const ProjectPrefabPlacementReview &review) {
  if (!Allowed(workspace, false) || workspace.Root() != review.root_ ||
      workspace.Project().id != review.project_ || !target.MatchesPreparedSave(review.expected_))
    return false;
  const auto current = PrefabAssets::Load(workspace, review.source_.id);
  if (!current || *current != review.source_)
    return false;
  for (const auto &asset : review.graph_.assets) {
    const auto exact = PrefabAssets::LoadRevision(workspace, {asset.id, asset.revision});
    if (!exact || *exact != asset)
      return false;
  }
  return Allowed(workspace, false) && workspace.Root() == review.root_ &&
         workspace.Project().id == review.project_ && target.MatchesPreparedSave(review.expected_);
}
std::optional<ProjectPrefabPlacementReview>
ProjectPrefabPlacement::Prepare(const ProjectWorkspace &workspace, foundation::Uuid source_id,
                                const SceneDocument &target, std::string *error) {
  if (error)
    error->clear();
  if (!Allowed(workspace, false) || source_id.IsNil()) {
    Fail(error, "Prefab placement requires an open current project without pending recovery.");
    return {};
  }
  auto expected = target.PrepareSave();
  const auto root = workspace.Root();
  const auto project = workspace.Project().id;
  auto source = PrefabAssets::Load(workspace, source_id);
  auto graph = source ? PrefabAssets::ResolveProject(workspace, {source_id, source->revision})
                      : std::nullopt;
  if (!expected || !source || !graph || !graph->expanded_nodes) {
    Fail(error, "Published prefab source, exact closure or target observation is unavailable.");
    return {};
  }
  ProjectPrefabPlacementReview result{root, project, std::move(*source), std::move(*graph),
                                      std::move(*expected)};
  if (!Matches(workspace, target, result)) {
    Fail(error, "Prefab source, project or target changed while preparing placement.");
    return {};
  }
  return result;
}
std::optional<std::vector<InstantiatedPrefabNode>>
ProjectPrefabPlacement::Instantiate(const ProjectWorkspace &workspace, SceneDocument &target,
                                    const ProjectPrefabPlacementReview &review,
                                    foundation::Uuid instance, bool authorized,
                                    std::string *error) {
  if (error)
    error->clear();
  if (!authorized || instance.IsNil() || !Allowed(workspace, true) ||
      !Matches(workspace, target, review) || !Allowed(workspace, true)) {
    Fail(error, "Prefab placement requires current sources, target and authoring authority.");
    return {};
  }
  auto result = PrefabAssets::InstantiateBound(instance, review.Source(), review.graph_.assets,
                                               target, review.expected_, true);
  if (!result)
    Fail(error, "Prefab placement or binding validation rejected the complete import.");
  return result;
}
} // namespace nexora::editor
