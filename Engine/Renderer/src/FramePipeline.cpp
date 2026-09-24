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
  const bool native_vulkan_indirect = device.GetBackend() == rhi::Backend::Vulkan;
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
       [swapchain, pipeline, native_vulkan_indirect, width = swapchain_descriptor.width,
        height = swapchain_descriptor.height](rhi::CommandList &commands,
                                              std::span<const rhi::TextureHandle> textures) {
         commands.BeginRendering({textures[swapchain.id], width, height});
         commands.BindPipeline(pipeline);
         if (native_vulkan_indirect)
           commands.DrawIndirect(1);
         else
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

FrameResult ExecuteSceneFrame(rhi::Device &device, rhi::TextureHandle swapchain_texture,
                              const rhi::TextureDescriptor &swapchain_descriptor,
                              rhi::PipelineHandle pipeline, std::size_t visible_meshes) {
  RenderGraph graph;
  auto transient_descriptor = swapchain_descriptor;
  transient_descriptor.initial_state = rhi::ResourceState::Undefined;
  transient_descriptor.debug_name = "Scene shadow";
  const auto shadow = graph.CreateTransientTexture(transient_descriptor);
  transient_descriptor.debug_name = "Scene color";
  const auto scene_color = graph.CreateTransientTexture(transient_descriptor);
  const auto swapchain = graph.ImportTexture(swapchain_texture, swapchain_descriptor);
  const auto shadow_pass =
      graph.AddPass({"Shadow",
                     rhi::QueueType::Graphics,
                     {},
                     {{shadow, rhi::ResourceState::RenderTarget}},
                     [shadow, pipeline, visible_meshes, width = transient_descriptor.width,
                      height = transient_descriptor.height](
                         rhi::CommandList &commands, std::span<const rhi::TextureHandle> textures) {
                       commands.BeginRendering({textures[shadow.id], width, height});
                       commands.BindPipeline(pipeline);
                       commands.Draw(3, static_cast<std::uint32_t>(visible_meshes));
                       commands.EndRendering();
                     }});
  const auto light_culling =
      graph.AddPass({"Forward+ Light Culling",
                     rhi::QueueType::Graphics,
                     {},
                     {},
                     [](rhi::CommandList &, std::span<const rhi::TextureHandle>) {}});
  const auto forward =
      graph.AddPass({"Forward+",
                     rhi::QueueType::Graphics,
                     {{shadow, rhi::ResourceState::ShaderRead}},
                     {{scene_color, rhi::ResourceState::RenderTarget}},
                     [scene_color, pipeline, visible_meshes, width = transient_descriptor.width,
                      height = transient_descriptor.height](
                         rhi::CommandList &commands, std::span<const rhi::TextureHandle> textures) {
                       commands.BeginRendering({textures[scene_color.id], width, height});
                       commands.BindPipeline(pipeline);
                       commands.Draw(3, static_cast<std::uint32_t>(visible_meshes));
                       commands.EndRendering();
                     }});
  (void)graph.AddPass(
      {"PostProcess",
       rhi::QueueType::Graphics,
       {{scene_color, rhi::ResourceState::ShaderRead}},
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
  graph.AddDependency(shadow_pass, forward);
  graph.AddDependency(light_culling, forward);
  graph.Compile();
  graph.Execute(device);
  device.Present(swapchain_texture);
  const auto statistics = graph.GetStatistics();
  return {statistics.pass_count, statistics.barrier_count};
}
} // namespace nexora::renderer
