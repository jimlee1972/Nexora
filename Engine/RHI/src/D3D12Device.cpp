#if !defined(_WIN32)
#error "D3D12Device.cpp is only built on Windows"
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Nexora/RHI/Device.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

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

namespace nexora::rhi {
namespace {
using Microsoft::WRL::ComPtr;

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

DXGI_FORMAT ToFormat(TextureFormat format) {
  switch (format) {
  case TextureFormat::Rgba8Unorm:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case TextureFormat::Bgra8Unorm:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case TextureFormat::Depth32Float:
    return DXGI_FORMAT_D32_FLOAT;
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
  const auto result =
      D3DCompile(source, std::strlen(source), "NexoraTriangle.hlsl", nullptr, nullptr, entry,
                  profile, flags, 0, &bytecode, &errors);
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
  Check(D3DReadFileToBlob(std::filesystem::path(path).c_str(), &bytecode),
        "D3DReadFileToBlob");
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
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void EndRendering() override;

  [[nodiscard]] bool IsClosed() const noexcept { return !rendering_; }
  [[nodiscard]] bool IsSubmitted() const noexcept { return submitted_; }
  [[nodiscard]] bool BelongsTo(const D3D12Device &device) const noexcept {
    return &device_ == &device;
  }
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  void MarkSubmitted() noexcept { submitted_ = true; }
  void Close();

private:
  friend class D3D12Device;
  D3D12Device &device_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
  bool rendering_{false};
  bool pipeline_bound_{false};
  bool submitted_{false};
  bool closed_{false};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
};

class D3D12Device final : public Device {
  struct TextureRecord;
  struct PipelineRecord;

public:
  D3D12Device() {
    ComPtr<IDXGIFactory6> factory;
    Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

    ComPtr<IDXGIAdapter1> selected;
    for (UINT index = 0; factory->EnumAdapterByGpuPreference(
                               index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
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

    D3D12_COMMAND_QUEUE_DESC queue_description{};
    queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(device_->CreateCommandQueue(&queue_description, IID_PPV_ARGS(&queue_)),
          "CreateCommandQueue");

    D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
    heap_description.NumDescriptors = 256;
    heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    Check(device_->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&rtv_heap_)),
          "CreateDescriptorHeap");
    rtv_increment_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
          "CreateFence");
    fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_)
      throw std::runtime_error("CreateEventW failed");

    CreateRootSignature();
    CreateFrameConstants();
    vertex_shader_ = LoadShaderArtifact("NEXORA_SLANG_DXIL_VERTEX_PATH");
    pixel_shader_ = LoadShaderArtifact("NEXORA_SLANG_DXIL_FRAGMENT_PATH");
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

    TextureRecord record;
    Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resource_description,
                                           ToState(descriptor.initial_state), &clear,
                                           IID_PPV_ARGS(&record.resource)),
          "CreateCommittedResource");
    record.descriptor = descriptor;
    record.logical_state = descriptor.initial_state;
    record.rtv.ptr = rtv_heap_->GetCPUDescriptorHandleForHeapStart().ptr +
                     static_cast<SIZE_T>(next_rtv_++) * rtv_increment_;
    D3D12_RENDER_TARGET_VIEW_DESC rtv_description{};
    rtv_description.Format = format;
    rtv_description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device_->CreateRenderTargetView(record.resource.Get(), &rtv_description, record.rtv);

    std::lock_guard lock{mutex_};
    const auto handle = texture_pool_.Create();
    textures_.emplace(Key(handle), std::move(record));
    return handle;
  }

  void DestroyTexture(TextureHandle texture) override {
    std::lock_guard lock{mutex_};
    Require(texture_pool_.Contains(texture) && textures_.contains(Key(texture)),
            "destroying invalid D3D12 texture");
    textures_.erase(Key(texture));
    Require(texture_pool_.Destroy(texture), "destroying stale D3D12 texture");
  }

  PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) override {
    if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0)
      throw std::invalid_argument("pipeline hashes must be non-zero");
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

    PipelineRecord record;
    Check(device_->CreateGraphicsPipelineState(&pipeline_description,
                                               IID_PPV_ARGS(&record.pipeline)),
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
    if (queue != QueueType::Graphics)
      throw std::invalid_argument("D3D12 triangle backend only supports graphics queue");
    return std::make_unique<D3D12CommandList>(*this, queue);
  }

  void Submit(CommandList &commands) override {
    auto *validated = dynamic_cast<D3D12CommandList *>(&commands);
    std::uint64_t fence_value{};
    {
      std::lock_guard lock{mutex_};
      Require(validated != nullptr, "command list belongs to another device");
      Require(validated->BelongsTo(*this), "command list belongs to another device");
      Require(validated->IsClosed(), "cannot submit an open command list");
      Require(!validated->IsSubmitted(), "command list was already submitted");
      validated->Close();
      ID3D12CommandList *native_list = validated->list_.Get();
      queue_->ExecuteCommandLists(1, &native_list);
      fence_value = ++next_fence_;
      Check(queue_->Signal(fence_.Get(), fence_value), "ID3D12CommandQueue::Signal");
      validated->MarkSubmitted();
      ++diagnostics_.submitted_command_lists;
      diagnostics_.barriers += validated->Barriers();
      diagnostics_.draw_calls += validated->DrawCalls();
    }
    WaitForFence(fence_value);
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

  ID3D12Resource *RecordTransition(const Barrier &barrier) {
    std::lock_guard lock{mutex_};
    const auto found = textures_.find(Key(barrier.texture));
    Require(found != textures_.end(), "barrier references invalid D3D12 texture");
    Require(found->second.logical_state == barrier.before, "D3D12 barrier before-state mismatch");
    found->second.logical_state = barrier.after;
    return found->second.resource.Get();
  }

  TextureRecord &ValidateRenderTarget(TextureHandle texture) {
    std::lock_guard lock{mutex_};
    const auto found = textures_.find(Key(texture));
    Require(found != textures_.end(), "rendering references invalid D3D12 texture");
    Require(found->second.logical_state == ResourceState::RenderTarget,
            "D3D12 render target is not in RenderTarget state");
    return found->second;
  }

  ID3D12PipelineState *ValidatePipeline(PipelineHandle pipeline) {
    std::lock_guard lock{mutex_};
    const auto found = pipelines_.find(Key(pipeline));
    Require(found != pipelines_.end(), "binding invalid D3D12 pipeline");
    return found->second.pipeline.Get();
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
  };
  struct PipelineRecord final {
    ComPtr<ID3D12PipelineState> pipeline;
  };

  template <typename Pool, typename Handle>
  static std::uint64_t HandleKey(Handle handle) {
    return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
  }

  void Require(bool condition, const char *message) {
    if (condition)
      return;
    ++diagnostics_.validation_errors;
    throw std::logic_error(message);
  }

  void CreateRootSignature() {
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.Descriptor.ShaderRegister = 0;
    parameter.Descriptor.RegisterSpace = 0;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = 1;
    description.pParameters = &parameter;
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    Check(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1,
                                      &serialized, &errors),
          "D3D12SerializeRootSignature");
    Check(device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                       IID_PPV_ARGS(&root_signature_)),
          "CreateRootSignature");
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

  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12DescriptorHeap> rtv_heap_;
  ComPtr<ID3D12Fence> fence_;
  ComPtr<ID3D12RootSignature> root_signature_;
  ComPtr<ID3D12Resource> constants_;
  ComPtr<ID3DBlob> vertex_shader_;
  ComPtr<ID3DBlob> pixel_shader_;
  HANDLE fence_event_{};
  UINT rtv_increment_{};
  UINT next_rtv_{};
  std::uint64_t next_fence_{};
  mutable std::mutex mutex_;
  core::HandlePool<TextureTag> texture_pool_;
  core::HandlePool<PipelineTag> pipeline_pool_;
  std::unordered_map<std::uint64_t, TextureRecord> textures_;
  std::unordered_map<std::uint64_t, PipelineRecord> pipelines_;
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

void D3D12CommandList::Transition(const Barrier &barrier) {
  if (submitted_)
    throw std::logic_error("cannot record a submitted command list");
  if (rendering_)
    throw std::logic_error("D3D12 barriers cannot occur inside rendering");
  if (barrier.before == barrier.after)
    return;
  auto *resource = device_.RecordTransition(barrier);
  D3D12_RESOURCE_BARRIER native{};
  native.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  native.Transition.pResource = resource;
  native.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  native.Transition.StateBefore = ToState(barrier.before);
  native.Transition.StateAfter = ToState(barrier.after);
  list_->ResourceBarrier(1, &native);
  ++barriers_;
}

void D3D12CommandList::BeginRendering(const RenderingInfo &info) {
  if (submitted_ || rendering_ || info.width == 0 || info.height == 0)
    throw std::logic_error("invalid D3D12 BeginRendering");
  auto &target = device_.ValidateRenderTarget(info.color_target);
  list_->OMSetRenderTargets(1, &target.rtv, FALSE, nullptr);
  const float clear[] = {0.0F, 0.0F, 0.0F, 1.0F};
  list_->ClearRenderTargetView(target.rtv, clear, 0, nullptr);
  D3D12_VIEWPORT viewport{0.0F, 0.0F, static_cast<float>(info.width),
                          static_cast<float>(info.height), 0.0F, 1.0F};
  D3D12_RECT scissor{0, 0, static_cast<LONG>(info.width), static_cast<LONG>(info.height)};
  list_->RSSetViewports(1, &viewport);
  list_->RSSetScissorRects(1, &scissor);
  list_->SetGraphicsRootSignature(device_.RootSignature());
  list_->SetGraphicsRootConstantBufferView(0, device_.ConstantsAddress());
  rendering_ = true;
  pipeline_bound_ = false;
}

void D3D12CommandList::BindPipeline(PipelineHandle pipeline) {
  if (submitted_ || !rendering_)
    throw std::logic_error("D3D12 pipeline binding requires rendering");
  list_->SetPipelineState(device_.ValidatePipeline(pipeline));
  pipeline_bound_ = true;
}

void D3D12CommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  if (submitted_ || !rendering_ || !pipeline_bound_ || vertex_count == 0 ||
      instance_count == 0)
    throw std::logic_error("invalid D3D12 draw");
  list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list_->DrawInstanced(vertex_count, instance_count, 0, 0);
  ++draws_;
}

void D3D12CommandList::EndRendering() {
  if (submitted_ || !rendering_)
    throw std::logic_error("D3D12 EndRendering without BeginRendering");
  rendering_ = false;
}

void D3D12CommandList::Close() {
  if (closed_)
    return;
  Check(list_->Close(), "ID3D12GraphicsCommandList::Close");
  closed_ = true;
}
} // namespace

std::unique_ptr<Device> CreateDirect3D12Device() {
  return std::make_unique<D3D12Device>();
}
} // namespace nexora::rhi
