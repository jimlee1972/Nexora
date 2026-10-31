#pragma once
#include "Nexora/Editor/Api.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor {
enum class GameBuildStage : std::uint8_t {
  Cooking,
  RuntimeFiles,
  ScriptSnapshot,
  Configure,
  CompileAndTest,
  Verify,
  ReadyToPublish,
  Publishing,
  Published,
  Failed,
  Cancelled
};
[[nodiscard]] constexpr std::string_view GameBuildStageLabel(GameBuildStage stage) noexcept {
  switch (stage) {
  case GameBuildStage::Cooking:
    return "Cooking saved scenes";
  case GameBuildStage::RuntimeFiles:
    return "Preparing runtime files";
  case GameBuildStage::ScriptSnapshot:
    return "Capturing script sources";
  case GameBuildStage::Configure:
    return "Configuring script build";
  case GameBuildStage::CompileAndTest:
    return "Compiling and testing scripts";
  case GameBuildStage::Verify:
    return "Verifying game artifacts";
  case GameBuildStage::ReadyToPublish:
    return "Ready to publish";
  case GameBuildStage::Publishing:
    return "Publishing game";
  case GameBuildStage::Published:
    return "Game published";
  case GameBuildStage::Failed:
    return "Build failed";
  case GameBuildStage::Cancelled:
    return "Build cancelled";
  }
  return "Unknown build phase";
}
enum class GameBuildDiagnosticSeverity : std::uint8_t { Note, Warning, Error };
struct GameBuildDiagnostic final {
  GameBuildDiagnosticSeverity severity{};
  std::string file, message;
  std::uint32_t line{}, column{};
  bool operator==(const GameBuildDiagnostic &) const = default;
};
// Owning, bounded UI observations. Raw tool bytes remain private to the producer log.
struct GameBuildProgress final {
  static constexpr std::size_t kMaximumOutputBytes = 16 * 1024;
  static constexpr std::size_t kMaximumDiagnostics = 32;
  static constexpr std::size_t kMaximumDiagnosticBytes = 1024;
  GameBuildStage stage{GameBuildStage::Cooking};
  std::string output;
  std::vector<GameBuildDiagnostic> diagnostics;
  bool output_truncated{}, diagnostics_truncated{};
};
// Called serially on the preparation worker. A sink must own its captures and must not borrow
// workspace, document or UI objects. Exceptions propagate to the embedding worker boundary.
using GameBuildProgressSink = std::function<void(GameBuildProgress)>;
// Pure UTF-8/terminal-control sanitization and compiler-location extraction, with no IO.
[[nodiscard]] NEXORA_EDITOR_API GameBuildProgress
ObserveGameBuildProgress(GameBuildStage, std::string_view tool_output = {});
} // namespace nexora::editor
