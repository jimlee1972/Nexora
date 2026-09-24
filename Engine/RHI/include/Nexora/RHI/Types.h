#pragma once

#include "Nexora/Core/Handle.h"

#include <cstdint>
#include <string>

namespace nexora::rhi {
struct BufferTag;
struct TextureTag;
struct PipelineTag;
struct FenceTag;
using BufferHandle = core::Handle<BufferTag>;
using TextureHandle = core::Handle<TextureTag>;
using PipelineHandle = core::Handle<PipelineTag>;
using FenceHandle = core::Handle<FenceTag>;

enum class Backend : std::uint8_t { Null, Direct3D12, Vulkan, Metal };
enum class QueueType : std::uint8_t { Graphics, Compute, Copy };
enum class ResourceState : std::uint8_t {
  Undefined,
  CopySource,
  CopyDestination,
  ShaderRead,
  RenderTarget,
  Present
};
enum class TextureFormat : std::uint8_t { Rgba8Unorm, Bgra8Unorm, Depth32Float };
enum class ShaderStage : std::uint8_t { Vertex = 1, Fragment = 2, Compute = 4 };
enum class BindingType : std::uint8_t { ConstantBuffer, Texture, Sampler, StorageBuffer };
enum class IndexFormat : std::uint8_t { Uint16, Uint32 };
enum class PipelineType : std::uint8_t { Graphics, Compute };

struct BufferDescriptor final {
  std::uint64_t size{};
  std::string debug_name;
};
struct ScissorRect final {
  std::int32_t x{};
  std::int32_t y{};
  std::uint32_t width{};
  std::uint32_t height{};
};
struct TextureDescriptor final {
  std::uint32_t width{};
  std::uint32_t height{};
  TextureFormat format{TextureFormat::Rgba8Unorm};
  ResourceState initial_state{ResourceState::Undefined};
  std::string debug_name;
};
struct PipelineDescriptor final {
  std::uint64_t layout_hash{};
  std::uint64_t shader_hash{};
  TextureFormat color_format{TextureFormat::Rgba8Unorm};
  std::string debug_name;
  PipelineType type{PipelineType::Graphics};
};
struct RenderingInfo final {
  TextureHandle color_target;
  std::uint32_t width{};
  std::uint32_t height{};
};
struct Barrier final {
  TextureHandle texture;
  ResourceState before{ResourceState::Undefined};
  ResourceState after{ResourceState::Undefined};
  QueueType source_queue{QueueType::Graphics};
  QueueType destination_queue{QueueType::Graphics};
};
} // namespace nexora::rhi
