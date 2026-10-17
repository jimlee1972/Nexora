#if !defined(_WIN32)
#error "D3D12Device.cpp is only built on Windows"
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/Device.h"

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
  report(GraphicsFeature::Tessellation, {1, 0}, true);
  report(GraphicsFeature::ParallelCommandRecording, {1, 0});
  result.independent_compute_recording = true;
  result.independent_graphics_recording = true;

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

ComPtr<ID3DBlob> LoadShaderArtifact(const char *environment_name) {
  const auto *path = std::getenv(environment_name);
  if (!path || *path == '\0')
    return {};
  ComPtr<ID3DBlob> bytecode;
  Check(D3DReadFileToBlob(std::filesystem::path(path).c_str(), &bytecode), "D3DReadFileToBlob");
  if (!bytecode || bytecode->GetBufferSize() < 4 ||
      std::memcmp(bytecode->GetBufferPointer(), "DXBC", 4) != 0)
    throw std::runtime_error(std::string("DXIL artifact is not a valid DXBC container: ") + path);
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
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
  bool rendering_{false};
  bool pipeline_bound_{false};
  PipelineType pipeline_type_{PipelineType::Graphics};
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
  D3D12Device() {
    ComPtr<IDXGIFactory6> factory;
    Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

    ComPtr<IDXGIAdapter1> selected;
    for (UINT index = 0;
         factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                             IID_PPV_ARGS(&selected)) != DXGI_ERROR_NOT_FOUND;
         ++index) {
      DXGI_ADAPTER_DESC1 description{};
      Check(selected->GetDesc1(&description), "IDXGIAdapter1::GetDesc1");
      if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        continue;
      if (SUCCEEDED(
              D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
        break;
      }
      selected.Reset();
    }
    if (!device_) {
      Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&selected)), "EnumWarpAdapter");
      Check(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)),
            "D3D12CreateDevice(WARP)");
    }

    capabilities_ = QueryCapabilities(*device_.Get(), *selected.Get());
    D3D12_COMMAND_QUEUE_DESC queue_description{};
    queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(device_->CreateCommandQueue(&queue_description, IID_PPV_ARGS(&queue_)),
          "CreateCommandQueue");

    D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
    heap_description.NumDescriptors = RtvCapacity;
    free_rtv_slots_.reserve(RtvCapacity);
    heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    Check(device_->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&rtv_heap_)),
          "CreateDescriptorHeap");
    rtv_increment_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");
    fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_)
      throw std::runtime_error("CreateEventW failed");

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
    vertex_shader_ = LoadShaderArtifact("NEXORA_SLANG_DXIL_VERTEX_PATH");
    pixel_shader_ = LoadShaderArtifact("NEXORA_SLANG_DXIL_FRAGMENT_PATH");
    compute_shader_ = LoadShaderArtifact("NEXORA_SLANG_COMPUTE_DXIL_PATH");
    if (!vertex_shader_ || !pixel_shader_) {
      if (vertex_shader_ || pixel_shader_)
        throw std::runtime_error("both Slang DXIL entry-point artifacts are required");
      vertex_shader_ = CompileShader(kTriangleShader, "vsMain", "vs_5_0");
      pixel_shader_ = CompileShader(kTriangleShader, "psMain", "ps_5_0");
    }
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
    RequireGraphicsCapabilities(Capabilities(), descriptor.graphics_requirements);
    if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0)
      throw std::invalid_argument("pipeline hashes must be non-zero");
    PipelineRecord record;
    record.type = descriptor.type;
    record.format = descriptor.color_format;
    if (descriptor.type == PipelineType::Compute) {
      if (!compute_shader_)
        throw std::runtime_error("D3D12 compute pipeline requires Slang DXIL");
      D3D12_COMPUTE_PIPELINE_STATE_DESC compute_description{};
      compute_description.pRootSignature = root_signature_.Get();
      compute_description.CS = {compute_shader_->GetBufferPointer(),
                                compute_shader_->GetBufferSize()};
      Check(
          device_->CreateComputePipelineState(&compute_description, IID_PPV_ARGS(&record.pipeline)),
          "CreateComputePipelineState");
      std::lock_guard lock{mutex_};
      const auto handle = pipeline_pool_.Create();
      pipelines_.emplace(Key(handle), std::move(record));
      return handle;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_description{};
    pipeline_description.pRootSignature = root_signature_.Get();
    pipeline_description.VS = {vertex_shader_->GetBufferPointer(), vertex_shader_->GetBufferSize()};
    pipeline_description.PS = {pixel_shader_->GetBufferPointer(), pixel_shader_->GetBufferSize()};
    pipeline_description.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline_description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline_description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline_description.RasterizerState.DepthClipEnable = TRUE;
    pipeline_description.DepthStencilState.DepthEnable = FALSE;
    pipeline_description.DepthStencilState.StencilEnable = FALSE;
    pipeline_description.SampleMask = std::numeric_limits<UINT>::max();
    pipeline_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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

  std::unique_ptr<CommandList> CreateCommandList(QueueType queue) override {
    if (queue == QueueType::Copy)
      throw std::invalid_argument("D3D12 backend does not expose a copy command list");
    return std::make_unique<D3D12CommandList>(*this, queue);
  }

  std::uint64_t Submit(CommandList &commands) override {
    auto *validated = dynamic_cast<D3D12CommandList *>(&commands);
    std::uint64_t fence_value{};
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
      validated->Close();
      ID3D12CommandList *native_list = validated->list_.Get();
      queue_->ExecuteCommandLists(1, &native_list);
      // ExecuteCommandLists accepted the list, even if a later Signal/wait fails. Buffers
      // decay to COMMON at the execution boundary on this single DIRECT queue.
      for (const auto &entry : validated->buffer_snapshots_) {
        auto &record = buffers_.at(entry.first);
        record.gpu_state = D3D12_RESOURCE_STATE_COMMON;
        record.upload_dirty = false;
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
      diagnostics_.draw_calls += validated->DrawCalls();
      diagnostics_.compute_dispatches += validated->Dispatches();
      diagnostics_.indirect_draw_calls += validated->IndirectDraws();
      fence_value = ++next_fence_;
      Check(queue_->Signal(fence_.Get(), fence_value), "ID3D12CommandQueue::Signal");
    }
    WaitForFence(fence_value);
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
    parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
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
    if (fence_->GetCompletedValue() >= value)
      return;
    Check(fence_->SetEventOnCompletion(value, fence_event_), "ID3D12Fence::SetEventOnCompletion");
    if (WaitForSingleObject(fence_event_, INFINITE) != WAIT_OBJECT_0)
      throw std::runtime_error("WaitForSingleObject failed for D3D12 fence");
  }

  GraphicsDeviceCapabilities capabilities_;
  ComPtr<ID3D12Device> device_;
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
  mutable std::mutex mutex_;
  core::HandlePool<TextureTag> texture_pool_;
  core::HandlePool<PipelineTag> pipeline_pool_;
  core::HandlePool<BufferTag> buffer_pool_;
  std::unordered_map<std::uint64_t, TextureRecord> textures_;
  std::unordered_map<std::uint64_t, PipelineRecord> pipelines_;
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
  const auto transition = [&](D3D12_RESOURCE_STATES target) {
    if (snapshot.state == target)
      return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = record.gpu_resource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = snapshot.state;
    barrier.Transition.StateAfter = target;
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
  pipeline_type_ = record.type;
  if ((pipeline_type_ == PipelineType::Graphics) != rendering_)
    throw std::logic_error("D3D12 pipeline type does not match command-list rendering state");
  if (pipeline_type_ == PipelineType::Graphics &&
      record.format != SnapshotTexture(render_target_).format)
    throw std::logic_error("D3D12 pipeline format does not match render target");
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
  if (submitted_ || closed_ || rendering_ || binding >= storage_buffers_.size())
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
  if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ || vertex_count == 0 ||
      instance_count == 0)
    throw std::logic_error("invalid D3D12 draw");
  list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list_->DrawInstanced(vertex_count, instance_count, 0, 0);
  ++draws_;
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
  list_->Dispatch(group_count_x, group_count_y, group_count_z);
  ++dispatches_;
}

void D3D12CommandList::DrawIndirect(std::uint32_t draw_count) {
  if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ ||
      pipeline_type_ != PipelineType::Graphics || !indirect_buffer_ ||
      indirect_stride_ != GPUDrivenIndirectCommandStride || draw_count == 0)
    throw std::logic_error("invalid D3D12 indirect draw");
  if (!IndirectDrawRangeFits(indirect_size_, indirect_offset_, indirect_stride_, draw_count))
    throw std::logic_error("D3D12 indirect draw reads past the bound indirect buffer");
  list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list_->ExecuteIndirect(device_.draw_indirect_signature_.Get(), draw_count, indirect_buffer_,
                         indirect_offset_, nullptr, 0);
  ++draws_;
  ++indirect_draws_;
}

void D3D12CommandList::EndRendering() {
  if (submitted_ || closed_ || !rendering_)
    throw std::logic_error("D3D12 EndRendering without BeginRendering");
  rendering_ = false;
}

void D3D12CommandList::Close() {
  if (closed_)
    return;
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
      const auto original = ToState(record.logical_state);
      const auto transition = [&](D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        if (before == after)
          return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = record.resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        list->ResourceBarrier(1, &barrier);
      };
      transition(original, D3D12_RESOURCE_STATE_COPY_SOURCE);
      D3D12_TEXTURE_COPY_LOCATION destination{};
      destination.pResource = readback.Get();
      destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      destination.PlacedFootprint = footprint;
      D3D12_TEXTURE_COPY_LOCATION source{};
      source.pResource = record.resource.Get();
      source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
      transition(D3D12_RESOURCE_STATE_COPY_SOURCE, original);
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

std::unique_ptr<Device> CreateDirect3D12Device() { return std::make_unique<D3D12Device>(); }
} // namespace nexora::rhi
