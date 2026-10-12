#include "Nexora/Editor/PrefabDocumentSession.h"
#include "Nexora/Editor/PrefabComparison.h"
#include "Nexora/Editor/PrefabPropertyPlan.h"
#include <algorithm>
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
  if (source->base) {
    candidate->document.prefab_base_ =
        SceneDocument::PrefabBaseReference{source->base->asset, source->base->revision};
    auto signature = candidate->document.StateSignature();
    if (!signature)
      return Fail(error, "Prefab source reference could not be initialized.");
    candidate->document.saved_signature_ = std::move(*signature);
  }
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
  candidate->document.prefab_base_ =
      SceneDocument::PrefabBaseReference{owner_->previous->id, owner_->previous->revision};
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
std::optional<PrefabPropertyReview> PrefabDocumentSession::Review(std::string *error) const {
  if (!Allowed(false, error))
    return {};
  if (!owner_ || !owner_->previous) {
    Fail(error, "Save a prefab source before reviewing changes.");
    return {};
  }
  const auto &previous = *owner_->previous;
  const auto reference =
      previous.id != owner_->id
          ? PrefabRevisionReference{previous.id, previous.revision}
          : previous.base.value_or(PrefabRevisionReference{previous.id, previous.revision});
  const auto authored_reference = owner_->document.PrefabBase();
  auto source = PrefabAssets::LoadRevision(
      workspace_, authored_reference ? PrefabRevisionReference{authored_reference->asset,
                                                               authored_reference->revision}
                                     : reference);
  const auto expected = owner_->document.PrepareSave();
  // Inspection never fabricates stable identities for unsaved structural/field additions.
  const auto current = PrefabAssets::Capture(
      owner_->id, owner_->document, [] { return foundation::Uuid{}; }, &previous);
  if (!source || !expected || !current) {
    Fail(error, "Source revision is unavailable, or save new identities before reviewing.");
    return {};
  }
  auto changes = ComparePrefabRevisions(&*source, &*current, &*source, error);
  if (!changes)
    return {};
  auto candidate = BuildPrefabPropertySnapshot(*current, *source);
  if (candidate && *candidate == expected->Bytes())
    candidate.reset();
  // Metadata references are visible, but restoring document properties never changes them.
  if (!std::ranges::any_of(changes->rows,
                           [](const auto &row) { return row.stable_path.starts_with("nodes/"); }))
    candidate.reset();
  if (!Allowed(false, error) || !owner_->document.MatchesPreparedSave(*expected))
    return {};
  return PrefabPropertyReview{
      *expected, previous, std::move(*source), std::move(*changes), std::move(candidate),
      root_,     project_, owner_->id,         generation_};
}
bool PrefabDocumentSession::MatchesReview(const PrefabPropertyReview &review) const {
  return owner_ && owner_->previous && review.root_ == root_ && review.project_ == project_ &&
         review.asset_ == owner_->id && review.generation_ == generation_ &&
         review.previous_ == *owner_->previous &&
         owner_->document.MatchesPreparedSave(review.expected_);
}
std::optional<PrefabPropertyReview>
PrefabDocumentSession::SelectReview(const PrefabPropertyReview &review,
                                    std::span<const PrefabPropertySelection> selected,
                                    std::string *error) const {
  if (!Allowed(false, error))
    return {};
  if (selected.empty() || selected.size() > PrefabAssets::kMaximumProperties ||
      !MatchesReview(review)) {
    Fail(error, "Select known properties from an unchanged current review.");
    return {};
  }
  const auto source =
      PrefabAssets::LoadRevision(workspace_, {review.source_.id, review.source_.revision});
  if (!source || *source != review.source_) {
    Fail(error, "Reviewed source changed or is unavailable.");
    return {};
  }
  const auto current = PrefabAssets::Capture(
      owner_->id, owner_->document, [] { return foundation::Uuid{}; }, &*owner_->previous);
  auto candidate =
      current ? BuildPrefabPropertySnapshot(*current, *source, selected) : std::nullopt;
  if (!candidate) {
    Fail(error, "Selected identities or the resulting property hierarchy are incompatible.");
    return {};
  }
  if (*candidate == review.expected_.Bytes())
    candidate.reset();
  if (!Allowed(false, error) || !MatchesReview(review))
    return {};
  PrefabPropertyReview result{review.expected_, review.previous_,     review.source_,
                              review.changes_,  std::move(candidate), root_,
                              project_,         owner_->id,           generation_};
  result.selections_.assign(selected.begin(), selected.end());
  result.targeted_ = true;
  return result;
}
bool PrefabDocumentSession::Revert(const PrefabPropertyReview &review, bool authorized,
                                   std::string *error) {
  if (!Allowed(true, error))
    return false;
  if (!authorized || !review.candidate_ || !MatchesReview(review))
    return Fail(error, "Property review is stale or has no authorized compatible changes.");
  const auto source =
      PrefabAssets::LoadRevision(workspace_, {review.source_.id, review.source_.revision});
  if (!source || *source != review.source_)
    return Fail(error, "Reviewed source revision changed or is unavailable.");
  if (!Allowed(true, error))
    return false;
  Busy guard(busy_);
  if (!owner_->document.ApplyPropertySnapshot(review.expected_, *review.candidate_, true))
    return Fail(error, "Property transaction rejected; preserve the isolated document.");
  return true;
}
std::optional<PrefabRebaseReview> PrefabDocumentSession::ReviewRebase(std::string *error) const {
  if (!Allowed(false, error))
    return {};
  const auto reference = BaseReference();
  if (!owner_ || !owner_->published || !owner_->previous || !reference) {
    Fail(error, "Save an isolated variant before reviewing its source rebase.");
    return {};
  }
  auto base = PrefabAssets::LoadRevision(workspace_, *reference);
  auto source = PrefabAssets::Load(workspace_, reference->asset);
  const auto published = PrefabAssets::Load(workspace_, owner_->id);
  auto expected = owner_->document.PrepareSave();
  auto local = PrefabAssets::Capture(
      owner_->id, owner_->document, [] { return foundation::Uuid{}; }, &*owner_->previous);
  auto base_graph =
      base ? PrefabAssets::ResolveProject(workspace_, {base->id, base->revision}) : std::nullopt;
  auto source_graph = source
                          ? PrefabAssets::ResolveProject(workspace_, {source->id, source->revision})
                          : std::nullopt;
  if (!base || !source || !expected || !local || published != owner_->previous || !base_graph ||
      !source_graph) {
    Fail(error, "Rebase source, publication or stable identities are unavailable.");
    return {};
  }
  auto plan = BuildPrefabRebasePlan(*base, *local, *source, {}, error);
  if (!plan || !Allowed(false, error) || !owner_->document.MatchesPreparedSave(*expected))
    return {};
  return PrefabRebaseReview{std::move(*expected),
                            *owner_->previous,
                            std::move(*base),
                            std::move(*local),
                            std::move(*source),
                            std::move(*plan),
                            root_,
                            project_,
                            owner_->id,
                            generation_,
                            std::move(*base_graph),
                            std::move(*source_graph)};
}
bool PrefabDocumentSession::MatchesRebase(const PrefabRebaseReview &review) const {
  if (!owner_ || !owner_->published || !owner_->previous || review.root_ != root_ ||
      review.project_ != project_ || review.asset_ != owner_->id ||
      review.generation_ != generation_ || review.previous_ != *owner_->previous ||
      !owner_->document.MatchesPreparedSave(review.expected_))
    return false;
  const auto old = PrefabAssets::LoadRevision(workspace_, {review.base_.id, review.base_.revision});
  const auto source = PrefabAssets::Load(workspace_, review.source_.id);
  const auto published = PrefabAssets::Load(workspace_, owner_->id);
  const auto base_graph =
      PrefabAssets::ResolveProject(workspace_, {review.base_.id, review.base_.revision});
  const auto source_graph =
      PrefabAssets::ResolveProject(workspace_, {review.source_.id, review.source_.revision});
  return old && *old == review.base_ && source && *source == review.source_ &&
         published == owner_->previous && base_graph && source_graph &&
         base_graph->assets == review.base_graph_.assets &&
         source_graph->assets == review.source_graph_.assets;
}
std::optional<PrefabRebaseReview>
PrefabDocumentSession::ResolveRebase(const PrefabRebaseReview &review,
                                     std::span<const PrefabRebaseChoice> choices,
                                     std::string *error) const {
  if (!Allowed(false, error) || !MatchesRebase(review)) {
    Fail(error, "Rebase review is stale; prepare a current source comparison.");
    return {};
  }
  auto plan = BuildPrefabRebasePlan(review.base_, review.local_, review.source_, choices, error);
  if (!plan || !Allowed(false, error) || !MatchesRebase(review))
    return {};
  return PrefabRebaseReview{review.expected_,
                            review.previous_,
                            review.base_,
                            review.local_,
                            review.source_,
                            std::move(*plan),
                            root_,
                            project_,
                            owner_->id,
                            generation_,
                            review.base_graph_,
                            review.source_graph_};
}
bool PrefabDocumentSession::Rebase(const PrefabRebaseReview &review, bool authorized,
                                   std::string *error) {
  if (!Allowed(true, error))
    return false;
  if (!authorized || !review.plan_.candidate || !MatchesRebase(review))
    return Fail(error, "Rebase requires current sources, resolved conflicts and confirmation.");
  auto closure =
      PrefabAssets::ResolveProject(workspace_, {review.source_.id, review.source_.revision});
  if (!closure)
    return Fail(error, "Rebase source closure is unavailable or invalid.");
  auto candidate = review.local_;
  candidate.scene_bytes = *review.plan_.candidate;
  candidate.base = PrefabRevisionReference{review.source_.id, review.source_.revision};
  closure->assets.push_back(candidate);
  if (!PrefabAssets::Resolve({candidate.id, candidate.revision}, closure->assets) ||
      !Allowed(true, error) || !MatchesRebase(review))
    return Fail(error, "Rebase exceeds graph bounds or its sources changed.");
  Busy guard(busy_);
  if (!owner_->document.ApplyPrefabPropertySnapshot(review.expected_, *review.plan_.candidate,
                                                    {review.source_.id, review.source_.revision},
                                                    true))
    return Fail(error, "Rebase transaction rejected; preserve properties and reference.");
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
std::optional<PrefabRevisionReference> PrefabDocumentSession::BaseReference() const {
  if (!owner_ || !Current())
    return {};
  const auto reference = owner_->document.PrefabBase();
  return reference ? std::optional{PrefabRevisionReference{reference->asset, reference->revision}}
                   : std::nullopt;
}
std::optional<PrefabAsset> PrefabDocumentSession::SourceBaseline() const {
  return owner_ && Current() ? owner_->previous : std::nullopt;
}
bool PrefabDocumentSession::Dirty() const {
  return owner_ && (!owner_->published || owner_->document.Dirty());
}
} // namespace nexora::editor
