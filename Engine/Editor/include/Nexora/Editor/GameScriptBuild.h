#pragma once
#include "Nexora/Editor/GameBuild.h"

namespace nexora::editor {
struct GameScriptBuildRequest final {
  std::filesystem::path project_root, temporary_parent, script_manifest;
  GameBuildToolchain tools;
  std::size_t maximum_source_bytes{kMaximumGameBuildSourceBytes};
  std::size_t maximum_source_files{kMaximumGameBuildSourceFiles};
  GameBuildProgressSink progress;
};

// Owns frozen sources, canonical SDK/tool logs and one uniquely named compiled library.
// Keep this owner alive until all module code/finalizers have unloaded. Compilation does not
// instantiate gameplay; Current checks exact sources/inventory and FNV-size artifact integrity
// under the same cooperative single-writer model as GameBuild. Project/Play/source epochs must
// also pass before owner-thread use. This is not a sandbox or signature verification boundary.
class NEXORA_EDITOR_API PreparedGameScript final {
public:
  ~PreparedGameScript();
  PreparedGameScript(PreparedGameScript &&) noexcept;
  PreparedGameScript &operator=(PreparedGameScript &&) noexcept;
  PreparedGameScript(const PreparedGameScript &) = delete;
  PreparedGameScript &operator=(const PreparedGameScript &) = delete;
  [[nodiscard]] const std::filesystem::path &Library() const noexcept;
  [[nodiscard]] const std::string &Log() const noexcept;
  [[nodiscard]] bool Current(std::string *error = nullptr) const;

private:
  struct Implementation;
  explicit PreparedGameScript(std::unique_ptr<Implementation>);
  std::unique_ptr<Implementation> implementation_;
  friend NEXORA_EDITOR_API std::optional<PreparedGameScript>
  PrepareGameScript(GameScriptBuildRequest, const core::CancellationToken &, std::string *);
  friend NEXORA_EDITOR_API std::optional<PreparedGameBuild>
  PrepareGameBuild(GameBuildRequest, const core::CancellationToken &, std::string *);
};

[[nodiscard]] NEXORA_EDITOR_API std::optional<PreparedGameScript>
PrepareGameScript(GameScriptBuildRequest, const core::CancellationToken & = {},
                  std::string *error = nullptr);
} // namespace nexora::editor
