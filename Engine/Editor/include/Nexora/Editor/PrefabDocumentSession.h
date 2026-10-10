#pragma once
#include "Nexora/Editor/PrefabAssets.h"
#include "Nexora/Editor/PrefabPropertyPlan.h"
#include "Nexora/Editor/PrefabRebase.h"
#include "Nexora/Editor/SceneComparison.h"
#include <memory>

namespace nexora::editor {
// Owning inspection result. Its contents confer no write authority; Revert rechecks all scope,
// document and immutable source observations before publishing a property transaction.
class NEXORA_EDITOR_API PrefabPropertyReview final {
public:
  [[nodiscard]] const SceneComparison &Changes() const noexcept { return changes_; }
  [[nodiscard]] bool CanRevert() const noexcept { return candidate_.has_value(); }
  [[nodiscard]] bool CanApplyToSource() const noexcept {
    return candidate_ && previous_.id == asset_ && source_.id != asset_;
  }
  [[nodiscard]] bool Targeted() const noexcept { return targeted_; }
  [[nodiscard]] std::span<const PrefabPropertySelection> Selections() const noexcept {
    return selections_;
  }

private:
  friend class PrefabDocumentSession;
  PrefabPropertyReview(SceneDocument::PreparedSave expected, PrefabAsset previous,
                       PrefabAsset source, SceneComparison changes,
                       std::optional<std::string> candidate, std::filesystem::path root,
                       foundation::Uuid project, foundation::Uuid asset, std::uint64_t generation)
      : expected_(std::move(expected)), previous_(std::move(previous)), source_(std::move(source)),
        changes_(std::move(changes)), candidate_(std::move(candidate)), root_(std::move(root)),
        project_(project), asset_(asset), generation_(generation) {}
  SceneDocument::PreparedSave expected_;
  PrefabAsset previous_, source_;
  SceneComparison changes_;
  std::optional<std::string> candidate_;
  std::filesystem::path root_;
  foundation::Uuid project_, asset_;
  std::uint64_t generation_{};
  std::vector<PrefabPropertySelection> selections_{};
  bool targeted_{};
};
// Owning three-way review. Every source closure is rechecked before choices or application;
// successful application changes properties and reference in one history entry without IO.
class NEXORA_EDITOR_API PrefabRebaseReview final {
public:
  [[nodiscard]] const SceneComparison &Changes() const noexcept { return plan_.changes; }
  [[nodiscard]] std::span<const PrefabPropertySelection> Conflicts() const noexcept {
    return plan_.conflicts;
  }
  [[nodiscard]] std::size_t Unresolved() const noexcept { return plan_.unresolved; }
  [[nodiscard]] bool CanApply() const noexcept { return plan_.candidate.has_value(); }
  [[nodiscard]] PrefabRevisionReference PreviousReference() const noexcept {
    return {base_.id, base_.revision};
  }
  [[nodiscard]] PrefabRevisionReference NextReference() const noexcept {
    return {source_.id, source_.revision};
  }

private:
  friend class PrefabDocumentSession;
  PrefabRebaseReview(SceneDocument::PreparedSave expected, PrefabAsset previous, PrefabAsset base,
                     PrefabAsset local, PrefabAsset source, PrefabRebasePlan plan,
                     std::filesystem::path root, foundation::Uuid project, foundation::Uuid asset,
                     std::uint64_t generation, ResolvedPrefabGraph base_graph,
                     ResolvedPrefabGraph source_graph)
      : expected_(std::move(expected)), previous_(std::move(previous)), base_(std::move(base)),
        local_(std::move(local)), source_(std::move(source)), plan_(std::move(plan)),
        root_(std::move(root)), project_(project), asset_(asset), generation_(generation),
        base_graph_(std::move(base_graph)), source_graph_(std::move(source_graph)) {}
  SceneDocument::PreparedSave expected_;
  PrefabAsset previous_, base_, local_, source_;
  PrefabRebasePlan plan_;
  std::filesystem::path root_;
  foundation::Uuid project_, asset_;
  std::uint64_t generation_{};
  ResolvedPrefabGraph base_graph_, source_graph_;
};

// Serialized authoring owner. Workspace outlives this session. Document borrows expire on
// successful replacement, Close or destruction; no source World/document borrows are retained.
class NEXORA_EDITOR_API PrefabDocumentSession final {
public:
  explicit PrefabDocumentSession(const ProjectWorkspace &);
  ~PrefabDocumentSession();
  PrefabDocumentSession(const PrefabDocumentSession &) = delete;
  PrefabDocumentSession &operator=(const PrefabDocumentSession &) = delete;
  bool Open(foundation::Uuid, bool discard_dirty = false, std::string *error = nullptr);
  bool Create(foundation::Uuid, const SceneDocument &, bool discard_dirty = false,
              std::string *error = nullptr);
  bool Variant(foundation::Uuid, std::string *error = nullptr);
  bool Save(const std::function<foundation::Uuid()> &, std::string *error = nullptr);
  bool Close(bool discard_dirty = false, std::string *error = nullptr);
  [[nodiscard]] std::optional<PrefabPropertyReview> Review(std::string *error = nullptr) const;
  [[nodiscard]] std::optional<PrefabPropertyReview>
  SelectReview(const PrefabPropertyReview &, std::span<const PrefabPropertySelection>,
               std::string *error = nullptr) const;
  bool Revert(const PrefabPropertyReview &, bool authorized, std::string *error = nullptr);
  // Explicit saved-variant source publication. Advances only the exact current source; isolated
  // document/history/baseline and retained base reference remain unchanged until explicit rebase.
  [[nodiscard]] std::optional<PrefabAsset>
  ApplyToSource(const PrefabPropertyReview &, bool authorized, std::string *error = nullptr);

  [[nodiscard]] std::optional<PrefabRebaseReview> ReviewRebase(std::string *error = nullptr) const;
  [[nodiscard]] std::optional<PrefabRebaseReview> ResolveRebase(const PrefabRebaseReview &,
                                                                std::span<const PrefabRebaseChoice>,
                                                                std::string *error = nullptr) const;
  bool Rebase(const PrefabRebaseReview &, bool authorized, std::string *error = nullptr);
  [[nodiscard]] const SceneDocument *Document() const;
  [[nodiscard]] SceneDocument *EditableDocument();
  [[nodiscard]] foundation::Uuid AssetId() const;
  // Owning observation of the last published source (the base for an unsaved variant).
  [[nodiscard]] std::optional<PrefabAsset> SourceBaseline() const;
  // Current owning authoring reference, including unsaved undoable reference changes.
  [[nodiscard]] std::optional<PrefabRevisionReference> BaseReference() const;
  [[nodiscard]] bool Dirty() const;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  struct Owner;
  bool Current() const;
  bool Allowed(bool write, std::string *) const;
  bool Replaceable(bool discard_dirty, std::string *) const;
  bool MatchesReview(const PrefabPropertyReview &) const;
  bool MatchesRebase(const PrefabRebaseReview &) const;
  const ProjectWorkspace &workspace_;
  std::filesystem::path root_;
  foundation::Uuid project_;
  std::unique_ptr<Owner> owner_;
  std::uint64_t generation_{1};
  bool busy_{};
};
} // namespace nexora::editor
