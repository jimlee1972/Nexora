#pragma once

#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/Api.h"

namespace nexora::renderer {
struct FrameResult final {
  std::size_t passes{};
  std::size_t barriers{};
};
[[nodiscard]] NEXORA_RENDERER_API FrameResult ExecuteTriangleFrame(
    rhi::Device &device, rhi::TextureHandle swapchain_texture,
    const rhi::TextureDescriptor &swapchain_descriptor, rhi::PipelineHandle pipeline);
[[nodiscard]] NEXORA_RENDERER_API FrameResult
ExecuteSceneFrame(rhi::Device &device, rhi::TextureHandle swapchain_texture,
                  const rhi::TextureDescriptor &swapchain_descriptor, rhi::PipelineHandle pipeline,
                  std::size_t visible_meshes);
} // namespace nexora::renderer
