#include "Nexora/Renderer/RenderGraph.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using renderer::RenderGraph;
using rhi::BufferAccess;
using rhi::QueueType;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <typename Function> void Reject(Function function, const char *message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception &) {
    rejected = true;
  }
  Require(rejected, message);
}
RenderGraph::PassDescriptor Pass(std::string name, QueueType queue = QueueType::Compute) {
  return {
      std::move(name), queue, {}, {}, [](rhi::CommandList &, std::span<const rhi::TextureHandle>) {
      }};
}
// Capture the portable plan independently of backend state tracking and diagnostic counters.
class PlanDevice final : public rhi::Device {
public:
  class Commands final : public rhi::CommandList {
  public:
    explicit Commands(PlanDevice &device) : device_(device) {}
    void Transition(const rhi::Barrier &) override { throw std::logic_error("unexpected texture"); }
    void TransitionBuffer(const rhi::BufferBarrier &barrier) override {
      device_.barriers.push_back(barrier);
    }
    void BeginRendering(const rhi::RenderingInfo &) override {}
    void BindPipeline(rhi::PipelineHandle) override {}
    void Draw(std::uint32_t, std::uint32_t) override {}
    void EndRendering() override {}

  private:
    PlanDevice &device_;
  };
  rhi::Backend GetBackend() const noexcept override { return rhi::Backend::Null; }
  rhi::TextureHandle CreateTexture(const rhi::TextureDescriptor &) override {
    throw std::logic_error("unexpected allocation");
  }
  void DestroyTexture(rhi::TextureHandle) override {}
  rhi::PipelineHandle CreatePipeline(const rhi::PipelineDescriptor &) override { return {}; }
  void DestroyPipeline(rhi::PipelineHandle) override {}
  std::unique_ptr<rhi::CommandList> CreateCommandList(QueueType queue) override {
    queues.push_back(queue);
    return std::make_unique<Commands>(*this);
  }
  std::uint64_t Submit(rhi::CommandList &) override { return ++completed; }
  std::uint64_t CompletedSubmissionValue() const noexcept override { return completed; }
  void WaitForSubmission(std::uint64_t) override {}
  void Present(rhi::TextureHandle) override {}
  void WaitIdle() override {}
  rhi::DeviceDiagnostics Diagnostics() const noexcept override { return {}; }
  std::vector<rhi::BufferBarrier> barriers;
  std::vector<QueueType> queues;
  std::uint64_t completed{};
};
void PlanTests() {
  RenderGraph graph;
  const rhi::BufferHandle handle{7, 3};
  const auto buffer = graph.ImportBuffer(handle, BufferAccess::IndirectRead, QueueType::Graphics);
  std::vector<std::size_t> callback_barrier_counts;
  PlanDevice device;
  for (int index = 0; index < 3; ++index) {
    auto pass = Pass("captured plan", index == 2 ? QueueType::Graphics : QueueType::Compute);
    if (index == 2)
      pass.buffer_reads = {{buffer, BufferAccess::IndirectRead}};
    else
      pass.buffer_writes = {{buffer, BufferAccess::StorageReadWrite}};
    pass.execute = [&](rhi::CommandList &, std::span<const rhi::TextureHandle>) {
      callback_barrier_counts.push_back(device.barriers.size());
    };
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  for (int run = 0; run < 2; ++run) {
    device.barriers.clear();
    device.queues.clear();
    callback_barrier_counts.clear();
    graph.Execute(device);
    Require(callback_barrier_counts == std::vector<std::size_t>({1, 2, 3}),
            "barriers were not recorded before their callbacks");
    Require(device.queues == std::vector<QueueType>(
                                 {QueueType::Compute, QueueType::Compute, QueueType::Graphics}),
            "compiled submission queues changed");
    const std::array before{BufferAccess::IndirectRead, BufferAccess::StorageReadWrite,
                            BufferAccess::StorageReadWrite};
    const std::array after{BufferAccess::StorageReadWrite, BufferAccess::StorageReadWrite,
                           BufferAccess::IndirectRead};
    const std::array source{QueueType::Graphics, QueueType::Compute, QueueType::Compute};
    for (std::size_t index = 0; index < 3; ++index) {
      const auto &barrier = device.barriers[index];
      Require(barrier.buffer == handle && barrier.before == before[index] &&
                  barrier.after == after[index] && barrier.source_queue == source[index] &&
                  barrier.destination_queue == device.queues[index],
              "immutable buffer plan lost a handle, access scope or queue");
    }
  }
}

void DependencyTests() {
  // Reverse explicit edges expose missing inferred hazards as a successful (incorrect) compile.
  for (const auto first_write : {false, true}) {
    for (const auto second_write : {false, true}) {
      for (const auto cross_queue : {false, true}) {
        RenderGraph graph;
        const auto buffer = graph.ImportBuffer({0, 1});
        auto first = Pass("first");
        auto second = Pass("second", cross_queue ? QueueType::Graphics : QueueType::Compute);
        (first_write ? first.buffer_writes : first.buffer_reads)
            .push_back(
                {buffer, first_write ? BufferAccess::StorageReadWrite : BufferAccess::ShaderRead});
        (second_write ? second.buffer_writes : second.buffer_reads)
            .push_back(
                {buffer, second_write ? BufferAccess::StorageReadWrite : BufferAccess::ShaderRead});
        const auto a = graph.AddPass(std::move(first));
        const auto b = graph.AddPass(std::move(second));
        graph.AddDependency(b, a);
        if (first_write || second_write || cross_queue) {
          Reject([&] { graph.Compile(); }, "buffer hazard did not induce an edge");
          auto device = rhi::CreateValidationDevice();
          Reject([&] { graph.Execute(*device); }, "failed compile remained executable");
        } else {
          graph.Compile();
          Require(graph.ExecutionOrder()[0] == b, "same-state readers were unnecessarily ordered");
        }
      }
    }
  }
  for (const auto texture : {false, true}) {
    RenderGraph graph;
    const auto buffer = graph.ImportBuffer({0, 1});
    const auto image = graph.CreateTransientTexture(
        {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::Undefined, "graph texture"});
    for (int index = 0; index < 3; ++index) {
      auto pass = Pass("reader", index == 2 ? QueueType::Graphics : QueueType::Compute);
      if (texture)
        pass.reads = {{image, rhi::ResourceState::ShaderRead}};
      else
        pass.buffer_reads = {{buffer, BufferAccess::ShaderRead}};
      (void)graph.AddPass(std::move(pass));
    }
    graph.AddDependency(2, 0);
    Reject([&] { graph.Compile(); }, "queue handoff failed to wait for all earlier readers");
  }
  for (const auto texture : {false, true}) {
    RenderGraph graph;
    const auto buffer = graph.ImportBuffer({0, 1});
    const auto image = graph.CreateTransientTexture(
        {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::Undefined, "graph texture"});
    auto a = Pass("state a");
    auto b = Pass("state b");
    if (texture) {
      a.reads = {{image, rhi::ResourceState::ShaderRead}};
      b.reads = {{image, rhi::ResourceState::CopySource}};
    } else {
      a.buffer_reads = {{buffer, BufferAccess::ShaderRead}};
      b.buffer_reads = {{buffer, BufferAccess::IndirectRead}};
    }
    (void)graph.AddPass(std::move(a));
    (void)graph.AddPass(std::move(b));
    graph.AddDependency(1, 0);
    Reject([&] { graph.Compile(); }, "read/read state change was not ordered");
  }
}
void ValidationTests() {
  auto device = rhi::CreateValidationDevice();
  const auto buffer = device->CreateBuffer({64, "graph input"});
  RenderGraph graph;
  const auto imported = graph.ImportBuffer(buffer);
  Reject([&] { (void)graph.ImportBuffer(buffer); }, "duplicate buffer alias accepted");
  Reject([&] { (void)graph.ImportBuffer({}); }, "invalid buffer handle accepted");
  Reject([&] { (void)graph.ImportBuffer({4, 1}, static_cast<BufferAccess>(255)); },
         "invalid access accepted");
  const auto image = device->CreateTexture(
      {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::Undefined, "graph texture"});
  (void)graph.ImportTexture(image, {1, 1, rhi::TextureFormat::Rgba8Unorm,
                                    rhi::ResourceState::Undefined, "graph texture"});
  Reject(
      [&] {
        (void)graph.ImportTexture(image, {1, 1, rhi::TextureFormat::Rgba8Unorm,
                                          rhi::ResourceState::Undefined, "graph texture"});
      },
      "duplicate texture alias accepted");
  for (const auto access :
       {BufferAccess::Undefined, BufferAccess::StorageReadWrite, static_cast<BufferAccess>(255)}) {
    auto pass = Pass("invalid read");
    pass.buffer_reads = {{imported, access}};
    Reject([&] { (void)graph.AddPass(pass); }, "invalid read declaration accepted");
  }
  auto invalid = Pass("unknown buffer");
  invalid.buffer_writes = {{{99}, BufferAccess::StorageReadWrite}};
  Reject([&] { (void)graph.AddPass(invalid); }, "unknown graph buffer accepted");
  invalid.buffer_writes = {{imported, BufferAccess::ShaderRead}};
  Reject([&] { (void)graph.AddPass(invalid); }, "read-only write declaration accepted");
  invalid.buffer_writes = {{imported, BufferAccess::StorageReadWrite}};
  invalid.buffer_reads = {{imported, BufferAccess::ShaderRead}};
  Reject([&] { (void)graph.AddPass(invalid); }, "duplicate read/write buffer accepted");
  invalid = Pass("invalid queue", static_cast<QueueType>(255));
  Reject([&] { (void)graph.AddPass(invalid); }, "invalid queue accepted");
  invalid = Pass("external mix");
  invalid.execute = {};
  invalid.external_execute = [](const renderer::ExternalPassContext &) {};
  invalid.buffer_reads = {{imported, BufferAccess::ShaderRead}};
  Reject([&] { (void)graph.AddPass(invalid); }, "external pass accepted RHI buffers");
  RenderGraph external;
  (void)external.ImportBuffer(buffer);
  external.Compile();
  Reject([&] { external.ExecuteExternal(); }, "external executor accepted RHI buffer ownership");
  device->DestroyTexture(image);
  device->DestroyBuffer(buffer);
}
void ExecutionTests() {
  auto device = rhi::CreateValidationDevice();
  const auto buffer = device->CreateBuffer({64, "graph output"});
  RenderGraph graph;
  const auto imported = graph.ImportBuffer(buffer);
  std::vector<int> order;
  for (int index = 0; index < 4; ++index) {
    auto pass = Pass("ordered buffer", index == 3 ? QueueType::Graphics : QueueType::Compute);
    if (index == 3)
      pass.buffer_reads = {{imported, BufferAccess::IndirectRead}};
    else
      pass.buffer_writes = {{imported, BufferAccess::StorageReadWrite}};
    pass.execute = [&, index](rhi::CommandList &, std::span<const rhi::TextureHandle>) {
      order.push_back(index);
    };
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  const auto compiled_order =
      std::vector<std::size_t>(graph.ExecutionOrder().begin(), graph.ExecutionOrder().end());
  for (int run = 0; run < 2; ++run) {
    order.clear();
    graph.Execute(*device);
    Require(order == std::vector<int>({0, 1, 2, 3}), "buffer dependency execution order differs");
    const auto stats = graph.GetStatistics();
    Require(stats.buffer_barrier_count == 4 && stats.buffer_queue_transfer_count == 2 &&
                stats.completed_pass_count == 4 && stats.barrier_count == 0,
            "same-access WAW or queue barriers missing");
    Require(std::vector<std::size_t>(graph.ExecutionOrder().begin(),
                                     graph.ExecutionOrder().end()) == compiled_order,
            "execution mutated compiled order");
  }
  Require(device->Diagnostics().barriers == 8, "compiled barriers were not replayed");
  auto commands = device->CreateCommandList(QueueType::Compute);
  Reject([&] { commands->TransitionBuffer({buffer}); }, "undefined destination accepted");
  commands->TransitionBuffer({buffer, BufferAccess::Undefined, BufferAccess::StorageReadWrite});
  device->Submit(*commands);
  Reject(
      [&] {
        commands->TransitionBuffer({buffer, BufferAccess::Undefined, BufferAccess::ShaderRead});
      },
      "submitted list accepted buffer barrier");
  const auto target = device->CreateTexture(
      {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::RenderTarget, "barrier scope"});
  auto rendering = device->CreateCommandList(QueueType::Graphics);
  rendering->BeginRendering({target, 1, 1});
  Reject(
      [&] {
        rendering->TransitionBuffer({buffer, BufferAccess::Undefined, BufferAccess::ShaderRead});
      },
      "rendering scope accepted a buffer barrier");
  rendering->EndRendering();
  device->Submit(*rendering);
  device->DestroyTexture(target);
  RenderGraph failed;
  const auto failed_buffer = failed.ImportBuffer(buffer);
  auto first = Pass("failing callback");
  first.buffer_writes = {{failed_buffer, BufferAccess::StorageReadWrite}};
  first.execute = [](rhi::CommandList &, std::span<const rhi::TextureHandle>) {
    throw std::runtime_error("expected failure");
  };
  (void)failed.AddPass(std::move(first));
  auto second = Pass("must not run");
  second.buffer_reads = {{failed_buffer, BufferAccess::ShaderRead}};
  bool successor = false;
  second.execute = [&](rhi::CommandList &, std::span<const rhi::TextureHandle>) {
    successor = true;
  };
  (void)failed.AddPass(std::move(second));
  failed.Compile();
  const auto submissions = device->Diagnostics().submitted_command_lists;
  Reject([&] { failed.Execute(*device); }, "callback failure swallowed");
  Require(!successor && failed.GetStatistics().completed_pass_count == 0 &&
              device->Diagnostics().submitted_command_lists == submissions,
          "failed callback submitted commands or executed successors");
  device->DestroyBuffer(buffer);
  bool callback = false;
  RenderGraph stale;
  const auto stale_buffer =
      stale.ImportBuffer(buffer, BufferAccess::ShaderRead, QueueType::Compute);
  auto pass = Pass("stale initial-state reader");
  pass.buffer_reads = {{stale_buffer, BufferAccess::ShaderRead}};
  pass.execute = [&](rhi::CommandList &, std::span<const rhi::TextureHandle>) { callback = true; };
  (void)stale.AddPass(std::move(pass));
  stale.Compile();
  Reject([&] { stale.Execute(*device); }, "stale same-state import accepted");
  Require(!callback, "stale resource reached callback");
  // A first transient use without a state change must still establish its queue owner.
  RenderGraph texture_graph;
  const auto texture = texture_graph.CreateTransientTexture(
      {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::ShaderRead, "read texture"});
  for (const auto queue : {QueueType::Compute, QueueType::Graphics}) {
    auto reader = Pass("texture reader", queue);
    reader.reads = {{texture, rhi::ResourceState::ShaderRead}};
    (void)texture_graph.AddPass(std::move(reader));
  }
  texture_graph.Compile();
  texture_graph.Execute(*device);
  Require(texture_graph.GetStatistics().queue_transfer_count == 1,
          "same-state first transient use lost queue ownership");
}
} // namespace
int main() {
  try {
    PlanTests();
    DependencyTests();
    ValidationTests();
    ExecutionTests();
    std::cout << "RenderGraph buffer dependency contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
