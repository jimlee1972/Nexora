#pragma once
#include "Nexora/Editor/PrefabAssets.h"

namespace nexora::editor {
// Owning observation only. Missing/corrupt sources retain inspectable binding identity,
// never authoring authority. SourceScope's borrow expires with this observation.
class NEXORA_EDITOR_API PrefabPlacementInspection final {
public:
  [[nodiscard]] foundation::Uuid Instance() const noexcept { return placement_.instance; }
  [[nodiscard]] PrefabRevisionReference Source() const noexcept {
    return {placement_.source, placement_.revision};
  }
  [[nodiscard]] foundation::Uuid SourceNode() const noexcept { return node_.source_node; }
  [[nodiscard]] std::span<const foundation::Uuid> SourceScope() const noexcept {
    return node_.scope;
  }
  [[nodiscard]] std::optional<PrefabRevisionReference> ScopedSource() const noexcept {
    return scoped_source_;
  }
  [[nodiscard]] std::optional<std::uint64_t> PublishedRevision() const noexcept {
    return current_ ? std::optional(current_->revision) : std::nullopt;
  }
  [[nodiscard]] bool Resolved() const noexcept { return graph_.has_value(); }
  [[nodiscard]] std::size_t MappedNodes() const noexcept { return placement_.nodes.size(); }

private:
  friend class PrefabPlacementInspector;
  PrefabPlacementInspection(std::filesystem::path root, foundation::Uuid project,
                            SceneDocument::NodeKey target, SceneDocument::PrefabPlacement placement,
                            SceneDocument::PrefabPlacementNode node,
                            SceneDocument::PreparedSave expected,
                            std::optional<PrefabAsset> current,
                            std::optional<ResolvedPrefabGraph> graph,
                            std::optional<PrefabRevisionReference> scoped_source)
      : root_(std::move(root)), project_(project), target_(target),
        placement_(std::move(placement)), node_(std::move(node)), expected_(std::move(expected)),
        current_(std::move(current)), graph_(std::move(graph)), scoped_source_(scoped_source) {}
  std::filesystem::path root_;
  foundation::Uuid project_;
  SceneDocument::NodeKey target_;
  SceneDocument::PrefabPlacement placement_;
  SceneDocument::PrefabPlacementNode node_;
  SceneDocument::PreparedSave expected_;
  std::optional<PrefabAsset> current_;
  std::optional<ResolvedPrefabGraph> graph_;
  std::optional<PrefabRevisionReference> scoped_source_;
};
// Explicit synchronous authoring-thread inspection, not per-frame IO or a filesystem lease.
class NEXORA_EDITOR_API PrefabPlacementInspector final {
public:
  [[nodiscard]] static std::optional<PrefabPlacementInspection>
  Inspect(const ProjectWorkspace &, const SceneDocument &, SceneDocument::NodeKey);
  [[nodiscard]] static bool Matches(const ProjectWorkspace &, const SceneDocument &,
                                    const PrefabPlacementInspection &);
};
} // namespace nexora::editor
