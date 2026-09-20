#include "Nexora/Core/Engine.h"
#include "Nexora/Core/Handle.h"
#include "Nexora/Core/Platform.h"
#include "Nexora/Core/TaskGraph.h"
#include "Nexora/Core/Timer.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#if defined(__linux__)
#include <pthread.h>
#endif

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct TestHandleTag;
struct TestEvent final {
  int value;
};
} // namespace

int RunTests() {
  using namespace nexora::core;

  HandlePool<TestHandleTag> pool;
  const auto old_handle = pool.Create();
  Require(pool.Destroy(old_handle), "live handle must be destroyed");
  Require(!pool.Contains(old_handle), "stale handle must be rejected");
  const auto new_handle = pool.Create();
  Require(new_handle.index == old_handle.index && new_handle.generation != old_handle.generation,
          "reused handle must advance generation");

  TrackingAllocator allocator;
  constexpr MemoryTag kTag = 7;
  auto *memory = allocator.Allocate(64, alignof(std::max_align_t), kTag);
  Require(allocator.BytesForTag(kTag) == 64, "tag accounting must record allocations");
  allocator.Deallocate(memory);
  Require(allocator.Statistics().live_bytes == 0, "allocation must be released");

  FrameArena arena{128};
  (void)arena.Allocate(16, 8);
  const auto generation = arena.Generation();
  arena.Reset();
  Require(arena.Used() == 0 && arena.Generation() != generation,
          "frame reset must invalidate generation");

  JobSystem jobs{2};
  jobs.Start();
  std::atomic_int sequence{0};
  const auto first = jobs.Submit({[&sequence](const CancellationToken &) { sequence.store(1); },
                                  JobPriority::Normal,
                                  {},
                                  "first"});
  const std::array dependencies{first};
  const auto second = jobs.Submit({[&sequence](const CancellationToken &) {
                                     if (sequence.load() == 1)
                                       sequence.store(2);
                                   },
                                   JobPriority::High,
                                   {},
                                   "second"},
                                  dependencies);
  jobs.Wait(second);
  Require(sequence.load() == 2, "job dependency must complete first");
  CancellationSource cancellation;
  cancellation.Cancel();
  const auto cancelled = jobs.Submit(
      {[](const CancellationToken &) {}, JobPriority::Normal, cancellation.Token(), "cancelled"});
  jobs.Wait(cancelled);
  Require(cancelled.Status() == JobStatus::Cancelled, "cancelled job must not execute");

  Require(platform::HardwareConcurrency() >= 1, "hardware concurrency must never report zero");
  platform::SetCurrentThreadName("Nexora.CoreTests");
#if defined(__linux__)
  {
    char observed_name[16] = {};
    Require(pthread_getname_np(pthread_self(), observed_name, sizeof(observed_name)) == 0 &&
                std::string_view{observed_name} == "Nexora.CoreTest",
            "thread name must round-trip on Linux");
  }
#endif

  TaskGraph graph;
  std::atomic_int graph_value{0};
  (void)graph.Add(
      {"writer", {}, {1}, [&graph_value](const CancellationToken &) { graph_value.store(9); }});
  const auto reader_index =
      graph.Add({"reader", {1}, {}, [&graph_value](const CancellationToken &) {
                   if (graph_value.load() == 9)
                     graph_value.store(10);
                 }});
  graph.Compile();
  Require(graph.Dependencies(reader_index).size() == 1, "resource conflict must create an edge");
  const auto graph_jobs = graph.Execute(jobs);
  jobs.Wait(graph_jobs.back());
  Require(graph_value.load() == 10, "compiled task graph must preserve resource ordering");

  const auto temporary = std::filesystem::temp_directory_path() / "nexora-core-tests";
  std::filesystem::create_directories(temporary);
  {
    std::ofstream file{temporary / "sample.bin", std::ios::binary};
    file << "nexora";
  }
  VirtualFileSystem vfs{jobs};
  Require(vfs.Mount("test", temporary), "mount must succeed");
  const auto read = vfs.Read("test/sample.bin");
  Require(read.status == ReadResult::Status::Completed && read.bytes.size() == 6,
          "read must succeed");
  Require(vfs.Read("test/../escape").status == ReadResult::Status::InvalidPath,
          "path traversal must be rejected");
  CancellationSource read_cancel;
  read_cancel.Cancel();
  const auto async = vfs.ReadAsync("test/sample.bin", read_cancel.Token());
  for (int attempt = 0; attempt < 100 && async.Get().status == ReadResult::Status::Pending;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  Require(async.Get().status == ReadResult::Status::Cancelled,
          "async cancellation must be observed");
  jobs.Stop();
  std::filesystem::remove_all(temporary);

  EventBus events;
  int event_value = 0;
  const auto subscription = events.Subscribe<TestEvent>(
      [&event_value](const TestEvent &event) { event_value = event.value; });
  events.Enqueue(TestEvent{42});
  events.DispatchDeferred();
  Require(event_value == 42, "deferred event must dispatch");
  Require(events.Unsubscribe(subscription), "subscription must be removable");
  events.Publish(TestEvent{7});
  Require(event_value == 42, "removed subscription must not run");

  FixedTickClock clock{0.01, 2, 0.1};
  const auto tick = clock.Advance(0.05);
  Require(tick.fixed_ticks == 2 && clock.State().fixed_tick == 2, "catch-up must be bounded");

  TimerScheduler timers;
  int timer_runs = 0;
  const auto timer = timers.Schedule(0.01, [&timer_runs] { ++timer_runs; }, 0.01);
  timers.Advance(0.035);
  Require(timer_runs == 1, "repeating timer must coalesce missed intervals");
  Require(timers.Cancel(timer), "live timer must be cancellable");

  AsyncLogService log{8};
  log.Start();
  std::thread writer{[&log] { log.Write(LogLevel::Info, "test", "worker"); }};
  writer.join();
  log.Flush();
  Require(log.CrashRingSnapshot().size() == 1, "cross-thread log must reach crash ring");
  log.Stop();

  for (int iteration = 0; iteration < 3; ++iteration) {
    Engine engine;
    engine.Initialize({2, 16, 1024, "."});
    Require(engine.IsInitialized() && engine.Services().jobs->WorkerCount() == 2,
            "engine services must start");
    engine.BeginFrame();
    engine.Shutdown();
    Require(!engine.IsInitialized(), "engine must shut down cleanly");
  }
  return 0;
}

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
