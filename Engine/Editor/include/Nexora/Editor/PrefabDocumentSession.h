#pragma once
#include "Nexora/Editor/PrefabAssets.h"
#include <memory>

namespace nexora::editor {
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
