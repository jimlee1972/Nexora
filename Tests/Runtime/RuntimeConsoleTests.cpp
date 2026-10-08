#include "Nexora/Runtime/EditorSdk.h"

#include "../../Engine/Runtime/src/RuntimeConsoleAdmission.h"

#include <atomic>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora::runtime;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

RuntimeLogRecord Record(std::string message = "message") {
  return {999, RuntimeLogSeverity::Info, "category", 123, "source", std::move(message)};
}

bool Equal(const RuntimeLogRecord &left, const RuntimeLogRecord &right) {
  return left.sequence == right.sequence && left.severity == right.severity &&
         left.category == right.category &&
         left.timestamp_nanoseconds == right.timestamp_nanoseconds && left.source == right.source &&
         left.message == right.message;
}

void Admission() {
  RuntimeConsole console(1);
  auto exact = Record(std::string(RuntimeConsole::kMaxMessageBytes, 'm'));
  exact.category.assign(RuntimeConsole::kMaxCategoryBytes, 'c');
  exact.source.assign(RuntimeConsole::kMaxSourceBytes, 's');
  Require(console.Push(exact), "exact byte limits rejected");
  const auto baseline = console.Snapshot();
  Require(baseline.size() == 1 && baseline.front().sequence == 1 &&
              baseline.front().timestamp_nanoseconds == exact.timestamp_nanoseconds,
          "admission failed to replace caller sequence or preserve timestamp");
  std::uint64_t rejected{};
  const auto reject = [&](RuntimeLogRecord candidate) {
    Require(!console.Push(std::move(candidate)), "invalid record admitted");
    ++rejected;
    const auto after = console.Snapshot();
    Require(after.size() == 1 && Equal(after.front(), baseline.front()) &&
                console.DroppedCount() == rejected,
            "rejection evicted or mutated accepted history/drop count");
  };
  auto oversized = exact;
  oversized.category.push_back('c');
  reject(oversized);
  oversized = exact;
  oversized.source.push_back('s');
  reject(oversized);
  oversized = exact;
  oversized.message.push_back('m');
  reject(oversized);
  for (const auto severity :
       {static_cast<RuntimeLogSeverity>(-1), static_cast<RuntimeLogSeverity>(5),
        static_cast<RuntimeLogSeverity>(std::numeric_limits<int>::max())}) {
    auto bad = Record();
    bad.severity = severity;
    reject(std::move(bad));
  }

  // Isolated continuation, truncated widths, overlong encodings, surrogates, out-of-range scalar,
  // illegal leading bytes, and embedded NUL must reject in every displayed field.
  const std::vector<std::string> invalid{std::string("a\0b", 3),
                                         "\x80",
                                         "\xC2",
                                         "\xC0\xAF",
                                         "\xE0\x80\xAF",
                                         "\xED\xA0\x80",
                                         "\xF0\x80\x80\xAF",
                                         "\xF4\x90\x80\x80",
                                         "\xF5\x80\x80\x80",
                                         "\xFF",
                                         "\xC2"
                                         "x",
                                         "\xE2\x82",
                                         "\xF0\x9F\x98"};
  for (const auto &text : invalid) {
    auto bad = Record();
    bad.category = text;
    reject(bad);
    bad = Record();
    bad.source = text;
    reject(bad);
    bad = Record(text);
    reject(bad);
  }
  Require(console.Push({555, RuntimeLogSeverity::Fatal, {}, 0, {}, {}}),
          "empty fields or valid Fatal severity rejected");
  const auto empty = console.Snapshot();
  Require(empty.size() == 1 && empty.front().sequence == 2 && empty.front().message.empty() &&
              console.DroppedCount() == rejected + 1,
          "invalid records consumed accepted sequences or wrong eviction count");

  const std::string unicode = "\x01\n\t\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xED\x9F\xBF"
                              "\xEE\x80\x80\xEF\xBF\xBF\xF0\x90\x80\x80\xF4\x8F\xBF\xBF";
  auto valid = Record(unicode);
  valid.category = unicode;
  valid.source = unicode;
  for (const auto severity :
       {RuntimeLogSeverity::Trace, RuntimeLogSeverity::Info, RuntimeLogSeverity::Warning,
        RuntimeLogSeverity::Error, RuntimeLogSeverity::Fatal}) {
    valid.severity = severity;
    Require(console.Push(valid), "valid Unicode scalar boundary/control/severity rejected");
  }
  const auto owned = console.Snapshot();
  valid.category.clear();
  valid.source.clear();
  valid.message.clear();
  Require(owned.front().category == unicode && owned.front().source == unicode &&
              owned.front().message == unicode,
          "snapshot retained producer string borrows");
  Require(console.Push(Record("replacement")) && owned.front().message == unicode,
          "snapshot invalidated by replacement");

  auto unicode_limit = Record();
  unicode_limit.category.clear();
  unicode_limit.source.clear();
  unicode_limit.message.clear();
  for (std::size_t index = 0; index < RuntimeConsole::kMaxCategoryBytes / 4; ++index)
    unicode_limit.category += "\xF0\x9F\x98\x80";
  for (std::size_t index = 0; index < RuntimeConsole::kMaxSourceBytes / 4; ++index)
    unicode_limit.source += "\xF0\x9F\x98\x80";
  for (std::size_t index = 0; index < RuntimeConsole::kMaxMessageBytes / 4; ++index)
    unicode_limit.message += "\xF0\x9F\x98\x80";
  Require(console.Push(unicode_limit), "exact multibyte Unicode byte budgets rejected");
  const auto unicode_snapshot = console.Snapshot();
  unicode_limit.message.pop_back();
  Require(!console.Push(unicode_limit) &&
              Equal(console.Snapshot().front(), unicode_snapshot.front()),
          "invalid Unicode at byte-budget boundary mutated history");

  auto reserved = Record("small");
  reserved.category.reserve(1024 * 1024);
  reserved.source.reserve(1024 * 1024);
  reserved.message.reserve(1024 * 1024);
  Require(console.Push(std::move(reserved)), "short producer-reserved record rejected");
  auto internal_reserved = Record("small");
  internal_reserved.category.reserve(1024 * 1024);
  internal_reserved.source.reserve(1024 * 1024);
  internal_reserved.message.reserve(1024 * 1024);
  std::vector<RuntimeLogRecord> compact;
  std::uint64_t next = 1;
  std::uint64_t drops = 0;
  Require(detail::PushConsoleRecord(std::move(internal_reserved), 1, compact, next, drops),
          "production transaction rejected short reserved strings");
  Require(compact.front().category.capacity() <= RuntimeConsole::kMaxCategoryBytes &&
              compact.front().source.capacity() <= RuntimeConsole::kMaxSourceBytes &&
              compact.front().message.capacity() <= RuntimeConsole::kMaxMessageBytes,
          "ingress retained producer reserve allocations");
}

void CapacityAndCounters() {
  RuntimeConsole disabled(0);
  Require(!disabled.Push(Record()) && disabled.Snapshot().empty() && disabled.DroppedCount() == 1,
          "zero-capacity compatibility changed");
  RuntimeConsole clamped(std::numeric_limits<std::size_t>::max());
  for (std::size_t index = 0; index <= RuntimeConsole::kMaxRecords; ++index)
    Require(clamped.Push(Record()), "clamped console rejected valid record");
  const auto bounded = clamped.Snapshot();
  Require(bounded.size() == RuntimeConsole::kMaxRecords && bounded.front().sequence == 2 &&
              bounded.back().sequence == RuntimeConsole::kMaxRecords + 1 &&
              clamped.DroppedCount() == 1,
          "huge requested capacity escaped hard limit");

  // Exercise the production admission transaction with counters impossible to reach in a test's
  // lifetime; the public API deliberately exposes no counter injection/reset operation.
  auto records = std::vector<RuntimeLogRecord>{Record("retained")};
  records.front().sequence = std::numeric_limits<std::uint64_t>::max() - 1;
  auto next = std::numeric_limits<std::uint64_t>::max();
  auto dropped = std::numeric_limits<std::uint64_t>::max() - 1;
  auto invalid = Record();
  invalid.severity = static_cast<RuntimeLogSeverity>(-1);
  Require(!detail::PushConsoleRecord(invalid, 1, records, next, dropped) &&
              records.front().message == "retained" &&
              next == std::numeric_limits<std::uint64_t>::max() &&
              dropped == std::numeric_limits<std::uint64_t>::max(),
          "rejection at counter limit changed sequence/history or failed saturation");
  Require(detail::PushConsoleRecord(Record("last"), 1, records, next, dropped) && next == 0 &&
              records.front().sequence == std::numeric_limits<std::uint64_t>::max() &&
              records.front().message == "last" &&
              dropped == std::numeric_limits<std::uint64_t>::max(),
          "last valid sequence wrapped or eviction drop overflowed");
  const auto last = records.front();
  Require(!detail::PushConsoleRecord(Record("after"), 1, records, next, dropped) &&
              Equal(records.front(), last) && next == 0 &&
              dropped == std::numeric_limits<std::uint64_t>::max(),
          "sequence exhaustion reused zero/one or mutated history/drop count");
  Require(!detail::PushConsoleRecord(Record(), 0, records, next, dropped) &&
              Equal(records.front(), last) && dropped == std::numeric_limits<std::uint64_t>::max(),
          "disabled admission overflowed saturated drop count");
}

void ConcurrentOwnership() {
  constexpr std::size_t producers = 4;
  constexpr std::size_t records_per_producer = 1000;
  constexpr std::size_t capacity = 64;
  RuntimeConsole console(capacity);
  std::atomic<std::size_t> finished{};
  std::atomic<bool> good{true};
  std::vector<std::thread> workers;
  for (std::size_t producer = 0; producer < producers; ++producer)
    workers.emplace_back([&, producer] {
      for (std::size_t index = 0; index < records_per_producer; ++index) {
        const auto text = std::to_string(producer) + ":" + std::to_string(index);
        if (!console.Push(Record(text)))
          good = false;
        auto invalid = Record(std::string("bad\0text", 8));
        if (console.Push(std::move(invalid)))
          good = false;
      }
      ++finished;
    });
  std::vector<RuntimeLogRecord> retained;
  do {
    auto snapshot = console.Snapshot();
    if (snapshot.size() > capacity)
      good = false;
    for (std::size_t index = 1; index < snapshot.size(); ++index)
      if (snapshot[index].sequence != snapshot[index - 1].sequence + 1)
        good = false;
    if (retained.empty() && !snapshot.empty())
      retained = std::move(snapshot);
    std::this_thread::yield();
  } while (finished != producers);
  for (auto &worker : workers)
    worker.join();
  const auto final = console.Snapshot();
  constexpr auto accepted = producers * records_per_producer;
  Require(good && final.size() == capacity && final.front().sequence == accepted - capacity + 1 &&
              final.back().sequence == accepted &&
              console.DroppedCount() == accepted * 2 - capacity,
          "concurrent producers lost ordering, invalid admission or drop accounting");
  Require(!retained.empty() && retained.front().message.find(':') != std::string::npos,
          "producer eviction invalidated earlier owning snapshot");
}
} // namespace

int main() {
  try {
    Admission();
    CapacityAndCounters();
    ConcurrentOwnership();
    std::cout << "Runtime Console record admission passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
