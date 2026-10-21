#pragma once
#include "Nexora/Core/Cancellation.h"
#include "Nexora/Editor/SceneFiles.h"
#include "Nexora/Editor/SceneMerge.h"

namespace nexora::editor {
struct SceneMergeArchiveChoice final {
  std::string stable_path;
  SceneMergeChoice choice{};
  bool operator==(const SceneMergeArchiveChoice &) const = default;
};
struct SceneMergeArchive final {
  std::uint32_t schema{1};
  foundation::Uuid project;
  std::filesystem::path source;
  std::optional<std::string> base;
  std::string local, disk, candidate;
  std::vector<SceneMergeArchiveChoice> choices;
  bool operator==(const SceneMergeArchive &) const = default;
};
class NEXORA_EDITOR_API SceneMergePublication final {
public:
  [[nodiscard]] const std::filesystem::path &Source() const noexcept;
  [[nodiscard]] const std::filesystem::path &Archive() const noexcept;
  [[nodiscard]] std::string_view CandidateBytes() const noexcept;
  [[nodiscard]] bool Consumed() const noexcept;

private:
  struct State;
  friend class SceneMergePublisher;
  friend class SceneMergePublicationTestAccess;
  explicit SceneMergePublication(std::shared_ptr<State> state) : state_(std::move(state)) {}
  std::shared_ptr<State> state_;
};
enum class SceneMergePublishStatus : std::uint8_t {
  Rejected,
  Cancelled,
  Published,
  PublishedRecoveryRequired,
  PublicationUncertain
};
struct SceneMergePublishResult final {
  SceneMergePublishStatus status{SceneMergePublishStatus::Rejected};
  std::filesystem::path archive;
  std::string message;
  [[nodiscard]] bool Published() const noexcept {
    return status == SceneMergePublishStatus::Published ||
           status == SceneMergePublishStatus::PublishedRecoveryRequired;
  }
};
// Owning preparations retain no session/document/workspace/plugin borrow. Calls (including copies
// of a preparation) are serialized by one authoring owner. Authorizing caller rechecks owned-role,
// stopped Play/modal/recovery policy; core independently repeats writer/session/raw-source checks.
// Source-only publication leaves live document, baselines, selection and history unchanged.
// Keep EditorCore loaded until every capability copy is destroyed and synchronous call drains.
// Moved-from capabilities expose empty data, are consumed and reject publication.
class NEXORA_EDITOR_API SceneMergePublisher final {
public:
  static constexpr std::size_t kMaximumManifestBytes = SceneComparison::kMaximumResultBytes;
  static constexpr std::size_t kMaximumArchiveBytes =
      4 * SceneComparison::kMaximumSourceBytes + kMaximumManifestBytes;
  [[nodiscard]] static std::optional<SceneMergePublication>
  Prepare(const SceneFileSession &, const SceneMergeReview &, const SceneMergeRequest &,
          bool authorized, std::string *error = nullptr);
  // Every attempt consumes all shared copies of the preparation. Cancellation ends before commit;
  // uncertain native commit/durability results require inspection, never blind retry.
  [[nodiscard]] static SceneMergePublishResult Publish(SceneFileSession &, SceneMergePublication &,
                                                       bool authorized,
                                                       const core::CancellationToken &cancel = {});
  // Strict version1 read-only owning inspection; unsupported/corrupt archives remain untouched.
  [[nodiscard]] static std::optional<SceneMergeArchive>
  Inspect(const ProjectWorkspace &, const std::filesystem::path &relative_archive,
          std::string *error = nullptr);

private:
  static bool Matches(const SceneFileSession &, const SceneMergePublication::State &, bool);
};
} // namespace nexora::editor
