#include "Nexora/Core/Log.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace nexora::core {
struct AsyncLogService::Implementation final {
  explicit Implementation(std::size_t capacity) : capacity(capacity) {}
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
      }
      drained.notify_all();
      if (stopping)
        return;
    }
  }
  std::size_t capacity;
  mutable std::mutex mutex;
  std::condition_variable available;
  std::condition_variable drained;
  std::deque<LogRecord> pending;
  std::deque<LogRecord> ring;
  std::thread consumer;
  bool running{false};
  bool stopping{false};
};

AsyncLogService::AsyncLogService(std::size_t capacity)
    : implementation_(std::make_unique<Implementation>(capacity)) {
  if (capacity == 0)
    throw std::invalid_argument("crash ring capacity must be non-zero");
}
AsyncLogService::~AsyncLogService() { Stop(); }
void AsyncLogService::Start() {
  std::lock_guard lock{implementation_->mutex};
  if (implementation_->running)
    return;
  implementation_->stopping = false;
  implementation_->running = true;
  implementation_->consumer = std::thread([this] { implementation_->Consume(); });
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
  implementation_->running = false;
}
void AsyncLogService::Write(LogLevel level, std::string category, std::string message) {
  std::lock_guard lock{implementation_->mutex};
  if (!implementation_->running || implementation_->stopping)
    return;
  const auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
  implementation_->pending.push_back({std::chrono::system_clock::now(), level, std::move(category),
                                      std::move(message), thread_id});
  implementation_->available.notify_one();
}
void AsyncLogService::Flush() {
  std::unique_lock lock{implementation_->mutex};
  implementation_->available.notify_one();
  implementation_->drained.wait(lock, [this] { return implementation_->pending.empty(); });
}
std::vector<LogRecord> AsyncLogService::CrashRingSnapshot() const {
  std::lock_guard lock{implementation_->mutex};
  return {implementation_->ring.begin(), implementation_->ring.end()};
}
} // namespace nexora::core
