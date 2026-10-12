#pragma once
#include "Nexora/Editor/PrefabAssets.h"

namespace nexora::editor {
// Owning read-only project/source/target observation. No source/World/workspace borrow or
// authority.
class NEXORA_EDITOR_API ProjectPrefabPlacementReview final {
public:
  [[nodiscard]] PrefabRevisionReference Source() const noexcept {
    return {source_.id, source_.revision};
  }
  [[nodiscard]] std::size_t NodeCount() const noexcept { return graph_.expanded_nodes; }

private:
  friend class ProjectPrefabPlacement;
  ProjectPrefabPlacementReview(std::filesystem::path root, foundation::Uuid project,
                               PrefabAsset source, ResolvedPrefabGraph graph,
                               SceneDocument::PreparedSave expected)
      : root_(std::move(root)), project_(project), source_(std::move(source)),
        graph_(std::move(graph)), expected_(std::move(expected)) {}
  std::filesystem::path root_;
  foundation::Uuid project_;
  PrefabAsset source_;
  ResolvedPrefabGraph graph_;
  SceneDocument::PreparedSave expected_;
};
// Authoring-thread calls serialize source/project/target owners. Prepared observations confer no
// authority; the host repeats current Play/modal/document role policy at the explicit action.
class NEXORA_EDITOR_API ProjectPrefabPlacement final {
public:
  [[nodiscard]] static std::optional<ProjectPrefabPlacementReview>
  Prepare(const ProjectWorkspace &, foundation::Uuid source, const SceneDocument &,
          std::string *error = nullptr);
  [[nodiscard]] static std::optional<std::vector<InstantiatedPrefabNode>>
  Instantiate(const ProjectWorkspace &, SceneDocument &, const ProjectPrefabPlacementReview &,
              foundation::Uuid instance, bool authorized, std::string *error = nullptr);

private:
  static bool Matches(const ProjectWorkspace &, const SceneDocument &,
                      const ProjectPrefabPlacementReview &);
};
} // namespace nexora::editor
