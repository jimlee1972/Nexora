#include "Nexora/Editor/GameBuildProgress.h"
#include "Nexora/Foundation/Types.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::editor;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  const auto progress = ObserveGameBuildProgress(
      GameBuildStage::CompileAndTest,
      "\x1b[31mC:/My Game/Scripts/main.zig:12:3: error: expected ': note: ' literal\x1b[0m\n"
      "Scripts/test.zig:20:7: note: referenced here\n"
      "\x1b]0;unexpected title\x07"
      "CMake Error at project.cmake:4 (message):\n");
  Require(progress.stage == GameBuildStage::CompileAndTest && progress.diagnostics.size() == 3,
          "compiler diagnostic/phase extraction failed");
  const auto &error = progress.diagnostics.front();
  Require(error.severity == GameBuildDiagnosticSeverity::Error &&
              error.file == "C:/My Game/Scripts/main.zig" && error.line == 12 &&
              error.column == 3 && error.message == "expected ': note: ' literal",
          "Windows path or severity inside diagnostic text was parsed incorrectly");
  Require(progress.diagnostics[1].severity == GameBuildDiagnosticSeverity::Note &&
              progress.diagnostics[2].severity == GameBuildDiagnosticSeverity::Error &&
              progress.output.find('\x1b') == std::string::npos &&
              progress.output.find("unexpected title") == std::string::npos,
          "terminal control sequences reached display output");
  const auto controls =
      ObserveGameBuildProgress(GameBuildStage::Verify, std::string("a\0b\rc\n", 7));
  Require(controls.output.find('\0') == std::string::npos &&
              controls.output.find('\r') == std::string::npos,
          "control bytes reached display output");
  const auto invalid = ObserveGameBuildProgress(GameBuildStage::Configure, std::string(1, '\xff'));
  Require(nexora::foundation::IsValidUtf8(invalid.output) && !invalid.output.empty(),
          "invalid UTF-8 output was published");
  const auto bounded = ObserveGameBuildProgress(
      GameBuildStage::CompileAndTest, std::string(GameBuildProgress::kMaximumOutputBytes * 2, 'a') +
                                          "\nScripts/main.zig:2:4: error: recent error\n");
  Require(bounded.output_truncated &&
              bounded.output.size() <= GameBuildProgress::kMaximumOutputBytes &&
              bounded.diagnostics.size() == 1 && bounded.diagnostics.front().line == 2,
          "bounded output lost the recent compiler error");
  std::string errors;
  for (int i = 0; i < 100; ++i)
    errors += "Scripts/main.zig:2:4: error: repeated error\n";
  const auto many = ObserveGameBuildProgress(GameBuildStage::CompileAndTest, errors);
  Require(many.diagnostics_truncated &&
              many.diagnostics.size() == GameBuildProgress::kMaximumDiagnostics,
          "unbounded diagnostic growth");
  const auto overflow =
      ObserveGameBuildProgress(GameBuildStage::CompileAndTest,
                               "Scripts/main.zig:99999999999999999:4: error: number overflow\n");
  Require(overflow.diagnostics.size() == 1 && overflow.diagnostics.front().file.empty() &&
              overflow.diagnostics.front().line == 0 && overflow.diagnostics.front().column == 0,
          "numeric location overflow accepted");
  std::string unicode;
  for (int i = 0; i < 10000; ++i)
    unicode += "\xe9\x81\x8a";
  const auto utf8 = ObserveGameBuildProgress(GameBuildStage::Verify, unicode);
  Require(utf8.output_truncated && nexora::foundation::IsValidUtf8(utf8.output),
          "output boundary split a UTF-8 scalar");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
