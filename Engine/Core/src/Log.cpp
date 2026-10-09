#include "Nexora/Core/Log.h"
#include "LogAdmission.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace nexora::core {
struct AsyncLogService::Implementation final {
  explicit Implementation(std::size_t capacity, std::size_t pending_capacity)
      : capacity(capacity), pending_capacity(pending_capacity) {}
  void Consume() {
    while (true) {
      std::unique_lock lock{mutex};
      available.wait(lock, [this] { return stopping || !pending.empty(); });
      while (!pending.empty()) {
        auto record = std::move(pending.front());
        pending.pop_front();
        if (ring.size() == capacity)
          ring.pop_front();
        ring.push_back(std::move(record));
        consumed_sequence = ring.back().sequence;
      }
      drained.notify_all();
      if (stopping)
        return;
    }
  }
  std::size_t capacity;
  std::size_t pending_capacity;
  mutable std::mutex mutex;
  std::condition_variable available;
  std::condition_variable drained;
  std::deque<LogRecord> pending;
  std::deque<LogRecord> ring;
  std::thread consumer;
  bool running{false};
  bool stopping{false};
  std::uint64_t next_sequence{1}, consumed_sequence{}, rejected_records{};
};

AsyncLogService::AsyncLogService(std::size_t capacity, std::size_t pending_capacity)
    : implementation_(std::make_unique<Implementation>(
          std::min(capacity, kMaximumRecords), std::min(pending_capacity, kMaximumRecords))) {
  if (capacity == 0)
    throw std::invalid_argument("crash ring capacity must be non-zero");
}
AsyncLogService::~AsyncLogService() { Stop(); }
void AsyncLogService::Start() {
  std::lock_guard lock{implementation_->mutex};
  if (implementation_->running)
    return;
  implementation_->stopping = false;
  implementation_->consumer = std::thread([state = implementation_.get()] { state->Consume(); });
  implementation_->running = true;
}
void AsyncLogService::Stop() {
  {
    std::lock_guard lock{implementation_->mutex};
    if (!implementation_->running)
      return;
    implementation_->stopping = true;
  }
  implementation_->available.notify_one();
  if (implementation_->consumer.joinable())
    implementation_->consumer.join();
  {
    std::lock_guard lock{implementation_->mutex};
    implementation_->running = false;
    std::deque<LogRecord>{}.swap(implementation_->pending);
  }
}
void AsyncLogService::Write(LogLevel level, std::string category, std::string message) {
  std::lock_guard lock{implementation_->mutex};
  if (!implementation_->running || implementation_->stopping) {
    detail::CountRejectedLog(implementation_->rejected_records);
    return;
  }
  const auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
  if (detail::AdmitLog({std::chrono::system_clock::now(), level, std::move(category),
                        std::move(message), thread_id},
                       implementation_->pending_capacity, implementation_->pending,
                       implementation_->next_sequence, implementation_->rejected_records))
    implementation_->available.notify_one();
}
void AsyncLogService::Flush() {
  std::unique_lock lock{implementation_->mutex};
  const auto accepted_sequence =
      implementation_->next_sequence == 0 ? UINT64_MAX : implementation_->next_sequence - 1;
  implementation_->available.notify_one();
  implementation_->drained.wait(lock, [this, accepted_sequence] {
    return implementation_->consumed_sequence >= accepted_sequence;
  });
}
void AsyncLogService::ReportRejected(std::uint64_t count) {
  std::lock_guard lock{implementation_->mutex};
  auto &rejected = implementation_->rejected_records;
  rejected = count > UINT64_MAX - rejected ? UINT64_MAX : rejected + count;
}
std::vector<LogRecord> AsyncLogService::CrashRingSnapshot() const {
  std::lock_guard lock{implementation_->mutex};
  return {implementation_->ring.begin(), implementation_->ring.end()};
}
LogSnapshot AsyncLogService::SnapshotSince(std::uint64_t sequence) const {
  std::lock_guard lock{implementation_->mutex};
  LogSnapshot snapshot{{}, implementation_->consumed_sequence, implementation_->rejected_records};
  for (const auto &record : implementation_->ring)
    if (record.sequence > sequence)
      snapshot.records.push_back(record);
  return snapshot;
}
} // namespace nexora::core
