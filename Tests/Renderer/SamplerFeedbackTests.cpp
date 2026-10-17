#include "Nexora/RHI/Capabilities.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"
#include <algorithm>
#include <array>
#include <barrier>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
using namespace nexora;
namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <class F> void Reject(F function, const char *message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception &) {
    rejected = true;
  }
  Require(rejected, message);
}
template <class F> void RejectContains(F function, const char *message) {
  try {
    function();
  } catch (const std::exception &error) {
    Require(std::string{error.what()}.find(message) != std::string::npos,
            "feedback rejection missed the required admission boundary");
    return;
  }
  throw std::runtime_error("invalid feedback artifact admitted");
}
constexpr std::uint32_t Sentinel = 0xcdcdcdcdU;
using Words = std::array<std::uint32_t, 20>;
Words Empty() {
  Words result;
  result.fill(Sentinel);
  return result;
}
std::vector<rhi::SamplerFeedbackRequest> Requests(std::uint32_t count, bool invalid = false) {
  std::vector<rhi::SamplerFeedbackRequest> result;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto cell = i % 16;
    rhi::SamplerFeedbackRequest request{(static_cast<float>(cell % 4) + .5F) / 4,
                                        (static_cast<float>(cell / 4) + .5F) / 4,
                                        2 - std::min(2U, i / 16), 1};
    const auto kind = invalid ? i % 4 + 12 : cell;
    if (kind == 12)
      request.enabled = 0;
    if (kind == 13)
      request.enabled = 2;
    if (kind == 14)
      request.mip = 3;
    if (kind == 15) {
      switch (i / 16) {
      case 0:
        request.u = std::numeric_limits<float>::quiet_NaN();
        break;
      case 1:
        request.u = std::numeric_limits<float>::infinity();
        break;
      case 2:
        request.u = -.25F;
        break;
      default:
        request.u = 1;
      }
    }
    result.push_back(request);
  }
  return result;
}
Words Reference(std::span<const rhi::SamplerFeedbackRequest> requests) {
  auto result = Empty();
  std::fill_n(result.begin(), 16, 255U);
  for (const auto &request : requests) {
    if (request.enabled != 1 || request.mip > 2 || !std::isfinite(request.u) ||
        !std::isfinite(request.v) || request.u < 0 || request.v < 0 || request.u >= 1 ||
        request.v >= 1)
      continue;
    const auto x = static_cast<std::size_t>(std::floor(static_cast<double>(request.u) * 4));
    const auto y = static_cast<std::size_t>(std::floor(static_cast<double>(request.v) * 4));
    result[y * 4 + x] = std::min(result[y * 4 + x], request.mip);
  }
  return result;
}
struct Workload final {
  std::array<rhi::BufferHandle, 5> buffers;
  std::uint32_t count;
  Words expected;
};
Workload Make(rhi::Device &device, std::uint32_t count, bool invalid = false) {
  const auto requests = Requests(count, invalid);
  std::vector<std::uint32_t> packed{count};
  for (const auto &request : requests) {
    packed.push_back(std::bit_cast<std::uint32_t>(request.u));
    packed.push_back(std::bit_cast<std::uint32_t>(request.v));
    packed.push_back(request.mip);
    packed.push_back(request.enabled);
  }
  Workload result{
      {device.CreateBuffer({packed.size() * 4, "feedback reference input"}),
       device.CreateBuffer({sizeof(Words), "decoded feedback plus canary"}),
       device.CreateBuffer({16, "feedback unused2"}), device.CreateBuffer({16, "feedback unused3"}),
       device.CreateBuffer(
           {std::max<std::size_t>(1, requests.size()) * sizeof(rhi::SamplerFeedbackRequest),
            "feedback requests"})},
      count,
      Reference(requests)};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{packed}));
  if (!requests.empty())
    device.WriteBuffer(result.buffers[4], 0, std::as_bytes(std::span{requests}));
  const auto empty = Empty();
  device.WriteBuffer(result.buffers[1], 0, std::as_bytes(std::span{empty}));
  return result;
}
void Destroy(rhi::Device &device, const Workload &work) {
  for (auto buffer : work.buffers)
    device.DestroyBuffer(buffer);
}
Words Read(rhi::Device &device, const Workload &work) {
  Words result{};
  device.ReadBufferForTesting(work.buffers[1], 0, std::as_writable_bytes(std::span{result}));
  return result;
}
void Compare(rhi::Device &device, const Workload &work, bool native = false) {
  const auto actual = Read(device, work);
  if (actual != work.expected) {
    std::cerr << "Feedback mismatch native=" << native << " count=" << work.count << ":";
    for (std::size_t i = 0; i < actual.size(); ++i)
      std::cerr << " [" << i << "]=" << actual[i] << '/' << work.expected[i];
    std::cerr << '\n';
    if (native && work.count) {
      std::array<std::uint32_t, 4> request{};
      device.ReadBufferForTesting(work.buffers[4], 0, std::as_writable_bytes(std::span{request}));
      std::cerr << "GPU request0 bits=" << request[0] << ',' << request[1] << " mip=" << request[2]
                << " enabled=" << request[3] << '\n';
    }
  }
  Require(actual == work.expected,
          "decoded min-mip/clear/untouched/canary differs from CPU reference");
}
void Record(rhi::CommandList &commands, rhi::PipelineHandle compute,
            rhi::SamplerFeedbackProgramHandle program, const Workload &work) {
  if (program.IsValid())
    commands.DispatchSamplerFeedback(program, work.buffers[4], work.buffers[1], work.count);
  else {
    commands.BindPipeline(compute);
    for (std::uint32_t slot = 0; slot < 4; ++slot)
      commands.BindStorageBuffer(slot, work.buffers[slot]);
    commands.Dispatch(1);
  }
}
void Execute(rhi::Device &device, rhi::PipelineHandle compute,
             rhi::SamplerFeedbackProgramHandle program, const Workload &work) {
  auto commands = device.CreateCommandList(rhi::QueueType::Compute);
  const auto before = device.Diagnostics();
  Record(*commands, compute, program, work);
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  Compare(device, work, program.IsValid());
  Require(device.Diagnostics().sampler_feedback_dispatch_calls ==
              before.sampler_feedback_dispatch_calls + (program.IsValid() ? 1 : 0),
          "accepted feedback dispatch accounting differs from selected path");
}
void Parallel(rhi::Device &device, rhi::PipelineHandle compute,
              rhi::SamplerFeedbackProgramHandle program) {
  std::array work{Make(device, 16), Make(device, 64)};
  renderer::RenderGraph graph;
  std::barrier overlap{2};
  core::CancellationSource cancellation;
  bool fail = false, cancel = false;
  for (std::size_t worker = 0; worker < 2; ++worker) {
    std::array<renderer::GraphBuffer, 5> buffers;
    for (std::size_t slot = 0; slot < buffers.size(); ++slot)
      buffers[slot] = graph.ImportBuffer(work[worker].buffers[slot], rhi::BufferAccess::Undefined,
                                         rhi::QueueType::Compute);
    renderer::RenderGraph::PassDescriptor pass{
        "min-mip feedback", rhi::QueueType::Compute, {}, {}, [&, worker](auto &commands, auto) {
          Record(commands, compute, program, work[worker]);
          overlap.arrive_and_wait();
          if (worker == 0 && fail)
            throw std::runtime_error("intentional feedback cohort failure");
          if (worker == 1 && cancel)
            cancellation.Cancel();
        }};
    for (auto slot : {0U, 4U})
      pass.buffer_reads.push_back({buffers[slot], rhi::BufferAccess::ShaderRead});
    for (auto slot : {1U, 2U, 3U})
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
    Reject([&] { graph.Execute(device, options); }, "aborted feedback cohort succeeded");
    const auto after = device.Diagnostics();
    Require(before.submitted_command_lists == after.submitted_command_lists &&
                before.sampler_feedback_dispatch_calls == after.sampler_feedback_dispatch_calls,
            "aborted feedback cohort committed work");
    for (const auto &item : work)
      Require(Read(device, item) == Empty(), "aborted feedback cohort changed output");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2,
          "feedback workers did not overlap");
  for (const auto &item : work)
    Compare(device, item);
  jobs.Stop();
  for (const auto &item : work)
    Destroy(device, item);
}
void Lifetime(rhi::Device &device, rhi::PipelineHandle compute,
              rhi::SamplerFeedbackProgramHandle program) {
  auto work = Make(device, 64);
  const auto small = device.CreateBuffer({4, "small feedback buffer"});
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Compute);
    Reject(
        [&] { abandoned->DispatchSamplerFeedback(program, work.buffers[4], work.buffers[1], 65); },
        "oversized request count admitted");
    Reject([&] { abandoned->DispatchSamplerFeedback(program, small, work.buffers[1], 64); },
           "small feedback input admitted");
    Reject([&] { abandoned->DispatchSamplerFeedback(program, work.buffers[4], small, 64); },
           "small feedback output admitted");
    Reject(
        [&] { abandoned->DispatchSamplerFeedback(program, work.buffers[4], work.buffers[4], 64); },
        "aliased feedback buffers admitted");
    Record(*abandoned, compute, program, work);
    Reject([&] { abandoned->Dispatch(1); }, "ordinary dispatch inherited feedback pipeline");
  }
  device.DestroyBuffer(small);
  Require(Read(device, work) == Empty(), "abandoned feedback list changed output");
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, work);
    auto changed = Requests(64);
    changed[0].enabled = 0;
    device.WriteBuffer(work.buffers[4], 0, std::as_bytes(std::span{changed}));
    const auto before = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); }, "stale feedback input submitted");
    Require(device.Diagnostics().submitted_command_lists == before,
            "stale feedback input committed work");
  }
  {
    const auto reset = Requests(64);
    device.WriteBuffer(work.buffers[4], 0, std::as_bytes(std::span{reset}));
  }
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, work);
    auto changed = Empty();
    changed[0] = 0x1234U;
    device.WriteBuffer(work.buffers[1], 0, std::as_bytes(std::span{changed}));
    Reject([&] { (void)device.Submit(*stale); }, "stale feedback output submitted");
    Require(Read(device, work) == changed, "stale feedback output overwrote CPU mutation");
  }
  {
    const auto reset = Empty();
    device.WriteBuffer(work.buffers[1], 0, std::as_bytes(std::span{reset}));
  }
  {
    auto temporary = device.CreateSamplerFeedbackProgram({});
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, temporary, work);
    device.DestroySamplerFeedbackProgram(temporary);
    const auto replacement = device.CreateSamplerFeedbackProgram({});
    Require(replacement != temporary, "feedback program generation reused");
    Reject([&] { (void)device.Submit(*stale); }, "destroyed feedback program submitted");
    device.DestroySamplerFeedbackProgram(replacement);
  }
  Require(Read(device, work) == Empty(), "stale feedback list changed output");
  {
    std::vector<Workload> held;
    held.reserve(rhi::SamplerFeedbackMaximumDispatchesPerList);
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    for (auto count : {0U, 1U, 16U, 64U}) {
      held.push_back(Make(device, count));
      Record(*commands, compute, program, held.back());
    }
    Reject([&] { Record(*commands, compute, program, work); }, "feedback context bound exceeded");
    device.WaitForSubmission(device.Submit(*commands));
    commands.reset();
    for (const auto &item : held) {
      Compare(device, item);
      Destroy(device, item);
    }
  }
  auto middle = Make(device, 1), last = Make(device, 16);
  {
    auto mixed = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*mixed, compute, program, work);
    Record(*mixed, compute, {}, middle);
    Record(*mixed, compute, program, last);
    device.WaitForSubmission(device.Submit(*mixed));
  }
  Compare(device, work);
  Compare(device, middle);
  Compare(device, last);
  Destroy(device, middle);
  Destroy(device, last);
  Destroy(device, work);
}
void MixedNodes(rhi::Device &device, rhi::PipelineHandle compute,
                rhi::SamplerFeedbackProgramHandle program) {
  if (!rhi::CheckGraphicsRequirements(device.Capabilities(),
                                      rhi::EffectiveWorkGraphRequirements({}))
           .Succeeded())
    return;
  auto node = device.CreateWorkGraph({});
  std::array<std::uint32_t, 40> empty;
  empty.fill(Sentinel);
  std::array buffers{device.CreateBuffer({sizeof(empty), "nodes before feedback"}),
                     device.CreateBuffer({sizeof(empty), "nodes after feedback"})};
  for (auto buffer : buffers)
    device.WriteBuffer(buffer, 0, std::as_bytes(std::span{empty}));
  auto work = Make(device, 16);
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    commands->BindWorkGraph(node);
    commands->BindStorageBuffer(1, buffers[0]);
    commands->DispatchWorkGraph({8, 11});
    Record(*commands, compute, program, work);
    Reject([&] { commands->DispatchWorkGraph({1, 33}); },
           "node program remained bound after feedback selection");
    commands->BindWorkGraph(node);
    commands->BindStorageBuffer(1, buffers[1]);
    commands->DispatchWorkGraph({1, 33});
    device.WaitForSubmission(device.Submit(*commands));
  }
  Compare(device, work);
  for (std::size_t index = 0; index < 2; ++index) {
    auto actual = empty, expected = empty;
    const std::size_t count = index == 0 ? 8 : 1;
    const std::uint32_t first = index == 0 ? 11 : 33;
    for (std::size_t record = 0; record < count; ++record) {
      expected[record * 5] = first + static_cast<std::uint32_t>(record);
      expected[record * 5 + 1] = 1;
      expected[record * 5 + 2] = 11;
      expected[record * 5 + 3] = 13;
      expected[record * 5 + 4] = 0;
    }
    device.ReadBufferForTesting(buffers[index], 0, std::as_writable_bytes(std::span{actual}));
    Require(actual == expected, "Node/Feedback/Node root/heap state restoration failed");
  }
  Destroy(device, work);
  for (auto buffer : buffers)
    device.DestroyBuffer(buffer);
  device.DestroyWorkGraph(node);
  std::cout << "Native Node/Feedback/Node root/descriptor restoration passed\n";
}
void Portable() {
  const auto requirements = rhi::EffectiveSamplerFeedbackRequirements({});
  const auto index = static_cast<std::size_t>(rhi::GraphicsFeature::SamplerFeedback);
  Require(requirements.minimum_tiers[index] == rhi::FeatureVersion{0, 9} &&
              requirements.minimum_dxil_shader_model == rhi::FeatureVersion{6, 5},
          "implicit feedback tier/profile requirements lost");
  rhi::GraphicsDeviceCapabilities caps;
  caps.features[index] = {rhi::CapabilitySupport::Supported, {1, 0}, true, true, {0, 9}};
  caps.dxil_shader_model_support = rhi::CapabilitySupport::Supported;
  caps.highest_dxil_shader_model = {6, 8};
  rhi::RequireGraphicsCapabilities(caps, requirements);
  rhi::SamplerFeedbackProgramDescriptor descriptor{};
  descriptor.graphics_requirements.minimum_tiers[index] = {1, 0};
  Require(
      rhi::CheckGraphicsRequirements(caps, rhi::EffectiveSamplerFeedbackRequirements(descriptor))
              .failure == rhi::CapabilityFailure::InsufficientImplementedTier,
      "unimplemented full feedback tier admitted");
  auto model = rhi::CreateValidationDevice();
  Reject([&] { (void)model->CreateSamplerFeedbackProgram({}); },
         "Validation fabricated feedback program");
  rhi::PipelineDescriptor pipeline{1, 2, rhi::TextureFormat::Rgba8Unorm, "feedback cache",
                                   rhi::PipelineType::Compute};
  const auto ordinary = renderer::MakePipelineCacheKey(pipeline);
  pipeline.compute_shader = rhi::ComputeShaderVariant::RayReference;
  const auto ray = renderer::MakePipelineCacheKey(pipeline);
  pipeline.compute_shader = rhi::ComputeShaderVariant::SamplerFeedbackReference;
  const auto feedback = renderer::MakePipelineCacheKey(pipeline);
  Require(feedback != ordinary && feedback != ray, "feedback compute cache identity collided");
  Reject([&] { (void)model->CreatePipeline(pipeline); },
         "Validation simulated native feedback reference");
  pipeline.type = rhi::PipelineType::Graphics;
  Reject([&] { (void)rhi::EffectivePipelineRequirements(pipeline); },
         "graphics pipeline admitted feedback compute variant");
}
#if defined(_WIN32)
class Environment final {
public:
  Environment(const char *name, const char *value) : name_(name) {
    if (const auto *old = std::getenv(name)) {
      previous_ = old;
      existed_ = true;
    }
    Require(_putenv_s(name, value ? value : "") == 0, "environment override failed");
  }
  ~Environment() { (void)_putenv_s(name_.c_str(), existed_ ? previous_.c_str() : ""); }

private:
  std::string name_, previous_;
  bool existed_{};
};
#endif
void Native(bool enhanced) {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
  constexpr auto reference = "NEXORA_SLANG_FEEDBACK_REFERENCE_DXIL_PATH";
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
  constexpr auto reference = "NEXORA_SLANG_FEEDBACK_REFERENCE_METAL_PATH";
#else
  constexpr auto backend = rhi::Backend::Vulkan;
  constexpr auto reference = "NEXORA_SLANG_FEEDBACK_REFERENCE_SPIRV_PATH";
#endif
  const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS"),
             *path = std::getenv(reference);
  if (!path || !*path || !rhi::IsBackendAvailable(backend)) {
    Require(!required || *required != '1', "required feedback/reference backend unavailable");
    std::cout << "Native feedback/reference unavailable\n";
    return;
  }
  const std::string reference_path{path};
  rhi::DeviceCreationOptions options{true, false, enhanced};
#if defined(NEXORA_TEST_FEEDBACK_PREVIEW)
  options.enable_d3d12_preview_runtime = true;
#endif
  auto device = rhi::CreateDeviceWithOptions(backend, options);
  const auto caps = device->Capabilities();
  const bool enabled =
      rhi::CheckGraphicsRequirements(caps, rhi::EffectiveSamplerFeedbackRequirements({}))
          .Succeeded();
#if defined(NEXORA_TEST_FEEDBACK_PREVIEW)
  Require(enabled && caps.d3d12_preview_runtime_enabled &&
              caps.highest_dxil_shader_model >= rhi::FeatureVersion{6, 10},
          "required isolated preview feedback device unavailable");
  std::cout << "Feedback preview SM=" << caps.highest_dxil_shader_model.major << '.'
            << caps.highest_dxil_shader_model.minor << '\n';
#endif
  rhi::SamplerFeedbackProgramHandle program{};
  if (enabled) {
#if defined(_WIN32)
    {
      Environment missing{"NEXORA_SLANG_FEEDBACK_WRITE_DXIL_PATH", nullptr};
      RejectContains([&] { (void)device->CreateSamplerFeedbackProgram({}); },
                     "sampler feedback artifact is unset");
    }
    {
      Environment wrong{"NEXORA_SLANG_FEEDBACK_WRITE_DXIL_PATH", reference_path.c_str()};
      RejectContains([&] { (void)device->CreateSamplerFeedbackProgram({}); },
                     "shader model is too low");
    }
    const auto *negative = std::getenv("NEXORA_SLANG_FEEDBACK_WRONG_IDENTITY_DXIL_PATH");
    Require(negative && *negative, "real same-profile feedback identity negative is missing");
    const std::string negative_path{negative};
    {
      Environment wrong{"NEXORA_SLANG_FEEDBACK_WRITE_DXIL_PATH", negative_path.c_str()};
      RejectContains([&] { (void)device->CreateSamplerFeedbackProgram({}); },
                     "sampler feedback recipe identity");
    }
    const auto *vertex = std::getenv("NEXORA_SLANG_DXIL_VERTEX_PATH");
    Require(vertex && *vertex, "real wrong-stage feedback artifact is missing");
    const std::string vertex_path{vertex};
    {
      Environment wrong{"NEXORA_SLANG_FEEDBACK_PACK_DXIL_PATH", vertex_path.c_str()};
      RejectContains([&] { (void)device->CreateSamplerFeedbackProgram({}); },
                     "stage does not match");
    }
#endif
    program = device->CreateSamplerFeedbackProgram({});
  } else
    Reject([&] { (void)device->CreateSamplerFeedbackProgram({}); },
           "unavailable native feedback admitted");
  rhi::PipelineDescriptor pipeline{1, 2, rhi::TextureFormat::Rgba8Unorm, "feedback reference",
                                   rhi::PipelineType::Compute};
  pipeline.compute_shader = rhi::ComputeShaderVariant::SamplerFeedbackReference;
  const auto compute = device->CreatePipeline(pipeline);
  for (auto count : {0U, 1U, 16U, 64U}) {
    auto fallback = Make(*device, count);
    Execute(*device, compute, {}, fallback);
    const auto expected = Read(*device, fallback);
    auto actual = Make(*device, count);
    Execute(*device, compute, program, actual);
    Require(Read(*device, actual) == expected,
            "native decoded feedback differs from actual compute fallback");
    Destroy(*device, fallback);
    Destroy(*device, actual);
  }
  auto invalid = Make(*device, 4, true);
  Execute(*device, compute, program, invalid);
  Destroy(*device, invalid);
  Parallel(*device, compute, program);
  if (enabled) {
    Lifetime(*device, compute, program);
    MixedNodes(*device, compute, program);
  }
  const auto diagnostics = device->Diagnostics();
  Require(enabled ? diagnostics.sampler_feedback_dispatch_calls > 0
                  : diagnostics.sampler_feedback_dispatch_calls == 0,
          "feedback execution diagnostics differ from admission");
  const auto barrier =
      caps.features[static_cast<std::size_t>(rhi::GraphicsFeature::EnhancedBarriers)];
  if (enhanced && barrier.enabled)
    Require(diagnostics.enhanced_buffer_barriers > 0,
            "requested feedback enhanced buffer path unused");
  else
    Require(diagnostics.enhanced_buffer_barriers == 0,
            "legacy feedback fabricated enhanced barriers");
  std::cout << "Sampler feedback backend=" << static_cast<unsigned>(backend)
            << " enabled=" << enabled
            << " dispatches=" << diagnostics.sampler_feedback_dispatch_calls
            << " enhanced=" << barrier.enabled
            << " enhanced_buffers=" << diagnostics.enhanced_buffer_barriers
            << "; decoded min-mip, clear255, duplicate minima, untouched/canary, invalid requests, "
               "cancellation/replay passed\n";
  if (program.IsValid())
    device->DestroySamplerFeedbackProgram(program);
  device->DestroyPipeline(compute);
}
} // namespace
int main(int argc, char **argv) {
  try {
    const bool enhanced = argc == 2 && std::string_view{argv[1]} == "--enhanced-barriers";
    Require(argc == 1 || enhanced, "usage: [--enhanced-barriers]");
    Portable();
    Native(enhanced);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
