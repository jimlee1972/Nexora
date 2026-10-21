#pragma once

#include "Nexora/Editor/BuildProcess.h"
#include <memory>

namespace nexora::editor {
enum class GitDiffTarget : std::uint8_t { WorkingTree, Index };
struct GitDiffRequest final {
  GitDiffTarget target{GitDiffTarget::WorkingTree};
  // One literal repository-relative UTF-8 file path, or both old/new rename paths.
  std::vector<std::string> paths;
  friend bool operator==(const GitDiffRequest &, const GitDiffRequest &) = default;
};
enum class GitDiffError : std::uint8_t {
  None,
  InvalidRequest,
  Busy,
  OperationExhausted,
  ToolUnavailable,
  RepositoryUnavailable,
  ProcessRejected,
  ProcessFailed,
  OutputBudgetExceeded,
  AllocationFailed,
  Cancelled,
  Stale,
  Stopped
};
enum class GitDiffPhase : std::uint8_t {
  Idle,
  Comparing,
  Ready,
  Failed,
  Cancelled,
  Stale,
  Stopped
};
struct GitDiffQuery final {
  std::uint64_t scope{}, operation{};
  GitDiffPhase phase{GitDiffPhase::Idle};
  GitDiffError error{GitDiffError::None};
  std::optional<std::uint32_t> exit_code{};
  std::uint64_t dropped_output_bytes{};
};
struct GitDiffCapture final {
  std::uint64_t scope{}, operation{};
  std::filesystem::path repository_root;
  GitDiffRequest request;
  // Complete raw merged Git stdout/stderr, including possible warnings/binary/conflict notices.
  // Not a parsed/validated patch, stable filesystem snapshot, redacted log or publication plan.
  std::string output;
};
// Serialized owner calls. JobSystem outlives the provider; shutdown drains process work.
// The owner explicitly trusts the absolute executable, repository config and inherited environment.
// Direct read-only argv disables external diff/textconv/pagers and treats every path literally.
// No shell/network/index-mutation command is launched. Other configured Git helpers (for example
// attribute clean filters) remain part of the owner's trusted configuration, not a sandbox.
// No project/document/native-module borrows are retained.
class NEXORA_EDITOR_API GitDiffProvider final {
public:
  static constexpr std::size_t kMaximumPaths = 2;
  static constexpr std::size_t kMaximumPathBytes = 4096;
  static constexpr std::size_t kMaximumOutputBytes = BuildProcess::kMaximumOutputBytes;
  explicit GitDiffProvider(core::JobSystem &);
  ~GitDiffProvider();
  GitDiffProvider(const GitDiffProvider &) = delete;
  GitDiffProvider &operator=(const GitDiffProvider &) = delete;
  // Nonzero scope identifies one canonical root. Rejected intake preserves current state.
  // IDs never wrap; paths must be nonempty, unique and contain no empty/dot/parent components.
  bool Compare(std::filesystem::path git, std::filesystem::path repository_root,
               std::uint64_t scope, GitDiffRequest, GitDiffError *error = nullptr);
  bool Poll(std::uint64_t current_scope);
  bool Cancel() noexcept;
  [[nodiscard]] bool Busy() const;
  [[nodiscard]] GitDiffQuery Query() const noexcept;
  // Owning captures survive later queries/teardown. Same-scope failure retains last-good WITH its
  // original request identity; a stale scope permanently revokes that operation's publication.
  // Consumers escape/redact raw bytes before display/persistence and release before library unload.
  [[nodiscard]] std::shared_ptr<const GitDiffCapture> Capture(std::uint64_t current_scope) const;
  void Shutdown() noexcept;

private:
  friend class GitDiffProviderTestAccess;
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
