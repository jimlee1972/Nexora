#pragma once

#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/Editor/SceneFiles.h"
#include "Nexora/Editor/SceneSaveBatch.h"

#include <memory>

namespace nexora::editor {
struct AdditiveDocumentView final {
  SceneDocumentId id{};
  SceneFileToken token{};
  std::optional<std::filesystem::path> path;
  bool owned{}, dirty{}, save_blocked{};
  std::vector<SceneDocumentId> dependencies;
};

// Borrows one workspace and Editor World, which must outlive this owner. The host serializes calls,
// stops Play and drains background readers before membership changes or publication. Document/file
// borrows survive other admissions and active switches; removal or destruction expires them.
// References are inspection-only through this API. Permissions are host policy, not a sandbox.
class NEXORA_EDITOR_API AdditiveSceneSession final {
public:
  static constexpr std::size_t kMaximumDocuments = 16;
  AdditiveSceneSession(const ProjectWorkspace &, runtime::World &);
  ~AdditiveSceneSession();
  AdditiveSceneSession(const AdditiveSceneSession &) = delete;
  AdditiveSceneSession &operator=(const AdditiveSceneSession &) = delete;
  // Existing primary document/file owners must outlive this session. Detaching does not release
  // their World scene. Named/unnamed association and history remain owned by the original caller.
  [[nodiscard]] std::optional<SceneDocumentId>
  Attach(SceneDocument &, SceneFileSession &, bool owned = true, std::string *error = nullptr);
  [[nodiscard]] std::optional<SceneDocumentId> Open(const std::filesystem::path &,
                                                    bool owned = true,
                                                    std::vector<SceneDocumentId> dependencies = {},
                                                    std::string *error = nullptr);
  [[nodiscard]] std::optional<SceneDocumentId> New(std::string name, std::string *error = nullptr);
  bool Select(SceneDocumentId, SceneFileToken, std::string *error = nullptr);
  bool SetDependencies(SceneDocumentId, SceneFileToken, std::vector<SceneDocumentId>,
                       std::string *error = nullptr);
  bool Remove(SceneDocumentId, SceneFileToken, bool discard_dirty = false,
              std::string *error = nullptr);
  [[nodiscard]] std::optional<SceneDocumentId> Active() const;
  [[nodiscard]] std::vector<AdditiveDocumentView> Snapshot() const;
  [[nodiscard]] const SceneDocument *Document(SceneDocumentId) const;
  [[nodiscard]] const SceneFileSession *Files(SceneDocumentId) const;
  [[nodiscard]] SceneDocument *EditableDocument(SceneDocumentId);
  [[nodiscard]] SceneFileSession *WritableFiles(SceneDocumentId);
  SceneFileResult SaveAs(SceneDocumentId, SceneFileToken, const std::filesystem::path &,
                         bool replace_existing = false,
                         std::optional<SceneOverwriteToken> overwrite_token = std::nullopt);
  [[nodiscard]] SceneSaveBatchResult SaveAll();

private:
  struct Entry;
  [[nodiscard]] Entry *Find(SceneDocumentId) const;
  [[nodiscard]] bool Live(const Entry &, SceneFileToken) const;
  [[nodiscard]] bool Current() const;
  [[nodiscard]] bool Allowed(std::string *error) const;
  [[nodiscard]] bool DistinctPath(const std::optional<std::filesystem::path> &,
                                  SceneDocumentId ignore = 0) const;
  [[nodiscard]] std::optional<SceneDocumentId> Admit(std::unique_ptr<Entry>, bool,
                                                     std::vector<SceneDocumentId>, std::string *);
  const ProjectWorkspace &workspace_;
  runtime::World &world_;
  std::filesystem::path root_;
  foundation::Uuid project_;
  AdditiveSceneGraph graph_;
  std::vector<std::unique_ptr<Entry>> entries_;
  SceneDocumentId active_{};
};
} // namespace nexora::editor
