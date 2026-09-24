#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

bool IsRequiredNativeBackend(nexora::rhi::Backend backend) {
  const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
  if (!required || *required != '1')
    return false;
#if defined(_WIN32)
  return backend == nexora::rhi::Backend::Direct3D12;
#elif defined(__APPLE__)
  return backend == nexora::rhi::Backend::Metal;
#else
  return backend == nexora::rhi::Backend::Vulkan;
#endif
}

void VerifyNativeBackend(nexora::rhi::Backend backend) {
  if (!nexora::rhi::IsBackendAvailable(backend)) {
    Require(!IsRequiredNativeBackend(backend), "required native backend is unavailable");
    return;
  }

  using namespace nexora;
  auto device = rhi::CreateDevice(backend);
  Require(device->GetBackend() == backend, "native backend factory returned the wrong backend");
  core::JobSystem jobs{2};
  jobs.Start();
  {
    renderer::PipelineCache cache{*device, jobs};
    const auto layout = rhi::TrianglePipelineLayout();
    const auto future =
        cache.Request({layout.layout_hash, 0x1234, rhi::TextureFormat::Rgba8Unorm, "Triangle"});
    future.Wait();
    const auto pipeline = future.Get();
    const rhi::TextureDescriptor swapchain_descriptor{640, 360, rhi::TextureFormat::Rgba8Unorm,
                                                      rhi::ResourceState::Present, "Native target"};
    const auto swapchain = device->CreateTexture(swapchain_descriptor);
    const auto frame =
        renderer::ExecuteTriangleFrame(*device, swapchain, swapchain_descriptor, pipeline);
    Require(frame.passes == 3 && frame.barriers == 4,
            "native backend generated unexpected render graph work");
    const auto diagnostics = device->Diagnostics();
    const auto expected_draws = backend == rhi::Backend::Vulkan ? 1U : 2U;
    const auto expected_indirect = backend == rhi::Backend::Vulkan ? 1U : 0U;
    Require(diagnostics.submitted_command_lists == 3 && diagnostics.draw_calls == expected_draws &&
                diagnostics.indirect_draw_calls == expected_indirect && diagnostics.barriers == 4 &&
                diagnostics.presents == 1 && diagnostics.validation_errors == 0,
            "native backend diagnostics are unexpected");
    device->DestroyTexture(swapchain);
  }
  jobs.Stop();
}

int RunTests() {
  using namespace nexora;
  const auto dxil = rhi::TrianglePipelineLayout();
  const auto spirv = rhi::TrianglePipelineLayout();
  const auto msl = rhi::TrianglePipelineLayout();
  Require(rhi::IsCanonicalLayout(dxil, spirv) && rhi::IsCanonicalLayout(spirv, msl),
          "cross-backend canonical reflection differs");
  Require(dxil.layout_hash != 0 && dxil.bindings.size() == 1,
          "pipeline layout metadata is invalid");

  auto device = rhi::CreateValidationDevice();
  core::JobSystem jobs{2};
  jobs.Start();
  rhi::PipelineDescriptor pipeline_descriptor{dxil.layout_hash, 0x1234,
                                              rhi::TextureFormat::Rgba8Unorm, "Triangle"};
  rhi::PipelineHandle pipeline;
  {
    renderer::PipelineCache cache{*device, jobs};
    const auto first = cache.Request(pipeline_descriptor);
    const auto duplicate = cache.Request(pipeline_descriptor);
    first.Wait();
    Require(first.IsReady() && duplicate.IsReady(), "asynchronous pipeline did not become ready");
    Require(cache.Size() == 1, "pipeline cache did not coalesce identical requests");
    pipeline = first.Get();
    const auto failed = cache.Request({0, 0, rhi::TextureFormat::Rgba8Unorm, "Invalid"});
    failed.Wait();
    bool rejected_pipeline = false;
    try {
      (void)failed.Get();
    } catch (const std::invalid_argument &) {
      rejected_pipeline = true;
    }
    Require(rejected_pipeline, "asynchronous pipeline errors must propagate without hanging");

    const rhi::TextureDescriptor swapchain_descriptor{640, 360, rhi::TextureFormat::Rgba8Unorm,
                                                      rhi::ResourceState::Present, "Swapchain"};
    const auto swapchain = device->CreateTexture(swapchain_descriptor);
    const auto frame =
        renderer::ExecuteTriangleFrame(*device, swapchain, swapchain_descriptor, pipeline);
    Require(frame.passes == 3 && frame.barriers == 4,
            "offscreen/main/present graph generated unexpected work");
    const auto diagnostics = device->Diagnostics();
    Require(diagnostics.submitted_command_lists == 3 && diagnostics.draw_calls == 2 &&
                diagnostics.barriers == 4 && diagnostics.presents == 1 &&
                diagnostics.validation_errors == 0,
            "validation backend diagnostics are unexpected");

    const std::array<std::byte, 12> vertex_data{};
    const std::array<std::byte, 6> index_data{};
    const auto vertices = device->CreateBuffer({vertex_data.size(), "UI vertices"});
    const auto indices = device->CreateBuffer({index_data.size(), "UI indices"});
    const auto sampled = device->CreateTexture(
        {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::ShaderRead, "UI texture"});
    const std::array<std::byte, 4> texel{};
    device->WriteTextureRgba8(sampled, texel, 4);
    device->WriteBuffer(vertices, 0, vertex_data);
    device->WriteBuffer(indices, 0, index_data);
    auto indexed = device->CreateCommandList(rhi::QueueType::Graphics);
    indexed->Transition({swapchain, rhi::ResourceState::Present, rhi::ResourceState::RenderTarget});
    indexed->BeginRendering({swapchain, 640, 360});
    indexed->BindPipeline(pipeline);
    indexed->BindVertexBuffer(vertices);
    indexed->BindIndexBuffer(indices, rhi::IndexFormat::Uint16);
    indexed->BindTexture(0, sampled);
    indexed->SetScissor({8, 12, 320, 180});
    indexed->DrawIndexed(3, 1, 0, 0, 0);
    indexed->EndRendering();
    indexed->Transition({swapchain, rhi::ResourceState::RenderTarget, rhi::ResourceState::Present});
    const auto completion = device->Submit(*indexed);
    Require(completion > 0 && device->CompletedSubmissionValue() >= completion,
            "submission completion did not advance");
    device->WaitForSubmission(completion);
    Require(device->Diagnostics().draw_calls == 3,
            "indexed draw was not reported by validation diagnostics");
    device->DestroyTexture(sampled);
    device->DestroyBuffer(indices);
    device->DestroyBuffer(vertices);

    auto submitted = device->CreateCommandList(rhi::QueueType::Graphics);
    device->Submit(*submitted);
    bool rejected_resubmit = false;
    try {
      device->Submit(*submitted);
    } catch (const std::logic_error &) {
      rejected_resubmit = true;
    }
    Require(rejected_resubmit, "command lists must be single-use");
    device->DestroyTexture(swapchain);
  }
  jobs.Stop();

  renderer::RenderGraph cyclic;
  (void)cyclic.AddPass({"A", rhi::QueueType::Graphics, {}, {}, [](rhi::CommandList &, auto) {}});
  (void)cyclic.AddPass({"B", rhi::QueueType::Graphics, {}, {}, [](rhi::CommandList &, auto) {}});
  cyclic.AddDependency(0, 1);
  cyclic.AddDependency(1, 0);
  bool rejected_cycle = false;
  try {
    cyclic.Compile();
  } catch (const std::logic_error &) {
    rejected_cycle = true;
  }
  Require(rejected_cycle, "render graph cycle must be rejected");

  renderer::RenderGraph invalid_pass;
  const auto texture = invalid_pass.CreateTransientTexture(
      {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::Undefined, "Conflict"});
  bool rejected_conflicting_use = false;
  try {
    (void)invalid_pass.AddPass({"Conflict",
                                rhi::QueueType::Graphics,
                                {{texture, rhi::ResourceState::ShaderRead}},
                                {{texture, rhi::ResourceState::RenderTarget}},
                                [](rhi::CommandList &, auto) {}});
  } catch (const std::invalid_argument &) {
    rejected_conflicting_use = true;
  }
  Require(rejected_conflicting_use, "a pass must not ambiguously read and write one texture");

  renderer::RenderGraph queues;
  const auto shared = queues.CreateTransientTexture(
      {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::Undefined, "Queue shared"});
  (void)queues.AddPass({"Cull",
                        rhi::QueueType::Compute,
                        {},
                        {{shared, rhi::ResourceState::ShaderRead}},
                        [](rhi::CommandList &commands, auto) { commands.Dispatch(1); }});
  (void)queues.AddPass({"Consume",
                        rhi::QueueType::Graphics,
                        {{shared, rhi::ResourceState::ShaderRead}},
                        {},
                        [](rhi::CommandList &, auto) {}});
  queues.Compile();
  queues.Execute(*device);
  Require(queues.GetStatistics().queue_transfer_count == 1,
          "render graph owns graphics/compute queue transfers");

  for (const auto backend :
       std::array{rhi::Backend::Direct3D12, rhi::Backend::Vulkan, rhi::Backend::Metal}) {
    VerifyNativeBackend(backend);
  }
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
