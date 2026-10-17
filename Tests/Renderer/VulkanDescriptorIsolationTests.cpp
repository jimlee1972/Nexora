#include "Nexora/Renderer/GPUDrivenPipeline.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <algorithm>
#include <thread>

#include <array>
#include <barrier>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace nexora;
using namespace nexora::renderer;

namespace {
#if defined(NEXORA_TEST_D3D12)
constexpr auto kBackend = rhi::Backend::Direct3D12;
constexpr auto kComputeArtifact = "NEXORA_SLANG_COMPUTE_DXIL_PATH";
constexpr auto kBackendName = "D3D12";
#elif defined(NEXORA_TEST_METAL)
constexpr auto kBackend = rhi::Backend::Metal;
constexpr auto kComputeArtifact = "NEXORA_SLANG_COMPUTE_METAL_PATH";
constexpr auto kBackendName = "Metal";
#else
constexpr auto kBackend = rhi::Backend::Vulkan;
constexpr auto kComputeArtifact = "NEXORA_SLANG_COMPUTE_SPIRV_PATH";
constexpr auto kBackendName = "Vulkan";
#endif
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct Workload {
  std::array<rhi::BufferHandle, 4> buffers;
  GPUDrivenResult reference;
};

Workload MakeWorkload(rhi::Device &device, std::uint32_t identity) {
  GPUSceneReferenceObject object;
  object.object = {identity, 1};
  object.descriptor.world_bounds = {{0, 0, 0.25F}, 0.05F};
  object.descriptor.mesh_resource_index = identity + 7;
  object.descriptor.material_resource_index = identity + 11;
  object.descriptor.lod.lod_count = 1;
  GPUSceneReferenceSnapshot scene;
  scene.objects.push_back(object);
  GPUDrivenView view;
  view.maximum_distance = 10;
  // Canonical packed GPU-driven input: one visible object with a unique identity and bin.
  std::array<std::uint32_t, 56> packed{};
  packed[0] = 1;
  packed[4] = std::bit_cast<std::uint32_t>(view.maximum_distance);
  packed[6] = static_cast<std::uint32_t>(HiZPolicy::Invalidated);
  for (std::size_t index = 0; index < view.view_projection.values.size(); ++index)
    packed[20 + index] = std::bit_cast<std::uint32_t>(view.view_projection.values[index]);
  packed[46] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.center.z);
  packed[47] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.radius);
  packed[48] = identity;
  packed[49] = 1;
  packed[50] = object.descriptor.mesh_resource_index;
  packed[51] = object.descriptor.material_resource_index;
  packed[52] = object.descriptor.visibility_flags;
  packed[53] = 1;
  Workload result{{device.CreateBuffer({sizeof(packed), "isolated input"}),
                   device.CreateBuffer({5 * sizeof(std::uint32_t), "isolated visible"}),
                   device.CreateBuffer({rhi::GPUDrivenIndirectCommandStride, "isolated indirect"}),
                   device.CreateBuffer({8 * sizeof(std::uint32_t), "isolated counts"})},
                  BuildGPUDrivenCommands(scene, view)};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{packed}));
  Require(result.reference.instances.size() == 1 && result.reference.commands.size() == 1,
          "descriptor isolation fixture must produce a nonempty reference");
  return result;
}

void Bind(rhi::CommandList &commands, const Workload &workload) {
  for (std::uint32_t slot = 0; slot < workload.buffers.size(); ++slot)
    commands.BindStorageBuffer(slot, workload.buffers[slot]);
}

void Compare(rhi::Device &device, const Workload &workload) {
  std::array<std::uint32_t, 5> visible{};
  std::array<std::uint32_t, rhi::GPUDrivenIndirectCommandWords> indirect{};
  std::array<std::uint32_t, 8> counts{};
  device.ReadBufferForTesting(workload.buffers[1], 0, std::as_writable_bytes(std::span{visible}));
  device.ReadBufferForTesting(workload.buffers[2], 0, std::as_writable_bytes(std::span{indirect}));
  device.ReadBufferForTesting(workload.buffers[3], 0, std::as_writable_bytes(std::span{counts}));
  Require(counts[4] == 1 && counts[5] == 1,
          "each descriptor snapshot must execute its own workload");
  GPUDrivenResult output;
  output.statistics = {counts[0], counts[1], counts[2], counts[3]};
  output.instances.push_back({{visible[0], visible[1]}, visible[2], visible[3], visible[4]});
  output.commands.push_back({indirect[4], indirect[5], indirect[6], indirect[7], indirect[8]});
  Require(CompareGPUDrivenResults(workload.reference, output).matches,
          "isolated dispatch output must match its unique CPU reference");
  Require(indirect[0] == 3 && indirect[1] == 1 && indirect[2] == 0 && indirect[3] == 0,
          "isolated dispatch must preserve the native indirect draw prefix");
}

void TestIsolation(rhi::Device &device, rhi::PipelineHandle pipeline, bool parallel) {
  constexpr std::size_t workers = 4;
  // Cross the 32-set pool boundary and rebind all slots after every dispatch.
  constexpr std::size_t dispatches_per_worker = 40;
  std::vector<Workload> workloads;
  for (std::uint32_t index = 0; index < workers * dispatches_per_worker; ++index)
    workloads.push_back(MakeWorkload(device, index + 1));
  std::array<std::unique_ptr<rhi::CommandList>, workers> lists;
  for (auto &list : lists)
    list = device.CreateCommandList(rhi::QueueType::Compute);
  const auto record = [&](std::size_t worker) {
    auto &list = *lists[worker];
    list.BindPipeline(pipeline);
    for (std::size_t index = 0; index < dispatches_per_worker; ++index) {
      Bind(list, workloads[worker * dispatches_per_worker + index]);
      list.Dispatch(1);
    }
    // Binding without dispatch must not retroactively change the final recorded dispatch.
    Bind(list, workloads[(worker * dispatches_per_worker + 1) % workloads.size()]);
  };
  const auto before = device.Diagnostics();
  if (parallel) {
    std::barrier start{static_cast<std::ptrdiff_t>(workers)};
    std::array<std::future<void>, workers> jobs;
    std::size_t launched = 0;
    try {
      for (; launched < workers; ++launched) {
        const auto worker = launched;
        jobs[worker] = std::async(std::launch::async, [&, worker] {
          start.arrive_and_wait();
          record(worker);
        });
      }
    } catch (...) {
      // A failed thread launch must not leave already launched workers at the barrier.
      for (auto missing = launched; missing < workers; ++missing)
        start.arrive_and_drop();
      for (auto &job : jobs)
        if (job.valid())
          job.wait();
      throw;
    }
    // Drain every worker before a failure can release resources captured by other workers.
    std::exception_ptr failure;
    for (auto &job : jobs) {
      try {
        job.get();
      } catch (...) {
        failure = std::current_exception();
      }
    }
    if (failure)
      std::rethrow_exception(failure);
  } else {
    for (std::size_t worker = 0; worker < workers; ++worker)
      record(worker);
  }
  // No list is submitted until all lists have finished recording. Reverse submission order
  // makes the result independent of both recording order and another list's descriptor writes.
  Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists,
          "recording workers must not submit work");
  Require(device.Diagnostics().readbacks == before.readbacks, "recording must not read back");
  for (std::size_t index = workers; index-- > 0;) {
    const auto completion = device.Submit(*lists[index]);
    device.WaitForSubmission(completion);
    Require(device.CompletedSubmissionValue() >= completion,
            "submission must complete before retirement");
    lists[index].reset();
  }
  Require(device.Diagnostics().compute_dispatches - before.compute_dispatches == workloads.size(),
          "every isolated dispatch must execute exactly once");
  for (const auto &workload : workloads)
    Compare(device, workload);
  for (const auto &workload : workloads)
    for (const auto buffer : workload.buffers)
      device.DestroyBuffer(buffer);
}

void TestGraphRecording(rhi::Device &device, rhi::PipelineHandle pipeline) {
  Require(device.Capabilities().independent_compute_recording,
          "native backend must admit the limited independent compute path");
  const auto full =
      device.Capabilities()
          .features[static_cast<std::size_t>(rhi::GraphicsFeature::ParallelCommandRecording)];
  Require(!full.enabled && !full.implemented,
          "limited compute admission enabled the full parallel gate");
  constexpr std::size_t workload_count = 24;
  std::vector<Workload> workloads;
  for (std::uint32_t index = 0; index < workload_count; ++index)
    workloads.push_back(MakeWorkload(device, index + 2000));
  RenderGraph graph;
  std::vector<std::array<GraphBuffer, 4>> buffers;
  for (const auto &workload : workloads) {
    std::array<GraphBuffer, 4> imported;
    for (std::size_t slot = 0; slot < imported.size(); ++slot)
      imported[slot] = graph.ImportBuffer(workload.buffers[slot]);
    buffers.push_back(imported);
  }
  const auto coordinator = std::this_thread::get_id();
  std::array<std::thread::id, workload_count * 2> threads;
  bool inject_failure = false;
  bool inject_cancellation = false;
  core::CancellationSource cancellation;
  bool parallel_callbacks = false;
  struct Meeting final {
    std::mutex mutex;
    std::condition_variable condition;
    unsigned arrived{};
  };
  std::array<Meeting, workload_count / 2> meetings;
  for (std::size_t index = 0; index < workload_count * 2; ++index) {
    const auto workload = index % workload_count;
    RenderGraph::PassDescriptor pass{
        "graph compute",
        rhi::QueueType::Compute,
        {},
        {},
        [&, index, workload](rhi::CommandList &commands, std::span<const rhi::TextureHandle>) {
          threads[index] = std::this_thread::get_id();
          if (parallel_callbacks) {
            auto &meeting = meetings[index / 4];
            std::unique_lock lock(meeting.mutex);
            ++meeting.arrived;
            meeting.condition.notify_all();
            Require(meeting.condition.wait_for(lock, std::chrono::seconds(5),
                                               [&] { return meeting.arrived == 4; }),
                    "native graph workers did not overlap");
          }
          Bind(commands, workloads[workload]);
          commands.BindPipeline(pipeline);
          commands.Dispatch(1);
          if (inject_cancellation && index == 0)
            cancellation.Cancel();
          if (inject_failure && index == 0)
            throw std::runtime_error("native graph injected failure");
        }};
    pass.buffer_reads = {{buffers[workload][0], rhi::BufferAccess::ShaderRead}};
    for (std::size_t slot = 1; slot < 4; ++slot)
      pass.buffer_writes.push_back({buffers[workload][slot], rhi::BufferAccess::StorageReadWrite});
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  core::JobSystem jobs{4};
  jobs.Start();
  const RenderGraph::ExecutionOptions parallel{RenderGraph::RecordingMode::Parallel, &jobs, 4};
  const auto run = [&](bool use_parallel) {
    parallel_callbacks = use_parallel;
    for (auto &meeting : meetings)
      meeting.arrived = 0;
    const auto before = device.Diagnostics();
    if (use_parallel)
      graph.Execute(device, parallel);
    else
      graph.Execute(device);
    const auto after = device.Diagnostics();
    Require(after.compute_dispatches - before.compute_dispatches == workload_count * 2 &&
                after.submitted_command_lists - before.submitted_command_lists ==
                    workload_count * 2 &&
                after.readbacks == before.readbacks,
            "graph dispatch/submission counts differ or recording read back output");
    const auto stats = graph.GetStatistics();
    Require(stats.completed_pass_count == workload_count * 2 &&
                stats.buffer_barrier_count == workload_count * 7,
            "native graph lost WAW barriers or passes");
    if (use_parallel) {
      Require(stats.parallel_recorded_pass_count == workload_count * 2 &&
                  stats.parallel_batch_count == workload_count / 2 &&
                  stats.max_parallel_batch_size == 4,
              "native graph did not use bounded four-worker recording");
      Require(std::ranges::all_of(threads, [&](auto thread) { return thread != coordinator; }),
              "native parallel callbacks ran on coordinator");
    } else {
      Require(stats.parallel_recorded_pass_count == 0 &&
                  stats.serial_recorded_pass_count == workload_count * 2,
              "explicit serial path did not remain available");
      Require(std::ranges::all_of(threads, [&](auto thread) { return thread == coordinator; }),
              "serial callback ran on a worker");
    }
    for (const auto &workload : workloads)
      Compare(device, workload);
  };
  run(false);
  // Prefill every output so accidental submission of any failed-batch dispatch is observable.
  constexpr std::uint32_t sentinel = 0xDEADBEEF;
  const std::array<std::size_t, 3> words{5, rhi::GPUDrivenIndirectCommandWords, 8};
  for (const auto &workload : workloads)
    for (std::size_t slot = 1; slot < 4; ++slot) {
      const std::vector<std::uint32_t> value(words[slot - 1], sentinel);
      device.WriteBuffer(workload.buffers[slot], 0, std::as_bytes(std::span{value}));
    }
  inject_failure = true;
  parallel_callbacks = true;
  for (auto &meeting : meetings)
    meeting.arrived = 0;
  const auto before_failure = device.Diagnostics();
  bool rejected = false;
  try {
    graph.Execute(device, parallel);
  } catch (const std::runtime_error &error) {
    rejected = std::string_view(error.what()) == "native graph injected failure";
  }
  Require(rejected && graph.GetStatistics().completed_pass_count == 0 &&
              device.Diagnostics().submitted_command_lists ==
                  before_failure.submitted_command_lists &&
              device.Diagnostics().readbacks == before_failure.readbacks,
          "failed native recording submitted work or swallowed its error");
  const auto verify_sentinels = [&] {
    for (const auto &workload : workloads)
      for (std::size_t slot = 1; slot < 4; ++slot) {
        std::vector<std::uint32_t> value(words[slot - 1]);
        device.ReadBufferForTesting(workload.buffers[slot], 0,
                                    std::as_writable_bytes(std::span{value}));
        Require(std::ranges::all_of(value, [](auto word) { return word == sentinel; }),
                "failed native graph changed GPU output");
      }
  };
  verify_sentinels();
  inject_failure = false;
  inject_cancellation = true;
  for (auto &meeting : meetings)
    meeting.arrived = 0;
  auto cancel_options = parallel;
  cancel_options.cancellation = cancellation.Token();
  const auto before_cancel = device.Diagnostics();
  bool cancelled = false;
  try {
    graph.Execute(device, cancel_options);
  } catch (const RenderGraphCancelled &) {
    cancelled = true;
  }
  Require(cancelled && graph.GetStatistics().completed_pass_count == 0 &&
              device.Diagnostics().submitted_command_lists ==
                  before_cancel.submitted_command_lists &&
              device.Diagnostics().readbacks == before_cancel.readbacks,
          "cancelled native recording submitted or failed to publish cancellation");
  verify_sentinels();
  inject_cancellation = false;
  run(true); // Reuses buffers after failed/cancelled recording jobs and their pools retire.
  jobs.Stop();
  for (const auto &workload : workloads)
    for (const auto buffer : workload.buffers)
      device.DestroyBuffer(buffer);
  std::cout << kBackendName
            << " graph: 24 unique workloads, serial/parallel WAW waves, 48 CPU comparisons; "
               "failed/cancelled batches preserve all sentinel output and retry succeeds\n";
}

void TestBufferSnapshots(rhi::Device &device, rhi::PipelineHandle pipeline) {
  if (kBackend != rhi::Backend::Direct3D12)
    return;
  auto workload = MakeWorkload(device, 3000);
  const auto record = [&] {
    auto list = device.CreateCommandList(rhi::QueueType::Compute);
    Bind(*list, workload);
    list->BindPipeline(pipeline);
    list->Dispatch(1);
    return list;
  };
  const auto run = [&] {
    auto list = record();
    device.WaitForSubmission(device.Submit(*list));
    Compare(device, workload);
  };
  const auto reject = [&](rhi::CommandList &list) {
    const auto before = device.Diagnostics();
    bool rejected = false;
    try {
      (void)device.Submit(list);
    } catch (const std::logic_error &) {
      rejected = true;
    }
    Require(rejected &&
                device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
                device.Diagnostics().compute_dispatches == before.compute_dispatches,
            "stale D3D12 snapshot must fail before native submission");
  };
  // Abandon the very first upload and state changes; retry must still upload canonical input.
  { auto abandoned = record(); }
  run();
  // Abandon a different final state after GPU output exists. Never consume GPU output as upload.
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Compute);
    abandoned->TransitionBuffer(
        {workload.buffers[1], rhi::BufferAccess::Undefined, rhi::BufferAccess::IndirectRead});
  }
  run();
  {
    auto stale = record();
    const std::array<std::uint32_t, 5> sentinel{1, 2, 3, 4, 5};
    device.WriteBuffer(workload.buffers[1], 0, std::as_bytes(std::span{sentinel}));
    reject(*stale);
  }
  run();
  {
    auto first = record();
    auto stale = record();
    device.WaitForSubmission(device.Submit(*first));
    reject(*stale);
  }
  run();
  {
    auto stale = record();
    Compare(device, workload); // Readback is also an execution boundary invalidating snapshots.
    reject(*stale);
  }
  run();
  {
    auto stale = record();
    device.DestroyBuffer(workload.buffers[3]);
    reject(*stale);
  }
  for (std::size_t index = 0; index < 3; ++index)
    device.DestroyBuffer(workload.buffers[index]);
  std::cout << "D3D12: abandoned uploads/states retry; upload/submission/readback/stale-handle "
               "snapshots reject before execution; 6 exact CPU comparisons passed\n";
}

void TestMetalBufferScopes(rhi::Device &device, rhi::PipelineHandle pipeline) {
  if (kBackend != rhi::Backend::Metal)
    return;
  auto workload = MakeWorkload(device, 4000);
  {
    auto list = device.CreateCommandList(rhi::QueueType::Compute);
    Bind(*list, workload); // Bindings may precede creation of the native compute encoder.
    list->BindPipeline(pipeline);
    list->Dispatch(1);
    for (std::size_t slot = 1; slot < workload.buffers.size(); ++slot)
      list->TransitionBuffer({workload.buffers[slot], rhi::BufferAccess::StorageReadWrite,
                              rhi::BufferAccess::StorageReadWrite, rhi::QueueType::Compute,
                              rhi::QueueType::Compute});
    list->BindPipeline(pipeline); // Recreate encoder and replay the list-local bindings.
    list->Dispatch(1);
    device.WaitForSubmission(device.Submit(*list));
  }
  Compare(device, workload);
  const auto before = device.Diagnostics();
  std::array<std::uint32_t, 2> words{};
  device.ReadBufferForTesting(workload.buffers[0], 48 * sizeof(std::uint32_t),
                              std::as_writable_bytes(std::span{words}));
  Require(words[0] == 4000 && words[1] == 1, "Metal offset readback differs from uploaded input");
  device.ReadBufferForTesting(workload.buffers[0], 56 * sizeof(std::uint32_t), {});
  bool rejected = false;
  try {
    device.ReadBufferForTesting(workload.buffers[0], 56 * sizeof(std::uint32_t) - 1,
                                std::as_writable_bytes(std::span{words}));
  } catch (const std::logic_error &) {
    rejected = true;
  }
  Require(rejected && device.Diagnostics().readbacks == before.readbacks + 1,
          "Metal empty/invalid readback changed counters or accepted an invalid range");
  for (const auto buffer : workload.buffers)
    device.DestroyBuffer(buffer);
  rejected = false;
  try {
    device.ReadBufferForTesting(workload.buffers[0], 0, std::as_writable_bytes(std::span{words}));
  } catch (const std::logic_error &) {
    rejected = true;
  }
  Require(rejected, "Metal readback accepted a destroyed buffer");
  std::cout << "Metal encoder WAW boundary replays bindings and matches CPU; "
               "offset/empty/invalid/stale readback checks passed\n";
}

void TestSharedInputGraph(rhi::Device &device, rhi::PipelineHandle pipeline) {
  Require(device.Capabilities().shared_buffer_read_recording,
          "native backend must admit immutable shared buffer reads");
  constexpr std::size_t count = 16;
  std::vector<Workload> workloads;
  for (std::size_t index = 0; index < count; ++index) {
    auto workload = MakeWorkload(device, 5000);
    if (!workloads.empty()) {
      device.DestroyBuffer(workload.buffers[0]);
      workload.buffers[0] = workloads.front().buffers[0];
    }
    workloads.push_back(std::move(workload));
  }
  RenderGraph graph;
  const auto input = graph.ImportBuffer(workloads.front().buffers[0]);
  bool parallel = true;
  bool fail = false;
  bool cancel = false;
  core::CancellationSource cancellation;
  std::mutex mutex;
  std::condition_variable condition;
  std::size_t arrived{};
  const auto coordinator = std::this_thread::get_id();
  for (std::size_t index = 0; index < count; ++index) {
    RenderGraph::PassDescriptor pass{
        "immutable shared input",
        rhi::QueueType::Compute,
        {},
        {},
        [&, index](rhi::CommandList &commands, auto) {
          if (parallel) {
            Require(std::this_thread::get_id() != coordinator,
                    "shared input callback stayed on coordinator");
            std::unique_lock lock{mutex};
            ++arrived;
            condition.notify_all();
            Require(condition.wait_for(lock, std::chrono::seconds(5),
                                       [&] { return arrived >= (index / 4 + 1) * 4; }),
                    "shared-input recording workers did not overlap");
          }
          commands.BindPipeline(pipeline);
          Bind(commands, workloads[index]);
          commands.Dispatch(1);
          if (cancel && index == 1)
            cancellation.Cancel();
          if (fail && index == 1)
            throw std::runtime_error("injected shared-input failure");
        }};
    pass.buffer_reads = {{input, rhi::BufferAccess::ShaderRead}};
    for (std::size_t slot = 1; slot < 4; ++slot)
      pass.buffer_writes.push_back({graph.ImportBuffer(workloads[index].buffers[slot]),
                                    rhi::BufferAccess::StorageReadWrite});
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  core::JobSystem jobs{4};
  jobs.Start();
  RenderGraph::ExecutionOptions options{RenderGraph::RecordingMode::Parallel, &jobs, 4};
  const std::array<std::size_t, 3> words{5, rhi::GPUDrivenIndirectCommandWords, 8};
  const auto fill = [&](std::uint32_t value) {
    for (const auto &workload : workloads)
      for (std::size_t slot = 1; slot < 4; ++slot) {
        const std::vector<std::uint32_t> data(words[slot - 1], value);
        device.WriteBuffer(workload.buffers[slot], 0, std::as_bytes(std::span{data}));
      }
  };
  const auto compare = [&] {
    for (const auto &workload : workloads)
      Compare(device, workload);
  };
  // Parallel first also exercises D3D12's initial pending upload in every shared reader.
  for (const bool mode : {true, false}) {
    parallel = mode;
    arrived = 0;
    fill(0);
    options.recording_mode =
        mode ? RenderGraph::RecordingMode::Parallel : RenderGraph::RecordingMode::Serial;
    const auto before = device.Diagnostics();
    graph.Execute(device, options);
    Require(device.Diagnostics().readbacks == before.readbacks,
            "shared-input graph performed a production readback");
    Require(graph.GetStatistics().parallel_recorded_pass_count == (mode ? count : 0) &&
                graph.GetStatistics().parallel_batch_count == (mode ? count / 4 : 0),
            "immutable shared readers were not admitted in bounded cohorts");
    compare();
  }
  parallel = true;
  options.recording_mode = RenderGraph::RecordingMode::Parallel;
  constexpr std::uint32_t sentinel = 0xdeadbeef;
  for (const bool cancellation_case : {false, true}) {
    arrived = 0;
    fill(sentinel);
    fail = !cancellation_case;
    cancel = cancellation_case;
    options.cancellation = cancel ? cancellation.Token() : core::CancellationToken{};
    const auto before = device.Diagnostics();
    bool rejected = false;
    try {
      graph.Execute(device, options);
    } catch (const renderer::RenderGraphCancelled &) {
      rejected = cancellation_case;
    } catch (const std::runtime_error &error) {
      rejected =
          !cancellation_case && std::string_view{error.what()} == "injected shared-input failure";
    }
    Require(rejected &&
                device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
                device.Diagnostics().readbacks == before.readbacks,
            "failed/cancelled shared-input cohort submitted or read back");
    for (const auto &workload : workloads)
      for (std::size_t slot = 1; slot < 4; ++slot) {
        std::vector<std::uint32_t> data(words[slot - 1]);
        device.ReadBufferForTesting(workload.buffers[slot], 0,
                                    std::as_writable_bytes(std::span{data}));
        Require(std::ranges::all_of(data, [](auto value) { return value == sentinel; }),
                "aborted shared-input cohort changed a sentinel");
      }
    arrived = 0;
    fail = false;
    cancel = false;
    options.cancellation = {};
    fill(0);
    graph.Execute(device, options);
    compare();
  }
  jobs.Stop();
  device.DestroyBuffer(workloads.front().buffers[0]);
  for (const auto &workload : workloads)
    for (std::size_t slot = 1; slot < 4; ++slot)
      device.DestroyBuffer(workload.buffers[slot]);
  std::cout << kBackendName
            << " shared input: 64 CPU comparisons, initial-upload parallel readers, "
               "failed/cancelled sentinels and successful replay passed\n";
}
#if defined(NEXORA_TEST_D3D12)
void TestReadOnlyBufferSnapshots(rhi::Device &device, rhi::PipelineHandle pipeline) {
  const auto input = device.CreateBuffer({16, "shared read snapshot"});
  std::array<std::uint32_t, 4> values{11, 22, 33, 44};
  device.WriteBuffer(input, 0, std::as_bytes(std::span{values}));
  const auto record = [&] {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    commands->BindStorageBuffer(0, input);
    return commands;
  };
  auto first = record();
  auto second = record();
  device.WaitForSubmission(device.Submit(*second));
  device.WaitForSubmission(device.Submit(*first));
  first.reset();
  second.reset();
  std::array<std::uint32_t, 4> actual{};
  device.ReadBufferForTesting(input, 0, std::as_writable_bytes(std::span{actual}));
  Require(actual == values, "compatible initial-upload readers changed input data");
  auto stale_upload = record();
  values[0] = 99;
  device.WriteBuffer(input, 0, std::as_bytes(std::span{values}));
  const auto rejected_before = device.Diagnostics().submitted_command_lists;
  bool rejected = false;
  try {
    (void)device.Submit(*stale_upload);
  } catch (const std::logic_error &) {
    rejected = true;
  }
  Require(rejected && device.Diagnostics().submitted_command_lists == rejected_before,
          "upload failed to invalidate a read-only snapshot");
  stale_upload.reset();
  auto retry = record();
  device.ReadBufferForTesting(input, 0, {}); // Empty validation does not invalidate snapshots.
  device.WaitForSubmission(device.Submit(*retry));
  retry.reset();
  auto stale_readback = record();
  device.ReadBufferForTesting(input, 0, std::as_writable_bytes(std::span{actual}));
  Require(actual == values, "reader did not recover after an upload invalidation");
  rejected = false;
  try {
    (void)device.Submit(*stale_readback);
  } catch (const std::logic_error &) {
    rejected = true;
  }
  Require(rejected, "readback failed to invalidate a read-only snapshot");
  stale_readback.reset();
  device.DestroyBuffer(input);
  auto writer = MakeWorkload(device, 5001);
  auto stale_writer = device.CreateCommandList(rhi::QueueType::Compute);
  stale_writer->BindStorageBuffer(0, writer.buffers[1]);
  auto commands = device.CreateCommandList(rhi::QueueType::Compute);
  commands->BindPipeline(pipeline);
  Bind(*commands, writer);
  commands->Dispatch(1);
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  rejected = false;
  try {
    (void)device.Submit(*stale_writer);
  } catch (const std::logic_error &) {
    rejected = true;
  }
  Require(rejected, "accepted UAV write failed to invalidate an older read-only snapshot");
  stale_writer.reset();
  Compare(device, writer);
  for (const auto buffer : writer.buffers)
    device.DestroyBuffer(buffer);
  std::cout << "D3D12 read-only snapshots: reverse compatible initial uploads, stale upload/"
               "readback/GPU-write rejection, empty-read compatibility and recovery passed\n";
}
#endif
void TestRejectedBindings(rhi::Device &device, rhi::PipelineHandle pipeline) {
  const auto rejects = [](auto &&call) {
    try {
      call();
    } catch (const std::logic_error &) {
      return true;
    }
    return false;
  };
  auto list = device.CreateCommandList(rhi::QueueType::Compute);
  list->BindPipeline(pipeline);
  Require(rejects([&] { list->Dispatch(1); }), "dispatch must reject missing storage bindings");
  auto workload = MakeWorkload(device, 999);
  Bind(*list, workload);
  device.DestroyBuffer(workload.buffers[0]);
  Require(rejects([&] { list->Dispatch(1); }), "dispatch must revalidate a stale bound buffer");
  list.reset();
  for (std::size_t slot = 1; slot < workload.buffers.size(); ++slot)
    device.DestroyBuffer(workload.buffers[slot]);
  Require(device.Diagnostics().compute_dispatches == 0, "rejected dispatches must not execute");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--missing-compute-artifact") {
      Require(kBackend == rhi::Backend::Metal, "missing-artifact case requires Metal");
      bool rejected = false;
      try {
        (void)rhi::CreateDevice(kBackend);
      } catch (const std::runtime_error &error) {
        rejected = std::string_view(error.what()) == "Metal compute artifact is missing or empty";
      }
      Require(rejected, "missing requested Metal artifact silently used the compute smoke");
      std::cout << "Missing requested Metal compute artifact rejects without fallback\n";
      return 0;
    }
    const auto *graphics_artifact = std::getenv("NEXORA_SLANG_SPIRV_PATH");
    const auto *compute_artifact = std::getenv(kComputeArtifact);
    if ((kBackend == rhi::Backend::Vulkan && (!graphics_artifact || !*graphics_artifact)) ||
        !compute_artifact || !*compute_artifact || !rhi::IsBackendAvailable(kBackend)) {
      const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
      Require(!(required && *required == '1'), "required native backend/artifact unavailable");
      std::cout << "SKIP: " << kBackendName << " artifacts/device unavailable\n";
      return 77;
    }
    auto device = rhi::CreateDevice(kBackend);
    const auto pipeline = device->CreatePipeline(
        {1, 2, rhi::TextureFormat::Rgba8Unorm, "isolation", rhi::PipelineType::Compute});
    TestRejectedBindings(*device, pipeline);
    TestIsolation(*device, pipeline, false);
    TestIsolation(*device, pipeline, true);
    TestGraphRecording(*device, pipeline);
    TestSharedInputGraph(*device, pipeline);
#if defined(NEXORA_TEST_D3D12)
    TestReadOnlyBufferSnapshots(*device, pipeline);
#endif
    TestBufferSnapshots(*device, pipeline);
    TestMetalBufferScopes(*device, pipeline);
    device->DestroyPipeline(pipeline);
    std::cout << kBackendName
              << " descriptor/buffer isolation: serial and 4-worker recording, 320 exact dispatch "
                 "comparisons passed\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
