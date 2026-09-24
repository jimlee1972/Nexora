#if !defined(_WIN32)
#error "Dx12Surface.cpp is only built on Windows"
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Nexora/Presentation/Surface.h"
#include <array>
#include <atomic>
#include <cstring>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

namespace Nexora::Presentation {
std::unique_ptr<ISurface> CreateVulkanSurface(const SurfaceDescriptor &, Window::IWindowSystem &);
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kMaximumFrames = 3;
class Dx12Surface final : public ISurface {
public:
  Dx12Surface(const SurfaceDescriptor &d, Window::IWindowSystem &windows)
      : renderThread_(std::this_thread::get_id()),
        window_(static_cast<HWND>(windows.NativeHandle(d.window))), width_(d.width),
        height_(d.height), frames_(d.framesInFlight), mode_(d.presentMode) {
    if (!window_ || frames_ < 2 || frames_ > kMaximumFrames || d.colorSpace != ColorSpace::Srgb)
      return;
    UINT flags = 0;
    if (SUCCEEDED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)))) {
      BOOL tearing = FALSE;
      allowTearing_ = SUCCEEDED(factory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                              &tearing, sizeof(tearing))) &&
                      tearing;
    }
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory_ && factory_->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i) {
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
          SUCCEEDED(
              D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
        break;
      adapter.Reset();
    }
    if (!device_ && factory_) {
      factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
      D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
    }
    D3D12_COMMAND_QUEUE_DESC q{};
    if (!device_ || FAILED(device_->CreateCommandQueue(&q, IID_PPV_ARGS(&queue_))))
      return;
    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))))
      return;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_)
      return;
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = frames_;
    if (FAILED(device_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_))))
      return;
    increment_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < frames_; ++i)
      if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 IID_PPV_ARGS(&allocators_[i]))))
        return;
    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(),
                                          nullptr, IID_PPV_ARGS(&commands_))))
      return;
    commands_->Close();
    valid_ = CreateSwapchain(width_, height_) && CreateUiResources();
  }
  ~Dx12Surface() override { DrainAndDestroy(); }
  std::thread::id RenderThread() const noexcept override { return renderThread_; }
  SurfaceStatus NotifyWindowExtent(uint32_t w, uint32_t h) noexcept override {
    pendingWidth_.store(w);
    pendingHeight_.store(h);
    dirty_.store(true);
    return (!w || !h) ? SurfaceStatus::ZeroExtent : SurfaceStatus::Ready;
  }
  SurfaceStatus Acquire() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!valid_)
      return SurfaceStatus::Unsupported;
    if (destroyed_)
      return SurfaceStatus::SurfaceLost;
    if (auto s = ApplyResize(); s != SurfaceStatus::Ready)
      return s;
    frame_ = swapchain_->GetCurrentBackBufferIndex();
    if (fenceValues_[frame_] && fence_->GetCompletedValue() < fenceValues_[frame_]) {
      fence_->SetEventOnCompletion(fenceValues_[frame_], event_);
      WaitForSingleObject(event_, INFINITE);
      ++diagnostics_.fenceWaits;
    }
    retired_[frame_].clear();
    allocators_[frame_]->Reset();
    commands_->Reset(allocators_[frame_].Get(), nullptr);
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {buffers_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET};
    commands_->ResourceBarrier(1, &b);
    auto rtv = heap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += SIZE_T(frame_) * increment_;
    constexpr float color[] = {0.04f, 0.08f, 0.16f, 1};
    commands_->ClearRenderTargetView(rtv, color, 0, nullptr);
    ++diagnostics_.acquiredFrames;
    acquired_ = true;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus Present() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_)
      return SurfaceStatus::OutOfDate;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {buffers_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT};
    commands_->ResourceBarrier(1, &b);
    commands_->Close();
    ID3D12CommandList *l[] = {commands_.Get()};
    queue_->ExecuteCommandLists(1, l);
    auto hr = swapchain_->Present(
        mode_ == PresentMode::VSync ? 1 : 0,
        mode_ == PresentMode::Immediate && allowTearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0);
    diagnostics_.lastPlatformResult = hr;
    acquired_ = false;
    if (hr == DXGI_STATUS_OCCLUDED)
      return SurfaceStatus::Occluded;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
      return SurfaceStatus::DeviceLost;
    if (FAILED(hr))
      return SurfaceStatus::SurfaceLost;
    fenceValues_[frame_] = ++fenceValue_;
    queue_->Signal(fence_.Get(), fenceValue_);
    ++diagnostics_.presentedFrames;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus RenderUi(const UiDrawData &drawData) override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || drawData.vertices.empty() || drawData.indices.empty())
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &upload : drawData.textureUploads)
      if (!UploadUiTexture(upload))
        return SurfaceStatus::DeviceLost;
    const auto vertices = std::as_bytes(drawData.vertices);
    const auto required = vertices.size() + drawData.indices.size();
    if (!EnsureUiUpload(required))
      return SurfaceStatus::DeviceLost;
    void *mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    if (FAILED(uiUploads_[frame_]->Map(0, &noRead, &mapped)))
      return SurfaceStatus::DeviceLost;
    std::memcpy(mapped, vertices.data(), vertices.size());
    std::memcpy(static_cast<std::byte *>(mapped) + vertices.size(), drawData.indices.data(),
                drawData.indices.size());
    uiUploads_[frame_]->Unmap(0, nullptr);
    auto rtv = heap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += SIZE_T(frame_) * increment_;
    commands_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    commands_->SetGraphicsRootSignature(uiRootSignature_.Get());
    commands_->SetPipelineState(uiPipeline_.Get());
    ID3D12DescriptorHeap *heaps[] = {uiDescriptors_.Get()};
    commands_->SetDescriptorHeaps(1, heaps);
    const float transform[4] = {2.0F / static_cast<float>(width_),
                                -2.0F / static_cast<float>(height_), -1.0F, 1.0F};
    commands_->SetGraphicsRoot32BitConstants(0, 4, transform, 0);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_),
                                  0, 1};
    commands_->RSSetViewports(1, &viewport);
    const D3D12_VERTEX_BUFFER_VIEW vertexView{uiUploads_[frame_]->GetGPUVirtualAddress(),
                                              static_cast<UINT>(vertices.size()), sizeof(UiVertex)};
    const D3D12_INDEX_BUFFER_VIEW indexView{
        uiUploads_[frame_]->GetGPUVirtualAddress() + vertices.size(),
        static_cast<UINT>(drawData.indices.size()),
        drawData.indices32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT};
    commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands_->IASetVertexBuffers(0, 1, &vertexView);
    commands_->IASetIndexBuffer(&indexView);
    for (const auto &command : drawData.commands) {
      const auto texture = uiTextures_.find(command.textureId);
      if (texture == uiTextures_.end()) {
        ++diagnostics_.nativeUiRejectedTextures;
        continue;
      }
      auto descriptor = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
      descriptor.ptr += static_cast<UINT64>(texture->second.descriptor) * uiDescriptorIncrement_;
      commands_->SetGraphicsRootDescriptorTable(1, descriptor);
      const D3D12_RECT scissor{command.clipX, command.clipY,
                               command.clipX + static_cast<LONG>(command.clipWidth),
                               command.clipY + static_cast<LONG>(command.clipHeight)};
      commands_->RSSetScissorRects(1, &scissor);
      commands_->DrawIndexedInstanced(command.elementCount, 1, command.indexOffset,
                                      command.vertexOffset, 0);
      ++diagnostics_.nativeUiDrawCalls;
    }
    return SurfaceStatus::Ready;
  }
  SurfaceDiagnostics Diagnostics() const noexcept override { return diagnostics_; }
  SurfaceStatus DrainAndDestroy() override {
    if (destroyed_)
      return SurfaceStatus::Ready;
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (queue_ && fence_) {
      queue_->Signal(fence_.Get(), ++fenceValue_);
      fence_->SetEventOnCompletion(fenceValue_, event_);
      WaitForSingleObject(event_, INFINITE);
    }
    for (auto &b : buffers_)
      b.Reset();
    for (auto &upload : uiUploads_)
      upload.Reset();
    uiTextures_.clear();
    for (auto &retired : retired_)
      retired.clear();
    uiPipeline_.Reset();
    uiRootSignature_.Reset();
    uiDescriptors_.Reset();
    swapchain_.Reset();
    destroyed_ = true;
    if (event_) {
      CloseHandle(event_);
      event_ = nullptr;
    }
    return SurfaceStatus::Ready;
  }

private:
  struct UiTexture final {
    ComPtr<ID3D12Resource> resource;
    UINT descriptor{};
  };
  bool OnThread() const { return std::this_thread::get_id() == renderThread_; }
  bool CreateUiResources() {
    D3D12_DESCRIPTOR_HEAP_DESC descriptors{};
    descriptors.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptors.NumDescriptors = 4096;
    descriptors.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device_->CreateDescriptorHeap(&descriptors, IID_PPV_ARGS(&uiDescriptors_))))
      return false;
    uiDescriptorIncrement_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.Num32BitValues = 4;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &range;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = 2;
    root.pParameters = parameters;
    root.NumStaticSamplers = 1;
    root.pStaticSamplers = &sampler;
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature, errors;
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature,
                                           &errors)) ||
        FAILED(device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                            signature->GetBufferSize(),
                                            IID_PPV_ARGS(&uiRootSignature_))))
      return false;
    constexpr char shader[] = R"(
      cbuffer Transform : register(b0) { float2 scale; float2 translate; };
      struct VSInput { float2 position : POSITION; float2 uv : TEXCOORD0; float4 color : COLOR0; };
      struct PSInput { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; };
      PSInput VSMain(VSInput input) { PSInput output; output.position = float4(input.position * scale + translate, 0, 1); output.uv = input.uv; output.color = input.color; return output; }
      Texture2D texture0 : register(t0); SamplerState sampler0 : register(s0);
      float4 PSMain(PSInput input) : SV_Target { return input.color * texture0.Sample(sampler0, input.uv); }
    )";
    ComPtr<ID3DBlob> vertex, pixel;
    if (FAILED(D3DCompile(shader, sizeof(shader), "NexoraEditorUi", nullptr, nullptr, "VSMain",
                          "vs_5_0", 0, 0, &vertex, &errors)) ||
        FAILED(D3DCompile(shader, sizeof(shader), "NexoraEditorUi", nullptr, nullptr, "PSMain",
                          "ps_5_0", 0, 0, &pixel, &errors)))
      return false;
    const D3D12_INPUT_ELEMENT_DESC inputs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UiVertex, position),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UiVertex, uv),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(UiVertex, color),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = uiRootSignature_.Get();
    pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    pipeline.BlendState.RenderTarget[0].BlendEnable = TRUE;
    pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pipeline.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pipeline.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.SampleMask = UINT_MAX;
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    pipeline.InputLayout = {inputs, static_cast<UINT>(std::size(inputs))};
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline.SampleDesc.Count = 1;
    return SUCCEEDED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&uiPipeline_)));
  }
  bool EnsureUiUpload(std::size_t required) {
    if (uiUploadCapacity_[frame_] >= required)
      return true;
    uiUploadCapacity_[frame_] = 4096;
    while (uiUploadCapacity_[frame_] < required)
      uiUploadCapacity_[frame_] *= 2;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = uiUploadCapacity_[frame_];
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&uiUploads_[frame_]))))
      return false;
    ++diagnostics_.nativeUiBufferReallocations;
    return true;
  }
  bool UploadUiTexture(const UiTextureUpload &upload) {
    if (upload.textureId == 0 || upload.width == 0 || upload.height == 0 ||
        upload.rowPitch != upload.width * 4U ||
        upload.pixels.size() != static_cast<std::size_t>(upload.rowPitch) * upload.height)
      return false;
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC texture{};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = upload.width;
    texture.Height = upload.height;
    texture.DepthOrArraySize = 1;
    texture.MipLevels = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count = 1;
    ComPtr<ID3D12Resource> resource;
    if (FAILED(device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texture,
                                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                IID_PPV_ARGS(&resource))))
      return false;
    const UINT alignedPitch = (upload.rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &
                              ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC stagingDesc{};
    stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    stagingDesc.Width = static_cast<UINT64>(alignedPitch) * upload.height;
    stagingDesc.Height = 1;
    stagingDesc.DepthOrArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> staging;
    if (FAILED(device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &stagingDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&staging))))
      return false;
    void *mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    if (FAILED(staging->Map(0, &noRead, &mapped)))
      return false;
    for (std::uint32_t row = 0; row < upload.height; ++row)
      std::memcpy(static_cast<std::byte *>(mapped) + static_cast<std::size_t>(row) * alignedPitch,
                  upload.pixels.data() + static_cast<std::size_t>(row) * upload.rowPitch,
                  upload.rowPitch);
    staging->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = resource.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = staging.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, upload.width, upload.height, 1,
                                        alignedPitch};
    commands_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                          D3D12_RESOURCE_STATE_COPY_DEST,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    commands_->ResourceBarrier(1, &barrier);
    if (nextUiDescriptor_ >= 4096)
      return false;
    const UINT descriptor = nextUiDescriptor_++;
    if (const auto previous = uiTextures_.find(upload.textureId); previous != uiTextures_.end()) {
      retired_[frame_].push_back(previous->second.resource);
    }
    auto cpu = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(descriptor) * uiDescriptorIncrement_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(resource.Get(), &view, cpu);
    uiTextures_[upload.textureId] = {resource, descriptor};
    retired_[frame_].push_back(staging);
    ++diagnostics_.nativeUiTextureUploads;
    return true;
  }
  bool CreateSwapchain(UINT w, UINT h) {
    if (!w || !h)
      return true;
    DXGI_SWAP_CHAIN_DESC1 d{};
    d.Width = w;
    d.Height = h;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = frames_;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    d.Flags = allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    ComPtr<IDXGISwapChain1> s;
    if (FAILED(factory_->CreateSwapChainForHwnd(queue_.Get(), window_, &d, nullptr, nullptr, &s)))
      return false;
    s.As(&swapchain_);
    factory_->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER);
    return RebuildBuffers();
  }
  bool RebuildBuffers() {
    for (UINT i = 0; i < frames_; ++i) {
      if (FAILED(swapchain_->GetBuffer(i, IID_PPV_ARGS(&buffers_[i]))))
        return false;
      auto r = heap_->GetCPUDescriptorHandleForHeapStart();
      r.ptr += SIZE_T(i) * increment_;
      device_->CreateRenderTargetView(buffers_[i].Get(), nullptr, r);
    }
    return true;
  }
  SurfaceStatus ApplyResize() {
    if (!dirty_.exchange(false))
      return (!width_ || !height_) ? SurfaceStatus::ZeroExtent : SurfaceStatus::Ready;
    auto w = pendingWidth_.load(), h = pendingHeight_.load();
    if (!w || !h) {
      width_ = w;
      height_ = h;
      return SurfaceStatus::ZeroExtent;
    }
    queue_->Signal(fence_.Get(), ++fenceValue_);
    fence_->SetEventOnCompletion(fenceValue_, event_);
    WaitForSingleObject(event_, INFINITE);
    for (auto &b : buffers_)
      b.Reset();
    auto hr = swapchain_->ResizeBuffers(frames_, w, h, DXGI_FORMAT_R8G8B8A8_UNORM,
                                        allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    diagnostics_.lastPlatformResult = hr;
    if (FAILED(hr))
      return hr == DXGI_ERROR_DEVICE_REMOVED ? SurfaceStatus::DeviceLost : SurfaceStatus::OutOfDate;
    width_ = w;
    height_ = h;
    ++diagnostics_.resizeGenerations;
    return RebuildBuffers() ? SurfaceStatus::Ready : SurfaceStatus::SurfaceLost;
  }
  std::thread::id renderThread_;
  HWND window_{};
  uint32_t width_{}, height_{};
  UINT frames_{}, frame_{};
  PresentMode mode_{};
  bool allowTearing_{}, valid_{}, destroyed_{}, acquired_{};
  std::atomic<uint32_t> pendingWidth_{}, pendingHeight_{};
  std::atomic_bool dirty_{};
  HANDLE event_{};
  UINT increment_{};
  UINT uiDescriptorIncrement_{};
  UINT nextUiDescriptor_{};
  uint64_t fenceValue_{};
  std::array<uint64_t, kMaximumFrames> fenceValues_{};
  SurfaceDiagnostics diagnostics_{};
  ComPtr<IDXGIFactory6> factory_;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12Fence> fence_;
  ComPtr<ID3D12DescriptorHeap> heap_;
  ComPtr<ID3D12DescriptorHeap> uiDescriptors_;
  ComPtr<ID3D12RootSignature> uiRootSignature_;
  ComPtr<ID3D12PipelineState> uiPipeline_;
  ComPtr<ID3D12GraphicsCommandList> commands_;
  std::array<ComPtr<ID3D12CommandAllocator>, kMaximumFrames> allocators_;
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> buffers_;
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> uiUploads_;
  std::array<std::size_t, kMaximumFrames> uiUploadCapacity_{};
  std::array<std::vector<ComPtr<ID3D12Resource>>, kMaximumFrames> retired_;
  std::unordered_map<std::uint64_t, UiTexture> uiTextures_;
  ComPtr<IDXGISwapChain3> swapchain_;
};
} // namespace
std::unique_ptr<ISurface> CreateSurface(const SurfaceDescriptor &d, Window::IWindowSystem &w) {
#if defined(NEXORA_HAS_VULKAN_PRESENTATION)
  if (d.backend == SurfaceBackend::Vulkan)
    return CreateVulkanSurface(d, w);
#endif
  if (d.backend != SurfaceBackend::Automatic && d.backend != SurfaceBackend::Dx12)
    return {};
  return std::make_unique<Dx12Surface>(d, w);
}
} // namespace Nexora::Presentation
