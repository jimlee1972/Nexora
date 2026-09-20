#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
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
