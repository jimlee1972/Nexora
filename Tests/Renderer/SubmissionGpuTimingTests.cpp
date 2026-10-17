#include "Nexora/Renderer/RenderGraph.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
const rhi::TextureDescriptor descriptor{32, 32, rhi::TextureFormat::Rgba8Unorm,
                                        rhi::ResourceState::ShaderRead, "timed target"};
void Draw(rhi::CommandList &commands, rhi::TextureHandle target, rhi::PipelineHandle pipeline) {
  for (int scope = 0; scope < 2; ++scope) {
    commands.Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    commands.BeginRendering({target, descriptor.width, descriptor.height});
    commands.BindPipeline(pipeline);
    for (int draw = 0; draw < 16; ++draw)
      commands.Draw(3);
    commands.EndRendering();
    commands.Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
  }
}
void TestValidationUnavailable() {
  auto device = rhi::CreateDeviceWithOptions(rhi::Backend::Null, {true, true});
  Require(!device->Capabilities().recording_resource_reuse_enabled &&
              device->Diagnostics().command_pool_reuses == 0 &&
              device->Diagnostics().cached_command_pools == 0,
          "Null must not claim requested native allocator reuse");
  Require(device->Capabilities().gpu_timing_support == rhi::CapabilitySupport::Unsupported &&
              !device->Capabilities().gpu_timing_enabled,
          "validation model must not advertise native GPU timing");
  const auto target = device->CreateTexture(descriptor);
  const auto pipeline = device->CreatePipeline({1, 1, descriptor.format, "model timing"});
  auto commands = device->CreateCommandList(rhi::QueueType::Graphics);
  Draw(*commands, target, pipeline);
  device->WaitForSubmission(device->Submit(*commands));
  commands.reset();
  Require(device->Diagnostics().gpu_timing == rhi::SubmissionGpuTiming{},
          "validation model fabricated GPU time or completion stream");
  device->DestroyTexture(target);
  device->DestroyPipeline(pipeline);
}
#if defined(NEXORA_TEST_NATIVE_GPU_TIMING)
#if defined(_WIN32)
constexpr auto backend = rhi::Backend::Direct3D12;
constexpr auto source = rhi::GpuTimingSource::Dx12Timestamps;
#elif defined(__APPLE__)
constexpr auto backend = rhi::Backend::Metal;
constexpr auto source = rhi::GpuTimingSource::MetalCommandBuffer;
#else
constexpr auto backend = rhi::Backend::Vulkan;
constexpr auto source = rhi::GpuTimingSource::VulkanTimestamps;
#endif
std::vector<std::byte> Read(rhi::Device &device, rhi::TextureHandle target) {
  std::vector<std::byte> pixels(descriptor.width * descriptor.height * 4);
  device.ReadTextureForTesting(target, pixels);
  return pixels;
}
void TestNative() {
  if (!rhi::IsBackendAvailable(backend)) {
    const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
    Require(!(required && *required == '1'),
            "native timing acceptance requires its backend/artifacts");
    std::cout << "Native GPU timing unavailable; portable validation-model checks completed\n";
    return;
  }
  auto plain = rhi::CreateDevice(backend);
  Require(!plain->Capabilities().gpu_timing_enabled, "default device enabled timing");
  const auto plain_target = plain->CreateTexture(descriptor);
  const auto plain_pipeline =
      plain->CreatePipeline({1, 1, descriptor.format, "default timing off"});
  auto plain_commands = plain->CreateCommandList(rhi::QueueType::Graphics);
  Draw(*plain_commands, plain_target, plain_pipeline);
  plain->WaitForSubmission(plain->Submit(*plain_commands));
  plain_commands.reset();
  const auto reference = Read(*plain, plain_target);
  bool colored = false;
  for (std::size_t index = 0; index < reference.size(); index += 4)
    colored |= reference[index] != std::byte{} || reference[index + 1] != std::byte{} ||
               reference[index + 2] != std::byte{};
  Require(colored && plain->Diagnostics().gpu_timing == rhi::SubmissionGpuTiming{},
          "default timing fabricated measurements or reference is empty");
  plain->DestroyTexture(plain_target);
  plain->DestroyPipeline(plain_pipeline);
  plain.reset();

  auto device = rhi::CreateDeviceWithOptions(backend, {true});
  const bool enabled = device->Capabilities().gpu_timing_enabled;
  Require(!enabled ||
              device->Capabilities().gpu_timing_support == rhi::CapabilitySupport::Supported,
          "unknown/unsupported GPU timestamps were enabled");
  const auto pipeline = device->CreatePipeline({1, 1, descriptor.format, "timed graphics"});
  std::array<rhi::TextureHandle, 4> targets;
  std::array<std::unique_ptr<rhi::CommandList>, 4> lists;
  for (std::size_t index = 0; index < targets.size(); ++index) {
    targets[index] = device->CreateTexture(descriptor);
    lists[index] = device->CreateCommandList(rhi::QueueType::Graphics);
    Draw(*lists[index], targets[index], pipeline);
  }
  Require(device->Diagnostics().gpu_timing == rhi::SubmissionGpuTiming{},
          "recording published an uncompleted timing");
  std::size_t available{};
  std::size_t positive{};
  for (std::size_t index = targets.size(); index-- > 0;) {
    const auto completion = device->Submit(*lists[index]);
    device->WaitForSubmission(completion);
    const auto observation = device->Diagnostics().gpu_timing;
    if (enabled) {
      Require(observation.completed_submission == completion && observation.source == source,
              "timing does not identify its completed native submission");
      if (observation.milliseconds) {
        Require(std::isfinite(*observation.milliseconds) && *observation.milliseconds >= 0,
                "native GPU timing is nonfinite or negative");
        ++available;
        positive += *observation.milliseconds > 0;
      }
    } else {
      Require(observation == rhi::SubmissionGpuTiming{},
              "unsupported timing fabricated a completion stream");
    }
    lists[index].reset(); // Query resources retire; copied observation must still own its data.
    Require(Read(*device, targets[index]) == reference &&
                device->Diagnostics().gpu_timing == observation,
            "timing changed pixels or test readback replaced the submitted measurement");
  }
  renderer::RenderGraph graph;
  core::CancellationSource cancellation;
  bool cancel = true;
  std::mutex mutex;
  std::condition_variable condition;
  std::size_t arrived{};
  std::array<renderer::GraphTexture, 4> imported;
  for (std::size_t index = 0; index < targets.size(); ++index) {
    imported[index] = graph.ImportTexture(targets[index], descriptor);
    renderer::RenderGraph::PassDescriptor pass{
        "timed cohort",
        rhi::QueueType::Graphics,
        {},
        {{imported[index], rhi::ResourceState::RenderTarget}},
        [&, index](rhi::CommandList &commands, auto) {
          {
            std::unique_lock lock{mutex};
            ++arrived;
            condition.notify_all();
            Require(condition.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 4; }),
                    "timed recording cohort did not overlap");
          }
          commands.BeginRendering({targets[index], descriptor.width, descriptor.height});
          if (!cancel) {
            commands.BindPipeline(pipeline);
            commands.Draw(3);
          }
          commands.EndRendering();
          if (cancel && index == 1)
            cancellation.Cancel();
        }};
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  for (auto target : imported)
    (void)graph.AddPass({"restore timed target",
                         rhi::QueueType::Graphics,
                         {{target, rhi::ResourceState::ShaderRead}},
                         {},
                         [](auto &, auto) {}});
  graph.Compile();
  core::JobSystem jobs{4};
  jobs.Start();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 4};
  options.cancellation = cancellation.Token();
  const auto before_cancel = device->Diagnostics();
  bool cancelled = false;
  try {
    graph.Execute(*device, options);
  } catch (const renderer::RenderGraphCancelled &) {
    cancelled = true;
  }
  Require(cancelled &&
              device->Diagnostics().submitted_command_lists ==
                  before_cancel.submitted_command_lists &&
              device->Diagnostics().gpu_timing == before_cancel.gpu_timing,
          "cancelled unsubmitted queries published timing or submitted work");
  for (auto target : targets)
    Require(Read(*device, target) == reference, "cancelled timed cohort changed output");
  arrived = 0;
  cancel = false;
  options.cancellation = {};
  graph.Execute(*device, options);
  for (auto target : targets)
    Require(Read(*device, target) == reference, "timed cohort could not replay after cancellation");
  jobs.Stop();
  for (auto target : targets)
    device->DestroyTexture(target);
  device->DestroyPipeline(pipeline);
  std::cout << "Native GPU timing: enabled=" << enabled
            << " source=" << static_cast<unsigned>(source) << " available=" << available
            << "/4 positive=" << positive
            << "; default-off, copied observations, reverse submission, cancelled queries and "
               "pixels passed\n";
}
#endif
} // namespace
int main() {
  try {
    TestValidationUnavailable();
#if defined(NEXORA_TEST_NATIVE_GPU_TIMING)
    TestNative();
#endif
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
