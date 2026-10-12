#pragma once

#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/Api.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nexora::editor {
enum class BuildProcessPhase : std::uint8_t {
  Idle,
  Queued,
  Running,
  AwaitingOwner,
  Exited,
  Failed,
  Cancelled,
  Stale,
  Unsupported
};
struct BuildProcessRequest final {
  std::uint64_t scope{};
  std::filesystem::path executable;
  std::filesystem::path working_directory;
  std::vector<std::string> arguments;
  std::size_t output_capacity{64 * 1024};
};
struct BuildProcessSnapshot final {
  std::uint64_t operation{}, scope{};
  BuildProcessPhase phase{BuildProcessPhase::Idle};
  std::optional<std::uint32_t> exit_code;
  std::string message;
  // Raw merged stdout/stderr bytes. Sanitize/redact before any display or persistence.
  std::string output;
  std::uint64_t dropped_output_bytes{};
};
// One owning process operation. Start/Poll/Cancel/Snapshot/Shutdown are serialized owner calls;
// the referenced JobSystem outlives this object. Workers retain no document/workspace borrows.
// Absolute executable/cwd and owning UTF-8 argv launch directly, without a shell. The caller
// authorizes its executable and project build scripts; this runner is not an execution sandbox.
// Exited means code zero, never verified artifacts/build/deployment success. Poll checks scope.
// Cancel returns false after a worker publishes its terminal outcome, including before Poll.
// Cancellation/shutdown stop the managed process group/job and drain the direct child/worker.
// Commands, output and environment are not persisted by this API. Child environment is inherited.
// POSIX hosts must retain normal SIGCHLD wait policy and never externally reap owned children.
class NEXORA_EDITOR_API BuildProcess final {
public:
  static constexpr std::size_t kMaximumArguments = 256;
  static constexpr std::size_t kMaximumCommandBytes = 32 * 1024;
  static constexpr std::size_t kMaximumOutputBytes = 256 * 1024;
  explicit BuildProcess(core::JobSystem &jobs);
  ~BuildProcess();
  BuildProcess(const BuildProcess &) = delete;
  BuildProcess &operator=(const BuildProcess &) = delete;
  bool Start(BuildProcessRequest request, std::string *error = nullptr);
  bool Poll(std::uint64_t current_scope);
  bool Cancel() noexcept;
  [[nodiscard]] bool Busy() const;
  [[nodiscard]] BuildProcessSnapshot Snapshot() const;
  void Shutdown() noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
