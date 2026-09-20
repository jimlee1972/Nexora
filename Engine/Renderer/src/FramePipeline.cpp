#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/RenderGraph.h"

namespace nexora::renderer {
FrameResult ExecuteTriangleFrame(rhi::Device &device, rhi::TextureHandle swapchain_texture,
                                 const rhi::TextureDescriptor &swapchain_descriptor,
                                 rhi::PipelineHandle pipeline) {
  RenderGraph graph;
  auto offscreen_descriptor = swapchain_descriptor;
  offscreen_descriptor.initial_state = rhi::ResourceState::Undefined;
  offscreen_descriptor.debug_name = "Triangle offscreen";
  const auto offscreen = graph.CreateTransientTexture(offscreen_descriptor);
  const auto swapchain = graph.ImportTexture(swapchain_texture, swapchain_descriptor);
  (void)graph.AddPass(
      {"Offscreen",
       rhi::QueueType::Graphics,
       {},
       {{offscreen, rhi::ResourceState::RenderTarget}},
       [offscreen, pipeline, width = offscreen_descriptor.width,
        height = offscreen_descriptor.height](rhi::CommandList &commands,
                                              std::span<const rhi::TextureHandle> textures) {
         commands.BeginRendering({textures[offscreen.id], width, height});
         commands.BindPipeline(pipeline);
         commands.Draw(3);
         commands.EndRendering();
       }});
  (void)graph.AddPass(
      {"Main",
       rhi::QueueType::Graphics,
       {{offscreen, rhi::ResourceState::ShaderRead}},
       {{swapchain, rhi::ResourceState::RenderTarget}},
       [swapchain, pipeline, width = swapchain_descriptor.width,
        height = swapchain_descriptor.height](rhi::CommandList &commands,
                                              std::span<const rhi::TextureHandle> textures) {
         commands.BeginRendering({textures[swapchain.id], width, height});
         commands.BindPipeline(pipeline);
         commands.Draw(3);
         commands.EndRendering();
       }});
  (void)graph.AddPass({"Present",
                       rhi::QueueType::Graphics,
                       {},
                       {{swapchain, rhi::ResourceState::Present}},
                       [](rhi::CommandList &, std::span<const rhi::TextureHandle>) {}});
  graph.Compile();
  graph.Execute(device);
  device.Present(swapchain_texture);
  const auto statistics = graph.GetStatistics();
  return {statistics.pass_count, statistics.barrier_count};
}
} // namespace nexora::renderer
