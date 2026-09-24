#include "Nexora/Core/JobSystem.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Nexora/Core/Platform.h"

namespace nexora::core {

struct JobHandle::State final {
  mutable std::mutex mutex;
  std::condition_variable completed;
  JobStatus status{JobStatus::Queued};
  std::exception_ptr failure;
};

bool JobHandle::IsValid() const noexcept { return state_ != nullptr; }
JobStatus JobHandle::Status() const noexcept {
  if (!state_)
    return JobStatus::Failed;
  std::lock_guard lock{state_->mutex};
  return state_->status;
}

struct JobSystem::Implementation final {
  struct Work final {
    JobDescriptor descriptor;
    std::vector<JobHandle> dependencies;
    std::shared_ptr<JobHandle::State> state;
  };

  explicit Implementation(std::size_t count)
      : requested_workers(count == 0 ? platform::HardwareConcurrency() : count) {}

  void Complete(const std::shared_ptr<JobHandle::State> &state, JobStatus status,
                std::exception_ptr failure = {}) {
    {
      std::lock_guard lock{state->mutex};
      state->status = status;
      state->failure = std::move(failure);
    }
    state->completed.notify_all();
    available.notify_all();
  }

  void Worker(std::size_t index) {
    platform::SetCurrentThreadName("Nexora.Worker" + std::to_string(index));
    while (true) {
      Work work;
      {
        std::unique_lock lock{mutex};
        available.wait(lock, [this] { return stopping || !queue.empty(); });
        if (stopping && queue.empty())
          return;
        const auto is_ready = [](const Work &candidate) {
          return std::ranges::all_of(candidate.dependencies, [](const JobHandle &dependency) {
            std::lock_guard dependency_lock{dependency.state_->mutex};
            const auto status = dependency.state_->status;
            return status == JobStatus::Completed || status == JobStatus::Cancelled ||
                   status == JobStatus::Failed;
          });
        };
        auto selected = queue.end();
        for (auto candidate = queue.begin(); candidate != queue.end(); ++candidate) {
          if (is_ready(*candidate) &&
              (selected == queue.end() ||
               selected->descriptor.priority < candidate->descriptor.priority)) {
            selected = candidate;
          }
        }
        if (selected == queue.end()) {
          available.wait_for(lock, std::chrono::milliseconds{1});
          continue;
        }
        work = std::move(*selected);
        queue.erase(selected);
      }
      if (work.descriptor.cancellation.IsCancellationRequested()) {
        Complete(work.state, JobStatus::Cancelled);
        continue;
      }
      {
        std::lock_guard lock{work.state->mutex};
        work.state->status = JobStatus::Running;
      }
      try {
        work.descriptor.function(work.descriptor.cancellation);
        Complete(work.state, work.descriptor.cancellation.IsCancellationRequested()
                                 ? JobStatus::Cancelled
                                 : JobStatus::Completed);
      } catch (...) {
        Complete(work.state, JobStatus::Failed, std::current_exception());
      }
    }
  }

  std::size_t requested_workers;
  std::mutex mutex;
  std::condition_variable available;
  std::deque<Work> queue;
  std::vector<std::thread> workers;
  bool running{false};
  bool stopping{false};
};

JobSystem::JobSystem(std::size_t worker_count)
    : implementation_(std::make_unique<Implementation>(worker_count)) {}
JobSystem::~JobSystem() { Stop(); }

void JobSystem::Start() {
  auto *const implementation = implementation_.get();
  std::lock_guard lock{implementation->mutex};
  if (implementation->running)
    return;
  implementation->stopping = false;
  implementation->running = true;
  for (std::size_t index = 0; index < implementation->requested_workers; ++index) {
    // Capture the stable implementation allocation directly. Capturing JobSystem's `this` made
    // worker teardown depend on the wrapper object's lifetime even though Stop joins the workers.
    implementation->workers.emplace_back(
        [implementation, index] { implementation->Worker(index); });
  }
}

void JobSystem::Stop() {
  {
    std::lock_guard lock{implementation_->mutex};
    if (!implementation_->running)
      return;
    implementation_->stopping = true;
  }
  implementation_->available.notify_all();
  for (auto &worker : implementation_->workers)
    if (worker.joinable())
      worker.join();
  {
    std::lock_guard lock{implementation_->mutex};
    implementation_->workers.clear();
    implementation_->running = false;
    implementation_->stopping = false;
  }
}

JobHandle JobSystem::Submit(JobDescriptor descriptor, std::span<const JobHandle> dependencies) {
  if (!descriptor.function)
    throw std::invalid_argument("job function is required");
  for (const auto &dependency : dependencies) {
    if (!dependency.IsValid())
      throw std::invalid_argument("job dependency is invalid");
  }
  auto state = std::make_shared<JobHandle::State>();
  {
    std::lock_guard lock{implementation_->mutex};
    if (!implementation_->running || implementation_->stopping) {
      throw std::logic_error("job system is not running");
    }
    implementation_->queue.push_back(
        {std::move(descriptor), {dependencies.begin(), dependencies.end()}, state});
  }
  implementation_->available.notify_one();
  return JobHandle{std::move(state)};
}

void JobSystem::Wait(const JobHandle &handle) {
  if (!handle.state_)
    throw std::invalid_argument("job handle is invalid");
  std::unique_lock lock{handle.state_->mutex};
  handle.state_->completed.wait(lock, [&handle] {
    const auto status = handle.state_->status;
    return status == JobStatus::Completed || status == JobStatus::Cancelled ||
           status == JobStatus::Failed;
  });
  if (handle.state_->failure)
    std::rethrow_exception(handle.state_->failure);
}

std::size_t JobSystem::WorkerCount() const noexcept { return implementation_->workers.size(); }

} // namespace nexora::core
