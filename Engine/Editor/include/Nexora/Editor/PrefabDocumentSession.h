#pragma once
#include "Nexora/Editor/PrefabAssets.h"
#include "Nexora/Editor/SceneComparison.h"
#include <memory>

namespace nexora::editor {
// Owning inspection result. Its contents confer no write authority; Revert rechecks all scope,
// document and immutable source observations before publishing a property transaction.
class NEXORA_EDITOR_API PrefabPropertyReview final {
public:
  [[nodiscard]] const SceneComparison &Changes() const noexcept { return changes_; }
  [[nodiscard]] bool CanRevert() const noexcept { return candidate_.has_value(); }

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
  bool Revert(const PrefabPropertyReview &, bool authorized, std::string *error = nullptr);
  [[nodiscard]] const SceneDocument *Document() const;
  [[nodiscard]] SceneDocument *EditableDocument();
  [[nodiscard]] foundation::Uuid AssetId() const;
  // Owning observation of the last published source (the base for an unsaved variant).
  [[nodiscard]] std::optional<PrefabAsset> SourceBaseline() const;
  [[nodiscard]] bool Dirty() const;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  struct Owner;
  bool Current() const;
  bool Allowed(bool write, std::string *) const;
  bool Replaceable(bool discard_dirty, std::string *) const;
  const ProjectWorkspace &workspace_;
  std::filesystem::path root_;
  foundation::Uuid project_;
  std::unique_ptr<Owner> owner_;
  std::uint64_t generation_{1};
  bool busy_{};
};
} // namespace nexora::editor
