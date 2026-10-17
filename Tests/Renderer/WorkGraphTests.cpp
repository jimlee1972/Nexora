#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/DxilArtifact.h"
#include "Nexora/Renderer/GPUDrivenPipeline.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <array>
#include <barrier>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace nexora;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class Function> void Reject(Function function, const char *message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception &) {
    rejected = true;
  }
  Require(rejected, message);
}
template <class Function> void RejectContains(Function function, const char *message) {
  try {
    function();
  } catch (const std::exception &error) {
    Require(std::string{error.what()}.find(message) != std::string::npos,
            "native graph rejection did not exercise the expected admission boundary");
    return;
  }
  throw std::runtime_error("invalid native graph artifact was admitted");
}
constexpr std::uint32_t Sentinel = 0xcdcdcdcdU;
using Words =
    std::array<std::uint32_t, rhi::WorkGraphMaximumRecords * rhi::WorkGraphVisibleRecordWords>;
Words Empty() {
  Words words;
  words.fill(Sentinel);
  return words;
}
void TestPortable() {
  using namespace rhi;
  Require(IsWorkGraphInputValid({0, UINT32_MAX}) && IsWorkGraphInputValid({8, UINT32_MAX - 8}) &&
              !IsWorkGraphInputValid({9, 1}) && !IsWorkGraphInputValid({1, UINT32_MAX}) &&
              !IsWorkGraphInputValid({8, UINT32_MAX - 7}),
          "bounded node input overflow admission");
  WorkGraphDescriptor descriptor{};
  const auto requirements = EffectiveWorkGraphRequirements(descriptor);
  const auto index = static_cast<std::size_t>(GraphicsFeature::WorkGraphs);
  Require(requirements.minimum_tiers[index] == FeatureVersion{1, 0} &&
              requirements.minimum_dxil_shader_model == FeatureVersion{6, 8},
          "Work Graph recipe lost implicit capability/profile requirements");
  GraphicsDeviceCapabilities capabilities;
  capabilities.features[index] = {CapabilitySupport::Supported, {2, 0}, true, true, {1, 0}};
  capabilities.dxil_shader_model_support = CapabilitySupport::Supported;
  capabilities.highest_dxil_shader_model = {6, 8};
  RequireGraphicsCapabilities(capabilities, requirements);
  descriptor.graphics_requirements.minimum_tiers[index] = {2, 0};
  Require(
      CheckGraphicsRequirements(capabilities, EffectiveWorkGraphRequirements(descriptor)).failure ==
          CapabilityFailure::InsufficientImplementedTier,
      "unimplemented newer graph tier admitted");
  descriptor.graphics_requirements = {};
  descriptor.graphics_requirements.minimum_dxil_shader_model = {6, 9};
  Require(SelectGraphicsPath(capabilities, EffectiveWorkGraphRequirements(descriptor), {}).path ==
              GraphicsPath::Fallback,
          "newer graph profile must preserve explicit fallback");
  auto model = CreateValidationDevice();
  Reject([&] { (void)model->CreateWorkGraph({}); }, "Validation fabricated native graph creation");
  auto commands = model->CreateCommandList(QueueType::Compute);
  Reject([&] { commands->BindWorkGraph({1, 1}); }, "Validation fabricated native graph binding");
  Reject([&] { commands->DispatchWorkGraph({1, 7}); }, "Validation fabricated graph dispatch");
  Require(model->Diagnostics().work_graph_dispatch_calls == 0, "model fabricated graph execution");
}
struct Workload final {
  std::array<rhi::BufferHandle, 4> buffers;
  rhi::WorkGraphInput input;
  renderer::GPUDrivenResult reference;
  Words expected;
};
Workload MakeWorkload(rhi::Device &device, rhi::WorkGraphInput input) {
  Require(rhi::IsWorkGraphInputValid(input), "invalid test input");
  renderer::GPUSceneReferenceSnapshot scene;
  renderer::GPUDrivenView view;
  view.maximum_distance = 10;
  std::vector<std::uint32_t> packed(44 + input.count * 12);
  packed[0] = input.count;
  packed[4] = std::bit_cast<std::uint32_t>(view.maximum_distance);
  packed[6] = static_cast<std::uint32_t>(renderer::HiZPolicy::Invalidated);
  for (std::size_t i = 0; i < view.view_projection.values.size(); ++i)
    packed[20 + i] = std::bit_cast<std::uint32_t>(view.view_projection.values[i]);
  for (std::uint32_t i = 0; i < input.count; ++i) {
    renderer::GPUSceneReferenceObject object;
    object.object = {input.first_object_slot + i, 1};
    object.descriptor.world_bounds = {{0, 0, 0.25F}, 0.05F};
    object.descriptor.mesh_resource_index = 11;
    object.descriptor.material_resource_index = 13;
    object.descriptor.lod.lod_count = 1;
    scene.objects.push_back(object);
    const auto base = 44 + i * 12;
    packed[base + 2] = std::bit_cast<std::uint32_t>(0.25F);
    packed[base + 3] = std::bit_cast<std::uint32_t>(0.05F);
    packed[base + 4] = object.object.slot;
    packed[base + 5] = 1;
    packed[base + 6] = 11;
    packed[base + 7] = 13;
    packed[base + 8] = object.descriptor.visibility_flags;
    packed[base + 9] = 1;
  }
  Workload result{
      {device.CreateBuffer({packed.size() * sizeof(std::uint32_t), "graph fallback candidates"}),
       device.CreateBuffer({sizeof(Words), "graph visible output"}),
       device.CreateBuffer({rhi::WorkGraphMaximumRecords * rhi::GPUDrivenIndirectCommandStride,
                            "graph fallback indirect"}),
       device.CreateBuffer({8 * sizeof(std::uint32_t), "graph fallback statistics"})},
      input,
      renderer::BuildGPUDrivenCommands(scene, view),
      Empty()};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{packed}));
  device.WriteBuffer(result.buffers[1], 0, std::as_bytes(std::span{result.expected}));
  Require(result.reference.instances.size() == input.count, "CPU expansion fixture is not visible");
  for (std::size_t i = 0; i < result.reference.instances.size(); ++i) {
    const auto &instance = result.reference.instances[i];
    result.expected[i * 5] = instance.object.slot;
    result.expected[i * 5 + 1] = instance.object.generation;
    result.expected[i * 5 + 2] = instance.mesh_resource_index;
    result.expected[i * 5 + 3] = instance.material_resource_index;
    result.expected[i * 5 + 4] = instance.lod_index;
  }
  return result;
}
void Destroy(rhi::Device &device, const Workload &workload) {
  for (auto buffer : workload.buffers)
    device.DestroyBuffer(buffer);
}
Words Read(rhi::Device &device, rhi::BufferHandle buffer) {
  Words words{};
  device.ReadBufferForTesting(buffer, 0, std::as_writable_bytes(std::span{words}));
  return words;
}
void Record(rhi::CommandList &commands, rhi::PipelineHandle compute, rhi::WorkGraphHandle graph,
            const Workload &workload) {
  if (graph.IsValid()) {
    commands.BindWorkGraph(graph);
    commands.BindStorageBuffer(1, workload.buffers[1]);
    commands.DispatchWorkGraph(workload.input);
  } else {
    commands.BindPipeline(compute);
    for (std::uint32_t slot = 0; slot < workload.buffers.size(); ++slot)
      commands.BindStorageBuffer(slot, workload.buffers[slot]);
    commands.Dispatch(1);
  }
}
Words Execute(rhi::Device &device, rhi::PipelineHandle compute, rhi::WorkGraphHandle graph,
              const Workload &workload) {
  auto commands = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*commands, compute, graph, workload);
  const auto before = device.Diagnostics();
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  const auto after = device.Diagnostics();
  Require(after.work_graph_dispatch_calls ==
              before.work_graph_dispatch_calls + (graph.IsValid() ? 1 : 0),
          "selected graph execution count is incorrect");
  const auto actual = Read(device, workload.buffers[1]);
  Require(actual == workload.expected,
          "native expansion differs from CPU reference or overwrote unused records");
  if (!graph.IsValid()) {
    std::array<std::uint32_t, 8> counts{};
    device.ReadBufferForTesting(workload.buffers[3], 0, std::as_writable_bytes(std::span{counts}));
    Require(counts[0] == workload.input.count && counts[4] == workload.input.count &&
                counts[5] == workload.reference.commands.size(),
            "native compute/indirect fallback counts differ");
  }
  return actual;
}
void TestGraphRecording(rhi::Device &device, rhi::PipelineHandle compute,
                        rhi::WorkGraphHandle program, std::uint32_t count) {
  std::array workloads{MakeWorkload(device, {count, 31}), MakeWorkload(device, {count, 57})};
  renderer::RenderGraph graph;
  core::CancellationSource cancellation;
  std::barrier overlap{2};
  bool fail = false, cancel = false;
  for (std::size_t worker = 0; worker < workloads.size(); ++worker) {
    std::array<renderer::GraphBuffer, 4> buffers;
    for (std::size_t slot = 0; slot < buffers.size(); ++slot)
      buffers[slot] = graph.ImportBuffer(workloads[worker].buffers[slot],
                                         rhi::BufferAccess::Undefined, rhi::QueueType::Compute);
    renderer::RenderGraph::PassDescriptor pass{
        "visible expansion", rhi::QueueType::Compute, {}, {}, [&, worker](auto &commands, auto) {
          Record(commands, compute, program, workloads[worker]);
          overlap.arrive_and_wait();
          if (worker == 0 && fail)
            throw std::runtime_error("intentional graph recording failure");
          if (worker == 1 && cancel)
            cancellation.Cancel();
        }};
    pass.buffer_reads.push_back({buffers[0], rhi::BufferAccess::ShaderRead});
    for (std::size_t slot = 1; slot < buffers.size(); ++slot)
      pass.buffer_writes.push_back({buffers[slot], rhi::BufferAccess::StorageReadWrite});
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  core::JobSystem jobs{2};
  jobs.Start();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 2};
  for (unsigned mode = 0; mode < 2; ++mode) {
    fail = mode == 0;
    cancel = mode == 1;
    options.cancellation = cancel ? cancellation.Token() : core::CancellationToken{};
    const auto before = device.Diagnostics();
    Reject([&] { graph.Execute(device, options); }, "aborted node/fallback cohort succeeded");
    const auto after = device.Diagnostics();
    Require(before.submitted_command_lists == after.submitted_command_lists &&
                before.work_graph_dispatch_calls == after.work_graph_dispatch_calls,
            "aborted node/fallback cohort committed work");
    for (const auto &workload : workloads)
      Require(Read(device, workload.buffers[1]) == Empty(),
              "aborted node/fallback cohort changed output");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2,
          "node/fallback workers did not overlap");
  for (const auto &workload : workloads)
    Require(Read(device, workload.buffers[1]) == workload.expected,
            "node/fallback cohort output differs");
  jobs.Stop();
  for (const auto &workload : workloads)
    Destroy(device, workload);
}
void TestNativeLifetime(rhi::Device &device, rhi::PipelineHandle compute,
                        rhi::WorkGraphHandle program) {
  auto workload = MakeWorkload(device, {8, 7});
  auto small = device.CreateBuffer({4, "small node output"});
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Compute);
    abandoned->BindWorkGraph(program);
    Reject([&] { abandoned->DispatchWorkGraph(workload.input); },
           "node dispatch accepted missing output");
    abandoned->BindStorageBuffer(1, small);
    Reject([&] { abandoned->DispatchWorkGraph(workload.input); },
           "node dispatch accepted small output");
    abandoned->BindStorageBuffer(1, workload.buffers[1]);
    Reject([&] { abandoned->DispatchWorkGraph({9, 7}); }, "node fanout exceeded its record limit");
    Reject([&] { abandoned->DispatchWorkGraph({8, UINT32_MAX}); }, "node object slots overflowed");
    abandoned->DispatchWorkGraph(workload.input);
    Reject([&] { abandoned->Dispatch(1); }, "ordinary dispatch inherited a graph binding");
  }
  device.DestroyBuffer(small);
  Require(Read(device, workload.buffers[1]) == Empty(), "abandoned node list changed output");
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, workload);
    auto replacement = Empty();
    replacement[0] = 0x1234U;
    device.WriteBuffer(workload.buffers[1], 0, std::as_bytes(std::span{replacement}));
    const auto before = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); }, "stale node output reached native execution");
    Require(device.Diagnostics().submitted_command_lists == before, "stale node list submitted");
    Require(Read(device, workload.buffers[1]) == replacement,
            "stale node list overwrote CPU mutation");
  }
  const auto reset = Empty();
  device.WriteBuffer(workload.buffers[1], 0, std::as_bytes(std::span{reset}));
  {
    auto temporary = device.CreateWorkGraph({});
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, temporary, workload);
    device.DestroyWorkGraph(temporary);
    const auto recreated = device.CreateWorkGraph({});
    Require(recreated != temporary, "Work Graph handle generation was reused");
    const auto before = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); },
           "destroyed/recreated graph program reached execution");
    Require(device.Diagnostics().submitted_command_lists == before,
            "destroyed graph list submitted");
    device.DestroyWorkGraph(recreated);
  }
  Require(Read(device, workload.buffers[1]) == reset, "destroyed graph program changed output");
  (void)Execute(device, compute, program, workload);
  Destroy(device, workload);
  // Four different CPU records reuse the caller's stack during recording. The API must copy
  // them immediately; every dispatch also owns a different initialization/backing context.
  std::vector<Workload> held;
  held.reserve(rhi::WorkGraphMaximumDispatchesPerList);
  auto commands = device.CreateCommandList(rhi::QueueType::Compute);
  for (std::uint32_t i = 0; i < rhi::WorkGraphMaximumDispatchesPerList; ++i) {
    held.push_back(MakeWorkload(device, {8, 7 + i * 16}));
    Record(*commands, compute, program, held.back());
  }
  commands->BindWorkGraph(program);
  commands->BindStorageBuffer(1, held.front().buffers[1]);
  Reject([&] { commands->DispatchWorkGraph({8, 99}); }, "per-list node context bound was exceeded");
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  for (const auto &sample : held) {
    Require(Read(device, sample.buffers[1]) == sample.expected,
            "CPU record copy or node context ownership failed");
    Destroy(device, sample);
  }
  std::array mixed{MakeWorkload(device, {1, 7}), MakeWorkload(device, {8, 23}),
                   MakeWorkload(device, {8, 55})};
  commands = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*commands, compute, program, mixed[0]);
  Record(*commands, compute, {}, mixed[1]);
  Record(*commands, compute, program, mixed[2]);
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  for (const auto &sample : mixed) {
    Require(Read(device, sample.buffers[1]) == sample.expected,
            "graph/compute root and program switching corrupted output");
    Destroy(device, sample);
  }
}
#if defined(_WIN32)
class Environment final {
public:
  Environment(const char *name, const char *replacement) : name_(name) {
    if (const auto *value = std::getenv(name)) {
      existed_ = true;
      previous_ = value;
    }
    _putenv_s(name, replacement ? replacement : "");
  }
  ~Environment() { _putenv_s(name_, existed_ ? previous_.c_str() : ""); }

private:
  const char *name_;
  std::string previous_;
  bool existed_{};
};
#endif
bool Required() {
  const auto *value = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
  return value && *value == '1';
}
void TestNative(bool enhanced) {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
  constexpr auto artifact = "NEXORA_SLANG_COMPUTE_DXIL_PATH";
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
  constexpr auto artifact = "NEXORA_SLANG_COMPUTE_METAL_PATH";
#else
  constexpr auto backend = rhi::Backend::Vulkan;
  constexpr auto artifact = "NEXORA_SLANG_COMPUTE_SPIRV_PATH";
#endif
  const auto *path = std::getenv(artifact);
  if (!path || !*path || !rhi::IsBackendAvailable(backend)) {
    Require(!Required(), "required native node/compute fallback is unavailable");
    std::cout << "Native Work Graph/fallback unavailable\n";
    return;
  }
  auto device = rhi::CreateDeviceWithOptions(backend, {true, false, enhanced});
  const std::string compute_artifact_path{path};
  const auto caps = device->Capabilities();
  const bool enabled =
      rhi::CheckGraphicsRequirements(caps, rhi::EffectiveWorkGraphRequirements({})).Succeeded();
  Require(!enabled || backend == rhi::Backend::Direct3D12,
          "unimplemented backend advertised native Work Graphs");
  rhi::WorkGraphHandle program{};
  if (enabled) {
#if defined(_WIN32)
    {
      Environment missing{"NEXORA_D3D12_WORK_GRAPH_DXIL_PATH", nullptr};
      RejectContains([&] { (void)device->CreateWorkGraph({}); }, "library is unset");
    }
    {
      Environment wrong{"NEXORA_D3D12_WORK_GRAPH_DXIL_PATH", compute_artifact_path.c_str()};
      RejectContains([&] { (void)device->CreateWorkGraph({}); }, "DXIL library artifact");
    }
    const auto *wrong_abi = std::getenv("NEXORA_D3D12_WORK_GRAPH_WRONG_ABI_DXIL_PATH");
    Require(wrong_abi && *wrong_abi, "required real negative node ABI artifact is missing");
    const std::string wrong_abi_path{wrong_abi};
    {
      Environment wrong{"NEXORA_D3D12_WORK_GRAPH_DXIL_PATH", wrong_abi_path.c_str()};
      RejectContains([&] { (void)device->CreateWorkGraph({}); }, "entry record ABI");
    }
#endif
    program = device->CreateWorkGraph({});
  } else {
    Reject([&] { (void)device->CreateWorkGraph({}); }, "unavailable native node program admitted");
  }
  auto compute = device->CreatePipeline(
      {1, 2, rhi::TextureFormat::Rgba8Unorm, "node reference", rhi::PipelineType::Compute});
  for (const auto count : {0U, 1U, 8U}) {
    const rhi::WorkGraphInput input{count, count == 0   ? UINT32_MAX
                                           : count == 8 ? UINT32_MAX - 8
                                                        : 7};
    auto reference = MakeWorkload(*device, input);
    auto actual = MakeWorkload(*device, input);
    const auto computed = Execute(*device, compute, {}, reference);
    Require(Execute(*device, compute, program, actual) == computed,
            "node fanout differs from native compute fallback");
    Destroy(*device, reference);
    Destroy(*device, actual);
    TestGraphRecording(*device, compute, program, count);
  }
  if (enabled)
    TestNativeLifetime(*device, compute, program);
  const auto diagnostics = device->Diagnostics();
  Require(enabled ? diagnostics.work_graph_dispatch_calls > 0
                  : diagnostics.work_graph_dispatch_calls == 0,
          "native graph execution counts do not match selection");
  const auto barrier =
      caps.features[static_cast<std::size_t>(rhi::GraphicsFeature::EnhancedBarriers)];
  if (enhanced && barrier.enabled)
    Require(diagnostics.enhanced_buffer_barriers > 0,
            "selected Work Graph path omitted enhanced buffer barriers");
  else
    Require(diagnostics.enhanced_buffer_barriers == 0 && diagnostics.enhanced_texture_barriers == 0,
            "legacy Work Graph path fabricated enhanced barriers");
  if (program.IsValid())
    device->DestroyWorkGraph(program);
  device->DestroyPipeline(compute);
  std::cout << "Work Graph backend=" << static_cast<unsigned>(backend) << " enabled=" << enabled
            << " enhanced=" << barrier.enabled
            << " enhanced_buffers=" << diagnostics.enhanced_buffer_barriers
            << " dispatches=" << diagnostics.work_graph_dispatch_calls
            << "; fanout0/1/8, CPU/compute parity, bounded records, two-worker cancellation/replay "
               "passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    const bool enhanced = argc == 2 && std::string_view{argv[1]} == "--enhanced-barriers";
    Require(argc == 1 || enhanced, "usage: [--enhanced-barriers]");
#if !defined(_WIN32)
    Require(!enhanced, "enhanced Work Graph path requires D3D12");
#endif
    TestPortable();
    TestNative(enhanced);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
