#include "Nexora/Core/Engine.h"

#include <stdexcept>

namespace nexora::core {
struct Engine::Implementation final {
  std::unique_ptr<TrackingAllocator> memory;
  std::unique_ptr<JobSystem> jobs;
  std::unique_ptr<VirtualFileSystem> vfs;
  std::unique_ptr<AsyncLogService> log;
  std::unique_ptr<EventBus> events;
  std::unique_ptr<FixedTickClock> clock;
  std::unique_ptr<FrameArena> frame_arena;
  bool initialized{false};
};
Engine::Engine() : implementation_(std::make_unique<Implementation>()) {}
Engine::~Engine() { Shutdown(); }

void Engine::Initialize(const EngineConfiguration &configuration) {
  if (implementation_->initialized)
    throw std::logic_error("engine is already initialized");
  auto memory = std::make_unique<TrackingAllocator>();
  auto log = std::make_unique<AsyncLogService>(configuration.crash_ring_capacity);
  auto jobs = std::make_unique<JobSystem>(configuration.worker_count);
  auto vfs = std::make_unique<VirtualFileSystem>(*jobs);
  auto events = std::make_unique<EventBus>();
  auto clock = std::make_unique<FixedTickClock>(1.0 / 60.0, 4, 0.25);
  auto frame_arena = std::make_unique<FrameArena>(configuration.frame_arena_capacity);
  log->Start();
  jobs->Start();
  if (!vfs->Mount("content", configuration.content_root)) {
    jobs->Stop();
    log->Stop();
    throw std::runtime_error("failed to mount content root");
  }
  implementation_->memory = std::move(memory);
  implementation_->log = std::move(log);
  implementation_->jobs = std::move(jobs);
  implementation_->vfs = std::move(vfs);
  implementation_->events = std::move(events);
  implementation_->clock = std::move(clock);
  implementation_->frame_arena = std::move(frame_arena);
  implementation_->initialized = true;
  implementation_->log->Write(LogLevel::Info, "Engine", "initialized");
}

void Engine::Shutdown() noexcept {
  if (!implementation_->initialized)
    return;
  implementation_->log->Write(LogLevel::Info, "Engine", "shutting down");
  implementation_->jobs->Stop();
  implementation_->log->Stop();
  implementation_->frame_arena.reset();
  implementation_->clock.reset();
  implementation_->events.reset();
  implementation_->vfs.reset();
  implementation_->jobs.reset();
  implementation_->log.reset();
  implementation_->memory.reset();
  implementation_->initialized = false;
}
bool Engine::IsInitialized() const noexcept { return implementation_->initialized; }
EngineServices Engine::Services() noexcept {
  return {implementation_->memory.get(), implementation_->jobs.get(), implementation_->vfs.get(),
          implementation_->log.get(), implementation_->events.get()};
}
FixedTickClock &Engine::Clock() {
  if (!implementation_->initialized)
    throw std::logic_error("engine is not initialized");
  return *implementation_->clock;
}
void Engine::BeginFrame() {
  if (!implementation_->initialized)
    throw std::logic_error("engine is not initialized");
  implementation_->frame_arena->Reset();
  implementation_->events->DispatchDeferred();
}
} // namespace nexora::core
