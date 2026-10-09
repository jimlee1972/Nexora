#include "Nexora/Editor/AdditiveSceneSession.h"
#include "Nexora/Editor/SceneSaveBatch.h"

#include <algorithm>

namespace nexora::editor {
namespace {
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
std::string PathKey(const std::filesystem::path &path) {
  const auto bytes = path.lexically_normal().generic_u8string();
  std::string key(bytes.begin(), bytes.end());
  std::ranges::transform(key, key.begin(), [](unsigned char byte) {
    return static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + 32 : byte);
  });
  return key;
}
} // namespace
struct AdditiveSceneSession::Entry final {
  runtime::World &world;
  SceneDocumentId id{};
  bool release{};
  std::unique_ptr<SceneDocument> document_owner;
  std::unique_ptr<SceneFileSession> files_owner;
  SceneDocument *document{};
  SceneFileSession *files{};
  explicit Entry(runtime::World &owner) : world(owner) {}
  ~Entry() {
    files_owner.reset();
    document_owner.reset();
    if (release && id)
      static_cast<void>(world.RemoveEditorScene(id));
  }
};
AdditiveSceneSession::AdditiveSceneSession(const ProjectWorkspace &workspace, runtime::World &world)
    : workspace_(workspace), world_(world), root_(workspace.Root()),
      project_(workspace.Project().id) {}
AdditiveSceneSession::~AdditiveSceneSession() = default;
bool AdditiveSceneSession::Current() const {
  return !root_.empty() && world_.Kind() == runtime::WorldKind::Editor &&
         workspace_.Root() == root_ && workspace_.Project().id == project_;
}
bool AdditiveSceneSession::Allowed(std::string *error) const {
  if (!Current() || workspace_.HasRecoveryJournal() || workspace_.HasExternalChange())
    return Fail(error,
                "Scene composition is stale or recovery/external workspace changes are pending.");
  if (error)
    error->clear();
  return true;
}
bool AdditiveSceneSession::Live(const Entry &entry, SceneFileToken token) const {
  const auto *scene = world_.FindScene(entry.id);
  return Current() && entry.files->Live(token) && scene &&
         scene->state != runtime::SceneState::Unloading &&
         scene->state != runtime::SceneState::Unloaded;
}
AdditiveSceneSession::Entry *AdditiveSceneSession::Find(SceneDocumentId id) const {
  const auto found =
      std::ranges::find_if(entries_, [=](const auto &entry) { return entry->id == id; });
  return found == entries_.end() ? nullptr : found->get();
}
bool AdditiveSceneSession::DistinctPath(const std::optional<std::filesystem::path> &path,
                                        SceneDocumentId ignore) const {
  if (!path)
    return true;
  const auto key = PathKey(*path);
  for (const auto &entry : entries_) {
    if (entry->id == ignore)
      continue;
    const auto other = entry->files->CurrentPath();
    if (!other)
      continue;
    std::error_code error;
    if (PathKey(*other) == key || std::filesystem::equivalent(root_ / *path, root_ / *other, error))
      return false;
  }
  return true;
}
std::optional<SceneDocumentId>
AdditiveSceneSession::Admit(std::unique_ptr<Entry> entry, bool owned,
                            std::vector<SceneDocumentId> dependencies, std::string *error) {
  if (!DistinctPath(entry->files->CurrentPath())) {
    Fail(error, "A scene destination is already open, aliased or ASCII case-colliding.");
    return std::nullopt;
  }
  for (const auto dependency : dependencies) {
    const auto *parent = Find(dependency);
    if (!parent || !Live(*parent, parent->files->Token())) {
      Fail(error, "Scene dependencies must refer to live documents.");
      return std::nullopt;
    }
  }
  const auto path = entry->files->CurrentPath();
  const auto descriptor_path = path ? PathKey(*path) : "untitled:" + std::to_string(entry->id);
  // Build the complete next graph and reserve owner storage before publishing either.
  auto next = graph_;
  if (!next.Add({entry->id, descriptor_path, owned, std::move(dependencies)})) {
    Fail(error, "Scene dependencies must refer to existing distinct documents.");
    return std::nullopt;
  }
  entries_.reserve(entries_.size() + 1);
  const auto id = entry->id;
  entries_.push_back(std::move(entry));
  graph_ = std::move(next);
  active_ = id;
  if (error)
    error->clear();
  return id;
}
std::optional<SceneDocumentId> AdditiveSceneSession::Attach(SceneDocument &document,
                                                            SceneFileSession &files, bool owned,
                                                            std::string *error) {
  if (!Allowed(error) || entries_.size() >= kMaximumDocuments || &document.world_ != &world_ ||
      &files.workspace_ != &workspace_ || &files.document_ != &document ||
      !files.Live(files.Token()) || !world_.FindScene(document.scene_) || Find(document.scene_)) {
    Fail(error, "The primary scene requires a unique live owner in this project and World.");
    return std::nullopt;
  }
  const auto *scene = world_.FindScene(document.scene_);
  if (scene->state == runtime::SceneState::Unloading ||
      scene->state == runtime::SceneState::Unloaded) {
    Fail(error, "The primary authoring scene must have a live lifecycle state.");
    return std::nullopt;
  }
  auto entry = std::make_unique<Entry>(world_);
  entry->id = document.scene_;
  entry->document = &document;
  entry->files = &files;
  return Admit(std::move(entry), owned, {}, error);
}
std::optional<SceneDocumentId> AdditiveSceneSession::Open(const std::filesystem::path &path,
                                                          bool owned,
                                                          std::vector<SceneDocumentId> dependencies,
                                                          std::string *error) {
  if (!Allowed(error) || entries_.size() >= kMaximumDocuments || !DistinctPath(path)) {
    Fail(error, "Scene admission is blocked or this destination is already open.");
    return std::nullopt;
  }
  auto entry = std::make_unique<Entry>(world_);
  entry->id = world_.LoadScene("Additive");
  entry->release = true;
  entry->document_owner = std::make_unique<SceneDocument>(world_, entry->id);
  entry->document = entry->document_owner.get();
  entry->files_owner = std::make_unique<SceneFileSession>(workspace_, *entry->document);
  entry->files = entry->files_owner.get();
  const auto opened = entry->files->Open(entry->files->Token(), path, true);
  if (!opened.Applied() || !world_.Activate(entry->id)) {
    if (error)
      *error = opened.Applied() ? "Scene activation failed." : opened.message;
    return std::nullopt;
  }
  return Admit(std::move(entry), owned, std::move(dependencies), error);
}
std::optional<SceneDocumentId> AdditiveSceneSession::New(std::string name, std::string *error) {
  if (!Allowed(error) || !workspace_.Writable() || entries_.size() >= kMaximumDocuments ||
      name.empty() || name.size() > 256 || !foundation::IsValidUtf8(name) ||
      std::ranges::any_of(name, [](unsigned char c) { return c < 32 || c == 127; })) {
    Fail(error,
         "New additive scenes require a writable project, valid name and available capacity.");
    return std::nullopt;
  }
  auto entry = std::make_unique<Entry>(world_);
  entry->id = world_.LoadScene(std::move(name));
  entry->release = true;
  entry->document_owner = std::make_unique<SceneDocument>(world_, entry->id);
  entry->document = entry->document_owner.get();
  if (!entry->document->NewScene() || !world_.Activate(entry->id)) {
    Fail(error, "The new additive scene could not be initialized.");
    return std::nullopt;
  }
  entry->files_owner = std::make_unique<SceneFileSession>(workspace_, *entry->document);
  entry->files = entry->files_owner.get();
  return Admit(std::move(entry), true, {}, error);
}
bool AdditiveSceneSession::Select(SceneDocumentId id, SceneFileToken token, std::string *error) {
  const auto *entry = Find(id);
  if (!Allowed(error) || !entry || !Live(*entry, token))
    return Fail(error, "Choose a current additive scene document.");
  active_ = id;
  return true;
}
bool AdditiveSceneSession::SetDependencies(SceneDocumentId id, SceneFileToken token,
                                           std::vector<SceneDocumentId> dependencies,
                                           std::string *error) {
  const auto *entry = Find(id);
  for (const auto dependency : dependencies) {
    const auto *parent = Find(dependency);
    if (!parent || !Live(*parent, parent->files->Token()))
      return Fail(error, "Scene dependencies must refer to live documents.");
  }
  if (!Allowed(error) || !entry || !Live(*entry, token) ||
      !graph_.SetDependencies(id, std::move(dependencies)))
    return Fail(error, "Scene dependencies must be live, acyclic and complete.");
  return true;
}
bool AdditiveSceneSession::Remove(SceneDocumentId id, SceneFileToken token, bool discard_dirty,
                                  std::string *error) {
  const auto *entry = Find(id);
  if (!Allowed(error) || !entry || !Live(*entry, token) ||
      (entry->document->Dirty() && !discard_dirty))
    return Fail(error, "Save or explicitly discard this live document before closing it.");
  auto next = graph_;
  if (!next.Remove(id))
    return Fail(error, "Other open scenes depend on this document.");
  const auto remaining = next.LoadOrder();
  const auto next_active = active_ == id ? (remaining.empty() ? 0 : remaining.front()) : active_;
  std::erase_if(entries_, [=](const auto &candidate) { return candidate->id == id; });
  graph_ = std::move(next);
  active_ = next_active;
  return true;
}
std::optional<SceneDocumentId> AdditiveSceneSession::Active() const {
  const auto *entry = Find(active_);
  return entry && Live(*entry, entry->files->Token()) ? std::optional<SceneDocumentId>(active_)
                                                      : std::nullopt;
}
std::vector<AdditiveDocumentView> AdditiveSceneSession::Snapshot() const {
  std::vector<AdditiveDocumentView> result;
  if (!Current())
    return result;
  for (const auto id : graph_.LoadOrder()) {
    const auto *entry = Find(id);
    const auto *descriptor = graph_.Find(id);
    result.push_back({id, entry->files->Token(), entry->files->CurrentPath(), descriptor->owned,
                      entry->document->Dirty(),
                      entry->files->SaveBlocked() || !Live(*entry, entry->files->Token()),
                      descriptor->dependencies});
  }
  return result;
}
const SceneDocument *AdditiveSceneSession::Document(SceneDocumentId id) const {
  const auto *entry = Find(id);
  return entry && Live(*entry, entry->files->Token()) ? entry->document : nullptr;
}
const SceneFileSession *AdditiveSceneSession::Files(SceneDocumentId id) const {
  const auto *entry = Find(id);
  return Document(id) ? entry->files : nullptr;
}
SceneDocument *AdditiveSceneSession::EditableDocument(SceneDocumentId id) {
  const auto *descriptor = graph_.Find(id);
  return descriptor && descriptor->owned && workspace_.Writable() && Allowed(nullptr) &&
                 Document(id)
             ? Find(id)->document
             : nullptr;
}
SceneFileSession *AdditiveSceneSession::WritableFiles(SceneDocumentId id) {
  return EditableDocument(id) ? Find(id)->files : nullptr;
}
SceneFileResult AdditiveSceneSession::SaveAs(SceneDocumentId id, SceneFileToken token,
                                             const std::filesystem::path &path,
                                             bool replace_existing,
                                             std::optional<SceneOverwriteToken> overwrite_token) {
  auto *files = WritableFiles(id);
  const auto destination = files ? files->Resolve(path) : std::nullopt;
  if (!files || !files->Live(token) || !destination ||
      !DistinctPath(destination->lexically_relative(root_), id))
    return {
        SceneFileStatus::Rejected,
        "Save As requires a live owned document and a destination not used by another open scene."};
  return files->SaveAs(token, path, replace_existing, overwrite_token);
}
SceneSaveBatchResult AdditiveSceneSession::SaveAll() {
  if (!Allowed(nullptr) || !workspace_.Writable())
    return {SceneSaveBatchStatus::Rejected,
            "Save All needs the current writer and resolved recovery."};
  std::vector<SceneFileSession *> files;
  for (const auto id : graph_.LoadOrder()) {
    const auto *entry = Find(id);
    if (!Live(*entry, entry->files->Token()))
      return {SceneSaveBatchStatus::Rejected, "An open composition document is no longer live."};
    if (graph_.Find(id)->owned)
      files.push_back(entry->files);
  }
  if (files.empty())
    return {SceneSaveBatchStatus::Published, "No owned scene files to save."};
  SceneSaveBatch batch(workspace_);
  std::string error;
  if (!batch.Prepare(files, &error))
    return {SceneSaveBatchStatus::Rejected, std::move(error)};
  return batch.Publish();
}
} // namespace nexora::editor
