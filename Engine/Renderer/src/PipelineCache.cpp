#include "Nexora/Renderer/PipelineCache.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace nexora::renderer {
struct PipelineFuture::State final {
  std::atomic_bool ready{false};
  bool done{false};
  std::mutex mutex;
  std::condition_variable completed;
  std::exception_ptr failure;
  rhi::PipelineHandle pipeline;
};
bool PipelineFuture::IsReady() const noexcept {
  return state_ && state_->ready.load(std::memory_order_acquire);
}
void PipelineFuture::Wait() const {
  if (!state_)
    throw std::logic_error("pipeline future is invalid");
  std::unique_lock lock{state_->mutex};
  state_->completed.wait(lock, [this] { return state_->done; });
}
rhi::PipelineHandle PipelineFuture::Get() const {
  Wait();
  std::lock_guard lock{state_->mutex};
  if (state_->failure)
    std::rethrow_exception(state_->failure);
  if (!state_->ready.load(std::memory_order_acquire))
    throw std::logic_error("pipeline is not ready");
  return state_->pipeline;
}

struct PipelineCache::Implementation final {
  struct Key final {
    std::uint64_t layout_hash{};
    std::uint64_t shader_hash{};
    rhi::TextureFormat color_format{};
    friend bool operator==(const Key &, const Key &) = default;
  };
  struct KeyHash final {
    std::size_t operator()(const Key &key) const noexcept {
      auto hash = static_cast<std::size_t>(key.layout_hash);
      hash ^= static_cast<std::size_t>(key.shader_hash) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
      hash ^=
          static_cast<std::size_t>(key.color_format) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
      return hash;
    }
  };
  struct Entry final {
    std::shared_ptr<PipelineFuture::State> state;
    core::JobHandle job;
  };
  Implementation(rhi::Device &device, core::JobSystem &jobs) : device(device), jobs(jobs) {}
  rhi::Device &device;
  core::JobSystem &jobs;
  mutable std::mutex mutex;
  std::unordered_map<Key, Entry, KeyHash> entries;
};
PipelineCache::PipelineCache(rhi::Device &device, core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(device, jobs)) {}
PipelineCache::~PipelineCache() {
  std::vector<Implementation::Entry> entries;
  {
    std::lock_guard lock{implementation_->mutex};
    for (const auto &[key, entry] : implementation_->entries) {
      (void)key;
      entries.push_back(entry);
    }
  }
  for (const auto &entry : entries) {
    try {
      implementation_->jobs.Wait(entry.job);
    } catch (...) {
      continue;
    }
    if (entry.state->ready.load(std::memory_order_acquire)) {
      implementation_->device.DestroyPipeline(entry.state->pipeline);
    }
  }
}
PipelineFuture PipelineCache::Request(const rhi::PipelineDescriptor &descriptor) {
  const Implementation::Key key{descriptor.layout_hash, descriptor.shader_hash,
                                descriptor.color_format};
  std::lock_guard lock{implementation_->mutex};
  if (const auto found = implementation_->entries.find(key);
      found != implementation_->entries.end()) {
    return {found->second.state, found->second.job};
  }
  auto state = std::make_shared<PipelineFuture::State>();
  auto job =
      implementation_->jobs.Submit({[this, state, descriptor](const core::CancellationToken &) {
                                      try {
                                        const auto pipeline =
                                            implementation_->device.CreatePipeline(descriptor);
                                        std::lock_guard state_lock{state->mutex};
                                        state->pipeline = pipeline;
                                        state->ready.store(true, std::memory_order_release);
                                        state->done = true;
                                      } catch (...) {
                                        std::lock_guard state_lock{state->mutex};
                                        state->failure = std::current_exception();
                                        state->done = true;
                                        state->completed.notify_all();
                                        throw;
                                      }
                                      state->completed.notify_all();
                                    },
                                    core::JobPriority::Normal,
                                    {},
                                    "Create graphics pipeline"});
  implementation_->entries.emplace(key, Implementation::Entry{state, job});
  return {std::move(state), std::move(job)};
}
std::size_t PipelineCache::Size() const noexcept {
  std::lock_guard lock{implementation_->mutex};
  return implementation_->entries.size();
}
} // namespace nexora::renderer
