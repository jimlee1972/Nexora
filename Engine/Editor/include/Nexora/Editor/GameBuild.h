#pragma once

#include "Nexora/Core/Cancellation.h"
#include "Nexora/Editor/GameBuildCapture.h"
#include <functional>

namespace nexora::editor {
enum class GameBuildMode : std::uint8_t { SceneOnly, ScriptProject };
struct GameBuildToolchain final {
  std::filesystem::path player, cmake, ninja, zig;
  // Explicit engine distribution inventory; never search the authored project or current CWD.
  std::vector<std::filesystem::path> runtime_files;
};
// Read the trusted Player inventory: flat installed filenames or absolute build-tree DSOs.
// Compiler paths come from host configuration; authored projects never select build executables.
[[nodiscard]] NEXORA_EDITOR_API std::optional<GameBuildToolchain>
ReadGameBuildToolchain(const std::filesystem::path &profile, const std::filesystem::path &cmake,
                       const std::filesystem::path &ninja, const std::filesystem::path &zig,
                       std::string *error = nullptr);
struct GameBuildRequest final {
  std::filesystem::path project_root, temporary_parent;
  GameBuildCapture capture;
  GameBuildMode mode{GameBuildMode::SceneOnly};
  // ScriptProject only: saved manifest and its complete relative source subtree are frozen.
  std::filesystem::path script_manifest;
  GameBuildToolchain tools;
  std::uint32_t maximum_frames{};
};
// Owns a private operation directory. Preparation is synchronous worker work with no authoring
// borrows. Publication is an explicit owner-thread step; an Editor guard checks its current
// workspace/lease/recovery/document/content generations. Source bytes are always checked again.
class NEXORA_EDITOR_API PreparedGameBuild final {
public:
  ~PreparedGameBuild();
  PreparedGameBuild(PreparedGameBuild &&) noexcept;
  PreparedGameBuild &operator=(PreparedGameBuild &&) noexcept;
  PreparedGameBuild(const PreparedGameBuild &) = delete;
  PreparedGameBuild &operator=(const PreparedGameBuild &) = delete;
  [[nodiscard]] const std::filesystem::path &Directory() const noexcept;
  [[nodiscard]] const std::string &Log() const noexcept;
  // Output must be absent, or an unmodified prior engine build for this same project. A
  // failed/stale/cancelled preparation or publication preserves the previous output.
  bool Publish(const std::filesystem::path &output, const core::CancellationToken &,
               const std::function<bool()> &owner_current, std::string *error = nullptr);

private:
  struct Implementation;
  explicit PreparedGameBuild(std::unique_ptr<Implementation>);
  std::unique_ptr<Implementation> implementation_;
  friend NEXORA_EDITOR_API std::optional<PreparedGameBuild>
  PrepareGameBuild(GameBuildRequest, const core::CancellationToken &, std::string *);
};
[[nodiscard]] NEXORA_EDITOR_API std::optional<PreparedGameBuild>
PrepareGameBuild(GameBuildRequest, const core::CancellationToken & = {},
                 std::string *error = nullptr);
} // namespace nexora::editor
