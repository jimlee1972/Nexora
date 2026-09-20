#include <iostream>

#include "Nexora/Core/Engine.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/PipelineCache.h"

int main() {
  const auto info = nexora::foundation::GetBuildInfo();
  std::cout << "Nexora " << info.engine_version << " (ABI " << info.abi_version << ", "
            << info.build_configuration << ", " << info.link_mode << ")\n";
  nexora::core::Engine engine;
  engine.Initialize();
  engine.BeginFrame();
  {
    auto device = nexora::rhi::CreateValidationDevice();
    const auto layout = nexora::rhi::TrianglePipelineLayout();
    nexora::renderer::PipelineCache pipelines{*device, *engine.Services().jobs};
    const auto future = pipelines.Request(
        {layout.layout_hash, 0x1234, nexora::rhi::TextureFormat::Rgba8Unorm, "Triangle"});
    future.Wait();
    const nexora::rhi::TextureDescriptor output_descriptor{
        640, 360, nexora::rhi::TextureFormat::Rgba8Unorm, nexora::rhi::ResourceState::Present,
        "Validation output"};
    const auto output = device->CreateTexture(output_descriptor);
    const auto frame =
        nexora::renderer::ExecuteTriangleFrame(*device, output, output_descriptor, future.Get());
    std::cout << "Validation frame: " << frame.passes << " passes, " << frame.barriers
              << " barriers\n";
    device->DestroyTexture(output);
  }
  engine.Shutdown();
  return 0;
}
