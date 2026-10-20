#pragma once

#include "Nexora/Editor/BuildProcess.h"
#include <memory>
#include <string_view>

namespace nexora::editor {
enum class GitStatusKind : std::uint8_t { Changed, Renamed, Copied, Unmerged, Untracked, Ignored };
struct GitStatusEntry final {
  GitStatusKind kind{};
  char index{}, working_tree{};
  // Owning, unquoted Git path bytes, relative to the working-tree root. These are observations,
  // not filesystem authority; preserve non-UTF8 names and escape them before GUI display.
  std::string path, original_path;
  friend bool operator==(const GitStatusEntry &, const GitStatusEntry &) = default;
};
enum class GitStatusError : std::uint8_t {
  None,
  InvalidRequest,
  Busy,
  OperationExhausted,
  ToolUnavailable,
  RepositoryUnavailable,
  ProcessRejected,
  ProcessFailed,
  OutputBudgetExceeded,
  MalformedOutput,
  AllocationFailed,
  Cancelled,
  Stale,
  Stopped
};
enum class GitStatusPhase : std::uint8_t {
  Idle,
  Refreshing,
  Ready,
  Failed,
  Cancelled,
  Stale,
  Stopped
};
struct GitStatusCapture final {
  std::uint64_t scope{}, operation{};
  std::filesystem::path repository_root;
  std::vector<GitStatusEntry> entries;
};
struct GitStatusQuery final {
  std::uint64_t scope{}, operation{};
  GitStatusPhase phase{GitStatusPhase::Idle};
  GitStatusError error{GitStatusError::None};
  std::optional<std::uint32_t> exit_code{};
  std::uint64_t dropped_output_bytes{};
};
// Serialized owner calls; JobSystem outlives the provider. Workers own only process inputs.
// The owner authorizes the absolute Git executable, repository configuration and inherited
// environment. This is a cooperating local Git query, not a process/configuration sandbox.
// Refresh runs directly without a shell/network/index mutation command. Complete bounded
// porcelain-v1 -z output is required; failures/cancellation retain same-scope last-good data.
// Project/provider replacement requires a fresh nonzero scope; polling another scope cancels/drains
// and permanently invalidates that operation. Captures own all data and retain no process borrows.
class NEXORA_EDITOR_API GitStatusProvider final {
public:
  static constexpr std::size_t kMaximumEntries = 4096;
  static constexpr std::size_t kMaximumPathBytes = 4096;
  static constexpr std::size_t kMaximumOutputBytes = BuildProcess::kMaximumOutputBytes;
  explicit GitStatusProvider(core::JobSystem &);
  ~GitStatusProvider();
  GitStatusProvider(const GitStatusProvider &) = delete;
  GitStatusProvider &operator=(const GitStatusProvider &) = delete;
  // Canonical working-tree root must contain a .git directory or gitfile (linked worktrees).
  // Provider-local operation IDs never wrap. Rejected intake preserves state. Reusing one scope for
  // another root rejects.
  bool Refresh(std::filesystem::path git, std::filesystem::path repository_root,
               std::uint64_t scope, GitStatusError *error = nullptr);
  bool Poll(std::uint64_t current_scope);
  bool Cancel() noexcept;
  [[nodiscard]] bool Busy() const;
  [[nodiscard]] GitStatusQuery Query() const noexcept;
  [[nodiscard]] std::shared_ptr<const GitStatusCapture> Capture(std::uint64_t current_scope) const;
  void Shutdown() noexcept;
  // No IO. Full rejection on malformed/truncated/duplicate paths or budget/allocation failure.
  // Empty output is an available clean-tree result, distinct from unavailable/failure.
  [[nodiscard]] static std::optional<std::vector<GitStatusEntry>>
  DecodePorcelainV1(std::string_view, GitStatusError *error = nullptr);

private:
  friend class GitStatusProviderTestAccess;
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
