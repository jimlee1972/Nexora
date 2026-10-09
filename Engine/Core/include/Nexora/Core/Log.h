#pragma once

#include "Nexora/Core/Api.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nexora::core {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warning, Error, Fatal };

struct LogRecord final {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level{LogLevel::Info};
  std::string category;
  std::string message;
  std::uint64_t thread_id{};
  // Assigned on accepted ingress; monotonic across Start/Stop, never reused.
  std::uint64_t sequence{};
};

struct LogSnapshot final {
  std::vector<LogRecord> records;
  std::uint64_t consumed_sequence{};
  std::uint64_t rejected_records{};
};

class NEXORA_CORE_API AsyncLogService final {
public:
  static constexpr std::size_t kMaximumRecords = 4096;
  static constexpr std::size_t kMaximumCategoryBytes = 256;
  static constexpr std::size_t kMaximumMessageBytes = 16 * 1024;
  explicit AsyncLogService(std::size_t crash_ring_capacity = 256,
                           std::size_t pending_capacity = 1024);
  ~AsyncLogService();
  AsyncLogService(const AsyncLogService &) = delete;
  AsyncLogService &operator=(const AsyncLogService &) = delete;

  // Lifecycle calls are serialized by the owner; Write/ReportRejected/Flush/snapshots may run
  // concurrently. Stop/destruction require all callers to finish before destroying the service.
  void Start();
  void Stop();
  // Concurrent writers transfer bounded, NUL-free UTF-8 strings to the service. Capacities clamp
  // to kMaximumRecords; pending zero disables admission. Rejections are counted, never truncated.
  // Flush blocks until all records accepted before the call have reached the
  // crash ring. Stop drains accepted records and is idempotent.
  void Write(LogLevel level, std::string category, std::string message);
  // Embeddings account rejected pointer/length wire data before allocating an owning record.
  void ReportRejected(std::uint64_t count = 1);
  void Flush();
  [[nodiscard]] std::vector<LogRecord> CrashRingSnapshot() const;
  // Copies consumed retained records newer than the cursor, plus consumed/rejected watermarks.
  // Never flushes or waits for producer traffic. Sequence gaps identify unread ring eviction.
  [[nodiscard]] LogSnapshot SnapshotSince(std::uint64_t sequence) const;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};

} // namespace nexora::core
