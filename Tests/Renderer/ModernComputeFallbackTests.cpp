#include "Nexora/RHI/Capabilities.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <array>
#include <barrier>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>

using namespace nexora;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F>
void Reject(F function, std::source_location location = std::source_location::current()) {
  try {
    function();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("invalid modern reference operation admitted at line " +
                           std::to_string(location.line()));
}
constexpr std::uint32_t Sentinel = 0xcdcdcdcdU;
using Words = std::array<std::uint32_t, 8>;
constexpr std::array Variants{rhi::ComputeShaderVariant::LongVectorReference,
                              rhi::ComputeShaderVariant::LinearAlgebraReference,
                              rhi::ComputeShaderVariant::GroupSharedReference};
rhi::PipelineDescriptor Descriptor(rhi::ComputeShaderVariant kind) {
  rhi::PipelineDescriptor result{31, 41, rhi::TextureFormat::Rgba8Unorm, "modern equivalent",
                                 rhi::PipelineType::Compute};
  result.compute_shader = kind;
  return result;
}
Words Empty() {
  Words result;
  result.fill(Sentinel);
  return result;
}
struct Work final {
  std::array<rhi::BufferHandle, 4> buffers;
  Words expected;
};
Work Make(rhi::Device &device, rhi::ComputeShaderVariant kind, std::uint32_t seed) {
  std::array<std::uint32_t, 32> input{};
  auto expected = Empty();
  if (kind == rhi::ComputeShaderVariant::LinearAlgebraReference) {
    for (unsigned i = 0; i < 16; ++i) {
      const auto value = seed ? static_cast<int>((i * 3 + seed % 19) % 19) - 9 : 0;
      input[i] = std::bit_cast<std::uint32_t>(static_cast<float>(value) * .25F);
    }
    for (unsigned i = 0; i < 4; ++i) {
      const auto value = static_cast<int>((i * 5 + seed % 11) % 11) - 5;
      input[16 + i] = std::bit_cast<std::uint32_t>(static_cast<float>(value) * .5F);
    }
    for (unsigned row = 0; row < 4; ++row) {
      double sum = 0;
      for (unsigned column = 0; column < 4; ++column)
        sum += static_cast<double>(std::bit_cast<float>(input[row * 4 + column])) *
               std::bit_cast<float>(input[16 + column]);
      expected[row] = std::bit_cast<std::uint32_t>(static_cast<float>(sum));
    }
  } else {
    std::uint64_t sum = 0, weighted = 0;
    for (unsigned i = 0; i < 8; ++i) {
      input[i] = seed + i * 3;
      sum += input[i];
      weighted += static_cast<std::uint64_t>(input[i]) * (i + 1);
    }
    // An independent closed form checks the reduction's loop extent and modulo arithmetic.
    expected[0] = static_cast<std::uint32_t>(kind == rhi::ComputeShaderVariant::LongVectorReference
                                                 ? weighted
                                                 : sum * 2048 + std::uint64_t{16383} * 16384 / 2);
  }
  Work result{{device.CreateBuffer({sizeof(input), "equivalent input"}),
               device.CreateBuffer({sizeof(Words), "equivalent output plus canaries"}),
               device.CreateBuffer({16, "unused root2"}),
               device.CreateBuffer({16, "unused root3"})},
              expected};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{input}));
  const auto empty = Empty();
  device.WriteBuffer(result.buffers[1], 0, std::as_bytes(std::span{empty}));
  return result;
}
Words Read(rhi::Device &device, const Work &work) {
  Words result{};
  device.ReadBufferForTesting(work.buffers[1], 0, std::as_writable_bytes(std::span{result}));
  return result;
}
void Destroy(rhi::Device &device, const Work &work) {
  for (auto buffer : work.buffers)
    device.DestroyBuffer(buffer);
}
void Bind(rhi::CommandList &commands, rhi::PipelineHandle pipeline, const Work &work) {
  commands.BindPipeline(pipeline);
  for (unsigned slot = 0; slot < 4; ++slot)
    commands.BindStorageBuffer(slot, work.buffers[slot]);
}
void Record(rhi::CommandList &commands, rhi::PipelineHandle pipeline, const Work &work) {
  Bind(commands, pipeline, work);
  commands.Dispatch(1);
}
void Check(rhi::Device &device, const Work &work) {
  Require(Read(device, work) == work.expected, "equivalent GPU result or output canary differs");
}
void Lifetime(rhi::Device &device, rhi::PipelineHandle pipeline) {
  auto work = Make(device, Variants[0], 7);
  const auto before = device.Diagnostics();
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*commands, pipeline, work);
  }
  Require(Read(device, work) == Empty(), "abandoned equivalent dispatched GPU work");
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*commands, pipeline, work);
    const std::uint32_t changed = 29;
    device.WriteBuffer(work.buffers[0], 0, std::as_bytes(std::span{&changed, 1}));
    Reject([&] { (void)device.Submit(*commands); });
  }
  Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
              device.Diagnostics().compute_dispatches == before.compute_dispatches,
          "stale or abandoned equivalent changed accepted counters");
  Require(Read(device, work) == Empty(), "stale equivalent changed output");
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    commands->BindPipeline(pipeline);
    commands->BindStorageBuffer(0, work.buffers[0]);
    Reject([&] { commands->Dispatch(1); });
  }
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*commands, pipeline, work);
    auto changed = Empty();
    changed[0] = 42;
    device.WriteBuffer(work.buffers[1], 0, std::as_bytes(std::span{changed}));
    Reject([&] { (void)device.Submit(*commands); });
    Require(Read(device, work) == changed, "stale equivalent overwrote CPU output mutation");
  }
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*commands, pipeline, work);
    const auto old = work.buffers[0];
    device.DestroyBuffer(old);
    work.buffers[0] = device.CreateBuffer({128, "recreated equivalent input"});
    Require(work.buffers[0] != old, "recreated input reused an old generation");
    Reject([&] { (void)device.Submit(*commands); });
  }
  Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
              device.Diagnostics().compute_dispatches == before.compute_dispatches,
          "stale output or recreated input changed accepted counters");
  for (unsigned failure = 0; failure < 3; ++failure) {
    auto invalid = work;
    rhi::BufferHandle small{};
    if (failure == 0)
      invalid.buffers[1] = invalid.buffers[0];
    else {
      small = device.CreateBuffer({failure == 1 ? 127U : 31U, "undersized equivalent"});
      invalid.buffers[failure == 1 ? 0 : 1] = small;
    }
    {
      auto commands = device.CreateCommandList(rhi::QueueType::Compute);
      Bind(*commands, pipeline, invalid);
      Reject([&] { commands->Dispatch(1); });
    }
    if (small.IsValid())
      device.DestroyBuffer(small);
  }
  Destroy(device, work);
}
void Parallel(rhi::Device &device, const std::array<rhi::PipelineHandle, 3> &pipelines) {
  std::array work{Make(device, Variants[0], 93), Make(device, Variants[1], 93),
                  Make(device, Variants[2], 93)};
  renderer::RenderGraph graph;
  std::barrier overlap{3};
  core::CancellationSource cancellation;
  bool fail = false, cancel = false;
  for (std::size_t worker = 0; worker < work.size(); ++worker) {
    std::array<renderer::GraphBuffer, 4> buffers;
    for (std::size_t slot = 0; slot < buffers.size(); ++slot)
      buffers[slot] = graph.ImportBuffer(work[worker].buffers[slot], rhi::BufferAccess::Undefined,
                                         rhi::QueueType::Compute);
    renderer::RenderGraph::PassDescriptor pass{
        "modern equivalent", rhi::QueueType::Compute, {}, {}, [&, worker](auto &commands, auto) {
          Record(commands, pipelines[worker], work[worker]);
          overlap.arrive_and_wait();
          if (worker == 0 && fail)
            throw std::runtime_error("intentional equivalent cohort failure");
          if (worker == 1 && cancel)
            cancellation.Cancel();
        }};
    pass.buffer_reads.push_back({buffers[0], rhi::BufferAccess::ShaderRead});
    for (auto slot : {1U, 2U, 3U})
      pass.buffer_writes.push_back({buffers[slot], rhi::BufferAccess::StorageReadWrite});
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  core::JobSystem jobs{3};
  jobs.Start();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 3};
  for (unsigned mode = 0; mode < 2; ++mode) {
    fail = mode == 0;
    cancel = mode == 1;
    options.cancellation = cancel ? cancellation.Token() : core::CancellationToken{};
    const auto before = device.Diagnostics();
    Reject([&] { graph.Execute(device, options); });
    Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
                device.Diagnostics().compute_dispatches == before.compute_dispatches,
            "aborted equivalent cohort submitted work");
    for (const auto &item : work)
      Require(Read(device, item) == Empty(), "aborted equivalent cohort changed output");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 3,
          "equivalent workers did not overlap");
  for (const auto &item : work)
    Check(device, item);
  jobs.Stop();
  for (const auto &item : work)
    Destroy(device, item);
}
void Mixed(rhi::Device &device, const std::array<rhi::PipelineHandle, 3> &pipelines) {
  std::array work{Make(device, Variants[0], 7), Make(device, Variants[1], 7),
                  Make(device, Variants[2], 7), Make(device, Variants[0], 1)};
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    for (std::size_t recipe = 0; recipe < pipelines.size(); ++recipe)
      Record(*commands, pipelines[recipe], work[recipe]);
    Bind(*commands, pipelines[0],
         work[3]); // Rebinding without dispatch cannot redirect prior work.
    device.WaitForSubmission(device.Submit(*commands));
  }
  for (std::size_t recipe = 0; recipe < pipelines.size(); ++recipe)
    Check(device, work[recipe]);
  Require(Read(device, work[3]) == Empty(),
          "final binding redirected an earlier equivalent dispatch");
  for (const auto &item : work)
    Destroy(device, item);
}
void Portable() {
  auto device = rhi::CreateValidationDevice();
  auto key = renderer::MakePipelineCacheKey(Descriptor(Variants[0]));
  for (const auto variant : Variants) {
    const auto descriptor = Descriptor(variant);
    Require(rhi::EffectivePipelineRequirements(descriptor).minimum_dxil_shader_model ==
                rhi::FeatureVersion{},
            "lower-profile equivalent inherited modern DXIL admission");
    rhi::RequirePipelineCapabilities({}, descriptor);
    Reject([&] { (void)device->CreatePipeline(descriptor); });
    if (variant != Variants[0])
      Require(key != renderer::MakePipelineCacheKey(descriptor),
              "equivalent cache identity aliases");
  }
  Require(device->Diagnostics().compute_dispatches == 0, "validation simulated a GPU equivalent");
}
void Native() {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
  constexpr auto artifact = "NEXORA_SLANG_LONG_VECTOR_REFERENCE_DXIL_PATH";
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
  constexpr auto artifact = "NEXORA_SLANG_LONG_VECTOR_REFERENCE_METAL_PATH";
#else
  constexpr auto backend = rhi::Backend::Vulkan;
  constexpr auto artifact = "NEXORA_SLANG_LONG_VECTOR_REFERENCE_SPIRV_PATH";
#endif
  const auto *path = std::getenv(artifact),
             *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
  if (!path || !*path || !rhi::IsBackendAvailable(backend)) {
    Require(!required || *required != '1', "required equivalent GPU backend/artifacts unavailable");
    std::cout << "Native modern equivalent unavailable\n";
    return;
  }
  auto device = rhi::CreateDevice(backend);
  std::array<rhi::PipelineHandle, 3> pipelines;
  for (std::size_t recipe = 0; recipe < Variants.size(); ++recipe) {
    pipelines[recipe] = device->CreatePipeline(Descriptor(Variants[recipe]));
    for (const auto seed : {0U, 1U, 7U, 93U, 0xfffffff0U}) {
      auto work = Make(*device, Variants[recipe], seed);
      {
        auto commands = device->CreateCommandList(rhi::QueueType::Compute);
        Bind(*commands, pipelines[recipe], work);
        Reject([&] { commands->Dispatch(2); });
        commands->Dispatch(1);
        device->WaitForSubmission(device->Submit(*commands));
      }
      Check(*device, work);
      Destroy(*device, work);
    }
  }
  Lifetime(*device, pipelines[0]);
  Parallel(*device, pipelines);
  Mixed(*device, pipelines);
  for (auto pipeline : pipelines)
    device->DestroyPipeline(pipeline);
  std::cout
      << "Modern equivalent backend=" << static_cast<unsigned>(backend)
      << "; all three recipes, overflow/mixed-sign/zero inputs, canaries, bounds, stale/abandon "
         "and three-worker failure/cancel/replay passed\n";
}
} // namespace
int main() {
  try {
    Portable();
    Native();
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
