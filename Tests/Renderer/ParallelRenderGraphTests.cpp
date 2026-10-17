#include "Nexora/Renderer/RenderGraph.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using renderer::RenderGraph;
using rhi::BufferAccess;
using rhi::QueueType;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <typename Function> std::string Reject(Function function) {
  try {
    function();
  } catch (const std::exception &error) {
    return error.what();
  }
  throw std::runtime_error("expected rejection");
}
class ProbeDevice final : public rhi::Device {
public:
  class Commands final : public rhi::CommandList {
  public:
    Commands(ProbeDevice &owner, std::unique_ptr<rhi::CommandList> inner)
        : owner(owner), inner(std::move(inner)) {
      ++owner.live_lists;
    }
    ~Commands() override {
      if (owner.active.load() != 0 || owner.in_flight)
        owner.unsafe_retirement = true;
      --owner.live_lists;
    }
    void Transition(const rhi::Barrier &value) override { inner->Transition(value); }
    void TransitionBuffer(const rhi::BufferBarrier &value) override {
      inner->TransitionBuffer(value);
    }
    void BeginRendering(const rhi::RenderingInfo &value) override { inner->BeginRendering(value); }
    void BindPipeline(rhi::PipelineHandle value) override { inner->BindPipeline(value); }
    void Draw(std::uint32_t vertices, std::uint32_t instances) override {
      inner->Draw(vertices, instances);
    }
    void EndRendering() override { inner->EndRendering(); }
    ProbeDevice &owner;
    std::unique_ptr<rhi::CommandList> inner;
    std::size_t marker{};
  };
  rhi::Backend GetBackend() const noexcept override { return rhi::Backend::Null; }
  rhi::GraphicsDeviceCapabilities Capabilities() const noexcept override {
    auto result = inner->Capabilities();
    result.independent_compute_recording = admission;
    return result;
  }
  rhi::TextureHandle CreateTexture(const rhi::TextureDescriptor &value) override {
    return inner->CreateTexture(value);
  }
  void DestroyTexture(rhi::TextureHandle value) override {
    Require(active == 0, "transient destroyed while callbacks still run");
    inner->DestroyTexture(value);
  }
  rhi::BufferHandle CreateBuffer(const rhi::BufferDescriptor &value) override {
    return inner->CreateBuffer(value);
  }
  void DestroyBuffer(rhi::BufferHandle value) override { inner->DestroyBuffer(value); }
  rhi::PipelineHandle CreatePipeline(const rhi::PipelineDescriptor &value) override {
    return inner->CreatePipeline(value);
  }
  void DestroyPipeline(rhi::PipelineHandle value) override { inner->DestroyPipeline(value); }
  std::unique_ptr<rhi::CommandList> CreateCommandList(QueueType queue) override {
    Require(std::this_thread::get_id() == coordinator, "command allocator created on worker");
    return std::make_unique<Commands>(*this, inner->CreateCommandList(queue));
  }
  std::uint64_t Submit(rhi::CommandList &value) override {
    Require(std::this_thread::get_id() == coordinator && active == 0,
            "worker submitted or recording overlapped submission");
    auto &commands = dynamic_cast<Commands &>(value);
    if (submitted.size() == fail_submission_at) {
      in_flight = true; // Model an error after the native queue accepted work.
      throw std::runtime_error("injected submission failure");
    }
    Require(!in_flight, "next submission started before the previous completion fence");
    submitted.push_back(commands.marker);
    const auto completion = inner->Submit(*commands.inner);
    if (defer_completion) {
      in_flight = true;
      pending_completion = completion;
    }
    return completion;
  }
  std::uint64_t CompletedSubmissionValue() const noexcept override {
    return defer_completion ? reported_completion : inner->CompletedSubmissionValue();
  }
  void WaitForSubmission(std::uint64_t value) override {
    Require(std::this_thread::get_id() == coordinator, "worker waited for submission");
    inner->WaitForSubmission(value);
    if (defer_completion) {
      Require(value == pending_completion, "wrong completion value waited");
      reported_completion = value;
      in_flight = false;
      waited_values.push_back(value);
    }
  }
  void Present(rhi::TextureHandle value) override { inner->Present(value); }
  void WaitIdle() override {
    Require(active == 0, "WaitIdle ran before every callback joined");
    in_flight = false;
    ++idle_calls;
    inner->WaitIdle();
  }
  rhi::DeviceDiagnostics Diagnostics() const noexcept override { return inner->Diagnostics(); }
  std::unique_ptr<rhi::Device> inner = rhi::CreateValidationDevice();
  std::thread::id coordinator = std::this_thread::get_id();
  std::vector<std::size_t> submitted;
  std::atomic<unsigned> active{};
  std::size_t live_lists{};
  std::size_t idle_calls{};
  std::size_t fail_submission_at{static_cast<std::size_t>(-1)};
  std::vector<std::uint64_t> waited_values;
  std::uint64_t pending_completion{};
  std::uint64_t reported_completion{};
  bool defer_completion{};
  bool in_flight{};
  bool unsafe_retirement{};
  bool admission{true};
};
struct Active final {
  explicit Active(ProbeDevice &device) : device(device) { ++device.active; }
  ~Active() { --device.active; }
  ProbeDevice &device;
};
struct Rendezvous final {
  std::mutex mutex;
  std::condition_variable condition;
  unsigned arrived{};
  bool second_finished{};
  void Meet(bool first) {
    std::unique_lock lock(mutex);
    ++arrived;
    condition.notify_all();
    Require(condition.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 2; }),
            "independent callbacks did not overlap");
    if (first)
      Require(condition.wait_for(lock, std::chrono::seconds(5), [&] { return second_finished; }),
              "reverse completion did not occur");
    else {
      second_finished = true;
      condition.notify_all();
    }
  }
};
RenderGraph::PassDescriptor Pass(std::size_t marker, renderer::GraphBuffer buffer,
                                 bool write = true) {
  RenderGraph::PassDescriptor pass{
      "compute",
      QueueType::Compute,
      {},
      {},
      [marker](rhi::CommandList &commands, std::span<const rhi::TextureHandle>) {
        dynamic_cast<ProbeDevice::Commands &>(commands).marker = marker;
      }};
  (write ? pass.buffer_writes : pass.buffer_reads)
      .push_back({buffer, write ? BufferAccess::StorageReadWrite : BufferAccess::ShaderRead});
  pass.allow_parallel_recording = true;
  return pass;
}
RenderGraph::ExecutionOptions Parallel(core::JobSystem &jobs, std::size_t count = 2) {
  return {RenderGraph::RecordingMode::Parallel, &jobs, count};
}
void TestOverlapAndOrder() {
  ProbeDevice device;
  device.defer_completion = true;
  core::JobSystem jobs{4};
  jobs.Start();
  RenderGraph graph;
  std::array<Rendezvous, 3> rendezvous;
  std::vector<rhi::BufferHandle> buffers;
  for (std::size_t index = 0; index < 6; ++index) {
    buffers.push_back(device.CreateBuffer({64, "disjoint"}));
    auto pass = Pass(index, graph.ImportBuffer(buffers.back()));
    pass.execute = [&, index](rhi::CommandList &commands, std::span<const rhi::TextureHandle>) {
      Active active(device);
      Require(std::this_thread::get_id() != device.coordinator,
              "parallel callback stayed on coordinator");
      rendezvous[index / 2].Meet(index % 2 == 0);
      dynamic_cast<ProbeDevice::Commands &>(commands).marker = index;
    };
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  graph.Execute(device, Parallel(jobs));
  Require(device.submitted == std::vector<std::size_t>({0, 1, 2, 3, 4, 5}),
          "completion order changed submission order");
  const auto stats = graph.GetStatistics();
  Require(stats.parallel_recorded_pass_count == 6 && stats.parallel_batch_count == 3 &&
              stats.max_parallel_batch_size == 2 && stats.serial_recorded_pass_count == 0 &&
              stats.completed_pass_count == 6 && stats.buffer_barrier_count == 6,
          "parallel recording bounds or counters differ");
  Require(device.live_lists == 0 && !device.unsafe_retirement,
          "command lists retired before join/completion");
  Require(device.waited_values == std::vector<std::uint64_t>({1, 2, 3, 4, 5, 6}),
          "command-list lifetime did not follow completion values");
  jobs.Stop();
  for (auto buffer : buffers)
    device.DestroyBuffer(buffer);
}
void TestFallbacksAndHazards() {
  for (int scenario = 0; scenario < 12; ++scenario) {
    ProbeDevice device;
    core::JobSystem jobs{scenario == 2 ? 1U : 2U};
    if (scenario != 1)
      jobs.Start();
    device.admission = scenario != 0;
    RenderGraph graph;
    const auto first_buffer = device.CreateBuffer({64, "first"});
    const auto second_buffer = device.CreateBuffer({64, "second"});
    const auto a = graph.ImportBuffer(first_buffer);
    const auto b = scenario >= 7 && scenario <= 10 ? a : graph.ImportBuffer(second_buffer);
    auto first = Pass(0, a, scenario != 7 && scenario != 9);
    auto second = Pass(1, b, scenario != 7 && scenario != 8);
    if (scenario == 4)
      first.allow_parallel_recording = false;
    if (scenario == 5)
      first.queue = QueueType::Graphics;
    if (scenario == 6) {
      const auto texture =
          graph.CreateTransientTexture({1, 1, rhi::TextureFormat::Rgba8Unorm,
                                        rhi::ResourceState::Undefined, "fallback texture"});
      first.reads = {{texture, rhi::ResourceState::ShaderRead}};
    }
    for (auto *pass : {&first, &second}) {
      const auto original = pass->execute;
      pass->execute = [&, original](rhi::CommandList &commands,
                                    std::span<const rhi::TextureHandle> textures) {
        Require(std::this_thread::get_id() == device.coordinator,
                "unsafe pass escaped serial fallback");
        original(commands, textures);
      };
    }
    (void)graph.AddPass(std::move(first));
    (void)graph.AddPass(std::move(second));
    if (scenario == 11)
      graph.AddDependency(0, 1);
    graph.Compile();
    auto options = Parallel(jobs, scenario == 3 ? 1 : 2);
    graph.Execute(device, options);
    const auto stats = graph.GetStatistics();
    Require(stats.parallel_recorded_pass_count == 0 && stats.parallel_batch_count == 0 &&
                stats.serial_recorded_pass_count == 2 && stats.parallel_fallback_pass_count == 2,
            "unsupported/resource-overlap/dependency fallback was not serial");
    Require(device.submitted == std::vector<std::size_t>({0, 1}), "fallback changed graph order");
    jobs.Stop();
    device.DestroyBuffer(first_buffer);
    device.DestroyBuffer(second_buffer);
  }
}
void TestDependentBatches() {
  ProbeDevice device;
  core::JobSystem jobs{2};
  jobs.Start();
  RenderGraph graph;
  const auto a_handle = device.CreateBuffer({64, "a"});
  const auto b_handle = device.CreateBuffer({64, "b"});
  const auto a = graph.ImportBuffer(a_handle);
  const auto b = graph.ImportBuffer(b_handle);
  for (std::size_t index = 0; index < 4; ++index) {
    auto pass = Pass(index, index % 2 == 0 ? a : b, index < 2);
    const auto original = pass.execute;
    pass.execute = [&, index, original](rhi::CommandList &commands,
                                        std::span<const rhi::TextureHandle> textures) {
      Active active(device);
      if (index >= 2)
        Require(device.CompletedSubmissionValue() == 2,
                "dependent batch recorded before its predecessors submitted");
      original(commands, textures);
    };
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  graph.Execute(device, Parallel(jobs));
  Require(device.submitted == std::vector<std::size_t>({0, 1, 2, 3}) &&
              graph.GetStatistics().parallel_batch_count == 2,
          "dependent waves changed order or were not recorded in bounded batches");
  jobs.Stop();
  device.DestroyBuffer(a_handle);
  device.DestroyBuffer(b_handle);
}
void TestFailureContainment() {
  for (const bool submit_failure : {false, true}) {
    ProbeDevice device;
    core::JobSystem jobs{2};
    jobs.Start();
    RenderGraph graph;
    std::vector<rhi::BufferHandle> buffers;
    Rendezvous rendezvous;
    std::atomic<unsigned> finished{};
    bool successor = false;
    for (std::size_t index = 0; index < 3; ++index) {
      buffers.push_back(device.CreateBuffer({64, "failure fixture"}));
      auto pass = Pass(index, graph.ImportBuffer(buffers.back()));
      pass.allow_parallel_recording = index < 2;
      pass.execute = [&, index](rhi::CommandList &commands, std::span<const rhi::TextureHandle>) {
        Active active(device);
        if (index == 2) {
          successor = true;
          return;
        }
        rendezvous.Meet(index == 0);
        ++finished;
        dynamic_cast<ProbeDevice::Commands &>(commands).marker = index;
        if (!submit_failure)
          throw std::runtime_error(index == 0 ? "first callback failure"
                                              : "second callback failure");
      };
      (void)graph.AddPass(std::move(pass));
    }
    graph.Compile();
    if (submit_failure)
      device.fail_submission_at = 1;
    const auto error = Reject([&] { graph.Execute(device, Parallel(jobs)); });
    Require(error == (submit_failure ? "injected submission failure" : "first callback failure"),
            "failure selection was not deterministic");
    Require(finished == 2 && !successor && device.active == 0 && device.live_lists == 0 &&
                device.idle_calls > 0 && !device.unsafe_retirement,
            "failed batch escaped without joining/draining before cleanup");
    Require(device.submitted.size() == (submit_failure ? 1U : 0U) &&
                graph.GetStatistics().completed_pass_count == device.submitted.size(),
            "failed callback submitted work or submission failure ran successors");
    jobs.Stop();
    for (auto buffer : buffers)
      device.DestroyBuffer(buffer);
  }
}
void TestSerialFailureRetirement() {
  ProbeDevice device;
  RenderGraph graph;
  const auto buffer = device.CreateBuffer({64, "serial submission failure"});
  (void)graph.AddPass(Pass(0, graph.ImportBuffer(buffer)));
  graph.Compile();
  device.fail_submission_at = 0;
  Require(Reject([&] { graph.Execute(device); }) == "injected submission failure",
          "serial submission failure did not propagate");
  Require(device.idle_calls > 0 && device.live_lists == 0 && !device.unsafe_retirement,
          "serial submission failure freed its allocator before draining");
  device.DestroyBuffer(buffer);
}

void TestDefaultAndOptions() {
  ProbeDevice device;
  RenderGraph graph;
  std::vector<rhi::BufferHandle> buffers;
  for (std::size_t index = 0; index < 2; ++index) {
    buffers.push_back(device.CreateBuffer({64, "default serial"}));
    (void)graph.AddPass(Pass(index, graph.ImportBuffer(buffers.back())));
  }
  graph.Compile();
  graph.Execute(device);
  Require(graph.GetStatistics().serial_recorded_pass_count == 2 &&
              graph.GetStatistics().parallel_fallback_pass_count == 0,
          "legacy Execute no longer selects serial recording");
  RenderGraph::ExecutionOptions invalid;
  invalid.max_parallel_passes = 0;
  (void)Reject([&] { graph.Execute(device, invalid); });
  invalid.max_parallel_passes = 2;
  invalid.recording_mode = static_cast<RenderGraph::RecordingMode>(255);
  (void)Reject([&] { graph.Execute(device, invalid); });
  Require(device.submitted.size() == 2, "invalid options submitted work");
  auto options = RenderGraph::ExecutionOptions{RenderGraph::RecordingMode::Parallel, nullptr, 2};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_fallback_pass_count == 2,
          "missing pool did not fall back");
  for (auto buffer : buffers)
    device.DestroyBuffer(buffer);
}
} // namespace
int main() {
  try {
    TestOverlapAndOrder();
    TestFallbacksAndHazards();
    TestDependentBatches();
    TestFailureContainment();
    TestSerialFailureRetirement();
    TestDefaultAndOptions();
    std::cout << "Parallel graph recording, fallback, order and failure containment passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
