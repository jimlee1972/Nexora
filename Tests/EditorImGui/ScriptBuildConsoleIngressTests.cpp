#include "ScriptBuildConsoleIngress.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using editor::GameBuildDiagnostic;
using editor::GameBuildDiagnosticSeverity;
using editor::GameBuildProgress;
using editor::preview::ScriptBuildConsoleIngress;
using Context = ScriptBuildConsoleIngress::Context;
constexpr Context scope{7, 3, 2};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
GameBuildDiagnostic Diagnostic(std::string message = "broken", std::string file = "src/game.zig") {
  return {GameBuildDiagnosticSeverity::Error, std::move(file), std::move(message), 12, 7};
}
void StructuredProducerAndOwnership() {
  runtime::RuntimeConsole console{64};
  ScriptBuildConsoleIngress ingress{console};
  auto progress = editor::ObserveGameBuildProgress(
      editor::GameBuildStage::CompileAndTest,
      "\x1b[31msrc/\xe4\xbe\x86\xe6\xba\x90.zig:12:7: error: actual error\x1b[0m\n"
      "C:\\source file.cpp:3:4: warning: deprecated\n"
      "include.h:9:2: note: declared here\nerror: failed compiler command\n");
  const auto original = progress.output;
  const auto diagnostics = progress.diagnostics;
  Require(ingress.Poll(progress, scope, scope), "structured producer rejected");
  const auto rows = console.Snapshot();
  Require(rows.size() == 4 && rows[0].category == "ScriptDiagnostic" &&
              rows[0].severity == runtime::RuntimeLogSeverity::Error &&
              rows[0].source ==
                  "ScriptBuild project=7 play=3 source=2 | src/\xe4\xbe\x86\xe6\xba\x90.zig:12:7" &&
              rows[0].message == "actual error" && rows[0].timestamp_nanoseconds &&
              rows[1].severity == runtime::RuntimeLogSeverity::Warning &&
              rows[1].source.ends_with("C:\\source file.cpp:3:4") &&
              rows[2].severity == runtime::RuntimeLogSeverity::Info &&
              rows[3].source == "ScriptBuild project=7 play=3 source=2" &&
              rows[3].message == "error: failed compiler command",
          "severity, UTF-8/Windows location, timestamp or unlocated diagnostic changed");
  Require(progress.output == original && progress.diagnostics == diagnostics &&
              ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 4,
          "projection changed existing Info input or duplicated cumulative progress");
  progress.output.assign("replaced producer storage");
  progress.diagnostics[0].message.assign("changed");
  Require(rows[0].message == "actual error" && console.Snapshot()[0].message == "actual error",
          "Console or copied rows borrowed producer storage");
}
void ContextAndRollingSnapshots() {
  runtime::RuntimeConsole console{256};
  ScriptBuildConsoleIngress ingress{console};
  GameBuildProgress progress;
  progress.diagnostics = {Diagnostic()};
  Require(ingress.Poll(progress, scope, scope), "initial scope failed");
  auto next = scope;
  ++next.source;
  Require(ingress.Poll(progress, scope, next) && console.Snapshot().size() == 1,
          "obsolete compiler snapshot reached the current Console");
  Require(ingress.Poll(progress, next, next) && console.Snapshot().size() == 2 &&
              console.Snapshot().back().source.starts_with("ScriptBuild project=7 play=3 source=3"),
          "same diagnostic was lost across source generations");
  Require(!ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 2,
          "regressed same-Play source epoch accepted");
  auto restarted = next;
  ++restarted.play;
  restarted.source = 1;
  Require(ingress.Poll(progress, restarted, restarted) && console.Snapshot().size() == 3,
          "new Play failed to reset diagnostic identities");
  auto project = scope;
  ++project.project;
  Require(ingress.Poll(progress, project, project) && console.Snapshot().size() == 4 &&
              ingress.Poll(progress, {}, {}) && console.Snapshot().size() == 4,
          "new/detached project scope changed admission");

  progress.diagnostics.clear();
  for (int i = 0; i < 32; ++i)
    progress.diagnostics.push_back(Diagnostic("message " + std::to_string(i)));
  Require(ingress.Poll(progress, project, project) && ingress.RememberedDiagnostics() == 32,
          "full producer snapshot escaped the retained bound");
  // Put the new row first: evicting an old identity while traversing this input must not
  // republish the remaining rows which were present in the preceding cumulative snapshot.
  progress.diagnostics.pop_back();
  progress.diagnostics.insert(progress.diagnostics.begin(), Diagnostic("new first"));
  const auto before = console.Snapshot().size();
  Require(ingress.Poll(progress, project, project) && console.Snapshot().size() == before + 1 &&
              ingress.RememberedDiagnostics() == 32 && ingress.Poll(progress, project, project) &&
              console.Snapshot().size() == before + 1,
          "rolling/reordered cumulative snapshots replayed retained diagnostics");
  for (int i = 0; i < 96; ++i) {
    progress.diagnostics = {Diagnostic("later " + std::to_string(i))};
    Require(ingress.Poll(progress, project, project) && ingress.RememberedDiagnostics() == 1,
            "history grew with total diagnostics instead of the current bounded snapshot");
  }
}
void TruncationAndAdmission() {
  runtime::RuntimeConsole console{64};
  ScriptBuildConsoleIngress ingress{console};
  GameBuildProgress progress;
  progress.diagnostics = {Diagnostic()};
  progress.diagnostics_truncated = true;
  progress.output_truncated = true;
  Require(ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 2 &&
              console.Snapshot().back().category == "ScriptBuildStatus" &&
              console.Snapshot().back().severity == runtime::RuntimeLogSeverity::Warning &&
              console.Snapshot().back().message.find("record count unknown") != std::string::npos &&
              console.Snapshot().back().message.find("byte count unknown") != std::string::npos &&
              console.DroppedCount() == 0,
          "unknown producer truncation invented loss units or hid its warning");
  Require(ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 2,
          "cumulative truncation repeated its status warning");
  progress.diagnostics = {Diagnostic("long source", std::string(1024, 'p'))};
  Require(ingress.Poll(progress, scope, scope) && ingress.SuppressedDiagnostics() == 1 &&
              console.DroppedCount() == 1 && console.Snapshot().size() == 3 &&
              console.Snapshot().back().message.find("suppressed 1 diagnostic records") !=
                  std::string::npos,
          "known source-budget suppression was not counted in record units");
  Require(ingress.Poll(progress, scope, scope) && console.DroppedCount() == 1 &&
              console.Snapshot().size() == 3,
          "known suppression or rejection was retried");
  auto next = scope;
  ++next.source;
  progress.diagnostics.clear();
  Require(ingress.Poll(progress, next, next) && console.Snapshot().size() == 4,
          "new source lost its separate upstream truncation feedback");
  runtime::RuntimeConsole disabled{0};
  ScriptBuildConsoleIngress rejected{disabled};
  progress = {};
  progress.diagnostics = {Diagnostic()};
  Require(rejected.Poll(progress, scope, scope) && rejected.ForwardedRecords() == 0 &&
              disabled.DroppedCount() == 1 && rejected.Poll(progress, scope, scope) &&
              disabled.DroppedCount() == 1,
          "Console rejection duplicated the same derived diagnostic");
}
void MalformedAndByteLimits() {
  runtime::RuntimeConsole console{64};
  ScriptBuildConsoleIngress ingress{console};
  GameBuildProgress progress;
  progress.diagnostics = {Diagnostic(std::string(1024, 'm'))};
  Require(ingress.Poll(progress, scope, scope) && console.Snapshot()[0].message.size() == 1024,
          "inclusive message byte bound rejected");
  auto newer = scope;
  ++newer.source;
  auto invalid = progress;
  invalid.diagnostics[0].message += 'x';
  Require(!ingress.Poll(invalid, newer, newer), "oversized message accepted");
  invalid = progress;
  invalid.diagnostics[0].file.assign(1025, 'x');
  Require(!ingress.Poll(invalid, newer, newer), "oversized file accepted");
  invalid = progress;
  invalid.diagnostics[0].message = std::string(1, static_cast<char>(0xff));
  Require(!ingress.Poll(invalid, newer, newer), "invalid UTF-8 accepted");
  invalid.diagnostics[0].message = std::string("embedded\0nul", 12);
  Require(!ingress.Poll(invalid, newer, newer), "embedded NUL accepted");
  invalid = progress;
  invalid.diagnostics[0].severity = static_cast<GameBuildDiagnosticSeverity>(255);
  Require(!ingress.Poll(invalid, newer, newer), "invalid severity accepted");
  invalid = progress;
  invalid.stage = static_cast<editor::GameBuildStage>(255);
  Require(!ingress.Poll(invalid, newer, newer), "invalid stage accepted");
  invalid = progress;
  invalid.diagnostics.resize(33, invalid.diagnostics[0]);
  Require(!ingress.Poll(invalid, newer, newer) && console.Snapshot().size() == 1 &&
              console.DroppedCount() == 0 && ingress.RememberedDiagnostics() == 1 &&
              ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 1,
          "malformed input changed history, counters or accepted context");
}
void OptionalLocationsAndIndependentFlags() {
  runtime::RuntimeConsole console{32};
  ScriptBuildConsoleIngress ingress{console};
  GameBuildProgress progress;
  auto line_only = Diagnostic("line only", "partial.zig");
  line_only.column = 0;
  auto no_line = Diagnostic("unknown line", "reported.zig");
  no_line.line = 0;
  auto no_file = Diagnostic("unlocated", "");
  progress.diagnostics = {line_only, no_line, no_file, line_only};
  Require(ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 3 &&
              console.Snapshot()[0].source.ends_with("partial.zig:12") &&
              console.Snapshot()[1].source.ends_with("reported.zig") &&
              console.Snapshot()[2].source == "ScriptBuild project=7 play=3 source=2",
          "partial locations fabricated missing coordinates or duplicate rows");
  progress.diagnostics_truncated = true;
  Require(ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 4,
          "late diagnostic truncation was lost");
  progress.output_truncated = true;
  Require(ingress.Poll(progress, scope, scope) && console.Snapshot().size() == 5 &&
              console.DroppedCount() == 0 && ingress.Poll(progress, scope, scope) &&
              console.Snapshot().size() == 5,
          "independent late output truncation repeated or invented loss counts");
}
void ExactSourceBudget() {
  runtime::RuntimeConsole console{4};
  ScriptBuildConsoleIngress ingress{console};
  const std::string prefix = "ScriptBuild project=7 play=3 source=2 | ";
  GameBuildProgress progress;
  progress.diagnostics = {
      Diagnostic("exact source", std::string(runtime::RuntimeConsole::kMaxSourceBytes -
                                                 prefix.size() - std::string(":12:7").size(),
                                             'p'))};
  Require(ingress.Poll(progress, scope, scope) && ingress.ForwardedRecords() == 1 &&
              console.Snapshot()[0].source.size() == runtime::RuntimeConsole::kMaxSourceBytes,
          "inclusive labeled-source budget rejected");
  progress.diagnostics[0].file += 'p';
  Require(ingress.Poll(progress, scope, scope) && ingress.SuppressedDiagnostics() == 1 &&
              console.DroppedCount() == 1 && console.Snapshot().size() == 2,
          "one-byte source overflow was not a single known suppressed record");
}
} // namespace
int main() {
  try {
    StructuredProducerAndOwnership();
    ContextAndRollingSnapshots();
    TruncationAndAdmission();
    MalformedAndByteLimits();
    OptionalLocationsAndIndependentFlags();
    ExactSourceBudget();
    std::cout << "Scoped structured script diagnostics, rolling identities and explicit loss units "
                 "verified\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
