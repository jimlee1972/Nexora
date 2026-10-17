#if !defined(_WIN32)
#error "D3D12Device.cpp is only built on Windows"
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/RHI/DxilArtifact.h"

#include "D3D12CapabilityMapping.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "D3D12PreviewRuntime.h"
#if NEXORA_ENABLE_D3D12_PREVIEW
#include "ModernShaderArtifactIdentity.h"
#endif

#include "D3D12EnhancedBarriers.h"
#include "D3D12MeshPipeline.h"
#include "D3D12RayTracing.h"
#include "D3D12WorkGraphs.h"
#if NEXORA_HAS_PINNED_RAY_ARTIFACTS
#include "RayArtifactIdentity.h"
#endif

namespace nexora::rhi {
namespace {
using Microsoft::WRL::ComPtr;

// Failure to query an optional feature leaves it Unknown, not Unsupported. In particular an old
// SDK/runtime must not turn missing declarations or E_INVALIDARG into a hardware diagnosis.
GraphicsDeviceCapabilities QueryCapabilities(ID3D12Device &device, IDXGIAdapter1 &adapter) {
  GraphicsDeviceCapabilities result;
  result.backend = Backend::Direct3D12;
  DXGI_ADAPTER_DESC1 identity{};
  if (SUCCEEDED(adapter.GetDesc1(&identity))) {
    result.software_device = (identity.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    result.vendor_id = identity.VendorId;
    result.device_id = identity.DeviceId;
  }
  LARGE_INTEGER driver{};
  if (SUCCEEDED(adapter.CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver)))
    result.driver_version = static_cast<std::uint64_t>(driver.QuadPart);
  const auto report = [&result](GraphicsFeature feature, FeatureVersion tier, bool enabled = false,
                                bool implemented = false) {
    result.features[static_cast<std::size_t>(feature)] = {
        tier == FeatureVersion{} ? CapabilitySupport::Unsupported : CapabilitySupport::Supported,
        tier, enabled, implemented};
  };
  const auto report_native_tier = [&report](GraphicsFeature feature, std::uint32_t native_tier) {
    if (const auto tier = detail::DecodeD3D12Tier(feature, native_tier))
      report(feature, *tier);
  };
  report(GraphicsFeature::Compute, {1, 0}, true, true);
  report(GraphicsFeature::IndirectDraw, {1, 0}, true, true);
  // Creation requires feature level 11_0, which provides hull/domain hardware support.
  report(GraphicsFeature::Tessellation, {1, 0}, true, true);
  report(GraphicsFeature::ParallelCommandRecording, {1, 0});
  result.independent_compute_recording = true;
  result.independent_graphics_recording = true;
  result.shared_buffer_read_recording = true;

  // Numeric query values allow older SDK builds to query a newer runtime without referencing
  // newer compiler enums. Retry lower known models when the runtime rejects a newer query.
  constexpr unsigned shader_models[]{0x6a, 0x69, 0x68, 0x67, 0x66, 0x65,
                                     0x64, 0x63, 0x62, 0x61, 0x60, 0x51};
  for (const auto requested : shader_models) {
    D3D12_FEATURE_DATA_SHADER_MODEL model{static_cast<D3D_SHADER_MODEL>(requested)};
    if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model)))) {
      const auto actual = static_cast<unsigned>(model.HighestShaderModel);
      if (actual >= 0x51 && actual <= requested) {
        result.dxil_shader_model_support =
            actual >= 0x60 ? CapabilitySupport::Supported : CapabilitySupport::Unsupported;
        if (actual >= 0x60)
          result.highest_dxil_shader_model = {static_cast<std::uint16_t>(actual >> 4U),
                                              static_cast<std::uint16_t>(actual & 0xfU)};
      }
      break;
    }
  }
#if defined(__ID3D12Device5_INTERFACE_DEFINED__)
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
  if (SUCCEEDED(
          device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)))) {
    report_native_tier(GraphicsFeature::RayTracing,
                       static_cast<std::uint32_t>(options5.RaytracingTier));
  }
#endif
#if defined(__ID3D12GraphicsCommandList5_INTERFACE_DEFINED__)
  D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6{};
  if (SUCCEEDED(
          device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
    report_native_tier(GraphicsFeature::VariableRateShading,
                       static_cast<std::uint32_t>(options6.VariableShadingRateTier));
#endif
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__)
  D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
  if (SUCCEEDED(
          device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)))) {
    report_native_tier(GraphicsFeature::MeshShader,
                       static_cast<std::uint32_t>(options7.MeshShaderTier));
    report_native_tier(GraphicsFeature::SamplerFeedback,
                       static_cast<std::uint32_t>(options7.SamplerFeedbackTier));
  }
#endif
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
  D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12{};
  if (SUCCEEDED(
          device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12))))
    report(GraphicsFeature::EnhancedBarriers,
           options12.EnhancedBarriersSupported ? FeatureVersion{1, 0} : FeatureVersion{});
#endif
#if defined(__ID3D12GraphicsCommandList10_INTERFACE_DEFINED__)
  D3D12_FEATURE_DATA_D3D12_OPTIONS21 options21{};
  if (SUCCEEDED(device.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS21, &options21,
                                           sizeof(options21)))) {
    report_native_tier(GraphicsFeature::WorkGraphs,
                       static_cast<std::uint32_t>(options21.WorkGraphsTier));
  }
#endif
  // Ray-tracing tier or SM 6.9 alone does not prove an implemented/enabled SER path.
  return result;
}

[[noreturn]] void ThrowHResult(HRESULT result, const char *operation) {
  throw std::runtime_error(std::string(operation) + " failed with HRESULT " +
                           std::to_string(static_cast<unsigned long>(result)));
}

void Check(HRESULT result, const char *operation) {
  if (FAILED(result))
    ThrowHResult(result, operation);
}

std::uint64_t Key(TextureHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

std::uint64_t Key(PipelineHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

#if NEXORA_D3D12_WORK_GRAPHS
std::uint64_t Key(WorkGraphHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}
#endif

#if NEXORA_D3D12_RAY_TRACING
std::uint64_t Key(RaySceneHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}
std::uint64_t Key(RayProgramHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}
#endif

std::uint64_t Key(BufferHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

DXGI_FORMAT ToFormat(TextureFormat format) {
  switch (format) {
  case TextureFormat::Rgba8Unorm:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case TextureFormat::Bgra8Unorm:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case TextureFormat::Depth32Float:
    return DXGI_FORMAT_D32_FLOAT;
  case TextureFormat::Rgba16Float:
    break; // External Presentation-owned HDR metadata; native triangle allocation is unsupported.
  }
  throw std::invalid_argument("unsupported D3D12 texture format");
}

D3D12_RESOURCE_STATES ToState(ResourceState state) {
  switch (state) {
  case ResourceState::Undefined:
    return D3D12_RESOURCE_STATE_COMMON;
  case ResourceState::CopySource:
    return D3D12_RESOURCE_STATE_COPY_SOURCE;
  case ResourceState::CopyDestination:
    return D3D12_RESOURCE_STATE_COPY_DEST;
  case ResourceState::ShaderRead:
    return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  case ResourceState::RenderTarget:
    return D3D12_RESOURCE_STATE_RENDER_TARGET;
  case ResourceState::Present:
    return D3D12_RESOURCE_STATE_PRESENT;
  }
  throw std::invalid_argument("unsupported D3D12 resource state");
}

ComPtr<ID3DBlob> CompileShader(const char *source, const char *entry, const char *profile) {
  ComPtr<ID3DBlob> bytecode;
  ComPtr<ID3DBlob> errors;
  constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
  const auto result = D3DCompile(source, std::strlen(source), "NexoraTriangle.hlsl", nullptr,
                                 nullptr, entry, profile, flags, 0, &bytecode, &errors);
  if (FAILED(result)) {
    const auto *message =
        errors ? static_cast<const char *>(errors->GetBufferPointer()) : "unknown shader error";
    throw std::runtime_error(std::string("D3DCompile failed: ") + message);
  }
  return bytecode;
}

ComPtr<ID3DBlob> LoadShaderArtifact(const char *environment_name, ShaderStage stage) {
  const auto *path = std::getenv(environment_name);
  if (!path || *path == '\0')
    return {};
  ComPtr<ID3DBlob> bytecode;
  Check(D3DReadFileToBlob(std::filesystem::path(path).c_str(), &bytecode), "D3DReadFileToBlob");
  if (!bytecode || bytecode->GetBufferSize() < 4 ||
      std::memcmp(bytecode->GetBufferPointer(), "DXBC", 4) != 0)
    throw std::runtime_error(std::string("DXIL artifact is not a valid DXBC container: ") + path);
  (void)RequireDxilArtifact(
      {static_cast<const std::byte *>(bytecode->GetBufferPointer()), bytecode->GetBufferSize()},
      stage);
  return bytecode;
}

constexpr char kTriangleShader[] = R"(
cbuffer FrameConstants : register(b0) {
  float4x4 transform;
};
struct VertexOutput {
  float4 position : SV_Position;
  float3 color : COLOR0;
};
VertexOutput vsMain(uint id : SV_VertexID) {
  const float2 positions[3] = {
    float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5)
  };
  const float3 colors[3] = {
    float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0)
  };
  VertexOutput output;
  output.position = mul(transform, float4(positions[id], 0.0, 1.0));
  output.color = colors[id];
  return output;
}
float4 psMain(VertexOutput input) : SV_Target {
  return float4(input.color, 1.0);
}
)";

class D3D12Device;

class D3D12CommandList final : public CommandList {
public:
  D3D12CommandList(D3D12Device &device, QueueType queue);
  ~D3D12CommandList() override = default;

  void Transition(const Barrier &barrier) override;
  void TransitionBuffer(const BufferBarrier &barrier) override;
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) override;
  void BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset, std::uint32_t stride) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void Dispatch(std::uint32_t group_count_x, std::uint32_t group_count_y,
                std::uint32_t group_count_z) override;
  void DrawIndirect(std::uint32_t draw_count) override;
  void DispatchMesh(std::uint32_t x, std::uint32_t y, std::uint32_t z) override;
#if NEXORA_D3D12_WORK_GRAPHS
  void BindWorkGraph(WorkGraphHandle graph) override;
  void DispatchWorkGraph(WorkGraphInput input) override;
#endif
  [[nodiscard]] std::uint64_t WorkGraphDispatches() const noexcept {
#if NEXORA_D3D12_WORK_GRAPHS
    return work_graph_dispatches_;
#else
    return 0;
#endif
  }
#if NEXORA_D3D12_RAY_TRACING
  void TraceRays(RayProgramHandle, RaySceneHandle, BufferHandle, BufferHandle,
                 std::uint32_t) override;
#endif
  void EndRendering() override;

  [[nodiscard]] bool IsClosed() const noexcept { return !rendering_; }
  [[nodiscard]] bool IsSubmitted() const noexcept { return submitted_; }
  [[nodiscard]] bool BelongsTo(const D3D12Device &device) const noexcept {
    return &device_ == &device;
  }
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  [[nodiscard]] std::uint64_t Dispatches() const noexcept { return dispatches_; }
  [[nodiscard]] std::uint64_t IndirectDraws() const noexcept { return indirect_draws_; }
  void MarkSubmitted() noexcept { submitted_ = true; }
  void Close();

private:
  friend class D3D12Device;
  struct BufferSnapshot final {
    BufferHandle handle;
    std::uint64_t revision{};
    D3D12_RESOURCE_STATES state{};
    bool upload_pending{};
    bool writes{};
  };
  ID3D12Resource *PrepareBuffer(BufferHandle buffer, D3D12_RESOURCE_STATES state);
  struct TextureSnapshot final {
    TextureHandle handle;
    std::uint64_t revision;
    ResourceState state;
    TextureFormat format;
    std::uint32_t width;
    std::uint32_t height;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
  };
  TextureSnapshot &SnapshotTexture(TextureHandle texture);
  D3D12Device &device_;
  std::unordered_map<std::uint64_t, TextureSnapshot> texture_snapshots_;
  TextureHandle render_target_{};
  std::unordered_map<std::uint64_t, BufferSnapshot> buffer_snapshots_;
  std::optional<double>
  GpuMilliseconds(std::chrono::steady_clock::time_point submitted) const noexcept;
#if NEXORA_D3D12_WORK_GRAPHS
  struct WorkGraphSnapshot final {
    WorkGraphHandle handle;
    detail::NativeWorkGraph program;
  };
  // These references retire after every native command-list interface, even on abandonment.
  std::array<WorkGraphSnapshot, detail::WorkGraphListCapacity> work_graph_snapshots_{};
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, detail::WorkGraphListCapacity>
      work_graph_backing_{};
  std::size_t work_graph_snapshot_count_{};
  std::size_t bound_work_graph_{}; // One-based private snapshot index, zero is unbound.
  std::uint64_t work_graph_dispatches_{};
#endif
#if NEXORA_D3D12_RAY_TRACING
  struct RaySnapshot final {
    RayProgramHandle program_handle;
    RaySceneHandle scene_handle;
    detail::NativeRayProgram program;
    detail::NativeRayScene scene;
  };
  std::array<RaySnapshot, RayMaximumDispatchesPerList> ray_snapshots_{};
  std::size_t ray_snapshot_count_{};
  std::uint64_t ray_dispatches_{}, inline_ray_dispatches_{};
#endif
  Microsoft::WRL::ComPtr<ID3D12QueryHeap> timestamp_heap_;
  Microsoft::WRL::ComPtr<ID3D12Resource> timestamp_readback_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
#if NEXORA_D3D12_RAY_TRACING
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> ray_list_;
#endif
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> enhanced_list_;
#endif
  std::uint64_t enhanced_buffer_barriers_{};
  std::uint64_t enhanced_texture_barriers_{};

#if NEXORA_D3D12_WORK_GRAPHS
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> graph_list_;
#endif
#if defined(__ID3D12GraphicsCommandList5_INTERFACE_DEFINED__)
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList5> shading_list_;
#endif
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> mesh_list_;
#endif
  BufferHandle mesh_input_{};
  std::uint64_t mesh_dispatches_{};
  ShadingRate shading_rate_{ShadingRate::Full};
  std::uint64_t coarse_shading_draws_{};
  bool rendering_{false};
  bool pipeline_bound_{false};
  PipelineType pipeline_type_{PipelineType::Graphics};
  ComputeShaderVariant compute_shader_{ComputeShaderVariant::GPUDriven};
  PrimitiveTopology topology_{PrimitiveTopology::TriangleList};
  GraphicsShaderVariant graphics_shader_{GraphicsShaderVariant::Triangle};
  bool submitted_{false};
  bool closed_{false};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
  std::uint64_t dispatches_{};
  std::uint64_t indirect_draws_{};
  ID3D12Resource *indirect_buffer_{};
  std::uint64_t indirect_offset_{};
  std::uint64_t indirect_size_{};
  std::uint32_t indirect_stride_{};
  std::array<D3D12_GPU_VIRTUAL_ADDRESS, 4> storage_buffers_{};
  std::array<bool, 4> storage_bound_{};
  std::array<BufferHandle, 4> storage_handles_{};
};

class D3D12Device final : public Device {
  struct TextureRecord;
  struct PipelineRecord;
  struct BufferRecord;

public:
  explicit D3D12Device(const DeviceCreationOptions &options) {
    ComPtr<IDXGIAdapter1> selected;
    if (options.enable_d3d12_preview_runtime) {
#if NEXORA_ENABLE_D3D12_PREVIEW
      preview_runtime_.Create(device_, selected);
#else
      throw std::invalid_argument("D3D12 preview runtime was not compiled in");
#endif
    } else {
      ComPtr<IDXGIFactory6> factory;
      Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

      for (UINT index = 0;
           factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                               IID_PPV_ARGS(&selected)) != DXGI_ERROR_NOT_FOUND;
           ++index) {
        DXGI_ADAPTER_DESC1 description{};
        Check(selected->GetDesc1(&description), "IDXGIAdapter1::GetDesc1");
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
          continue;
        if (SUCCEEDED(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0,
                                        IID_PPV_ARGS(&device_)))) {
          break;
        }
        selected.Reset();
      }
      if (!device_) {
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&selected)), "EnumWarpAdapter");
        Check(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)),
              "D3D12CreateDevice(WARP)");
      }
    }

    capabilities_ = QueryCapabilities(*device_.Get(), *selected.Get());
#if NEXORA_ENABLE_D3D12_PREVIEW
    capabilities_.d3d12_preview_runtime_enabled = options.enable_d3d12_preview_runtime;
    D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW preview{};
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS_PREVIEW, &preview,
                                               sizeof(preview)))) {
      capabilities_.compute_shared_memory_support = CapabilitySupport::Supported;
      capabilities_.maximum_compute_shared_memory_bytes = preview.MaxGroupSharedMemoryPerGroupCS;
    }
    D3D12_FEATURE_DATA_LINEAR_ALGEBRA_SUPPORT algebra{};
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_LINEAR_ALGEBRA_SUPPORT, &algebra,
                                               sizeof(algebra)))) {
      if (algebra.LinearAlgebraTier == D3D12_LINEAR_ALGEBRA_TIER_NOT_SUPPORTED)
        capabilities_.linear_algebra_support = CapabilitySupport::Unsupported;
      else if (algebra.LinearAlgebraTier == D3D12_LINEAR_ALGEBRA_TIER_1_0)
        capabilities_.linear_algebra_support = CapabilitySupport::Supported;
    }
    D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT operation{};
    operation.OperationType = D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREAD_VECTOR_MATRIX_MULTIPLY;
    operation.ThreadVectorMatrixMultiply = {
        D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32, D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32,
        D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32, D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32,
        D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_NONE};
    if (SUCCEEDED(device_->CheckFeatureSupport(
            D3D12_FEATURE_LINEAR_ALGEBRA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT, &operation,
            sizeof(operation))))
      capabilities_.linear_algebra_f32_matvec_support =
          (operation.ThreadVectorMatrixMultiply.SupportFlags &
           D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED)
              ? CapabilitySupport::Supported
              : CapabilitySupport::Unsupported;
#endif
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
    auto &enhanced =
        capabilities_.features[static_cast<std::size_t>(GraphicsFeature::EnhancedBarriers)];
    enhanced.implemented = true;
    if (options.enable_enhanced_barriers && enhanced.support == CapabilitySupport::Supported) {
      ComPtr<ID3D12CommandAllocator> allocator;
      ComPtr<ID3D12GraphicsCommandList> probe;
      ComPtr<ID3D12GraphicsCommandList7> enhanced_interface;
      if (SUCCEEDED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&allocator))) &&
          SUCCEEDED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                               nullptr, IID_PPV_ARGS(&probe))) &&
          SUCCEEDED(probe.As(&enhanced_interface))) {
        enhanced.enabled = SUCCEEDED(probe->Close());
      }
    }
#endif

#if defined(__ID3D12GraphicsCommandList5_INTERFACE_DEFINED__)
    auto &vrs =
        capabilities_.features[static_cast<std::size_t>(GraphicsFeature::VariableRateShading)];
    vrs.implemented = true;
    vrs.implemented_tier = {1, 0};
    if (vrs.support == CapabilitySupport::Supported) {
      ComPtr<ID3D12CommandAllocator> allocator;
      ComPtr<ID3D12GraphicsCommandList> probe;
      ComPtr<ID3D12GraphicsCommandList5> shading_interface;
      if (SUCCEEDED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&allocator))) &&
          SUCCEEDED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                               nullptr, IID_PPV_ARGS(&probe))) &&
          SUCCEEDED(probe.As(&shading_interface)))
        vrs.enabled = SUCCEEDED(probe->Close());
    }
#endif
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
    auto &mesh = capabilities_.features[static_cast<std::size_t>(GraphicsFeature::MeshShader)];
    mesh.implemented = true;
    mesh.implemented_tier = {1, 0};
    if (mesh.support == CapabilitySupport::Supported && SUCCEEDED(device_.As(&mesh_device_))) {
      ComPtr<ID3D12CommandAllocator> allocator;
      ComPtr<ID3D12GraphicsCommandList> probe;
      ComPtr<ID3D12GraphicsCommandList6> mesh_interface;
      if (SUCCEEDED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&allocator))) &&
          SUCCEEDED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                               nullptr, IID_PPV_ARGS(&probe))) &&
          SUCCEEDED(probe.As(&mesh_interface)))
        mesh.enabled = SUCCEEDED(probe->Close());
    }
#endif
#if NEXORA_D3D12_WORK_GRAPHS
    auto &work_graph =
        capabilities_.features[static_cast<std::size_t>(GraphicsFeature::WorkGraphs)];
    work_graph.implemented = true;
    work_graph.implemented_tier = {1, 0};
    if (work_graph.support == CapabilitySupport::Supported &&
        capabilities_.highest_dxil_shader_model >= FeatureVersion{6, 8} &&
        SUCCEEDED(device_.As(&graph_device_))) {
      ComPtr<ID3D12CommandAllocator> allocator;
      ComPtr<ID3D12GraphicsCommandList> probe;
      ComPtr<ID3D12GraphicsCommandList10> graph_interface;
      if (SUCCEEDED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&allocator))) &&
          SUCCEEDED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                               nullptr, IID_PPV_ARGS(&probe))) &&
          SUCCEEDED(probe.As(&graph_interface)))
        work_graph.enabled = SUCCEEDED(probe->Close());
    }
#endif
#if NEXORA_D3D12_RAY_TRACING && NEXORA_HAS_PINNED_RAY_ARTIFACTS
    auto &ray = capabilities_.features[static_cast<std::size_t>(GraphicsFeature::RayTracing)];
    ray.implemented = true;
    ray.implemented_tier = {1, 1};
    if (ray.support == CapabilitySupport::Supported &&
        capabilities_.highest_dxil_shader_model >= FeatureVersion{6, 3} &&
        SUCCEEDED(device_.As(&ray_device_))) {
      ComPtr<ID3D12CommandAllocator> allocator;
      ComPtr<ID3D12GraphicsCommandList4> probe;
      if (SUCCEEDED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&allocator))) &&
          SUCCEEDED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                               nullptr, IID_PPV_ARGS(&probe))))
        ray.enabled = SUCCEEDED(probe->Close());
    }
#endif
    D3D12_COMMAND_QUEUE_DESC queue_description{};
    queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(device_->CreateCommandQueue(&queue_description, IID_PPV_ARGS(&queue_)),
          "CreateCommandQueue");
    if (SUCCEEDED(queue_->GetTimestampFrequency(&timestamp_frequency_)) && timestamp_frequency_)
      capabilities_.gpu_timing_support = CapabilitySupport::Supported;
    capabilities_.gpu_timing_enabled =
        options.enable_gpu_timing &&
        capabilities_.gpu_timing_support == CapabilitySupport::Supported;

    D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
    heap_description.NumDescriptors = RtvCapacity;
    free_rtv_slots_.reserve(RtvCapacity);
    heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    Check(device_->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&rtv_heap_)),
          "CreateDescriptorHeap");
    rtv_increment_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");

    CreateRootSignature();
    CreateFrameConstants();
    D3D12_INDIRECT_ARGUMENT_DESC indirect_argument{};
    indirect_argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC signature{};
    signature.ByteStride = GPUDrivenIndirectCommandStride;
    signature.NumArgumentDescs = 1;
    signature.pArgumentDescs = &indirect_argument;
    Check(device_->CreateCommandSignature(&signature, nullptr,
                                          IID_PPV_ARGS(&draw_indirect_signature_)),
          "CreateCommandSignature(draw indirect)");
    vertex_shader_ = LoadShaderArtifact("NEXORA_SLANG_DXIL_VERTEX_PATH", ShaderStage::Vertex);
    pixel_shader_ = LoadShaderArtifact("NEXORA_SLANG_DXIL_FRAGMENT_PATH", ShaderStage::Fragment);
    compute_shader_ = LoadShaderArtifact("NEXORA_SLANG_COMPUTE_DXIL_PATH", ShaderStage::Compute);
    if (!vertex_shader_ || !pixel_shader_) {
      if (vertex_shader_ || pixel_shader_)
        throw std::runtime_error("both Slang DXIL entry-point artifacts are required");
      vertex_shader_ = CompileShader(kTriangleShader, "vsMain", "vs_5_0");
      pixel_shader_ = CompileShader(kTriangleShader, "psMain", "ps_5_0");
    }
    // Create the raw event last: constructor failures (including artifact rejection) must not
    // leak a handle, since the device destructor does not run for a failed constructor.
    fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_)
      throw std::runtime_error("CreateEventW failed");
  }

  ~D3D12Device() override {
    try {
      WaitIdle();
    } catch (...) {
    }
    if (fence_event_)
      CloseHandle(fence_event_);
  }

  Backend GetBackend() const noexcept override { return Backend::Direct3D12; }
  GraphicsDeviceCapabilities Capabilities() const noexcept override { return capabilities_; }

  TextureHandle CreateTexture(const TextureDescriptor &descriptor) override {
    if (descriptor.width == 0 || descriptor.height == 0)
      throw std::invalid_argument("invalid texture extent");
    const auto format = ToFormat(descriptor.format);
    if (descriptor.format == TextureFormat::Depth32Float)
      throw std::invalid_argument("D3D12 triangle backend only supports color textures");

    D3D12_RESOURCE_DESC resource_description{};
    resource_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_description.Width = descriptor.width;
    resource_description.Height = descriptor.height;
    resource_description.DepthOrArraySize = 1;
    resource_description.MipLevels = 1;
    resource_description.Format = format;
    resource_description.SampleDesc.Count = 1;
    resource_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = format;
    clear.Color[3] = 1.0F;

    UINT slot{};
    {
      std::lock_guard lock{mutex_};
      Require(!free_rtv_slots_.empty() || next_rtv_ < RtvCapacity,
              "D3D12 RTV descriptor heap exhausted");
      if (free_rtv_slots_.empty())
        slot = next_rtv_++;
      else {
        slot = free_rtv_slots_.back();
        free_rtv_slots_.pop_back();
      }
    }
    try {
      TextureRecord record;
      Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resource_description,
                                             ToState(descriptor.initial_state), &clear,
                                             IID_PPV_ARGS(&record.resource)),
            "CreateCommittedResource");
      record.descriptor = descriptor;
      record.logical_state = descriptor.initial_state;
      record.rtv_slot = slot;
      record.rtv.ptr = rtv_heap_->GetCPUDescriptorHandleForHeapStart().ptr +
                       static_cast<SIZE_T>(slot) * rtv_increment_;
      D3D12_RENDER_TARGET_VIEW_DESC rtv_description{};
      rtv_description.Format = format;
      rtv_description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      device_->CreateRenderTargetView(record.resource.Get(), &rtv_description, record.rtv);
      std::lock_guard lock{mutex_};
      const auto handle = texture_pool_.Create();
      try {
        textures_.emplace(Key(handle), std::move(record));
      } catch (...) {
        (void)texture_pool_.Destroy(handle);
        throw;
      }
      return handle;
    } catch (...) {
      std::lock_guard lock{mutex_};
      free_rtv_slots_.push_back(slot); // Capacity was reserved during construction.
      throw;
    }
  }

  void ReadTextureForTesting(TextureHandle texture, std::span<std::byte> data) override;

  void DestroyTexture(TextureHandle texture) override {
    std::lock_guard lock{mutex_};
    Require(texture_pool_.Contains(texture) && textures_.contains(Key(texture)),
            "destroying invalid D3D12 texture");
    free_rtv_slots_.push_back(textures_.at(Key(texture)).rtv_slot);
    textures_.erase(Key(texture));
    Require(texture_pool_.Destroy(texture), "destroying stale D3D12 texture");
  }

  BufferHandle CreateBuffer(const BufferDescriptor &descriptor) override {
    if (descriptor.size == 0)
      throw std::invalid_argument("D3D12 buffer size must be non-zero");
    D3D12_RESOURCE_DESC resource{};
    resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource.Width = descriptor.size;
    resource.Height = 1;
    resource.DepthOrArraySize = 1;
    resource.MipLevels = 1;
    resource.SampleDesc.Count = 1;
    resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    BufferRecord record;
    record.descriptor = descriptor;
    Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resource,
                                           D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                           IID_PPV_ARGS(&record.resource)),
          "CreateCommittedResource(buffer)");
    auto gpu_resource_description = resource;
    gpu_resource_description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto gpu_heap = heap;
    gpu_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    Check(device_->CreateCommittedResource(
              &gpu_heap, D3D12_HEAP_FLAG_NONE, &gpu_resource_description,
              D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&record.gpu_resource)),
          "CreateCommittedResource(buffer GPU)");
    std::lock_guard lock{mutex_};
    const auto handle = buffer_pool_.Create();
    buffers_.emplace(Key(handle), std::move(record));
    return handle;
  }

  void WriteBuffer(BufferHandle buffer, std::uint64_t offset,
                   std::span<const std::byte> data) override {
    std::lock_guard lock{mutex_};
    auto &record = ValidateBufferLocked(buffer);
    Require(offset <= record.descriptor.size && data.size() <= record.descriptor.size - offset,
            "D3D12 buffer upload exceeds allocation");
    void *mapped{};
    D3D12_RANGE read_range{0, 0};
    Check(record.resource->Map(0, &read_range, &mapped), "Map(buffer)");
    std::memcpy(static_cast<std::byte *>(mapped) + offset, data.data(), data.size());
    const D3D12_RANGE written_range{static_cast<SIZE_T>(offset),
                                    static_cast<SIZE_T>(offset + data.size())};
    record.resource->Unmap(0, &written_range);
    record.upload_dirty = true;
    ++record.revision;
  }

  void ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                            std::span<std::byte> data) override;

  void DestroyBuffer(BufferHandle buffer) override {
    std::lock_guard lock{mutex_};
    Require(buffer_pool_.Contains(buffer) && buffers_.contains(Key(buffer)),
            "destroying invalid D3D12 buffer");
    buffers_.erase(Key(buffer));
    Require(buffer_pool_.Destroy(buffer), "destroying stale D3D12 buffer");
  }

  PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) override {
    RequirePipelineCapabilities(Capabilities(), descriptor);
    if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0)
      throw std::invalid_argument("pipeline hashes must be non-zero");
    const bool patch = descriptor.topology == PrimitiveTopology::PatchList;
    ComPtr<ID3DBlob> selected_vertex = vertex_shader_;
    ComPtr<ID3DBlob> selected_compute = compute_shader_;
    ComPtr<ID3DBlob> control, evaluation, mesh;
    const auto required_artifact = [](const char *environment, ShaderStage stage) {
      auto artifact = LoadShaderArtifact(environment, stage);
      if (!artifact)
        throw std::runtime_error(std::string("required geometry DXIL artifact is unset: ") +
                                 environment);
      return artifact;
    };
    if (descriptor.compute_shader >= ComputeShaderVariant::LongVector69) {
#if NEXORA_ENABLE_D3D12_PREVIEW
      if (descriptor.compute_shader != ComputeShaderVariant::LongVector69 &&
          !capabilities_.d3d12_preview_runtime_enabled)
        throw std::invalid_argument("Shader Model 6.10 requires the isolated preview runtime");
      if (descriptor.compute_shader == ComputeShaderVariant::LinearAlgebra610 &&
          (capabilities_.linear_algebra_support != CapabilitySupport::Supported ||
           capabilities_.linear_algebra_f32_matvec_support != CapabilitySupport::Supported))
        throw std::invalid_argument("F32 thread matrix-vector operation is unavailable");
      if (descriptor.compute_shader == ComputeShaderVariant::GroupShared610 &&
          (capabilities_.compute_shared_memory_support != CapabilitySupport::Supported ||
           capabilities_.maximum_compute_shared_memory_bytes < 65536))
        throw std::invalid_argument("64 KiB compute shared memory is unavailable");
      const auto kind = static_cast<std::size_t>(descriptor.compute_shader) -
                        static_cast<std::size_t>(ComputeShaderVariant::LongVector69);
      const char *environments[]{"NEXORA_MODERN_LONG_VECTOR_DXIL_PATH",
                                 "NEXORA_MODERN_LINEAR_ALGEBRA_DXIL_PATH",
                                 "NEXORA_MODERN_GROUP_SHARED_DXIL_PATH"};
      selected_compute = required_artifact(environments[kind], ShaderStage::Compute);
      (void)RequireDxilArtifact(
          {static_cast<const std::byte *>(selected_compute->GetBufferPointer()),
           selected_compute->GetBufferSize()},
          ShaderStage::Compute,
          descriptor.compute_shader == ComputeShaderVariant::LongVector69 ? FeatureVersion{6, 9}
                                                                          : FeatureVersion{6, 10});
      const auto expected = detail::ModernShaderBytes(kind);
      if (selected_compute->GetBufferSize() != expected.size() ||
          std::memcmp(selected_compute->GetBufferPointer(), expected.data(), expected.size()) != 0)
        throw std::invalid_argument("modern shader artifact does not match the pinned recipe");
#else
      throw std::invalid_argument("modern shader programs were not compiled in");
#endif
    } else if (descriptor.compute_shader == ComputeShaderVariant::RayReference) {
      selected_compute =
          required_artifact("NEXORA_SLANG_RAY_REFERENCE_DXIL_PATH", ShaderStage::Compute);
    }
    if (descriptor.graphics_shader == GraphicsShaderVariant::PreTessellatedPatch)
      selected_vertex =
          required_artifact("NEXORA_SLANG_PRETESSELLATED_DXIL_PATH", ShaderStage::Vertex);
    const bool mesh_pipeline = descriptor.graphics_shader == GraphicsShaderVariant::MeshTriangle;
    if (mesh_pipeline) {
      mesh = required_artifact("NEXORA_SLANG_MESH_DXIL_PATH", ShaderStage::Mesh);
      (void)RequireDxilArtifact(
          {static_cast<const std::byte *>(mesh->GetBufferPointer()), mesh->GetBufferSize()},
          ShaderStage::Mesh, {6, 5});
      // Native mesh PSOs require DXIL for every attached stage; FXC fallback is insufficient.
      (void)RequireDxilArtifact({static_cast<const std::byte *>(pixel_shader_->GetBufferPointer()),
                                 pixel_shader_->GetBufferSize()},
                                ShaderStage::Fragment, {6, 0});
    }
    if (patch) {
      control = required_artifact("NEXORA_SLANG_TESSELLATION_CONTROL_DXIL_PATH",
                                  ShaderStage::TessellationControl);
      evaluation = required_artifact("NEXORA_SLANG_TESSELLATION_EVALUATION_DXIL_PATH",
                                     ShaderStage::TessellationEvaluation);
    }
    const auto minimum_model = descriptor.graphics_requirements.minimum_dxil_shader_model;
    if (minimum_model != FeatureVersion{}) {
      const auto check = [&](ID3DBlob *artifact, ShaderStage stage) {
        const auto bytes = artifact ? std::span<const std::byte>{static_cast<const std::byte *>(
                                                                     artifact->GetBufferPointer()),
                                                                 artifact->GetBufferSize()}
                                    : std::span<const std::byte>{};
        (void)RequireDxilArtifact(bytes, stage, minimum_model);
      };
      if (descriptor.type == PipelineType::Compute)
        check(selected_compute.Get(), ShaderStage::Compute);
      else {
        if (mesh_pipeline)
          check(mesh.Get(), ShaderStage::Mesh);
        else
          check(selected_vertex.Get(), ShaderStage::Vertex);
        if (patch) {
          check(control.Get(), ShaderStage::TessellationControl);
          check(evaluation.Get(), ShaderStage::TessellationEvaluation);
        }
        check(pixel_shader_.Get(), ShaderStage::Fragment);
      }
    }
    PipelineRecord record;
    record.type = descriptor.type;
    record.format = descriptor.color_format;
    record.topology = descriptor.topology;
    record.graphics_shader = descriptor.graphics_shader;
    record.shading_rate = descriptor.shading_rate;
    record.compute_shader = descriptor.compute_shader;
    if (descriptor.type == PipelineType::Compute) {
      if (!selected_compute)
        throw std::runtime_error("D3D12 compute pipeline requires Slang DXIL");
      D3D12_COMPUTE_PIPELINE_STATE_DESC compute_description{};
      compute_description.pRootSignature = root_signature_.Get();
      compute_description.CS = {selected_compute->GetBufferPointer(),
                                selected_compute->GetBufferSize()};
      Check(
          device_->CreateComputePipelineState(&compute_description, IID_PPV_ARGS(&record.pipeline)),
          "CreateComputePipelineState");
      std::lock_guard lock{mutex_};
      const auto handle = pipeline_pool_.Create();
      pipelines_.emplace(Key(handle), std::move(record));
      return handle;
    }
    if (mesh_pipeline) {
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
      Check(detail::CreateMeshPipeline(
                *mesh_device_.Get(), root_signature_.Get(),
                {mesh->GetBufferPointer(), mesh->GetBufferSize()},
                {pixel_shader_->GetBufferPointer(), pixel_shader_->GetBufferSize()},
                ToFormat(descriptor.color_format), record.pipeline.GetAddressOf()),
            "CreatePipelineState(mesh)");
      std::lock_guard lock{mutex_};
      const auto handle = pipeline_pool_.Create();
      try {
        pipelines_.emplace(Key(handle), std::move(record));
      } catch (...) {
        (void)pipeline_pool_.Destroy(handle);
        throw;
      }
      return handle;
#else
      throw std::invalid_argument("native mesh pipeline requires a newer D3D12 SDK");
#endif
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_description{};
    pipeline_description.pRootSignature = root_signature_.Get();
    pipeline_description.VS = {selected_vertex->GetBufferPointer(),
                               selected_vertex->GetBufferSize()};
    if (patch) {
      pipeline_description.HS = {control->GetBufferPointer(), control->GetBufferSize()};
      pipeline_description.DS = {evaluation->GetBufferPointer(), evaluation->GetBufferSize()};
    }
    pipeline_description.PS = {pixel_shader_->GetBufferPointer(), pixel_shader_->GetBufferSize()};
    pipeline_description.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline_description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline_description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline_description.RasterizerState.DepthClipEnable = TRUE;
    pipeline_description.DepthStencilState.DepthEnable = FALSE;
    pipeline_description.DepthStencilState.StencilEnable = FALSE;
    pipeline_description.SampleMask = std::numeric_limits<UINT>::max();
    pipeline_description.PrimitiveTopologyType =
        patch ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_description.NumRenderTargets = 1;
    pipeline_description.RTVFormats[0] = ToFormat(descriptor.color_format);
    pipeline_description.SampleDesc.Count = 1;

    Check(
        device_->CreateGraphicsPipelineState(&pipeline_description, IID_PPV_ARGS(&record.pipeline)),
        "CreateGraphicsPipelineState");
    std::lock_guard lock{mutex_};
    const auto handle = pipeline_pool_.Create();
    pipelines_.emplace(Key(handle), std::move(record));
    return handle;
  }

  void DestroyPipeline(PipelineHandle pipeline) override {
    std::lock_guard lock{mutex_};
    Require(pipeline_pool_.Contains(pipeline) && pipelines_.contains(Key(pipeline)),
            "destroying invalid D3D12 pipeline");
    pipelines_.erase(Key(pipeline));
    Require(pipeline_pool_.Destroy(pipeline), "destroying stale D3D12 pipeline");
  }

#if NEXORA_D3D12_WORK_GRAPHS
  WorkGraphHandle CreateWorkGraph(const WorkGraphDescriptor &descriptor) override {
    const auto requirements = EffectiveWorkGraphRequirements(descriptor);
    RequireGraphicsCapabilities(Capabilities(), requirements);
    const auto *path = std::getenv("NEXORA_D3D12_WORK_GRAPH_DXIL_PATH");
    if (!path || !*path)
      throw std::runtime_error("required Work Graph DXIL library is unset");
    const auto size = std::filesystem::file_size(std::filesystem::path(path));
    if (size == 0 || size > 16ULL * 1024 * 1024)
      throw std::invalid_argument("Work Graph library exceeds the admitted size range");
    ComPtr<ID3DBlob> library;
    Check(D3DReadFileToBlob(std::filesystem::path(path).c_str(), &library),
          "Read Work Graph library");
    const auto bytes = std::span<const std::byte>{
        static_cast<const std::byte *>(library->GetBufferPointer()), library->GetBufferSize()};
    const auto info = RequireDxilLibrary(bytes, requirements.minimum_dxil_shader_model);
    if (info.shader_model > capabilities_.highest_dxil_shader_model)
      throw std::invalid_argument("selected Work Graph library profile exceeds the device limit");
    auto program = detail::CreateNativeWorkGraph(*graph_device_.Get(), bytes);
    std::lock_guard lock{mutex_};
    const auto handle = work_graph_pool_.Create();
    try {
      work_graphs_.emplace(Key(handle), std::move(program));
    } catch (...) {
      (void)work_graph_pool_.Destroy(handle);
      throw;
    }
    return handle;
  }
  void DestroyWorkGraph(WorkGraphHandle graph) override {
    std::lock_guard lock{mutex_};
    Require(work_graph_pool_.Contains(graph) && work_graphs_.contains(Key(graph)),
            "destroying invalid Work Graph program");
    work_graphs_.erase(Key(graph));
    Require(work_graph_pool_.Destroy(graph), "destroying stale Work Graph program");
  }
#endif
#if NEXORA_D3D12_RAY_TRACING && NEXORA_HAS_PINNED_RAY_ARTIFACTS
  RaySceneHandle CreateRayScene(std::span<const RayTriangle> triangles) override {
    RequireGraphicsCapabilities(Capabilities(), EffectiveRayProgramRequirements({}));
    if (!IsRaySceneValid(triangles))
      throw std::invalid_argument("invalid bounded ray triangles");
    // Immutable construction is caller-serialized with submit/mutation, like other resource
    // creation.
    std::lock_guard lock{mutex_};
    auto build = detail::BuildNativeRayScene(*ray_device_.Get(), triangles);
    Check(build.commands->Close(), "Close ray build list");
    ID3D12CommandList *commands = build.commands.Get();
    queue_->ExecuteCommandLists(1, &commands);
    const auto completion = ++next_fence_;
    Check(queue_->Signal(fence_.Get(), completion), "Signal ray build");
    WaitForFence(completion);
    const auto handle = ray_scene_pool_.Create();
    try {
      ray_scenes_.emplace(Key(handle), std::move(build.scene));
    } catch (...) {
      (void)ray_scene_pool_.Destroy(handle);
      throw;
    }
    return handle;
  }
  void DestroyRayScene(RaySceneHandle scene) override {
    std::lock_guard lock{mutex_};
    Require(ray_scene_pool_.Contains(scene) && ray_scenes_.contains(Key(scene)),
            "destroying stale ray scene");
    ray_scenes_.erase(Key(scene));
    Require(ray_scene_pool_.Destroy(scene), "destroying stale ray scene generation");
  }
  RayProgramHandle CreateRayProgram(const RayProgramDescriptor &descriptor) override {
    const auto requirements = EffectiveRayProgramRequirements(descriptor);
    RequireGraphicsCapabilities(Capabilities(), requirements);
    const auto *environment = descriptor.mode == RayTracingMode::Pipeline
                                  ? "NEXORA_SLANG_RAY_LIBRARY_DXIL_PATH"
                                  : "NEXORA_SLANG_RAY_QUERY_DXIL_PATH";
    const auto *path = std::getenv(environment);
    if (!path || !*path)
      throw std::runtime_error("required ray artifact is unset");
    const auto size = std::filesystem::file_size(std::filesystem::path(path));
    if (!size || size > 16ULL * 1024 * 1024)
      throw std::invalid_argument("ray artifact exceeds bounded size");
    ComPtr<ID3DBlob> artifact;
    Check(D3DReadFileToBlob(std::filesystem::path(path).c_str(), &artifact), "Read ray artifact");
    const auto bytes = std::span<const std::byte>{
        static_cast<const std::byte *>(artifact->GetBufferPointer()), artifact->GetBufferSize()};
    const auto model =
        descriptor.mode == RayTracingMode::Pipeline
            ? RequireDxilLibrary(bytes, requirements.minimum_dxil_shader_model).shader_model
            : RequireDxilArtifact(bytes, ShaderStage::Compute,
                                  requirements.minimum_dxil_shader_model)
                  .shader_model;
    if (model > capabilities_.highest_dxil_shader_model)
      throw std::invalid_argument("ray artifact exceeds device shader profile");
#if NEXORA_HAS_PINNED_RAY_ARTIFACTS
    const auto expected = descriptor.mode == RayTracingMode::Pipeline
                              ? std::span<const unsigned char>{detail::RayLibraryBytes}
                              : std::span<const unsigned char>{detail::RayQueryBytes};
    if (bytes.size() != expected.size() ||
        std::memcmp(bytes.data(), expected.data(), bytes.size()) != 0)
      throw std::invalid_argument("ray recipe identity does not match the reflected built-in ABI");
#else
    throw std::invalid_argument("ray recipe was not built with pinned Slang artifacts");
#endif
    auto program = detail::CreateNativeRayProgram(*ray_device_.Get(), descriptor.mode, bytes);
    std::lock_guard lock{mutex_};
    const auto handle = ray_program_pool_.Create();
    try {
      ray_programs_.emplace(Key(handle), std::move(program));
    } catch (...) {
      (void)ray_program_pool_.Destroy(handle);
      throw;
    }
    return handle;
  }
  void DestroyRayProgram(RayProgramHandle program) override {
    std::lock_guard lock{mutex_};
    Require(ray_program_pool_.Contains(program) && ray_programs_.contains(Key(program)),
            "destroying stale ray program");
    ray_programs_.erase(Key(program));
    Require(ray_program_pool_.Destroy(program), "destroying stale ray program generation");
  }
#endif
  std::unique_ptr<CommandList> CreateCommandList(QueueType queue) override {
    if (queue == QueueType::Copy)
      throw std::invalid_argument("D3D12 backend does not expose a copy command list");
    return std::make_unique<D3D12CommandList>(*this, queue);
  }

  std::uint64_t Submit(CommandList &commands) override {
    auto *validated = dynamic_cast<D3D12CommandList *>(&commands);
    std::uint64_t fence_value{};
    std::chrono::steady_clock::time_point submitted_at{};
    {
      std::lock_guard lock{mutex_};
      Require(validated != nullptr, "command list belongs to another device");
      Require(validated->BelongsTo(*this), "command list belongs to another device");
      Require(validated->IsClosed(), "cannot submit an open command list");
      Require(!validated->IsSubmitted(), "command list was already submitted");
      // Validate every snapshot before Close or native execution. Recording/abandoning a list
      // never reserves a resource or commits upload/state changes on the device.
      for (const auto &entry : validated->buffer_snapshots_) {
        const auto &snapshot = entry.second;
        const auto &record = ValidateBufferLocked(snapshot.handle);
        Require(record.revision == snapshot.revision, "stale D3D12 buffer recording snapshot");
      }
      for (const auto &entry : validated->texture_snapshots_) {
        const auto &snapshot = entry.second;
        Require(ValidateTextureLocked(snapshot.handle).revision == snapshot.revision,
                "stale D3D12 texture recording snapshot");
      }
#if NEXORA_D3D12_WORK_GRAPHS
      for (std::size_t index = 0; index < validated->work_graph_snapshot_count_; ++index) {
        const auto handle = validated->work_graph_snapshots_[index].handle;
        Require(work_graph_pool_.Contains(handle) && work_graphs_.contains(Key(handle)),
                "stale Work Graph recording snapshot");
      }
#endif
#if NEXORA_D3D12_RAY_TRACING
      for (std::size_t index = 0; index < validated->ray_snapshot_count_; ++index) {
        const auto &snapshot = validated->ray_snapshots_[index];
        Require(ray_program_pool_.Contains(snapshot.program_handle) &&
                    ray_programs_.contains(Key(snapshot.program_handle)) &&
                    ray_scene_pool_.Contains(snapshot.scene_handle) &&
                    ray_scenes_.contains(Key(snapshot.scene_handle)),
                "stale ray program or scene recording snapshot");
      }
#endif
      validated->Close();
      ID3D12CommandList *native_list = validated->list_.Get();
      if (capabilities_.gpu_timing_enabled)
        submitted_at = std::chrono::steady_clock::now();
      queue_->ExecuteCommandLists(1, &native_list);
      // ExecuteCommandLists accepted the list, even if a later Signal/wait fails. Buffers
      // decay to COMMON at the execution boundary on this single DIRECT queue.
      for (const auto &entry : validated->buffer_snapshots_) {
        auto &record = buffers_.at(entry.first);
        record.gpu_state = D3D12_RESOURCE_STATE_COMMON;
        record.upload_dirty = false;
        // Immutable SRV/indirect readers preserve the data revision. Their buffer states decay
        // to COMMON; a repeated initial upload copies the same revision's retained staging data.
        // Potential UAV writes still invalidate all older readers and writers.
        if (entry.second.writes)
          ++record.revision;
      }
      for (const auto &entry : validated->texture_snapshots_) {
        auto &texture = textures_.at(entry.first);
        texture.logical_state = entry.second.state; // Non-simultaneous textures do not decay.
        ++texture.revision;
      }
      validated->MarkSubmitted();
      ++diagnostics_.submitted_command_lists;
      diagnostics_.barriers += validated->Barriers();
      diagnostics_.enhanced_buffer_barriers += validated->enhanced_buffer_barriers_;
      diagnostics_.enhanced_texture_barriers += validated->enhanced_texture_barriers_;
      diagnostics_.draw_calls += validated->DrawCalls();
      diagnostics_.coarse_shading_draw_calls += validated->coarse_shading_draws_;
      diagnostics_.mesh_dispatch_calls += validated->mesh_dispatches_;
      diagnostics_.work_graph_dispatch_calls += validated->WorkGraphDispatches();
#if NEXORA_D3D12_RAY_TRACING
      diagnostics_.ray_dispatch_calls += validated->ray_dispatches_;
      diagnostics_.inline_ray_dispatch_calls += validated->inline_ray_dispatches_;
#endif
      diagnostics_.compute_dispatches += validated->Dispatches();
      diagnostics_.indirect_draw_calls += validated->IndirectDraws();
      fence_value = ++next_fence_;
      Check(queue_->Signal(fence_.Get(), fence_value), "ID3D12CommandQueue::Signal");
    }
    WaitForFence(fence_value);
    if (capabilities_.gpu_timing_enabled) {
      const auto measured = validated->GpuMilliseconds(submitted_at);
      std::lock_guard lock{mutex_};
      detail::PublishGpuTiming(diagnostics_.gpu_timing, GpuTimingSource::Dx12Timestamps,
                               fence_value, measured);
    }
    return fence_value;
  }

  std::uint64_t CompletedSubmissionValue() const noexcept override {
    return fence_ ? fence_->GetCompletedValue() : 0;
  }

  void WaitForSubmission(std::uint64_t value) override {
    {
      std::lock_guard lock{mutex_};
      Require(value <= next_fence_, "waiting for an unknown D3D12 submission");
    }
    WaitForFence(value);
  }

  void Present(TextureHandle texture) override {
    std::lock_guard lock{mutex_};
    const auto found = textures_.find(Key(texture));
    Require(found != textures_.end(), "presenting invalid D3D12 texture");
    Require(found->second.logical_state == ResourceState::Present,
            "present texture is not in Present state");
    ++diagnostics_.presents;
  }

  void WaitIdle() override {
    std::uint64_t fence_value{};
    {
      std::lock_guard lock{mutex_};
      fence_value = ++next_fence_;
      Check(queue_->Signal(fence_.Get(), fence_value), "ID3D12CommandQueue::Signal");
    }
    WaitForFence(fence_value);
  }

  [[nodiscard]] DeviceDiagnostics Diagnostics() const noexcept override {
    std::lock_guard lock{mutex_};
    return diagnostics_;
  }

  PipelineRecord &ValidatePipeline(PipelineHandle pipeline) {
    std::lock_guard lock{mutex_};
    const auto found = pipelines_.find(Key(pipeline));
    Require(found != pipelines_.end(), "binding invalid D3D12 pipeline");
    return found->second;
  }

  BufferRecord &ValidateBuffer(BufferHandle buffer) {
    std::lock_guard lock{mutex_};
    return ValidateBufferLocked(buffer);
  }

  ID3D12RootSignature *RootSignature() const noexcept { return root_signature_.Get(); }
  D3D12_GPU_VIRTUAL_ADDRESS ConstantsAddress() const noexcept {
    return constants_->GetGPUVirtualAddress();
  }

private:
  struct TextureRecord final {
    TextureDescriptor descriptor;
    ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    ResourceState logical_state{ResourceState::Undefined};
    std::uint64_t revision{};
    UINT rtv_slot{};
  };
  struct PipelineRecord final {
    ComPtr<ID3D12PipelineState> pipeline;
    PipelineType type{PipelineType::Graphics};
    TextureFormat format{TextureFormat::Rgba8Unorm};
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    GraphicsShaderVariant graphics_shader{GraphicsShaderVariant::Triangle};
    ShadingRate shading_rate{ShadingRate::Full};
    ComputeShaderVariant compute_shader{ComputeShaderVariant::GPUDriven};
  };
  struct BufferRecord final {
    BufferDescriptor descriptor;
    ComPtr<ID3D12Resource> resource;
    ComPtr<ID3D12Resource> gpu_resource;
    D3D12_RESOURCE_STATES gpu_state{D3D12_RESOURCE_STATE_COPY_DEST};
    bool upload_dirty{true};
    std::uint64_t revision{};
  };

  TextureRecord &ValidateTextureLocked(TextureHandle texture) {
    const auto found = textures_.find(Key(texture));
    Require(texture_pool_.Contains(texture) && found != textures_.end(),
            "command list references invalid D3D12 texture");
    return found->second;
  }

  BufferRecord &ValidateBufferLocked(BufferHandle buffer) {
    const auto found = buffers_.find(Key(buffer));
    Require(found != buffers_.end(), "binding invalid D3D12 buffer");
    return found->second;
  }

  template <typename Pool, typename Handle> static std::uint64_t HandleKey(Handle handle) {
    return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
  }

  void Require(bool condition, const char *message) {
    if (condition)
      return;
    ++diagnostics_.validation_errors;
    throw std::logic_error(message);
  }

  void CreateRootSignature() {
    std::array<D3D12_ROOT_PARAMETER, 5> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].Descriptor.RegisterSpace = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    for (std::uint32_t index = 1; index < 4; ++index) {
      parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
      parameters[index].Descriptor.ShaderRegister = index;
      parameters[index].Descriptor.RegisterSpace = 0;
      parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[4].Descriptor.ShaderRegister = 0;
    parameters[4].Descriptor.RegisterSpace = 0;
    // The same frame constant is used by baseline VS and the optional built-in MS.
    parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = static_cast<UINT>(parameters.size());
    description.pParameters = parameters.data();
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    Check(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized,
                                      &errors),
          "D3D12SerializeRootSignature");
    Check(device_->CreateRootSignature(0, serialized->GetBufferPointer(),
                                       serialized->GetBufferSize(), IID_PPV_ARGS(&root_signature_)),
          "ID3D12Device::CreateRootSignature");
  }
  void CreateFrameConstants() {
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = 256;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                                           D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                           IID_PPV_ARGS(&constants_)),
          "CreateCommittedResource(constants)");
    void *mapped = nullptr;
    D3D12_RANGE read_range{0, 0};
    Check(constants_->Map(0, &read_range, &mapped), "Map(constants)");
    const float identity[16] = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    std::memcpy(mapped, identity, sizeof(identity));
    constants_->Unmap(0, nullptr);
  }

  void WaitForFence(std::uint64_t value) {
    const auto completed = fence_->GetCompletedValue();
    if (completed == UINT64_MAX)
      throw std::runtime_error("D3D12 fence reports device removal");
    if (completed >= value)
      return;
    Check(fence_->SetEventOnCompletion(value, fence_event_), "ID3D12Fence::SetEventOnCompletion");
    if (WaitForSingleObject(fence_event_, INFINITE) != WAIT_OBJECT_0)
      throw std::runtime_error("WaitForSingleObject failed for D3D12 fence");
    if (fence_->GetCompletedValue() == UINT64_MAX)
      throw std::runtime_error("D3D12 fence reports device removal");
  }

  GraphicsDeviceCapabilities capabilities_;
  // The private SDK factory must outlive the device and all device-owned resources.
  detail::D3D12PreviewRuntime preview_runtime_;
  ComPtr<ID3D12Device> device_;
#if NEXORA_D3D12_WORK_GRAPHS
  ComPtr<ID3D12Device5> graph_device_;
#endif
#if NEXORA_D3D12_RAY_TRACING
  ComPtr<ID3D12Device5> ray_device_;
  core::HandlePool<RaySceneTag> ray_scene_pool_;
  core::HandlePool<RayProgramTag> ray_program_pool_;
  std::unordered_map<std::uint64_t, detail::NativeRayScene> ray_scenes_;
  std::unordered_map<std::uint64_t, detail::NativeRayProgram> ray_programs_;
#endif
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
  ComPtr<ID3D12Device2> mesh_device_;
#endif
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12DescriptorHeap> rtv_heap_;
  ComPtr<ID3D12Fence> fence_;
  ComPtr<ID3D12RootSignature> root_signature_;
  ComPtr<ID3D12CommandSignature> draw_indirect_signature_;
  ComPtr<ID3D12Resource> constants_;
  ComPtr<ID3DBlob> vertex_shader_;
  ComPtr<ID3DBlob> pixel_shader_;
  ComPtr<ID3DBlob> compute_shader_;
  HANDLE fence_event_{};
  UINT rtv_increment_{};
  static constexpr UINT RtvCapacity = 256;
  UINT next_rtv_{};
  std::vector<UINT> free_rtv_slots_;
  std::uint64_t next_fence_{};
  UINT64 timestamp_frequency_{};
  mutable std::mutex mutex_;
  core::HandlePool<TextureTag> texture_pool_;
  core::HandlePool<PipelineTag> pipeline_pool_;
  core::HandlePool<BufferTag> buffer_pool_;
  std::unordered_map<std::uint64_t, TextureRecord> textures_;
  std::unordered_map<std::uint64_t, PipelineRecord> pipelines_;
#if NEXORA_D3D12_WORK_GRAPHS
  core::HandlePool<WorkGraphTag> work_graph_pool_;
  std::unordered_map<std::uint64_t, detail::NativeWorkGraph> work_graphs_;
#endif
  std::unordered_map<std::uint64_t, BufferRecord> buffers_;
  DeviceDiagnostics diagnostics_;

  friend class D3D12CommandList;
};

D3D12CommandList::D3D12CommandList(D3D12Device &device, QueueType queue) : device_(device) {
  (void)queue;
  Check(device_.device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&allocator_)),
        "CreateCommandAllocator");
  Check(device_.device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(),
                                           nullptr, IID_PPV_ARGS(&list_)),
        "CreateCommandList");
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
  if (device_.capabilities_.features[static_cast<std::size_t>(GraphicsFeature::EnhancedBarriers)]
          .enabled)
    Check(list_.As(&enhanced_list_), "QueryInterface(command list7 enhanced barriers)");
#endif
  if (device_.capabilities_.gpu_timing_enabled) {
    D3D12_QUERY_HEAP_DESC query{};
    query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query.Count = 2;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC storage{};
    storage.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    storage.Width = 2 * sizeof(std::uint64_t);
    storage.Height = storage.DepthOrArraySize = storage.MipLevels = 1;
    storage.SampleDesc.Count = 1;
    storage.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (SUCCEEDED(device_.device_->CreateQueryHeap(&query, IID_PPV_ARGS(&timestamp_heap_))) &&
        SUCCEEDED(device_.device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &storage,
                                                           D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                           IID_PPV_ARGS(&timestamp_readback_)))) {
      list_->EndQuery(timestamp_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    } else {
      timestamp_heap_.Reset();
      timestamp_readback_.Reset();
    }
  }
}

std::optional<double>
D3D12CommandList::GpuMilliseconds(std::chrono::steady_clock::time_point submitted) const noexcept {
  if (!timestamp_heap_ || !timestamp_readback_ || !device_.timestamp_frequency_ ||
      device_.fence_->GetCompletedValue() == UINT64_MAX)
    return {};
  void *mapped{};
  const D3D12_RANGE range{0, 2 * sizeof(std::uint64_t)};
  if (FAILED(timestamp_readback_->Map(0, &range, &mapped)))
    return {};
  std::array<std::uint64_t, 2> values{};
  if (mapped)
    std::memcpy(values.data(), mapped, sizeof(values));
  const D3D12_RANGE written{0, 0};
  timestamp_readback_->Unmap(0, &written);
  if (!mapped)
    return {};
  return detail::TimestampMilliseconds(
      values[0], values[1], 64, 1000000000.0 / static_cast<double>(device_.timestamp_frequency_),
      detail::CompletionWallMilliseconds(submitted));
}

ID3D12Resource *D3D12CommandList::PrepareBuffer(BufferHandle buffer, D3D12_RESOURCE_STATES state) {
  std::lock_guard lock{device_.mutex_};
  const auto &record = device_.ValidateBufferLocked(buffer);
  auto &snapshot =
      buffer_snapshots_
          .try_emplace(Key(buffer), BufferSnapshot{buffer, record.revision, record.gpu_state,
                                                   record.upload_dirty})
          .first->second;
  device_.Require(snapshot.revision == record.revision,
                  "D3D12 buffer changed during command recording");
  snapshot.writes |= state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  const auto transition = [&](D3D12_RESOURCE_STATES target) {
    if (snapshot.state == target)
      return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = record.gpu_resource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = snapshot.state;
    barrier.Transition.StateAfter = target;
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
    if (enhanced_list_) {
      detail::EmitEnhancedBufferBarrier(*enhanced_list_.Get(), record.gpu_resource.Get(),
                                        snapshot.state, target);
      ++enhanced_buffer_barriers_;
    } else
#endif
      list_->ResourceBarrier(1, &barrier);
    snapshot.state = target;
  };
  if (snapshot.upload_pending) {
    transition(D3D12_RESOURCE_STATE_COPY_DEST);
    list_->CopyResource(record.gpu_resource.Get(), record.resource.Get());
    snapshot.upload_pending = false;
  }
  transition(state);
  return record.gpu_resource.Get();
}

D3D12CommandList::TextureSnapshot &D3D12CommandList::SnapshotTexture(TextureHandle handle) {
  std::lock_guard lock{device_.mutex_};
  const auto &texture = device_.ValidateTextureLocked(handle);
  const auto key = Key(handle);
  auto entry = texture_snapshots_.find(key);
  if (entry == texture_snapshots_.end())
    entry =
        texture_snapshots_
            .emplace(key, TextureSnapshot{handle, texture.revision, texture.logical_state,
                                          texture.descriptor.format, texture.descriptor.width,
                                          texture.descriptor.height, texture.resource, texture.rtv})
            .first;
  device_.Require(entry->second.revision == texture.revision,
                  "D3D12 texture changed during command recording");
  return entry->second;
}

void D3D12CommandList::Transition(const Barrier &barrier) {
  if (submitted_ || closed_)
    throw std::logic_error("cannot record a submitted command list");
  if (rendering_)
    throw std::logic_error("D3D12 barriers cannot occur inside rendering");
  if (!IsValidTextureBarrier(barrier))
    throw std::invalid_argument("invalid D3D12 texture barrier");
  auto &texture = SnapshotTexture(barrier.texture);
  if (texture.state != barrier.before) {
    std::lock_guard lock{device_.mutex_};
    device_.Require(false, "D3D12 barrier before-state mismatch");
  }
  if (barrier.before == barrier.after)
    return;
  texture.state = barrier.after;
  auto *resource = texture.resource.Get();
  D3D12_RESOURCE_BARRIER native{};
  native.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  native.Transition.pResource = resource;
  native.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  native.Transition.StateBefore = ToState(barrier.before);
  native.Transition.StateAfter = ToState(barrier.after);
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
  if (enhanced_list_) {
    detail::EmitEnhancedTextureBarrier(*enhanced_list_.Get(), resource, barrier.before,
                                       barrier.after);
    ++enhanced_texture_barriers_;
  } else
#endif
    list_->ResourceBarrier(1, &native);
  ++barriers_;
}

void D3D12CommandList::TransitionBuffer(const BufferBarrier &barrier) {
  if (submitted_ || closed_ || rendering_)
    throw std::logic_error("buffer barriers require an open command list outside rendering");
  if (!IsValidBufferBarrier(barrier))
    throw std::invalid_argument("invalid buffer barrier");
  const auto state = barrier.after == BufferAccess::IndirectRead
                         ? D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
                     : barrier.after == BufferAccess::StorageReadWrite
                         ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                         : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  auto *resource = PrepareBuffer(barrier.buffer, state);
  // PrepareBuffer handles uploads and actual native state transitions. A same-state UAV
  // dependency still needs ordering, including conservative Undefined -> UAV declarations.
  if (barrier.after == BufferAccess::StorageReadWrite) {
    D3D12_RESOURCE_BARRIER native{};
    native.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    native.UAV.pResource = resource;
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
    if (enhanced_list_) {
      detail::EmitEnhancedBufferBarrier(*enhanced_list_.Get(), resource,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      ++enhanced_buffer_barriers_;
    } else
#endif
      list_->ResourceBarrier(1, &native);
  }
  ++barriers_;
}

void D3D12CommandList::BeginRendering(const RenderingInfo &info) {
  if (submitted_ || closed_ || rendering_ || info.width == 0 || info.height == 0)
    throw std::logic_error("invalid D3D12 BeginRendering");
  auto &target = SnapshotTexture(info.color_target);
  if (target.state != ResourceState::RenderTarget || info.width > target.width ||
      info.height > target.height)
    throw std::logic_error("invalid D3D12 render target state/extent");
  render_target_ = info.color_target;
  list_->OMSetRenderTargets(1, &target.rtv, FALSE, nullptr);
  const float clear[] = {0.0F, 0.0F, 0.0F, 1.0F};
  list_->ClearRenderTargetView(target.rtv, clear, 0, nullptr);
  D3D12_VIEWPORT viewport{
      0.0F, 0.0F, static_cast<float>(info.width), static_cast<float>(info.height), 0.0F, 1.0F};
  D3D12_RECT scissor{0, 0, static_cast<LONG>(info.width), static_cast<LONG>(info.height)};
  list_->RSSetViewports(1, &viewport);
  list_->RSSetScissorRects(1, &scissor);
  list_->SetGraphicsRootSignature(device_.RootSignature());
  list_->SetGraphicsRootConstantBufferView(4, device_.ConstantsAddress());
  rendering_ = true;
  pipeline_bound_ = false;
}

void D3D12CommandList::BindPipeline(PipelineHandle pipeline) {
  if (submitted_ || closed_)
    throw std::logic_error("cannot bind a pipeline on a submitted D3D12 command list");
  const auto &record = device_.ValidatePipeline(pipeline);
#if NEXORA_D3D12_WORK_GRAPHS
  bound_work_graph_ = 0;
#endif
  pipeline_type_ = record.type;
  compute_shader_ = record.compute_shader;
  topology_ = record.topology;
  graphics_shader_ = record.graphics_shader;
  if ((pipeline_type_ == PipelineType::Graphics) != rendering_)
    throw std::logic_error("D3D12 pipeline type does not match command-list rendering state");
  if (pipeline_type_ == PipelineType::Graphics &&
      record.format != SnapshotTexture(render_target_).format)
    throw std::logic_error("D3D12 pipeline format does not match render target");
#if defined(__ID3D12GraphicsCommandList5_INTERFACE_DEFINED__)
  if (pipeline_type_ == PipelineType::Graphics) {
    if (record.shading_rate != ShadingRate::Full && !shading_list_)
      Check(list_.As(&shading_list_), "QueryInterface(command list5 shading rate)");
    if (shading_list_) {
      const D3D12_SHADING_RATE_COMBINER combiners[]{D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
                                                    D3D12_SHADING_RATE_COMBINER_PASSTHROUGH};
      shading_list_->RSSetShadingRate(record.shading_rate == ShadingRate::Full
                                          ? D3D12_SHADING_RATE_1X1
                                          : D3D12_SHADING_RATE_2X2,
                                      combiners);
    }
    shading_rate_ = record.shading_rate;
  }
#endif
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
  if (record.graphics_shader == GraphicsShaderVariant::MeshTriangle && !mesh_list_)
    Check(list_.As(&mesh_list_), "QueryInterface(command list6 mesh)");
#endif
  mesh_input_ = {}; // Mesh inputs must be explicitly bound after selecting this pipeline.
  list_->SetPipelineState(record.pipeline.Get());
  if (pipeline_type_ == PipelineType::Compute) {
    list_->SetComputeRootSignature(device_.RootSignature());
    for (std::uint32_t index = 0; index < storage_buffers_.size(); ++index) {
      if (!storage_bound_[index])
        continue;
      if (index == 0)
        list_->SetComputeRootShaderResourceView(index, storage_buffers_[index]);
      else
        list_->SetComputeRootUnorderedAccessView(index, storage_buffers_[index]);
    }
  }
  pipeline_bound_ = true;
}

void D3D12CommandList::BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) {
  if (rendering_) {
    if (submitted_ || closed_ || !pipeline_bound_ ||
        graphics_shader_ != GraphicsShaderVariant::MeshTriangle || binding != 0)
      throw std::logic_error("graphics storage input requires the bound mesh pipeline and slot0");
    const auto &record = device_.ValidateBuffer(buffer);
    if (record.descriptor.size < 5 * sizeof(std::uint32_t))
      throw std::logic_error("mesh statistics input requires at least five words");
    auto *resource = PrepareBuffer(buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list_->SetGraphicsRootShaderResourceView(0, resource->GetGPUVirtualAddress());
    mesh_input_ = buffer;
    return;
  }
  if (submitted_ || closed_ || binding >= storage_buffers_.size())
    throw std::logic_error("invalid D3D12 storage-buffer binding");
  const auto state = binding == 0 ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                                  : D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  auto *resource = PrepareBuffer(buffer, state);
  storage_buffers_[binding] = resource->GetGPUVirtualAddress();
  storage_bound_[binding] = true;
  storage_handles_[binding] = buffer;
  if (pipeline_bound_ && pipeline_type_ == PipelineType::Compute) {
    if (binding == 0)
      list_->SetComputeRootShaderResourceView(binding, storage_buffers_[binding]);
    else
      list_->SetComputeRootUnorderedAccessView(binding, storage_buffers_[binding]);
  }
}

void D3D12CommandList::BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset,
                                          std::uint32_t stride) {
  if (submitted_ || closed_ || stride != GPUDrivenIndirectCommandStride ||
      offset % alignof(std::uint32_t) != 0)
    throw std::logic_error("invalid D3D12 indirect-buffer binding");
  auto &record = device_.ValidateBuffer(buffer);
  if (offset > record.descriptor.size || record.descriptor.size - offset < DrawIndirectArgumentSize)
    throw std::logic_error("D3D12 indirect-buffer binding exceeds allocation");
  indirect_buffer_ = PrepareBuffer(buffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
  indirect_offset_ = offset;
  indirect_size_ = record.descriptor.size;
  indirect_stride_ = stride;
}

void D3D12CommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ ||
      !IsBuiltinGeometryDrawValid(topology_, graphics_shader_, vertex_count) || instance_count == 0)
    throw std::logic_error("invalid D3D12 draw");
  list_->IASetPrimitiveTopology(topology_ == PrimitiveTopology::PatchList
                                    ? D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST
                                    : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list_->DrawInstanced(vertex_count, instance_count, 0, 0);
  ++draws_;
  if (shading_rate_ != ShadingRate::Full)
    ++coarse_shading_draws_;
}

void D3D12CommandList::Dispatch(std::uint32_t group_count_x, std::uint32_t group_count_y,
                                std::uint32_t group_count_z) {
  if (submitted_ || closed_ || rendering_ || !pipeline_bound_ ||
      pipeline_type_ != PipelineType::Compute || group_count_x == 0 || group_count_y == 0 ||
      group_count_z == 0)
    throw std::logic_error("invalid D3D12 dispatch");
  for (std::size_t index = 0; index < storage_bound_.size(); ++index) {
    if (!storage_bound_[index])
      throw std::logic_error("D3D12 dispatch requires all storage bindings");
    (void)device_.ValidateBuffer(storage_handles_[index]);
  }
  if (compute_shader_ >= ComputeShaderVariant::LongVector69) {
    if (group_count_x != 1 || group_count_y != 1 || group_count_z != 1 ||
        storage_handles_[0] == storage_handles_[1] ||
        device_.ValidateBuffer(storage_handles_[0]).descriptor.size < 128 ||
        device_.ValidateBuffer(storage_handles_[1]).descriptor.size < 32)
      throw std::logic_error(
          "modern compute recipe requires one group and distinct 128/32-byte buffers");
  }
  list_->Dispatch(group_count_x, group_count_y, group_count_z);
  ++dispatches_;
}

void D3D12CommandList::DrawIndirect(std::uint32_t draw_count) {
  if (!SupportsBuiltinIndirectDraw(topology_, graphics_shader_) || submitted_ || closed_ ||
      !rendering_ || !pipeline_bound_ || pipeline_type_ != PipelineType::Graphics ||
      !indirect_buffer_ || indirect_stride_ != GPUDrivenIndirectCommandStride || draw_count == 0)
    throw std::logic_error("invalid D3D12 indirect draw");
  if (!IndirectDrawRangeFits(indirect_size_, indirect_offset_, indirect_stride_, draw_count))
    throw std::logic_error("D3D12 indirect draw reads past the bound indirect buffer");
  list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list_->ExecuteIndirect(device_.draw_indirect_signature_.Get(), draw_count, indirect_buffer_,
                         indirect_offset_, nullptr, 0);
  ++draws_;
  ++indirect_draws_;
  if (shading_rate_ != ShadingRate::Full)
    ++coarse_shading_draws_;
}

#if NEXORA_D3D12_WORK_GRAPHS
void D3D12CommandList::BindWorkGraph(WorkGraphHandle graph) {
  if (submitted_ || closed_ || rendering_)
    throw std::logic_error("invalid Work Graph binding scope");
  std::lock_guard lock{device_.mutex_};
  device_.Require(device_.work_graph_pool_.Contains(graph) &&
                      device_.work_graphs_.contains(Key(graph)),
                  "binding stale Work Graph program");
  std::size_t index = 0;
  while (index < work_graph_snapshot_count_ && work_graph_snapshots_[index].handle != graph)
    ++index;
  if (index == work_graph_snapshot_count_) {
    if (index == work_graph_snapshots_.size())
      throw std::logic_error("Work Graph list program capacity exceeded");
    work_graph_snapshots_[index] = {graph, device_.work_graphs_.at(Key(graph))};
    ++work_graph_snapshot_count_;
  }
  bound_work_graph_ = index + 1;
  pipeline_bound_ = false;
  storage_bound_[1] = false; // Require explicit output binding for this program selection.
}
void D3D12CommandList::DispatchWorkGraph(WorkGraphInput input) {
  if (submitted_ || closed_ || rendering_ || !bound_work_graph_ || !storage_bound_[1] ||
      !IsWorkGraphInputValid(input) || work_graph_dispatches_ == work_graph_backing_.size())
    throw std::logic_error("invalid or out-of-capacity Work Graph dispatch");
  const auto output = storage_handles_[1];
  if (device_.ValidateBuffer(output).descriptor.size <
      WorkGraphMaximumRecords * WorkGraphVisibleRecordWords * sizeof(std::uint32_t))
    throw std::logic_error("Work Graph output requires eight five-word visible records");
  auto *resource = PrepareBuffer(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  if (!graph_list_)
    Check(list_.As(&graph_list_), "QueryInterface(command list10 Work Graph)");
  const auto &program = work_graph_snapshots_[bound_work_graph_ - 1].program;
  ComPtr<ID3D12Resource> backing;
  if (program.backing_bytes) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = program.backing_bytes;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc = {1, 0};
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    Check(device_.device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
                                                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                   IID_PPV_ARGS(&backing)),
          "CreateCommittedResource(Work Graph private backing)");
  }
  // Each dispatch owns a fresh context and INITIALIZE, including rebinds in this same list.
  auto &owned = work_graph_backing_[static_cast<std::size_t>(work_graph_dispatches_)];
  owned = std::move(backing);
  list_->SetComputeRootSignature(program.root.Get());
  list_->SetComputeRootUnorderedAccessView(0, resource->GetGPUVirtualAddress());
  detail::RecordNativeWorkGraph(*graph_list_.Get(), program, owned.Get(), input);
  ++work_graph_dispatches_;
}
#endif

#if NEXORA_D3D12_RAY_TRACING
void D3D12CommandList::TraceRays(RayProgramHandle program_handle, RaySceneHandle scene_handle,
                                 BufferHandle input, BufferHandle output, std::uint32_t count) {
  if (submitted_ || closed_ || rendering_ || !count || count > RayMaximumInputs ||
      input == output || ray_snapshot_count_ == ray_snapshots_.size())
    throw std::logic_error("invalid bounded ray dispatch");
  RaySnapshot snapshot;
  {
    std::lock_guard lock{device_.mutex_};
    device_.Require(device_.ray_program_pool_.Contains(program_handle) &&
                        device_.ray_programs_.contains(Key(program_handle)) &&
                        device_.ray_scene_pool_.Contains(scene_handle) &&
                        device_.ray_scenes_.contains(Key(scene_handle)),
                    "binding stale ray program or scene");
    snapshot = {program_handle, scene_handle, device_.ray_programs_.at(Key(program_handle)),
                device_.ray_scenes_.at(Key(scene_handle))};
  }
  if (device_.ValidateBuffer(input).descriptor.size <
          static_cast<std::uint64_t>(count) * sizeof(RayInput) ||
      device_.ValidateBuffer(output).descriptor.size <
          static_cast<std::uint64_t>(count) * sizeof(RayHit))
    throw std::logic_error("ray buffers are smaller than the admitted count");
  auto *native_input = PrepareBuffer(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  auto *native_output = PrepareBuffer(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  if (!ray_list_)
    Check(list_.As(&ray_list_), "QueryInterface(ray command list4)");
  auto &owned = ray_snapshots_[ray_snapshot_count_];
  owned = std::move(snapshot);
  detail::RecordNativeRays(*ray_list_.Get(), owned.program, owned.scene, *native_input,
                           *native_output, count);
  ++ray_snapshot_count_;
  if (owned.program.mode == RayTracingMode::Pipeline)
    ++ray_dispatches_;
  else
    ++inline_ray_dispatches_;
  pipeline_bound_ = false;
  mesh_input_ = {};
#if NEXORA_D3D12_WORK_GRAPHS
  bound_work_graph_ = 0;
#endif
}
#endif

void D3D12CommandList::DispatchMesh(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ ||
      graphics_shader_ != GraphicsShaderVariant::MeshTriangle || !mesh_input_.IsValid() || x != 1 ||
      y != 1 || z != 1)
    throw std::logic_error(
        "built-in mesh dispatch requires one group and a bound statistics input");
  (void)PrepareBuffer(mesh_input_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
  if (!mesh_list_)
    throw std::logic_error("native mesh command interface is unavailable");
  mesh_list_->DispatchMesh(x, y, z);
  ++mesh_dispatches_;
  ++draws_;
  if (shading_rate_ != ShadingRate::Full)
    ++coarse_shading_draws_;
#else
  throw std::logic_error("native mesh dispatch requires a newer D3D12 SDK");
#endif
}

void D3D12CommandList::EndRendering() {
  if (submitted_ || closed_ || !rendering_)
    throw std::logic_error("D3D12 EndRendering without BeginRendering");
  rendering_ = false;
}

void D3D12CommandList::Close() {
  if (closed_)
    return;
  if (timestamp_heap_) {
    list_->EndQuery(timestamp_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    list_->ResolveQueryData(timestamp_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2,
                            timestamp_readback_.Get(), 0);
  }
  Check(list_->Close(), "ID3D12GraphicsCommandList::Close");
  closed_ = true;
}
void D3D12Device::ReadTextureForTesting(TextureHandle texture, std::span<std::byte> data) {
  WaitIdle();
  ComPtr<ID3D12Resource> readback;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  std::uint64_t fence_value{};
  std::uint64_t total{};
  std::uint32_t height{};
  std::uint64_t row_bytes{};
  bool accepted = false;
  try {
    {
      std::lock_guard lock{mutex_};
      auto &record = ValidateTextureLocked(texture);
      Require(record.descriptor.format == TextureFormat::Rgba8Unorm ||
                  record.descriptor.format == TextureFormat::Bgra8Unorm,
              "D3D12 texture readback requires a four-byte color format");
      row_bytes = static_cast<std::uint64_t>(record.descriptor.width) * 4;
      height = record.descriptor.height;
      Require(data.size() == row_bytes * height,
              "texture readback size must be width * height * 4");
      Require(record.logical_state != ResourceState::Undefined,
              "reading a D3D12 texture with undefined contents");
      const auto description = record.resource->GetDesc();
      device_->GetCopyableFootprints(&description, 0, 1, 0, &footprint, nullptr, nullptr, &total);
      D3D12_HEAP_PROPERTIES heap{};
      heap.Type = D3D12_HEAP_TYPE_READBACK;
      heap.CreationNodeMask = heap.VisibleNodeMask = 1;
      D3D12_RESOURCE_DESC storage{};
      storage.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      storage.Width = total;
      storage.Height = 1;
      storage.DepthOrArraySize = storage.MipLevels = 1;
      storage.SampleDesc.Count = 1;
      storage.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &storage,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                             IID_PPV_ARGS(&readback)),
            "CreateCommittedResource(texture readback)");
      Check(
          device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
          "CreateCommandAllocator(texture readback)");
      Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                       IID_PPV_ARGS(&list)),
            "CreateCommandList(texture readback)");
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
      ComPtr<ID3D12GraphicsCommandList7> enhanced;
      if (capabilities_.features[static_cast<std::size_t>(GraphicsFeature::EnhancedBarriers)]
              .enabled)
        Check(list.As(&enhanced), "QueryInterface(texture readback enhanced barriers)");
#endif
      // Undefined logical contents still have the COMMON layout assigned at resource creation.
      const auto original = record.logical_state == ResourceState::Undefined
                                ? ResourceState::Present
                                : record.logical_state;
      const auto transition = [&](ResourceState before, ResourceState after) {
        if (before == after)
          return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = record.resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = ToState(before);
        barrier.Transition.StateAfter = ToState(after);
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
        if (enhanced)
          detail::EmitEnhancedTextureBarrier(*enhanced.Get(), record.resource.Get(), before, after);
        else
#endif
          list->ResourceBarrier(1, &barrier);
      };
      transition(original, ResourceState::CopySource);
      D3D12_TEXTURE_COPY_LOCATION destination{};
      destination.pResource = readback.Get();
      destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      destination.PlacedFootprint = footprint;
      D3D12_TEXTURE_COPY_LOCATION source{};
      source.pResource = record.resource.Get();
      source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
      transition(ResourceState::CopySource, original);
      Check(list->Close(), "Close(texture readback)");
      ID3D12CommandList *native_list = list.Get();
      queue_->ExecuteCommandLists(1, &native_list);
      accepted = true;
      ++record.revision;
      fence_value = ++next_fence_;
      Check(queue_->Signal(fence_.Get(), fence_value), "Signal(texture readback)");
    }
    WaitForFence(fence_value);
  } catch (...) {
    if (accepted)
      WaitIdle();
    throw;
  }
  void *mapped{};
  const D3D12_RANGE range{0, static_cast<SIZE_T>(total)};
  Check(readback->Map(0, &range, &mapped), "Map(texture readback)");
  for (std::uint32_t row = 0; row < height; ++row)
    std::memcpy(data.data() + static_cast<std::size_t>(row * row_bytes),
                static_cast<const std::byte *>(mapped) + footprint.Offset +
                    static_cast<std::uint64_t>(row) * footprint.Footprint.RowPitch,
                static_cast<std::size_t>(row_bytes));
  const D3D12_RANGE written{0, 0};
  readback->Unmap(0, &written);
  std::lock_guard lock{mutex_};
  ++diagnostics_.readbacks;
}

void D3D12Device::ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                                       std::span<std::byte> data) {
  WaitIdle();
  ComPtr<ID3D12Resource> readback;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  std::uint64_t fence_value{};
  {
    std::lock_guard lock{mutex_};
    auto &record = ValidateBufferLocked(buffer);
    Require(offset <= record.descriptor.size && data.size() <= record.descriptor.size - offset,
            "D3D12 buffer readback exceeds allocation");
    if (data.empty())
      return;

    D3D12_HEAP_PROPERTIES readback_heap{};
    readback_heap.Type = D3D12_HEAP_TYPE_READBACK;
    readback_heap.CreationNodeMask = 1;
    readback_heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC readback_description{};
    readback_description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readback_description.Width = data.size();
    readback_description.Height = 1;
    readback_description.DepthOrArraySize = 1;
    readback_description.MipLevels = 1;
    readback_description.SampleDesc.Count = 1;
    readback_description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Check(device_->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE,
                                           &readback_description, D3D12_RESOURCE_STATE_COPY_DEST,
                                           nullptr, IID_PPV_ARGS(&readback)),
          "CreateCommittedResource(readback)");
    Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
          "CreateCommandAllocator(readback)");
    Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                     IID_PPV_ARGS(&list)),
          "CreateCommandList(readback)");

    const auto transition = [&](D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
      if (before == after)
        return;
      D3D12_RESOURCE_BARRIER barrier{};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = record.gpu_resource.Get();
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = before;
      barrier.Transition.StateAfter = after;
      list->ResourceBarrier(1, &barrier);
    };

    // Build the readback privately as well: a Close/allocation failure must not consume
    // pending upload data. Resource state is committed only after ExecuteCommandLists.
    auto state = record.gpu_state;
    if (record.upload_dirty) {
      transition(state, D3D12_RESOURCE_STATE_COPY_DEST);
      state = D3D12_RESOURCE_STATE_COPY_DEST;
      list->CopyResource(record.gpu_resource.Get(), record.resource.Get());
    }
    transition(state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    list->CopyBufferRegion(readback.Get(), 0, record.gpu_resource.Get(), offset, data.size());
    Check(list->Close(), "ID3D12GraphicsCommandList::Close(readback)");
    ID3D12CommandList *native_list = list.Get();
    queue_->ExecuteCommandLists(1, &native_list);
    record.gpu_state = D3D12_RESOURCE_STATE_COMMON;
    record.upload_dirty = false;
    ++record.revision;
    fence_value = ++next_fence_;
    Check(queue_->Signal(fence_.Get(), fence_value), "ID3D12CommandQueue::Signal(readback)");
  }
  WaitForFence(fence_value);
  void *mapped{};
  D3D12_RANGE read_range{0, data.size()};
  Check(readback->Map(0, &read_range, &mapped), "Map(readback)");
  std::memcpy(data.data(), mapped, data.size());
  readback->Unmap(0, nullptr);
  std::lock_guard lock{mutex_};
  ++diagnostics_.readbacks;
}
} // namespace

std::unique_ptr<Device> CreateDirect3D12Device(const DeviceCreationOptions &options) {
  return std::make_unique<D3D12Device>(options);
}
} // namespace nexora::rhi
