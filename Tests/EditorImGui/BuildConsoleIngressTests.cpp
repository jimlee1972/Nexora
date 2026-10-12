#include "BuildConsoleIngress.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  using namespace nexora;
  using namespace editor;
  runtime::RuntimeConsole console{64};
  preview::BuildConsoleIngress ingress{console};
  BuildProcessSnapshot sample;
  sample.operation = 1;
  sample.scope = 7;
  sample.phase = BuildProcessPhase::Running;
  sample.output = std::string("A\0", 2) + "\xff\x1b\n\\\"";
  Require(ingress.Poll(sample, 8) && ingress.Poll(sample, 0) && console.Snapshot().empty(),
          "Foreign/detached output reached Console");
  Require(ingress.Poll(sample, 7), "Actual binary snapshot rejected");
  const auto records = console.Snapshot();
  Require(records.size() == 1 && records[0].message == "A\\x00\\xff\\x1b\\x0a\\x5c\\x22" &&
              records[0].category == "BuildOutput" &&
              records[0].source == "BuildProcess scope=7 operation=1" &&
              records[0].severity == runtime::RuntimeLogSeverity::Info &&
              records[0].timestamp_nanoseconds && ingress.ConsumedRawBytes() == 7,
          "Arbitrary output bytes or receipt timestamp were not conserved");
  Require(ingress.Poll(sample, 7) && console.Snapshot().size() == 1,
          "Repeated snapshot duplicated output");
  sample.output += "tail";
  Require(ingress.Poll(sample, 7) && console.Snapshot().back().message == "tail",
          "Cumulative snapshot tail failed");
  sample.output = "end";
  sample.dropped_output_bytes = 15;
  Require(ingress.Poll(sample, 7) && ingress.MissingRawBytes() == 4 &&
              console.Snapshot().back().message == "end" &&
              console.Snapshot()[2].message.find("4 raw bytes") != std::string::npos &&
              console.Snapshot()[2].severity == runtime::RuntimeLogSeverity::Warning &&
              console.DroppedCount() == 0,
          "Missing source bytes were confused with Console record drops");
  const auto count = console.Snapshot().size();
  sample.output = "old";
  sample.dropped_output_bytes = 0;
  Require(!ingress.Poll(sample, 7) && console.Snapshot().size() == count,
          "Regressed snapshot mutated history");
  sample.operation = 2;
  sample.output.assign(64 * 1024, '\0');
  Require(ingress.Poll(sample, 7) && console.Snapshot().size() == count + 4 &&
              console.Snapshot().back().message.size() == runtime::RuntimeConsole::kMaxMessageBytes,
          "Per-poll or escaped-message budget exceeded");
  for (int i = 0; i < 3; ++i)
    Require(ingress.Poll(sample, 7), "Bounded backlog failed");
  const auto done = ingress.ConsumedRawBytes();
  Require(done == 14 + 64 * 1024 && ingress.Poll(sample, 7) && ingress.ConsumedRawBytes() == done,
          "Chunked draining duplicated or lost source bytes");
  sample.operation = 1;
  Require(!ingress.Poll(sample, 7), "Older operation reset scope cursor");
  sample.scope = 9;
  sample.output = "next";
  Require(ingress.Poll(sample, 9) && console.Snapshot().back().message == "next",
          "New project scope failed independent admission");
  const auto before = ingress.ForwardedRecords();
  sample.operation = 3;
  sample.dropped_output_bytes = std::numeric_limits<std::uint64_t>::max();
  Require(!ingress.Poll(sample, 9) && ingress.ForwardedRecords() == before,
          "Cumulative byte overflow mutated cursor or Console");
  sample.dropped_output_bytes = 0;
  sample.output.assign(BuildProcess::kMaximumOutputBytes + 1, 'x');
  Require(!ingress.Poll(sample, 9), "Oversized process snapshot accepted");
  sample.output = "stale";
  sample.phase = BuildProcessPhase::Stale;
  Require(ingress.Poll(sample, 9) && ingress.ForwardedRecords() == before,
          "Stale project bytes were republished");
  Require(records[0].message == "A\\x00\\xff\\x1b\\x0a\\x5c\\x22",
          "Retained owning records changed after later polls");
  runtime::RuntimeConsole disabled{0};
  preview::BuildConsoleIngress rejected{disabled};
  sample.phase = BuildProcessPhase::Failed;
  sample.output = "rejected";
  Require(rejected.Poll(sample, 9) && rejected.ForwardedRecords() == 0 &&
              rejected.ConsumedRawBytes() == 8 && disabled.DroppedCount() == 1 &&
              rejected.Poll(sample, 9) && disabled.DroppedCount() == 1,
          "Console admission failure retried or confused loss units");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Scoped real-byte Console ingress, loss units and budgets verified.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
