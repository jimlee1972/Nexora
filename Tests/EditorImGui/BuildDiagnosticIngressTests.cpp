#include "BuildConsoleIngress.h"
#include "BuildDiagnosticIngress.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using editor::BuildProcessPhase;
using editor::BuildProcessSnapshot;
using editor::preview::BuildDiagnosticIngress;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
BuildProcessSnapshot Sample(std::string text = {}) {
  BuildProcessSnapshot value;
  value.scope = 7;
  value.operation = 1;
  value.phase = BuildProcessPhase::Running;
  value.output = std::move(text);
  return value;
}
void EverySplit() {
  const std::string path = "src/\xe4\xbe\x86\xe6\xba\x90.zig";
  const auto bytes = "ordinary output\n" + path + ":12:7: error: broken token\r\n" +
                     "C:\\source file.cpp:3:4: warning: deprecated\n" +
                     "include.h:9:2: note: declared here\n";
  for (std::size_t split = 0; split <= bytes.size(); ++split) {
    runtime::RuntimeConsole console{16};
    BuildDiagnosticIngress ingress{console};
    auto sample = Sample(bytes.substr(0, split));
    Require(ingress.Poll(sample, 7), "partial snapshot rejected");
    sample.output = bytes;
    Require(ingress.Poll(sample, 7), "split continuation rejected");
    const auto rows = console.Snapshot();
    Require(rows.size() == 3 && rows[0].category == "BuildDiagnostic" &&
                rows[0].severity == runtime::RuntimeLogSeverity::Error &&
                rows[0].source == "BuildProcess scope=7 operation=1 | " + path + ":12:7" &&
                rows[0].message == "broken token" && rows[0].timestamp_nanoseconds &&
                rows[1].severity == runtime::RuntimeLogSeverity::Warning &&
                rows[1].source.ends_with("C:\\source file.cpp:3:4") &&
                rows[2].severity == runtime::RuntimeLogSeverity::Info &&
                rows[2].message == "declared here" && ingress.ConsumedRawBytes() == bytes.size(),
            "compiler locations, UTF-8, severity, CRLF or split bytes changed");
    Require(ingress.Poll(sample, 7) && console.Snapshot().size() == 3,
            "same snapshot duplicated derived records");
    sample.operation = 2;
    sample.output = "next.zig:1:1: error: next\n";
    Require(ingress.Poll(sample, 7) && rows[0].message == "broken token" &&
                console.Snapshot().back().source.starts_with("BuildProcess scope=7 operation=2"),
            "owning records or operation provenance changed");
  }
}
void BoundariesAndLoss() {
  runtime::RuntimeConsole console{256};
  BuildDiagnosticIngress ingress{console};
  auto sample = Sample("old.zig:1:1: err");
  Require(ingress.Poll(sample, 7) && console.Snapshot().empty(), "partial line published");
  sample.dropped_output_bytes = 100;
  sample.output = "or: spliced\nreal.zig:2:3: warning: retained\n";
  Require(ingress.Poll(sample, 7) && console.Snapshot().size() == 1 &&
              console.Snapshot()[0].source.ends_with("real.zig:2:3") &&
              ingress.MissingRawBytes() == 100 - std::string("old.zig:1:1: err").size() &&
              console.DroppedCount() == 0,
          "missing bytes spliced a false diagnostic or became record drops");
  sample.operation = 2;
  sample.dropped_output_bytes = 0;
  sample.output = "scope.zig:1:1: err";
  Require(ingress.Poll(sample, 7), "operation reset failed");
  sample.scope = 8;
  sample.output = "or: forbidden\ntail.zig:4:5: error: terminal";
  Require(ingress.Poll(sample, 8) && console.Snapshot().size() == 1,
          "scope change spliced buffered data or published running tail");
  sample.phase = BuildProcessPhase::Failed;
  Require(ingress.Poll(sample, 8) && console.Snapshot().size() == 2 &&
              console.Snapshot().back().message == "terminal" && ingress.Poll(sample, 8) &&
              console.Snapshot().size() == 2,
          "terminal tail missing or duplicated");
  sample.output += "\nlate.zig:1:1: error: late\n";
  Require(!ingress.Poll(sample, 8) && console.Snapshot().size() == 2,
          "terminal source mutated its accepted tail");
  sample.operation = 3;
  sample.phase = BuildProcessPhase::Running;
  sample.output.assign(BuildDiagnosticIngress::kMaximumLineBytes + 1, 'x');
  sample.output += "\na.zig:1:1: error: after long line\n";
  Require(ingress.Poll(sample, 8) && ingress.OversizedLines() == 1 &&
              ingress.PendingLineBytes() == 0 && console.Snapshot().size() == 4 &&
              console.Snapshot()[2].message == "after long line" &&
              console.Snapshot()[3].category == "BuildDiagnosticStatus",
          "oversized line escaped its bound or hid following diagnostic");
}
void BudgetsAndRejection() {
  runtime::RuntimeConsole console{256};
  BuildDiagnosticIngress ingress{console};
  auto sample = Sample();
  for (int i = 0; i < 100; ++i)
    sample.output += "a.zig:1:2: error: many\n";
  Require(ingress.Poll(sample, 7) && ingress.ForwardedRecords() == 64 &&
              ingress.SuppressedDiagnostics() == 36 && console.DroppedCount() == 36 &&
              console.Snapshot().size() == 65 &&
              console.Snapshot().back().message.find("36 parsed records") != std::string::npos,
          "derived admission budget or explicit record loss units incorrect");
  Require(ingress.Poll(sample, 7) && console.Snapshot().size() == 65 &&
              console.DroppedCount() == 36,
          "repeated snapshot duplicated suppression accounting");
  sample.operation = 2;
  sample.output.assign(64 * 1024, 'x');
  const auto before = ingress.ConsumedRawBytes();
  Require(ingress.Poll(sample, 7) && ingress.ConsumedRawBytes() == before + 16 * 1024 &&
              ingress.PendingLineBytes() <= BuildDiagnosticIngress::kMaximumLineBytes,
          "raw scan or retained line exceeded per-poll bounds");
  const auto history = console.Snapshot().size();
  auto invalid = sample;
  invalid.output.resize(32 * 1024);
  Require(!ingress.Poll(invalid, 7) && console.Snapshot().size() == history,
          "regressed observed snapshot changed history despite cursor lag");
  invalid = sample;
  invalid.dropped_output_bytes = UINT64_MAX;
  Require(!ingress.Poll(invalid, 7), "overflowing cumulative bytes accepted");
  invalid.dropped_output_bytes = 0;
  invalid.output.resize(editor::BuildProcess::kMaximumOutputBytes + 1);
  Require(!ingress.Poll(invalid, 7), "oversized snapshot accepted");
  invalid = Sample("foreign.zig:1:1: error: no\n");
  Require(ingress.Poll(invalid, 9) && ingress.Poll(invalid, 0) &&
              console.Snapshot().size() == history,
          "foreign/detached source reached Console");
  invalid.phase = BuildProcessPhase::Stale;
  Require(ingress.Poll(invalid, 7), "stale snapshot was not ignored");
  runtime::RuntimeConsole disabled{0};
  BuildDiagnosticIngress rejected{disabled};
  invalid = Sample("rejected.zig:1:1: error: no\n");
  Require(rejected.Poll(invalid, 7) && disabled.DroppedCount() == 1 &&
              rejected.ForwardedRecords() == 0 && rejected.Poll(invalid, 7) &&
              disabled.DroppedCount() == 1,
          "Console rejection retried or miscounted a derived record");
}
void RawAndMalformed() {
  runtime::RuntimeConsole console{32};
  editor::preview::BuildConsoleIngress raw{console};
  BuildDiagnosticIngress diagnostic{console};
  auto sample = Sample("a.zig:1:2: error: real\n");
  Require(raw.Poll(sample, 7) && diagnostic.Poll(sample, 7) && console.Snapshot().size() == 2 &&
              console.Snapshot()[0].category == "BuildOutput" &&
              console.Snapshot()[0].severity == runtime::RuntimeLogSeverity::Info &&
              console.Snapshot()[0].message == "a.zig:1:2: error: real\\x0a" &&
              raw.ConsumedRawBytes() == diagnostic.ConsumedRawBytes(),
          "derived projection changed original raw bytes, severity or counters");
  sample.operation = 2;
  sample.output = "a.zig:0:1: error: zero\na.zig:1:4294967296: error: overflow\n"
                  "a.zig:1:2: Error: unknown\na.zig:1:2: error: ";
  sample.output += std::string(1, static_cast<char>(0xff)) + "\n";
  sample.output += "a.zig:1:2: error: embedded" + std::string(1, '\0') + "nul\n";
  Require(diagnostic.Poll(sample, 7) && console.Snapshot().size() == 2,
          "invalid coordinates, severity spelling or unsafe bytes became diagnostics");
}
void ExactBoundsAndPrefix() {
  runtime::RuntimeConsole console{16};
  BuildDiagnosticIngress ingress{console};
  const std::string prefix = "a.zig:1:1: error: ";
  auto sample = Sample(
      prefix + std::string(BuildDiagnosticIngress::kMaximumLineBytes - prefix.size(), 'x') + "\n");
  Require(ingress.Poll(sample, 7) && ingress.ForwardedRecords() == 1 &&
              ingress.OversizedLines() == 0,
          "exact maximum line was incorrectly discarded");
  sample.operation = 2;
  sample.output =
      std::string(runtime::RuntimeConsole::kMaxSourceBytes, 'p') + ":1:1: error: too much source\n";
  Require(ingress.Poll(sample, 7) && ingress.SuppressedDiagnostics() == 1 &&
              console.DroppedCount() == 1 && ingress.ForwardedRecords() == 1,
          "oversized source bypassed admission or lost record accounting");
  sample.operation = 3;
  sample.dropped_output_bytes = 4;
  sample.output = "prefix.zig:1:1: error: unknown start\nvalid.zig:2:3: note: visible\n";
  Require(ingress.Poll(sample, 7) && ingress.MissingRawBytes() == 4 &&
              ingress.ForwardedRecords() == 2 && console.Snapshot().back().message == "visible",
          "initial retained prefix fabricated a diagnostic");
  const auto count = console.Snapshot().size();
  auto invalid = sample;
  invalid.operation = 2;
  Require(!ingress.Poll(invalid, 7), "older operation was accepted");
  invalid = sample;
  invalid.dropped_output_bytes = 3;
  invalid.output += "x";
  Require(!ingress.Poll(invalid, 7), "regressed drop cursor was accepted");
  invalid = sample;
  invalid.phase = static_cast<BuildProcessPhase>(255);
  Require(!ingress.Poll(invalid, 7) && console.Snapshot().size() == count,
          "invalid observations changed Console history");
}
} // namespace
int main() {
  try {
    EverySplit();
    BoundariesAndLoss();
    BudgetsAndRejection();
    RawAndMalformed();
    ExactBoundsAndPrefix();
    std::cout << "Bounded compiler locations/severity, split input, scope/loss and raw "
                 "conservation verified\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
