#pragma once

#include "Nexora/Core/Handle.h"

#include <array>
#include <compare>
#include <cstddef>
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

// Versions compare numerically: Shader Model 6.10 is newer than 6.9.
struct FeatureVersion final {
  std::uint16_t major{};
  std::uint16_t minor{};
  friend auto operator<=>(const FeatureVersion &, const FeatureVersion &) = default;
};
enum class GraphicsFeature : std::uint8_t {
  Compute,
  IndirectDraw,
  ParallelCommandRecording,
  Tessellation,
  EnhancedBarriers,
  RayTracing,
  MeshShader,
  VariableRateShading,
  SamplerFeedback,
  WorkGraphs,
  ShaderExecutionReordering,
  Count
};
inline constexpr auto GraphicsFeatureCount = static_cast<std::size_t>(GraphicsFeature::Count);
enum class CapabilitySupport : std::uint8_t { Unknown, Unsupported, Supported };
struct GraphicsFeatureCapability final {
  CapabilitySupport support{CapabilitySupport::Unknown};
  FeatureVersion tier{};
  bool enabled{};
  bool implemented{};
};
struct GraphicsDeviceCapabilities final {
  Backend backend{Backend::Null};
  bool validation_only{};
  bool software_device{};
  std::uint32_t vendor_id{};
  std::uint32_t device_id{};
  // Opaque backend driver identifier; not a portable semantic version.
  std::uint64_t driver_version{};
  std::array<GraphicsFeatureCapability, GraphicsFeatureCount> features{};
  CapabilitySupport dxil_shader_model_support{CapabilitySupport::Unknown};
  FeatureVersion highest_dxil_shader_model{};
};
struct GraphicsRequirements final {
  // {0, 0} means unused. Tierless features use {1, 0}; feedback may require {0, 9}.
  std::array<FeatureVersion, GraphicsFeatureCount> minimum_tiers{};
  // DXIL-only device limit. Compiler and artifact compatibility are separate checks.
  FeatureVersion minimum_dxil_shader_model{};
  friend bool operator==(const GraphicsRequirements &, const GraphicsRequirements &) = default;
};
enum class QueueType : std::uint8_t { Graphics, Compute, Copy };
enum class ResourceState : std::uint8_t {
  Undefined,
  CopySource,
  CopyDestination,
  ShaderRead,
  RenderTarget,
  Present
};
// Rgba16Float describes externally owned Presentation HDR resources; native RHI
// triangle allocation rejects it until that independent pipeline supports float targets.
enum class TextureFormat : std::uint8_t { Rgba8Unorm, Bgra8Unorm, Depth32Float, Rgba16Float };
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
  GraphicsRequirements graphics_requirements{};
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
