#include "Nexora/Editor/SceneMergePublication.h"
#include "SceneMergeStorage.h"
#include <algorithm>
#if defined(NEXORA_EDITOR_MERGE_TEST_ACCESS)
#include "SceneMergePublicationTestAccess.h"
#endif

namespace nexora::editor {
namespace storage = merge_storage;
struct SceneMergePublication::State final {
  std::filesystem::path root, archive;
  SceneFileToken token;
  std::uint64_t session{};
  SceneDocument::PreparedSave local;
  SceneMergeArchive data;
  std::string manifest, digest;
  bool consumed{};
#if defined(NEXORA_EDITOR_MERGE_TEST_ACCESS)
  std::shared_ptr<SceneMergePublicationGate> gate;
#endif
  State(std::filesystem::path root_path, SceneFileToken file_token, std::uint64_t session_id,
        SceneDocument::PreparedSave prepared, SceneMergeArchive source, std::string encoded,
        std::string hash)
      : root(std::move(root_path)),
        archive(std::filesystem::path(".nexora/scene-merge-history") / hash), token(file_token),
        session(session_id), local(std::move(prepared)), data(std::move(source)),
        manifest(std::move(encoded)), digest(std::move(hash)) {}
};
const std::filesystem::path &SceneMergePublication::Source() const noexcept {
  static const std::filesystem::path empty;
  return state_ ? state_->data.source : empty;
}
const std::filesystem::path &SceneMergePublication::Archive() const noexcept {
  static const std::filesystem::path empty;
  return state_ ? state_->archive : empty;
}
std::string_view SceneMergePublication::CandidateBytes() const noexcept {
  return state_ ? std::string_view(state_->data.candidate) : std::string_view{};
}
bool SceneMergePublication::Consumed() const noexcept { return !state_ || state_->consumed; }
bool SceneMergePublisher::Matches(const SceneFileSession &files,
                                  const SceneMergePublication::State &s, bool authorized) {
  if (!authorized || !files.Live(s.token) || files.session_id_ != s.session ||
      files.root_ != s.root || !files.workspace_.Writable() ||
      files.workspace_.HasRecoveryJournal() || files.workspace_.HasExternalChange() ||
      files.SaveBlocked() || files.current_ != s.data.source || !files.disk_baseline_ ||
      files.disk_baseline_->exists != s.data.base.has_value() ||
      (s.data.base && files.disk_baseline_->bytes != *s.data.base) ||
      !storage::SafeRelativeScene(s.data.source) || !storage::SafeParents(s.root, s.data.source))
    return false;
  const auto path = files.Resolve(s.data.source);
  if (!path || *path != (s.root / s.data.source).lexically_normal() ||
      !files.document_.MatchesPreparedSave(s.local))
    return false;
  const auto disk = storage::Read(*path, SceneComparison::kMaximumSourceBytes);
  return disk && *disk == s.data.disk;
}
std::optional<SceneMergePublication> SceneMergePublisher::Prepare(const SceneFileSession &files,
                                                                  const SceneMergeReview &review,
                                                                  const SceneMergeRequest &request,
                                                                  bool authorized,
                                                                  std::string *error) {
  if (error)
    error->clear();
  auto reject = [&](const char *message) -> std::optional<SceneMergePublication> {
    if (error)
      *error = message;
    return {};
  };
  if (!authorized || !files.Live(files.Token()) || !files.current_ || !files.disk_baseline_ ||
      !review.LocalBytes() || !review.RemoteBytes())
    return reject(
        "Merge publication requires a current named scene and present local/disk sources");
  const auto candidate = SceneMerge::Resolve(review, request, error);
  if (!candidate)
    return {};
  if (!candidate->bytes)
    return reject("Whole-scene deletion requires a separate publication contract");
  auto prepared = files.document_.PrepareSave();
  if (!prepared || prepared->Bytes() != *review.LocalBytes())
    return reject("Local scene changed since comparison");
  SceneMergeArchive archive;
  archive.project = files.project_;
  archive.source = *files.current_;
  if (review.BaseBytes())
    archive.base = std::string(*review.BaseBytes());
  archive.local = *review.LocalBytes();
  archive.disk = *review.RemoteBytes();
  archive.candidate = *candidate->bytes;
  for (const auto &choice : request.choices)
    archive.choices.push_back({review.Comparison().rows[choice.row].stable_path, choice.choice});
  auto manifest = storage::Encode(archive, error);
  if (!manifest)
    return {};
  auto digest = storage::Digest(*manifest);
  if (!digest)
    return reject("SHA256 backend unavailable");
  auto state = std::make_shared<SceneMergePublication::State>(
      files.root_, files.Token(), files.session_id_, std::move(*prepared), std::move(archive),
      std::move(*manifest), std::move(*digest));
  if (!Matches(files, *state, authorized))
    return reject("Scene source, baseline, writer scope or recovery policy changed");
  return SceneMergePublication(std::move(state));
}
SceneMergePublishResult SceneMergePublisher::Publish(SceneFileSession &files,
                                                     SceneMergePublication &publication,
                                                     bool authorized,
                                                     const core::CancellationToken &cancel) {
  if (!publication.state_)
    return {SceneMergePublishStatus::Rejected, {}, "Publication capability was moved from"};
  auto &s = *publication.state_;
  const bool was_consumed = std::exchange(s.consumed, true);
  // All allocations and potentially throwing validation finish before native replacement.
  SceneMergePublishResult rejected{SceneMergePublishStatus::Rejected, s.archive,
                                   "Merge publication rejected; original source retained"};
  SceneMergePublishResult cancelled{SceneMergePublishStatus::Cancelled, s.archive,
                                    "Merge publication cancelled before replacement"};
  SceneMergePublishResult published{SceneMergePublishStatus::Published, s.archive,
                                    "Merged source published; reload explicitly to adopt it"};
  SceneMergePublishResult recovery{
      SceneMergePublishStatus::PublishedRecoveryRequired, s.archive,
      "Source replaced but directory durability was not confirmed; inspect archive and source"};
  SceneMergePublishResult uncertain{
      SceneMergePublishStatus::PublicationUncertain, s.archive,
      "Native replacement did not confirm success; inspect source and archive before any retry"};
  if (was_consumed)
    return rejected;
  if (cancel.IsCancellationRequested())
    return cancelled;
  if (!Matches(files, s, authorized))
    return rejected;
  const auto history = s.root / ".nexora/scene-merge-history";
  const auto metadata = s.root / ".nexora";
  if (!storage::PlainDirectory(metadata))
    return rejected;
  std::error_code e;
  if (storage::Missing(history)) {
    if (!std::filesystem::create_directory(history, e) || e || !storage::SyncDirectory(metadata))
      return rejected;
  }
  if (!storage::PlainDirectory(history))
    return rejected;
  const auto directory = s.root / s.archive;
  if (storage::Missing(directory)) {
    if (!std::filesystem::create_directory(directory, e) || e || !storage::SyncDirectory(history))
      return rejected;
    const auto write = [&](const char *name, std::string_view bytes) {
      const auto destination = directory / name;
      return storage::WriteNew(destination, bytes, cancel) &&
             storage::Read(destination, SceneComparison::kMaximumSourceBytes) == bytes;
    };
    if ((s.data.base && !write("base.scene", *s.data.base)) ||
        !write("local.scene", s.data.local) || !write("disk.scene", s.data.disk) ||
        !write("candidate.scene", s.data.candidate) ||
        !storage::WriteNew(directory / "manifest.bin", s.manifest, cancel) ||
        !storage::SyncDirectory(directory))
      return cancel.IsCancellationRequested() ? cancelled : rejected;
  }
  const auto archive = storage::ReadArchive(s.root, s.data.project, s.archive, nullptr);
  if (!archive || *archive != s.data || !storage::SyncDirectory(directory) ||
      !storage::SyncDirectory(history))
    return rejected;
  const auto destination = s.root / s.data.source;
  const auto destination_parent = destination.parent_path();
  const auto temporary = destination_parent / (".nexora-merge-" + s.digest + ".tmp");
  if (cancel.IsCancellationRequested())
    return cancelled;
  if (!storage::SafeParents(s.root, s.data.source) ||
      !storage::WriteNew(temporary, s.data.candidate, cancel) ||
      storage::Read(temporary, SceneComparison::kMaximumSourceBytes) != s.data.candidate)
    return cancel.IsCancellationRequested() ? cancelled : rejected;
#if defined(NEXORA_EDITOR_MERGE_TEST_ACCESS)
  if (s.gate) {
    s.gate->entered.store(true, std::memory_order_release);
    s.gate->entered.notify_all();
    s.gate->release.wait(false, std::memory_order_acquire);
  }
#endif
  if (!Matches(files, s, authorized))
    return rejected;
  if (cancel.IsCancellationRequested())
    return cancelled;
  // No allocations/callbacks/document mutations after this point. This is an operation-time
  // cooperative-writer check, not an OS compare-and-swap or an adversarial filesystem sandbox.
  if (!storage::Replace(temporary, destination))
    return uncertain;
  if (!storage::SyncDirectory(destination_parent))
    return recovery;
  return published;
}
std::optional<SceneMergeArchive> SceneMergePublisher::Inspect(const ProjectWorkspace &workspace,
                                                              const std::filesystem::path &relative,
                                                              std::string *error) {
  if (error)
    error->clear();
  return storage::ReadArchive(workspace.Root(), workspace.Project().id, relative, error);
}
#if defined(NEXORA_EDITOR_MERGE_TEST_ACCESS)
void SceneMergePublicationTestAccess::Gate(SceneMergePublication &p,
                                           std::shared_ptr<SceneMergePublicationGate> gate) {
  p.state_->gate = std::move(gate);
}
#endif
} // namespace nexora::editor
