#include "LogAdmission.h"
#include <array>
#include <iostream>
#include <latch>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    using namespace nexora::core;
    std::deque<LogRecord> pending;
    std::uint64_t sequence = 1, rejected = 0;
    LogRecord record{std::chrono::system_clock::now(), LogLevel::Warning, "category", "message", 7};
    const auto admit = [&](LogRecord candidate, std::size_t capacity = 2) {
      return detail::AdmitLog(std::move(candidate), capacity, pending, sequence, rejected);
    };
    Require(admit(record) && admit(record) && !admit(record) && pending.size() == 2 &&
                sequence == 3 && rejected == 1 && pending[0].sequence == 1 &&
                pending[1].sequence == 2,
            "exact pending budget changed accepted records/sequence");
    pending.clear();
    const auto reject = [&](LogRecord candidate) {
      const auto prior = sequence, drops = rejected;
      Require(!admit(std::move(candidate)) && sequence == prior && pending.empty() &&
                  rejected == drops + 1,
              "invalid log changed retained ingress or sequence");
    };
    auto bad = record;
    bad.level = static_cast<LogLevel>(255);
    reject(bad);
    bad = record;
    bad.category.assign(AsyncLogService::kMaximumCategoryBytes + 1, 'a');
    reject(bad);
    bad = record;
    bad.message.assign(AsyncLogService::kMaximumMessageBytes + 1, 'a');
    reject(bad);
    for (const auto &text : {std::string("a\0b", 3), std::string("\xc0\xaf", 2),
                             std::string("\xed\xa0\x80", 3), std::string("\xf0\x9f", 2)}) {
      bad = record;
      bad.message = text;
      reject(bad);
      bad = record;
      bad.category = text;
      reject(bad);
    }
    record.category.assign(AsyncLogService::kMaximumCategoryBytes, 'a');
    record.message.assign(AsyncLogService::kMaximumMessageBytes, 'b');
    record.message.reserve(4 * 1024 * 1024);
    Require(admit(record) && pending.back().category.size() == 256 &&
                pending.back().message.size() == 16 * 1024 &&
                pending.back().message.capacity() < record.message.capacity(),
            "exact payload budget or compact ownership failed");
    pending.clear();
    record.category = "\xe7\xb9\xbc\xe7\xba\x8c";
    record.message.clear();
    Require(admit(record), "UTF-8 category or empty message rejected");
    pending.clear();
    sequence = UINT64_MAX;
    rejected = UINT64_MAX - 1;
    Require(admit(record) && pending.back().sequence == UINT64_MAX && sequence == 0 &&
                !admit(record) && !admit(record) && rejected == UINT64_MAX,
            "sequence exhaustion or rejected counter wrapped");
    pending.clear();
    sequence = 1;
    Require(!admit(record, 0) && pending.empty(), "disabled pending admission accepted a log");

    AsyncLogService service{16, 1024};
    service.Start();
    std::array<std::thread, 4> workers;
    for (unsigned index = 0; index < workers.size(); ++index)
      workers[index] = std::thread([&, index] {
        for (unsigned item = 0; item < 200; ++item)
          service.Write(LogLevel::Info, "worker",
                        std::to_string(index) + ":" + std::to_string(item));
      });
    for (auto &worker : workers)
      worker.join();
    service.Flush();
    auto snapshot = service.SnapshotSince(0);
    Require(snapshot.records.size() == 16 && snapshot.consumed_sequence == 800 &&
                snapshot.rejected_records == 0 && snapshot.records.front().sequence == 785 &&
                snapshot.records.back().sequence == 800,
            "real concurrent producers violated bounded ordered observation");
    for (std::size_t index = 1; index < snapshot.records.size(); ++index)
      Require(snapshot.records[index].sequence == snapshot.records[index - 1].sequence + 1,
              "concurrent accepted sequences were duplicated or reordered");
    Require(service.SnapshotSince(800).records.empty(), "cursor observation duplicated records");
    const auto suffix = service.SnapshotSince(798);
    Require(suffix.records.size() == 2 && suffix.records[0].sequence == 799,
            "incremental observation included old records");
    service.Stop();
    for (unsigned cycle = 0; cycle < 32; ++cycle) {
      service.Start();
      service.Write(LogLevel::Fatal, "restart", "last accepted before Stop");
      service.Stop();
      Require(service.SnapshotSince(0).consumed_sequence == 801 + cycle,
              "Stop failed to drain or Start reused an accepted sequence");
    }
    Require(snapshot.records.front().sequence == 785 &&
                snapshot.records.front().category == "worker",
            "copied observation borrowed overwritten crash-ring strings");
    service.Write(LogLevel::Info, "stopped", "rejected");
    Require(service.SnapshotSince(832).records.empty() &&
                service.SnapshotSince(832).rejected_records == 1,
            "stopped traffic admitted or concealed a rejected record");
    AsyncLogService disabled{2, 0};
    disabled.Start();
    disabled.Write(LogLevel::Info, "disabled", "rejected");
    disabled.Flush();
    disabled.Stop();
    Require(disabled.SnapshotSince(0).records.empty() &&
                disabled.SnapshotSince(0).rejected_records == 1,
            "zero pending capacity did not report loss or Flush blocked");
    disabled.ReportRejected(UINT64_MAX);
    disabled.ReportRejected();
    Require(disabled.SnapshotSince(0).rejected_records == UINT64_MAX &&
                disabled.SnapshotSince(0).consumed_sequence == 0,
            "embedding rejection accounting wrapped or invented a consumed record");
    for (unsigned cycle = 0; cycle < 16; ++cycle) {
      AsyncLogService stopping{8, 2};
      stopping.Start();
      std::latch ready{1}, release{1};
      std::thread writer([&] {
        stopping.Write(LogLevel::Info, "stop race", "initial");
        ready.count_down();
        release.wait();
        for (unsigned index = 0; index < 1000; ++index)
          stopping.Write(LogLevel::Info, "stop race", "concurrent writer");
      });
      ready.wait();
      release.count_down();
      stopping.Stop();
      writer.join();
      const auto observation = stopping.SnapshotSince(0);
      Require(observation.consumed_sequence + observation.rejected_records == 1001 &&
                  observation.records.size() <= 8 && observation.consumed_sequence >= 1,
              "concurrent Write/Stop lost accepted/rejected traffic or retained worker state");
      stopping.Flush();
    }
    AsyncLogService clamped{std::numeric_limits<std::size_t>::max(),
                            std::numeric_limits<std::size_t>::max()};
    clamped.Start();
    for (unsigned index = 0; index < AsyncLogService::kMaximumRecords + 1; ++index) {
      clamped.Write(LogLevel::Info, "clamp", "bounded");
      clamped.Flush();
    }
    clamped.Stop();
    Require(clamped.CrashRingSnapshot().size() == AsyncLogService::kMaximumRecords &&
                clamped.SnapshotSince(0).records.front().sequence == 2,
            "oversized crash-ring capacity was not clamped");
    std::cout
        << "PASS: bounded Core log ingress, payloads, sequence/cursor, concurrency and lifecycle\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
