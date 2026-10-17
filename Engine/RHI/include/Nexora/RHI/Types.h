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
struct WorkGraphTag;
using BufferHandle = core::Handle<BufferTag>;
using TextureHandle = core::Handle<TextureTag>;
using PipelineHandle = core::Handle<PipelineTag>;
using FenceHandle = core::Handle<FenceTag>;
using WorkGraphHandle = core::Handle<WorkGraphTag>;

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
  // Zero retains the legacy meaning: all reported hardware tiers are implemented.
  // A nonzero ceiling limits backend admission independently from hardware support.
  FeatureVersion implemented_tier{};
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
  // Limited admission: independent buffer-only compute lists; not the full parallel feature gate.
  bool independent_compute_recording{};
  // Limited disjoint color-target graphics lists; not the full parallel feature gate.
  bool independent_graphics_recording{};
  // Limited immutable buffer read/read sharing; excludes writes and shared textures.
  bool shared_buffer_read_recording{};
  CapabilitySupport gpu_timing_support{CapabilitySupport::Unknown};
  bool gpu_timing_enabled{};
  bool recording_resource_reuse_enabled{};
  // Separate optimization query: functional SER may execute without actual thread reordering.
  CapabilitySupport actual_shader_reordering_support{CapabilitySupport::Unknown};

  std::uint32_t shading_rate_image_tile_size{};
  bool d3d12_preview_runtime_enabled{};
  CapabilitySupport linear_algebra_support{CapabilitySupport::Unknown};
  CapabilitySupport linear_algebra_f32_matvec_support{CapabilitySupport::Unknown};
  CapabilitySupport compute_shared_memory_support{CapabilitySupport::Unknown};
  std::uint32_t maximum_compute_shared_memory_bytes{};
};
struct GraphicsRequirements final {
  // {0, 0} means unused. Tierless features use {1, 0}; feedback may require {0, 9}.
  std::array<FeatureVersion, GraphicsFeatureCount> minimum_tiers{};
  // DXIL-only device limit. Compiler and artifact compatibility are separate checks.
  FeatureVersion minimum_dxil_shader_model{};
  friend bool operator==(const GraphicsRequirements &, const GraphicsRequirements &) = default;
};
// Bounded visible-record expansion recipe; not a general graph authoring API.
inline constexpr std::uint32_t WorkGraphMaximumRecords = 8;
inline constexpr std::uint32_t WorkGraphMaximumDispatchesPerList = 4;
inline constexpr std::uint32_t WorkGraphVisibleRecordWords = 5;
struct WorkGraphDescriptor final {
  GraphicsRequirements graphics_requirements{};
  std::string debug_name;
};
struct WorkGraphInput final {
  std::uint32_t count{};
  std::uint32_t first_object_slot{};
};
static_assert(sizeof(WorkGraphInput) == 8);
static_assert(offsetof(WorkGraphInput, count) == 0 &&
              offsetof(WorkGraphInput, first_object_slot) == 4);
constexpr bool IsWorkGraphInputValid(WorkGraphInput input) noexcept {
  return input.count <= WorkGraphMaximumRecords &&
         (input.count == 0 || input.first_object_slot <= UINT32_MAX - input.count);
}
enum class QueueType : std::uint8_t { Graphics, Compute, Copy };
// Whole-buffer GPU dependency scopes. Undefined is conservative prior access, not discard.
enum class BufferAccess : std::uint8_t { Undefined, ShaderRead, StorageReadWrite, IndirectRead };
constexpr bool IsValidQueue(QueueType queue) noexcept { return queue <= QueueType::Copy; }
constexpr bool IsValidBufferAccess(BufferAccess access) noexcept {
  return access <= BufferAccess::IndirectRead;
}
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
enum class ShaderStage : std::uint8_t {
  Vertex = 1,
  Fragment = 2,
  Compute = 4,
  TessellationControl = 8,
  TessellationEvaluation = 16,
  Mesh = 32
};
inline constexpr std::uint8_t AllShaderStageBits = 63;
enum class PrimitiveTopology : std::uint8_t { TriangleList, PatchList };
// Tier1 per-draw control only; no rate-image or per-primitive combiners.
enum class ShadingRate : std::uint8_t { Full, Coarse2x2, Image };
// Built-in bootstrap shader selection; custom program assembly remains separate.
enum class GraphicsShaderVariant : std::uint8_t { Triangle, PreTessellatedPatch, MeshTriangle };
inline constexpr std::uint32_t PreTessellatedPatchVertexCount = 48;
constexpr bool IsBuiltinGeometryDrawValid(PrimitiveTopology topology, GraphicsShaderVariant shader,
                                          std::uint32_t vertices) noexcept {
  if (shader == GraphicsShaderVariant::MeshTriangle)
    return false;
  return topology == PrimitiveTopology::PatchList ? vertices == 3
         : shader == GraphicsShaderVariant::PreTessellatedPatch
             ? vertices == PreTessellatedPatchVertexCount
             : vertices != 0;
}
constexpr bool SupportsBuiltinIndirectDraw(PrimitiveTopology topology,
                                           GraphicsShaderVariant shader) noexcept {
  return topology == PrimitiveTopology::TriangleList && shader == GraphicsShaderVariant::Triangle;
}
enum class BindingType : std::uint8_t { ConstantBuffer, Texture, Sampler, StorageBuffer };
enum class IndexFormat : std::uint8_t { Uint16, Uint32 };
enum class PipelineType : std::uint8_t { Graphics, Compute };
enum class ComputeShaderVariant : std::uint8_t {
  GPUDriven,
  RayReference,
  LongVector69,
  LinearAlgebra610,
  GroupShared610,
  SamplerFeedbackReference
};

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
  PrimitiveTopology topology{PrimitiveTopology::TriangleList};
  std::uint32_t patch_control_points{};
  GraphicsShaderVariant graphics_shader{GraphicsShaderVariant::Triangle};
  ShadingRate shading_rate{ShadingRate::Full};
  ComputeShaderVariant compute_shader{ComputeShaderVariant::GPUDriven};
  // Explicit permission for experimental 6.10 programs; requires the isolated runtime too.
  bool allow_preview_shader_models{};
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
struct BufferBarrier final {
  BufferHandle buffer;
  BufferAccess before{BufferAccess::Undefined};
  BufferAccess after{BufferAccess::Undefined};
  QueueType source_queue{QueueType::Graphics};
  QueueType destination_queue{QueueType::Graphics};
};
constexpr bool IsValidResourceState(ResourceState state) noexcept {
  switch (state) {
  case ResourceState::Undefined:
  case ResourceState::CopySource:
  case ResourceState::CopyDestination:
  case ResourceState::ShaderRead:
  case ResourceState::RenderTarget:
  case ResourceState::Present:
    return true;
  }
  return false;
}
constexpr bool IsValidTextureBarrier(const Barrier &barrier) noexcept {
  return barrier.texture.IsValid() && IsValidResourceState(barrier.before) &&
         IsValidResourceState(barrier.after) && barrier.after != ResourceState::Undefined &&
         IsValidQueue(barrier.source_queue) && IsValidQueue(barrier.destination_queue);
}
constexpr bool IsValidBufferBarrier(const BufferBarrier &barrier) noexcept {
  return barrier.buffer.IsValid() && IsValidBufferAccess(barrier.before) &&
         IsValidBufferAccess(barrier.after) && barrier.after != BufferAccess::Undefined &&
         IsValidQueue(barrier.source_queue) && IsValidQueue(barrier.destination_queue);
}
} // namespace nexora::rhi
