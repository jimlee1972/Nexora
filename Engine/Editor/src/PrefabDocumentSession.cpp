#include "Nexora/Editor/PrefabDocumentSession.h"
#include <limits>
#include <utility>

namespace nexora::editor {
namespace {
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
struct Busy final {
  bool &value;
  explicit Busy(bool &flag) : value(flag) { value = true; }
  ~Busy() { value = false; }
};
} // namespace
struct PrefabDocumentSession::Owner final {
  runtime::World world;
  SceneDocument document{world, world.LoadScene("Prefab isolation")};
  foundation::Uuid id;
  std::optional<PrefabAsset> previous;
  bool published{};
};
PrefabDocumentSession::PrefabDocumentSession(const ProjectWorkspace &workspace)
    : workspace_(workspace), root_(workspace.Root()), project_(workspace.Project().id) {}
PrefabDocumentSession::~PrefabDocumentSession() = default;
bool PrefabDocumentSession::Current() const {
  return !root_.empty() && !project_.IsNil() && workspace_.Root() == root_ &&
         workspace_.Project().id == project_;
}
bool PrefabDocumentSession::Allowed(bool write, std::string *error) const {
  if (error)
    error->clear();
  if (busy_ || !Current() || workspace_.HasRecoveryJournal() || workspace_.HasExternalChange())
    return Fail(error, "Prefab scope is busy, stale or requires recovery.");
  if (write && !workspace_.Writable())
    return Fail(error, "Prefab authoring requires the project writer lease.");
  return true;
}
bool PrefabDocumentSession::Replaceable(bool discard_dirty, std::string *error) const {
  if (generation_ == std::numeric_limits<std::uint64_t>::max())
    return Fail(error, "Prefab session generation is exhausted.");
  return discard_dirty || !Dirty() ||
         Fail(error, "Preserve unsaved prefab edits before replacement.");
}
bool PrefabDocumentSession::Open(foundation::Uuid id, bool discard_dirty, std::string *error) {
  if (!Allowed(false, error) || !Replaceable(discard_dirty, error))
    return false;
  auto source = PrefabAssets::Load(workspace_, id);
  if (!source)
    return Fail(error, "Prefab source is unavailable or invalid.");
  auto candidate = std::make_unique<Owner>();
  if (!candidate->document.ReloadBytes(source->scene_bytes, PrefabAssets::kMaximumNodes))
    return Fail(error, "Prefab scene could not be isolated.");
  candidate->id = id;
  candidate->previous = std::move(source);
  candidate->published = true;
  owner_ = std::move(candidate);
  ++generation_;
  return true;
}
bool PrefabDocumentSession::Create(foundation::Uuid id, const SceneDocument &source,
                                   bool discard_dirty, std::string *error) {
  if (!Allowed(true, error) || !Replaceable(discard_dirty, error))
    return false;
  const auto prepared = source.PrepareSave();
  if (id.IsNil() || !prepared || prepared->Bytes().size() > PrefabAssets::kMaximumSceneBytes)
    return Fail(error, "Prefab draft identity or scene budget is invalid.");
  auto candidate = std::make_unique<Owner>();
  if (!candidate->document.ReloadBytes(prepared->Bytes(), PrefabAssets::kMaximumNodes))
    return Fail(error, "Prefab source could not be isolated.");
  candidate->id = id;
  owner_ = std::move(candidate);
  ++generation_;
  return true;
}
bool PrefabDocumentSession::Variant(foundation::Uuid id, std::string *error) {
  if (!Allowed(true, error))
    return false;
  if (!owner_ || Dirty() || !owner_->published || !owner_->previous || id.IsNil() ||
      id == owner_->id || generation_ == std::numeric_limits<std::uint64_t>::max())
    return Fail(error, "Variant requires a saved source and distinct nonnil identity.");
  const auto current = PrefabAssets::Load(workspace_, owner_->id);
  if (!current || *current != *owner_->previous)
    return Fail(error, "Variant base changed; preserve the isolated document.");
  const auto prepared = owner_->document.PrepareSave();
  if (!prepared || prepared->Bytes().size() > PrefabAssets::kMaximumSceneBytes)
    return Fail(error, "Variant source exceeds the supported scene budget.");
  auto candidate = std::make_unique<Owner>();
  if (!candidate->document.ReloadBytes(prepared->Bytes(), PrefabAssets::kMaximumNodes))
    return Fail(error, "Variant source could not be isolated.");
  candidate->id = id;
  candidate->previous = owner_->previous;
  owner_ = std::move(candidate);
  ++generation_;
  return true;
}
bool PrefabDocumentSession::Save(const std::function<foundation::Uuid()> &identity,
                                 std::string *error) {
  if (!Allowed(true, error))
    return false;
  if (!owner_)
    return Fail(error, "No isolated prefab document is open.");
  // Identity callbacks cannot replace/destroy the document borrowed by SaveDocument.
  Busy guard(busy_);
  auto saved = PrefabAssets::SaveDocument(workspace_, owner_->id, owner_->document, identity,
                                          owner_->previous ? &*owner_->previous : nullptr, error);
  if (!saved)
    return false;
  owner_->previous = std::move(saved);
  owner_->published = true;
  return true;
}
bool PrefabDocumentSession::Close(bool discard_dirty, std::string *error) {
  if (error)
    error->clear();
  // Stale/recovery scopes can release their owner, but callbacks cannot destroy an active save.
  if (busy_ || !Replaceable(discard_dirty, error))
    return busy_ ? Fail(error, "Prefab save is active.") : false;
  if (owner_) {
    owner_.reset();
    ++generation_;
  }
  return true;
}
const SceneDocument *PrefabDocumentSession::Document() const {
  return owner_ && Current() ? &owner_->document : nullptr;
}
SceneDocument *PrefabDocumentSession::EditableDocument() {
  return owner_ && Allowed(true, nullptr) ? &owner_->document : nullptr;
}
foundation::Uuid PrefabDocumentSession::AssetId() const {
  return owner_ && Current() ? owner_->id : foundation::Uuid{};
}
std::optional<PrefabAsset> PrefabDocumentSession::SourceBaseline() const {
  return owner_ && Current() ? owner_->previous : std::nullopt;
}
bool PrefabDocumentSession::Dirty() const {
  return owner_ && (!owner_->published || owner_->document.Dirty());
}
} // namespace nexora::editor
