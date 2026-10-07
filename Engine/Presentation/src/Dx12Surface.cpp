#if !defined(_WIN32)
#error "Dx12Surface.cpp is only built on Windows"
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Nexora/Presentation/Surface.h"
#include "PbrMaterialUpload.h"
#include "SceneInstanceUpload.h"
#include "ScenePbrHlslShaders.h"
#include "SceneTextureMipmaps.h"
#include "SceneToneHlslShaders.h"
#include "ToneParametersUpload.h"
#include "ToneVertexUpload.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
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
      diagnostics_.softwareRasterizer = true;
      factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
      D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
    }
    if (device_ && adapter) {
      DXGI_ADAPTER_DESC1 desc{};
      if (SUCCEEDED(adapter->GetDesc1(&desc))) {
        auto &identity = diagnostics_.device;
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, desc.Description, -1,
                                 identity.name.data(), static_cast<int>(identity.name.size()),
                                 nullptr, nullptr))
          identity.name.fill(0);
        identity.vendorId = desc.VendorId;
        identity.deviceId = desc.DeviceId;
        identity.deviceIdsAvailable = true;
        diagnostics_.softwareRasterizer = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        LARGE_INTEGER version{};
        if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version))) {
          identity.driverVersion = static_cast<std::uint64_t>(version.QuadPart);
          identity.driverVersionFormat = DriverVersionFormat::DxgiUmd;
        }
      }
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
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = frames_ * 2;
    if (FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))))
      return;
    dsvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    for (UINT i = 0; i < frames_; ++i)
      if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 IID_PPV_ARGS(&allocators_[i]))))
        return;
    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(),
                                          nullptr, IID_PPV_ARGS(&commands_))))
      return;
    commands_->Close();
    valid_ = CreateSwapchain(width_, height_) && CreateUiResources() && CreateSceneResources();
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
    if (acquired_)
      return SurfaceStatus::InvalidDescriptor;
    if (auto s = ApplyResize(); s != SurfaceStatus::Ready)
      return s;
    frame_ = swapchain_->GetCurrentBackBufferIndex();
    if (fenceValues_[frame_] && fence_->GetCompletedValue() < fenceValues_[frame_]) {
      fence_->SetEventOnCompletion(fenceValues_[frame_], event_);
      WaitForSingleObject(event_, INFINITE);
      ++diagnostics_.fenceWaits;
    }
    retired_[frame_].clear();
    freeUiDescriptors_.insert(freeUiDescriptors_.end(), retiredUiDescriptors_[frame_].begin(),
                              retiredUiDescriptors_[frame_].end());
    retiredUiDescriptors_[frame_].clear();
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
    if (dsvHeap_) {
      auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
      dsv.ptr += SIZE_T(frame_) * dsvIncrement_;
      commands_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
    ++diagnostics_.acquiredFrames;
    acquired_ = true;
    sceneDrawn_ = sceneOffscreen_ = sceneComposited_ = uiDrawn_ = compositeDrawn_ = false;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus CompositeRgba8(std::span<const std::byte> pixels, std::uint32_t width,
                               std::uint32_t height) override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || sceneDrawn_ || uiDrawn_ || compositeDrawn_ || width != width_ ||
        height != height_ || pixels.size() != static_cast<std::size_t>(width) * height * 4U)
      return SurfaceStatus::InvalidDescriptor;
    const UINT rowPitch = width * 4U;
    const UINT alignedPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &
                              ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC stagingDesc{};
    stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    stagingDesc.Width = static_cast<UINT64>(alignedPitch) * height;
    stagingDesc.Height = 1;
    stagingDesc.DepthOrArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> staging;
    if (FAILED(device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &stagingDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&staging))))
      return SurfaceStatus::DeviceLost;
    void *mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    if (FAILED(staging->Map(0, &noRead, &mapped)))
      return SurfaceStatus::DeviceLost;
    for (std::uint32_t row = 0; row < height; ++row)
      std::memcpy(static_cast<std::byte *>(mapped) + static_cast<std::size_t>(row) * alignedPitch,
                  pixels.data() + static_cast<std::size_t>(row) * rowPitch, rowPitch);
    staging->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER toCopyDest{};
    toCopyDest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopyDest.Transition = {buffers_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                             D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST};
    commands_->ResourceBarrier(1, &toCopyDest);
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = buffers_[frame_].Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = staging.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, alignedPitch};
    commands_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    D3D12_RESOURCE_BARRIER toRenderTarget{};
    toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRenderTarget.Transition = {buffers_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                 D3D12_RESOURCE_STATE_COPY_DEST,
                                 D3D12_RESOURCE_STATE_RENDER_TARGET};
    commands_->ResourceBarrier(1, &toRenderTarget);
    retired_[frame_].push_back(staging);
    compositeDrawn_ = true;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus Present() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_)
      return SurfaceStatus::OutOfDate;
    if (sceneOffscreen_ && !sceneComposited_)
      return SurfaceStatus::InvalidDescriptor;
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
    if (!acquired_ || uiDrawn_ || compositeDrawn_ || (sceneOffscreen_ && !sceneComposited_) ||
        drawData.vertices.empty() || drawData.indices.empty())
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
    uiDrawn_ = true;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus DrawScene(const SceneDrawData &drawData) override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || sceneDrawn_ || compositeDrawn_ || !scenePipeline_ ||
        drawData.vertices.empty() || drawData.indices.empty())
      return SurfaceStatus::InvalidDescriptor;
    const auto viewport = ResolveSceneViewport(drawData.viewport, width_, height_);
    if (!viewport ||
        (uiDrawn_ &&
         (drawData.offscreen || (drawData.viewport.x == 0 && drawData.viewport.y == 0 &&
                                 drawData.viewport.width == 0 && drawData.viewport.height == 0))) ||
        (drawData.offscreen && (drawData.viewport.x != 0 || drawData.viewport.y != 0 ||
                                drawData.viewport.width != 0 || drawData.viewport.height != 0)))
      return SurfaceStatus::InvalidDescriptor;
    if (drawData.vertices.size() > 65535 || drawData.indices.size() > 1048576 ||
        drawData.indices.size() % 3 != 0 || drawData.instances.size() > 4096 ||
        !ValidateSceneMeshBatches(drawData.batches, drawData.indices.size(),
                                  std::max<std::size_t>(drawData.instances.size(), 1)))
      return SurfaceStatus::InvalidDescriptor;
    for (const auto index : drawData.indices)
      if (index >= drawData.vertices.size())
        return SurfaceStatus::InvalidDescriptor;
    const auto packedInstances = PackSceneInstances(drawData.instances);
    if (!packedInstances)
      return SurfaceStatus::InvalidDescriptor;
    if (!ValidateSceneMaterials(drawData.materials, drawData.batches) || !ValidatePbrData(drawData))
      return SurfaceStatus::InvalidDescriptor;
    if (!ValidateEnvironmentResidency(drawData, [&](std::uint64_t id) {
          const auto found = linearSceneTextures_.find(id);
          return found == linearSceneTextures_.end() ? 0U : found->second.mipLevels;
        }))
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &upload : drawData.linearTextureUploads)
      if (sceneTextures_.contains(upload.textureId))
        return SurfaceStatus::InvalidDescriptor;
    for (const auto &upload : drawData.textureUploads)
      if (linearSceneTextures_.contains(upload.textureId))
        return SurfaceStatus::InvalidDescriptor;
    if (drawData.textureUploads.size() > 16)
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &upload : drawData.textureUploads)
      if (upload.textureId == 0 || upload.textureId >= UINT64_MAX - 1 || upload.width == 0 ||
          upload.height == 0 || upload.width > 1024 || upload.height > 1024 ||
          upload.rowPitch != upload.width * 4U ||
          upload.pixels.size() != static_cast<std::size_t>(upload.rowPitch) * upload.height)
        return SurfaceStatus::InvalidDescriptor;
    if (drawData.textureId >= UINT64_MAX - 1)
      return SurfaceStatus::InvalidDescriptor;
    const auto textureId = drawData.textureId ? drawData.textureId : UINT64_MAX;
    if (drawData.textureId && !sceneTextures_.contains(textureId) &&
        std::none_of(drawData.textureUploads.begin(), drawData.textureUploads.end(),
                     [textureId](const auto &upload) { return upload.textureId == textureId; }))
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &material : drawData.materials)
      if (material.textureId && !sceneTextures_.contains(material.textureId) &&
          std::none_of(
              drawData.textureUploads.begin(), drawData.textureUploads.end(),
              [&material](const auto &upload) { return upload.textureId == material.textureId; }))
        return SurfaceStatus::InvalidDescriptor;
    if (drawData.pbr) {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_R16G16B16A16_FLOAT};
      const auto required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE |
                            D3D12_FORMAT_SUPPORT1_MIP |
                            (drawData.hdr ? D3D12_FORMAT_SUPPORT1_RENDER_TARGET : 0);
      if (FAILED(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support,
                                              sizeof(support))) ||
          (support.Support1 & required) != required)
        return SurfaceStatus::Unsupported;
      if (!scenePbrPipeline_ || (drawData.hdr && (!sceneHdrPipeline_ || !tonePipeline_)))
        return SurfaceStatus::Unsupported;
      for (const auto &material : drawData.materials)
        for (const auto id :
             {material.normalTextureId, material.ormTextureId, material.emissionTextureId})
          if (id && !sceneTextures_.contains(id) &&
              std::none_of(drawData.textureUploads.begin(), drawData.textureUploads.end(),
                           [id](const auto &upload) { return upload.textureId == id; }))
            return SurfaceStatus::InvalidDescriptor;
    }
    const bool needsWhite =
        drawData.pbr || textureId == UINT64_MAX ||
        std::any_of(drawData.materials.begin(), drawData.materials.end(),
                    [](const auto &material) { return material.textureId == 0; });
    for (const auto &vertex : drawData.vertices)
      for (const auto value : vertex.uv)
        if (!std::isfinite(value))
          return SurfaceStatus::InvalidDescriptor;
    const SceneInstance identity{};
    const auto instances = drawData.instances.empty() ? std::span<const SceneInstance>(&identity, 1)
                                                      : drawData.instances;
    const auto instanceBytes =
        std::as_bytes(std::span<const SceneInstanceUpload>(*packedInstances));
    const auto vertexBytes = std::as_bytes(drawData.vertices);
    const auto indexBytes = std::as_bytes(drawData.indices);
    const auto instanceOffset = (vertexBytes.size() + indexBytes.size() + 3) & ~std::size_t{3};
    struct SceneConstants final {
      float mvp[16];
      float lightDirection[3];
      float _pad0{};
      float lightColor[3];
      float _pad1{};
      float baseColor[4];
    } constants{};
    std::memcpy(constants.mvp, drawData.model_view_projection, sizeof(constants.mvp));
    std::memcpy(constants.lightDirection, drawData.light_direction,
                sizeof(constants.lightDirection));
    std::memcpy(constants.lightColor, drawData.light_color, sizeof(constants.lightColor));
    std::memcpy(constants.baseColor, drawData.base_color, sizeof(constants.baseColor));
    // Constants live first, at offset 0 -- a committed resource's base GPU VA is always far more
    // aligned than the 256 bytes a root CBV requires, so offset 0 is always valid there. Vertex and
    // index data start after the bounded material palette (aligned constants per material),
    // so neither section can ever overlap regardless of how large the mesh grows.
    const std::size_t materialStride = drawData.pbr ? 768 : 256;
    const std::size_t shadowOffset =
        std::max<std::size_t>(drawData.materials.size(), 1) * materialStride;
    const std::size_t kGeometryOffset = shadowOffset + (drawData.shadow ? 256 : 0);
    const auto toneOffset = kGeometryOffset + instanceOffset + instanceBytes.size();
    const auto required = toneOffset + (drawData.hdr ? sizeof(toneVertices) : 0);
    toneOffsets_[frame_] = toneOffset;
    if (!EnsureSceneUpload(required))
      return SurfaceStatus::DeviceLost;
    void *mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    if (FAILED(sceneUploads_[frame_]->Map(0, &noRead, &mapped)))
      return SurfaceStatus::DeviceLost;
    for (std::size_t slot = 0; slot < std::max<std::size_t>(drawData.materials.size(), 1); ++slot) {
      const auto material = ResolveSceneMaterial(drawData, slot);
      std::copy(material.baseColor.begin(), material.baseColor.end(), constants.baseColor);
      std::memcpy(static_cast<std::byte *>(mapped) + slot * materialStride, &constants,
                  sizeof(constants));
      if (drawData.pbr) {
        const auto parameters = PackPbrMaterial(drawData, material, true, true, width_, height_);
        std::memcpy(static_cast<std::byte *>(mapped) + slot * materialStride + 256,
                    parameters.data(), sizeof(parameters));
      }
    }
    if (drawData.shadow) {
      std::array<float, 28> shadowConstants{};
      std::copy(drawData.shadow->lightViewProjection.begin(),
                drawData.shadow->lightViewProjection.end(), shadowConstants.begin());
      std::memcpy(static_cast<std::byte *>(mapped) + shadowOffset, shadowConstants.data(),
                  sizeof(shadowConstants));
    }
    std::memcpy(static_cast<std::byte *>(mapped) + kGeometryOffset, vertexBytes.data(),
                vertexBytes.size());
    std::memcpy(static_cast<std::byte *>(mapped) + kGeometryOffset + vertexBytes.size(),
                indexBytes.data(), indexBytes.size());
    std::memcpy(static_cast<std::byte *>(mapped) + kGeometryOffset + instanceOffset,
                instanceBytes.data(), instanceBytes.size());
    if (drawData.hdr)
      std::memcpy(static_cast<std::byte *>(mapped) + toneOffset, toneVertices.data(),
                  sizeof(toneVertices));
    sceneUploads_[frame_]->Unmap(0, nullptr);
    std::size_t additional = needsWhite && !sceneTextures_.contains(UINT64_MAX) ? 1 : 0;
    if (drawData.pbr && !sceneTextures_.contains(UINT64_MAX - 1))
      ++additional;
    for (std::size_t i = 0; i < drawData.textureUploads.size(); ++i) {
      const auto id = drawData.textureUploads[i].textureId;
      for (std::size_t j = 0; j < i; ++j)
        if (drawData.textureUploads[j].textureId == id)
          return SurfaceStatus::InvalidDescriptor;
      additional += sceneTextures_.contains(id) ? 0 : 1;
    }
    if (sceneTextures_.size() + additional > 64)
      return SurfaceStatus::Unsupported;
    std::size_t additionalLinear =
        drawData.pbr && !linearSceneTextures_.contains(UINT64_MAX) ? 1 : 0;
    for (const auto &upload : drawData.linearTextureUploads)
      additionalLinear += linearSceneTextures_.contains(upload.textureId) ? 0 : 1;
    if (linearSceneTextures_.size() + additionalLinear > 16)
      return SurfaceStatus::Unsupported;
    for (const auto &upload : drawData.linearTextureUploads)
      if (!linearSceneTextures_.contains(upload.textureId) &&
          !UploadUiTexture(
              {upload.textureId, upload.width, upload.height, upload.width * 8, upload.pixels},
              true, upload.mipLevels, true))
        return SurfaceStatus::DeviceLost;
    const std::array<std::byte, 8> blackEnvironment{};
    if (drawData.pbr && !linearSceneTextures_.contains(UINT64_MAX) &&
        !UploadUiTexture({UINT64_MAX, 1, 1, 8, blackEnvironment}, true, 1, true))
      return SurfaceStatus::DeviceLost;
    for (const auto &upload : drawData.textureUploads)
      if (!sceneTextures_.contains(upload.textureId)) {
        const auto mips =
            BuildSceneTextureMipmaps(upload, ResolveSceneMipSemantic(drawData, upload.textureId));
        if (!UploadUiTexture(mips.Upload(upload), true, mips.levels))
          return SurfaceStatus::DeviceLost;
      }
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    if (needsWhite && !sceneTextures_.contains(UINT64_MAX) &&
        !UploadUiTexture({UINT64_MAX, 1, 1, 4, white}, true))
      return SurfaceStatus::DeviceLost;
    const std::array<std::byte, 4> flatNormal{std::byte{128}, std::byte{128}, std::byte{255},
                                              std::byte{255}};
    if (drawData.pbr && !sceneTextures_.contains(UINT64_MAX - 1) &&
        !UploadUiTexture({UINT64_MAX - 1, 1, 1, 4, flatNormal}, true))
      return SurfaceStatus::DeviceLost;
    const bool refraction = HasSceneRefraction(drawData);
    const bool reflection = drawData.planarReflection.has_value();
    D3D12_CPU_DESCRIPTOR_HANDLE reflectionRtv{};
    const auto base = sceneUploads_[frame_]->GetGPUVirtualAddress();
    const auto geometryBase = base + kGeometryOffset;
    auto rtv = heap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += SIZE_T(frame_) * increment_;
    if (drawData.offscreen) {
      auto descriptor = buffers_[frame_]->GetDesc();
      descriptor.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
      if (drawData.hdr)
        descriptor.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      D3D12_HEAP_PROPERTIES heap{};
      heap.Type = D3D12_HEAP_TYPE_DEFAULT;
      heap.CreationNodeMask = heap.VisibleNodeMask = 1;
      D3D12_CLEAR_VALUE clear{};
      clear.Format = descriptor.Format;
      clear.Color[0] = 0.025F;
      clear.Color[1] = 0.045F;
      clear.Color[2] = 0.09F;
      clear.Color[3] = drawData.hdr ? 65504.0F : 1.0F;
      if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &descriptor,
                                                  D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
                                                  IID_PPV_ARGS(&sceneColors_[frame_]))))
        return SurfaceStatus::DeviceLost;
      rtv = sceneRtvHeap_->GetCPUDescriptorHandleForHeapStart();
      rtv.ptr += SIZE_T(frame_) * increment_;
      device_->CreateRenderTargetView(sceneColors_[frame_].Get(), nullptr, rtv);
      if (drawData.hdr) {
        auto handle = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += SIZE_T(frame_) * uiDescriptorIncrement_;
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(sceneColors_[frame_].Get(), &view, handle);
      }
      if (refraction) {
        descriptor.Flags = D3D12_RESOURCE_FLAG_NONE;
        if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &descriptor,
                                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                    nullptr,
                                                    IID_PPV_ARGS(&refractionColors_[frame_]))))
          return SurfaceStatus::DeviceLost;
        auto snapshotHandle = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
        snapshotHandle.ptr += SIZE_T(2 * kMaximumFrames + frame_) * uiDescriptorIncrement_;
        D3D12_SHADER_RESOURCE_VIEW_DESC snapshotView{};
        snapshotView.Format = descriptor.Format;
        snapshotView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        snapshotView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        snapshotView.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(refractionColors_[frame_].Get(), &snapshotView,
                                          snapshotHandle);
      }
      if (reflection) {
        descriptor.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &descriptor,
                                                    D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
                                                    IID_PPV_ARGS(&reflectionColors_[frame_]))))
          return SurfaceStatus::DeviceLost;
        reflectionRtv = sceneRtvHeap_->GetCPUDescriptorHandleForHeapStart();
        reflectionRtv.ptr += SIZE_T(2 * frames_ + frame_) * increment_;
        device_->CreateRenderTargetView(reflectionColors_[frame_].Get(), nullptr, reflectionRtv);
        auto reflectionSrv = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
        reflectionSrv.ptr += SIZE_T(3 * kMaximumFrames + frame_) * uiDescriptorIncrement_;
        D3D12_SHADER_RESOURCE_VIEW_DESC reflectionView{};
        reflectionView.Format = descriptor.Format;
        reflectionView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        reflectionView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        reflectionView.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(reflectionColors_[frame_].Get(), &reflectionView,
                                          reflectionSrv);
        commands_->ClearRenderTargetView(reflectionRtv, clear.Color, 0, nullptr);
      }
      commands_->ClearRenderTargetView(rtv, clear.Color, 0, nullptr);
    }
    if (drawData.shadow &&
        !RecordShadow(drawData, geometryBase, base + shadowOffset, vertexBytes.size(),
                      indexBytes.size(), instanceOffset, instances.size(), base, materialStride))
      return SurfaceStatus::DeviceLost;
    auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    dsv.ptr += SIZE_T(frame_) * dsvIncrement_;
    commands_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commands_->SetGraphicsRootSignature(drawData.pbr ? scenePbrRootSignature_.Get()
                                                     : sceneRootSignature_.Get());
    commands_->SetPipelineState(
        drawData.hdr ? sceneHdrPipeline_.Get()
                     : (drawData.pbr ? scenePbrPipeline_.Get() : scenePipeline_.Get()));
    ID3D12DescriptorHeap *heaps[]{uiDescriptors_.Get()};
    commands_->SetDescriptorHeaps(1, heaps);
    const D3D12_VIEWPORT nativeViewport{static_cast<float>(viewport->x),
                                        static_cast<float>(viewport->y),
                                        static_cast<float>(viewport->width),
                                        static_cast<float>(viewport->height),
                                        0,
                                        1};
    commands_->RSSetViewports(1, &nativeViewport);
    const D3D12_RECT scissor{static_cast<LONG>(viewport->x), static_cast<LONG>(viewport->y),
                             static_cast<LONG>(viewport->x + viewport->width),
                             static_cast<LONG>(viewport->y + viewport->height)};
    commands_->RSSetScissorRects(1, &scissor);
    const D3D12_VERTEX_BUFFER_VIEW vertexView{geometryBase, static_cast<UINT>(vertexBytes.size()),
                                              sizeof(SceneVertex)};
    const D3D12_INDEX_BUFFER_VIEW indexView{geometryBase + vertexBytes.size(),
                                            static_cast<UINT>(indexBytes.size()),
                                            DXGI_FORMAT_R16_UINT};
    commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VERTEX_BUFFER_VIEW views[]{vertexView,
                                           {geometryBase + instanceOffset,
                                            static_cast<UINT>(instanceBytes.size()),
                                            sizeof(SceneInstanceUpload)}};
    commands_->IASetVertexBuffers(0, 2, views);
    commands_->IASetIndexBuffer(&indexView);
    const SceneMeshBatch whole{0, static_cast<std::uint32_t>(drawData.indices.size()), 0,
                               static_cast<std::uint32_t>(instances.size()), 0};
    const auto batches =
        drawData.batches.empty() ? std::span<const SceneMeshBatch>(&whole, 1) : drawData.batches;
    for (unsigned scenePass = 0; scenePass < (reflection ? 2U : 1U); ++scenePass) {
      const bool mirrorPass = reflection && scenePass == 0;
      if (scenePass == 1) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {
            reflectionColors_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        commands_->ResourceBarrier(1, &barrier);
      }
      const auto sceneRtv = mirrorPass ? reflectionRtv : rtv;
      commands_->OMSetRenderTargets(1, &sceneRtv, FALSE, &dsv);
      commands_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
      for (unsigned phase = 0; phase < 2; ++phase) {
        if (phase == 1 && refraction) {
          std::array<D3D12_RESOURCE_BARRIER, 2> snapshotBarriers{};
          for (auto &barrier : snapshotBarriers)
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          auto *source = mirrorPass ? reflectionColors_[frame_].Get() : sceneColors_[frame_].Get();
          snapshotBarriers[0].Transition = {source, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_COPY_SOURCE};
          snapshotBarriers[1].Transition = {
              refractionColors_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
          commands_->ResourceBarrier(2, snapshotBarriers.data());
          commands_->CopyResource(refractionColors_[frame_].Get(), source);
          for (auto &barrier : snapshotBarriers)
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
          commands_->ResourceBarrier(2, snapshotBarriers.data());
        }
        for (const auto &batch : batches) {
          const auto material = ResolveSceneMaterial(drawData, batch.materialIndex);
          const bool transparent = material.opacity < 1;
          if (material.opacity == 0 || transparent != (phase == 1) ||
              (material.reflectionRole == SceneReflectionRole::ReflectedGeometry) != mirrorPass)
            continue;
          commands_->SetPipelineState(transparent
                                          ? sceneHdrBlendPipeline_.Get()
                                          : (drawData.hdr ? sceneHdrPipeline_.Get()
                                                          : (drawData.pbr ? scenePbrPipeline_.Get()
                                                                          : scenePipeline_.Get())));
          const bool refractive = material.refractionIndex > 1 && material.refractionThickness > 0;
          const float backgroundCoverage = refractive ? 0 : 1 - material.opacity;
          const float coverage[]{backgroundCoverage * material.transparencyTint[0],
                                 backgroundCoverage * material.transparencyTint[1],
                                 backgroundCoverage * material.transparencyTint[2], 1};
          commands_->OMSetBlendFactor(coverage);

          const auto id = material.textureId ? material.textureId : UINT64_MAX;
          auto handle = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
          handle.ptr += UINT64(sceneTextures_.at(id).descriptor) * uiDescriptorIncrement_;
          commands_->SetGraphicsRootConstantBufferView(0,
                                                       base + batch.materialIndex * materialStride);
          if (drawData.pbr) {
            commands_->SetGraphicsRootConstantBufferView(
                1, base + batch.materialIndex * materialStride + 256);
            const std::array ids{
                id, material.normalTextureId ? material.normalTextureId : UINT64_MAX - 1,
                material.ormTextureId ? material.ormTextureId : UINT64_MAX,
                material.emissionTextureId ? material.emissionTextureId : UINT64_MAX};
            for (UINT map = 0; map < ids.size(); ++map) {
              auto mapHandle = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
              const auto &texture = sceneTextures_.at(ids[map]);
              const auto descriptor =
                  (map == 0 || map == 3) ? texture.srgbDescriptor : texture.descriptor;
              mapHandle.ptr += UINT64(descriptor) * uiDescriptorIncrement_;
              commands_->SetGraphicsRootDescriptorTable(map + 2, mapHandle);
            }
            const std::array environmentIds{
                drawData.environment ? drawData.environment->diffuseTextureId : UINT64_MAX,
                drawData.environment ? drawData.environment->specularTextureId : UINT64_MAX,
                drawData.environment ? drawData.environment->brdfTextureId : UINT64_MAX};
            for (UINT map = 0; map < 3; ++map) {
              auto environmentHandle = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
              environmentHandle.ptr +=
                  UINT64(linearSceneTextures_.at(environmentIds[map]).descriptor) *
                  uiDescriptorIncrement_;
              commands_->SetGraphicsRootDescriptorTable(map + 6, environmentHandle);
            }
          } else {
            commands_->SetGraphicsRootDescriptorTable(1, handle);
          }
          if (drawData.pbr) {
            auto shadowHandle = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
            const auto shadowDescriptor = drawData.shadow
                                              ? kMaximumFrames + frame_
                                              : linearSceneTextures_.at(UINT64_MAX).descriptor;
            shadowHandle.ptr += UINT64(shadowDescriptor) * uiDescriptorIncrement_;
            commands_->SetGraphicsRootDescriptorTable(9, shadowHandle);
            auto snapshotGpu = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
            const auto snapshotSlot =
                reflection && material.reflectionRole == SceneReflectionRole::Receiver
                    ? 3 * kMaximumFrames + frame_
                    : (refraction ? 2 * kMaximumFrames + frame_
                                  : linearSceneTextures_.at(UINT64_MAX).descriptor);
            snapshotGpu.ptr += UINT64(snapshotSlot) * uiDescriptorIncrement_;
            commands_->SetGraphicsRootDescriptorTable(10, snapshotGpu);
          }
          commands_->DrawIndexedInstanced(batch.indexCount, batch.instanceCount, batch.firstIndex,
                                          0, batch.firstInstance);
        }
      }
    }
    if (drawData.shadow) {
      ++diagnostics_.sceneShadowPasses;
    }
    sceneDrawn_ = true;
    sceneOffscreen_ = drawData.offscreen;
    sceneHdr_ = drawData.hdr;
    sceneExposure_ = drawData.exposure;
    sceneBloom_ = drawData.bloom.value_or(SceneBloom{0, 1, 12});
    sceneColorGrade_ = drawData.colorGrade.value_or(SceneColorGrade{});
    sceneDepthOfField_ = drawData.depthOfField.value_or(SceneDepthOfField{10, 0, 12});
    sceneAntiAliasing_ = drawData.postProcessAntiAliasing;
    diagnostics_.sceneOffscreenDrawCalls += drawData.offscreen ? 1 : 0;
    ++diagnostics_.sceneDrawCalls;
    diagnostics_.sceneInstances += instances.size();
    return SurfaceStatus::Ready;
  }
  SurfaceStatus CompositeScene() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || !sceneDrawn_ || !sceneOffscreen_ || sceneComposited_ || uiDrawn_ ||
        compositeDrawn_)
      return SurfaceStatus::InvalidDescriptor;
    if (sceneHdr_) {
      D3D12_RESOURCE_BARRIER transition{};
      transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      transition.Transition = {sceneColors_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                               D3D12_RESOURCE_STATE_RENDER_TARGET,
                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
      commands_->ResourceBarrier(1, &transition);
      auto rtv = heap_->GetCPUDescriptorHandleForHeapStart();
      rtv.ptr += SIZE_T(frame_) * increment_;
      commands_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      commands_->SetGraphicsRootSignature(toneRootSignature_.Get());
      commands_->SetPipelineState(tonePipeline_.Get());
      ID3D12DescriptorHeap *heaps[]{uiDescriptors_.Get()};
      commands_->SetDescriptorHeaps(1, heaps);
      const auto settings =
          PackToneParameters(sceneExposure_, true, sceneBloom_, sceneColorGrade_, width_, height_,
                             sceneDepthOfField_, sceneAntiAliasing_);
      commands_->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(settings.size()),
                                               settings.data(), 0);
      auto handle = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
      handle.ptr += UINT64(frame_) * uiDescriptorIncrement_;
      commands_->SetGraphicsRootDescriptorTable(1, handle);
      const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_),
                                    0, 1};
      const D3D12_RECT scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
      commands_->RSSetViewports(1, &viewport);
      commands_->RSSetScissorRects(1, &scissor);
      commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      const D3D12_VERTEX_BUFFER_VIEW vertices{sceneUploads_[frame_]->GetGPUVirtualAddress() +
                                                  toneOffsets_[frame_],
                                              sizeof(toneVertices), 4 * sizeof(float)};
      commands_->IASetVertexBuffers(0, 1, &vertices);
      commands_->DrawInstanced(3, 1, 0, 0);
      sceneComposited_ = true;
      ++diagnostics_.sceneComposites;
      return SurfaceStatus::Ready;
    }
    D3D12_RESOURCE_BARRIER barriers[2]{};
    for (auto &barrier : barriers)
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition = {sceneColors_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                              D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE};
    barriers[1].Transition = {buffers_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                              D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST};
    commands_->ResourceBarrier(2, barriers);
    commands_->CopyResource(buffers_[frame_].Get(), sceneColors_[frame_].Get());
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commands_->ResourceBarrier(1, &barriers[1]);
    sceneComposited_ = true;
    ++diagnostics_.sceneComposites;
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
    acquired_ = false;
    for (auto &color : reflectionColors_)
      color.Reset();
    for (auto &color : refractionColors_)
      color.Reset();
    for (auto &color : sceneColors_)
      color.Reset();
    for (auto &color : shadowColors_)
      color.Reset();
    for (auto &depth : shadowDepths_)
      depth.Reset();
    sceneRtvHeap_.Reset();
    for (auto &b : buffers_)
      b.Reset();
    for (auto &d : depthBuffers_)
      d.Reset();
    for (auto &upload : uiUploads_)
      upload.Reset();
    for (auto &upload : sceneUploads_)
      upload.Reset();
    uiTextures_.clear();
    sceneTextures_.clear();
    linearSceneTextures_.clear();
    for (auto &retired : retired_)
      retired.clear();
    for (auto &descriptors : retiredUiDescriptors_)
      descriptors.clear();
    freeUiDescriptors_.clear();
    uiPipeline_.Reset();
    uiRootSignature_.Reset();
    uiDescriptors_.Reset();
    scenePipeline_.Reset();
    scenePbrPipeline_.Reset();
    sceneHdrPipeline_.Reset();
    sceneHdrBlendPipeline_.Reset();
    shadowPipeline_.Reset();
    tonePipeline_.Reset();
    toneRootSignature_.Reset();
    scenePbrRootSignature_.Reset();
    sceneRootSignature_.Reset();
    dsvHeap_.Reset();
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
    UINT srgbDescriptor{};
    std::uint32_t mipLevels{1};
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
  // A self-contained indexed/lit/depth-tested pipeline for the Rendering Room, independent of the
  // 2D CreateUiResources() pipeline above and of the offscreen rhi::Device (which this surface does
  // not use at all -- see Dx12Surface's own constructor for its own independent ID3D12Device).
  bool CreateSceneResources() {
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1, &range};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.MaxAnisotropy = 1;
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
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
                                            IID_PPV_ARGS(&sceneRootSignature_))))
      return false;
    D3D12_DESCRIPTOR_HEAP_DESC sceneHeap{};
    sceneHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    sceneHeap.NumDescriptors = frames_ * 3;
    if (FAILED(device_->CreateDescriptorHeap(&sceneHeap, IID_PPV_ARGS(&sceneRtvHeap_))))
      return false;
    // Column-vector convention (row_major storage, mul(matrix, vector)) to match
    // Nexora::Math::Matrix4's documented "row-major storage, column vectors" layout -- uploaded
    // verbatim with no transpose. Faces are not culled: a convex opaque cube with depth testing
    // looks identical either way, so this sidesteps depending on the mesh's winding convention.
    constexpr char shader[] = R"(
      cbuffer SceneConstants : register(b0) {
        row_major float4x4 mvp;
        float3 lightDirection; float _pad0;
        float3 lightColor; float _pad1;
        float4 baseColor;
      };
      Texture2D materialTexture : register(t0);
      SamplerState materialSampler : register(s0);
      struct VSInput { float3 position : POSITION; float2 uv : TEXCOORD; float3 normal : NORMAL; float4 model0 : INSTANCE_MODEL0; float4 model1 : INSTANCE_MODEL1; float4 model2 : INSTANCE_MODEL2; float4 normal0 : INSTANCE_NORMAL0; float4 normal1 : INSTANCE_NORMAL1; float4 normal2 : INSTANCE_NORMAL2; float4 color : INSTANCE_COLOR; };
      struct PSInput { float4 position : SV_Position; float2 uv : TEXCOORD; float3 normal : NORMAL; float4 color : COLOR; };
      float3 SafeNormal(float3 value) {
        float magnitude = max(max(abs(value.x), abs(value.y)), abs(value.z));
        return magnitude > 0.0 ? normalize(value / magnitude) : float3(0, 0, 0);
      }
      PSInput VSMain(VSInput input) {
        PSInput output;
        float4 p = float4(input.position, 1.0);
        float3 world = float3(dot(input.model0, p), dot(input.model1, p), dot(input.model2, p));
        output.position = mul(mvp, float4(world, 1.0));
        float3 normal = SafeNormal(input.normal);
        output.normal = float3(dot(input.normal0.xyz, normal), dot(input.normal1.xyz, normal), dot(input.normal2.xyz, normal));
        output.color = input.color;
        output.uv = input.uv;
        return output;
      }
      float4 PSMain(PSInput input) : SV_Target {
        float3 n = SafeNormal(input.normal);
        float ndotl = saturate(dot(n, SafeNormal(-lightDirection)));
        float3 ambient = baseColor.rgb * 0.15;
        float3 lit = baseColor.rgb * lightColor * ndotl;
        return float4((ambient + lit) * input.color.rgb, baseColor.a * input.color.a) * materialTexture.Sample(materialSampler, input.uv);
      }
    )";
    ComPtr<ID3DBlob> vertex, pixel;
    if (FAILED(D3DCompile(shader, sizeof(shader), "NexoraRenderingRoom", nullptr, nullptr, "VSMain",
                          "vs_5_0", 0, 0, &vertex, &errors)) ||
        FAILED(D3DCompile(shader, sizeof(shader), "NexoraRenderingRoom", nullptr, nullptr, "PSMain",
                          "ps_5_0", 0, 0, &pixel, &errors)))
      return false;
    const D3D12_INPUT_ELEMENT_DESC inputs[] = {
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, uv),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, position),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, normal),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"INSTANCE_MODEL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, model) + 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"INSTANCE_MODEL", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, model) + 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_MODEL", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, model) + 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, normal) + 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_NORMAL", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, normal) + 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_NORMAL", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, normal) + 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {"INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,
         offsetof(SceneInstanceUpload, color), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = sceneRootSignature_.Get();
    pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.SampleMask = UINT_MAX;
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    pipeline.DepthStencilState.DepthEnable = TRUE;
    pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipeline.InputLayout = {inputs, static_cast<UINT>(std::size(inputs))};
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline.SampleDesc.Count = 1;
    if (FAILED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&scenePipeline_))))
      return false;
    std::array<D3D12_DESCRIPTOR_RANGE, 9> pbrRanges{};
    std::array<D3D12_ROOT_PARAMETER, 11> pbrParameters{};
    for (UINT i = 0; i < 2; ++i) {
      pbrParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
      pbrParameters[i].Descriptor.ShaderRegister = i;
      pbrParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    std::array<D3D12_STATIC_SAMPLER_DESC, 9> pbrSamplers{};
    for (UINT i = 0; i < 9; ++i) {
      pbrRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      pbrRanges[i].NumDescriptors = 1;
      pbrRanges[i].BaseShaderRegister = i;
      pbrParameters[i + 2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      pbrParameters[i + 2].DescriptorTable = {1, &pbrRanges[i]};
      pbrParameters[i + 2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
      pbrSamplers[i] = sampler;
      pbrSamplers[i].ShaderRegister = i;
      if (i == 4 || i == 5)
        pbrSamplers[i].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      if (i == 7) {
        pbrSamplers[i].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        pbrSamplers[i].AddressU = pbrSamplers[i].AddressV = pbrSamplers[i].AddressW =
            D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        pbrSamplers[i].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
      }
    }
    root.NumParameters = static_cast<UINT>(pbrParameters.size());
    root.pParameters = pbrParameters.data();
    root.NumStaticSamplers = static_cast<UINT>(pbrSamplers.size());
    root.pStaticSamplers = pbrSamplers.data();
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature,
                                           &errors)) ||
        FAILED(device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                            signature->GetBufferSize(),
                                            IID_PPV_ARGS(&scenePbrRootSignature_))))
      return false;
    if (FAILED(D3DCompile(scene_pbr_hlsl_vert, sizeof(scene_pbr_hlsl_vert), "NexoraSharedPbrVertex",
                          nullptr, nullptr, "pbrVertexMain", "vs_5_0", 0, 0, &vertex, &errors)) ||
        FAILED(D3DCompile(scene_pbr_hlsl_frag, sizeof(scene_pbr_hlsl_frag),
                          "NexoraSharedPbrFragment", nullptr, nullptr, "pbrFragmentMain", "ps_5_0",
                          0, 0, &pixel, &errors)))
      return false;
    std::array<D3D12_INPUT_ELEMENT_DESC, std::size(inputs) + 1> pbrInputs{};
    std::copy(std::begin(inputs), std::end(inputs), pbrInputs.begin());
    pbrInputs.back() = {"TANGENT",
                        0,
                        DXGI_FORMAT_R32G32B32A32_FLOAT,
                        0,
                        offsetof(SceneVertex, tangent),
                        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                        0};
    pipeline.pRootSignature = scenePbrRootSignature_.Get();
    pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    pipeline.InputLayout = {pbrInputs.data(), static_cast<UINT>(pbrInputs.size())};
    if (FAILED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&scenePbrPipeline_))))
      return false;
    ComPtr<ID3DBlob> shadowPixel;
    if (FAILED(D3DCompile(scene_pbr_hlsl_shadow_frag, sizeof(scene_pbr_hlsl_shadow_frag),
                          "NexoraShadowDepth", nullptr, nullptr, "shadowFragmentMain", "ps_5_0", 0,
                          0, &shadowPixel, &errors)))
      return false;
    pipeline.pRootSignature = scenePbrRootSignature_.Get();
    pipeline.PS = {shadowPixel->GetBufferPointer(), shadowPixel->GetBufferSize()};
    pipeline.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
    if (FAILED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&shadowPipeline_))))
      return false;
    pipeline.pRootSignature = scenePbrRootSignature_.Get();
    pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    pipeline.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (FAILED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&sceneHdrPipeline_))))
      return false;
    auto &blend = pipeline.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_BLEND_FACTOR;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ONE;
    blend.BlendOpAlpha = D3D12_BLEND_OP_MIN;
    pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(
            device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&sceneHdrBlendPipeline_))))
      return false;
    blend.BlendEnable = FALSE;
    D3D12_DESCRIPTOR_RANGE toneRange{};
    toneRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    toneRange.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER toneParameters[2]{};
    toneParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    toneParameters[0].Constants = {0, 0, sizeof(ToneParametersUpload) / sizeof(float)};
    toneParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    toneParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    toneParameters[1].DescriptorTable = {1, &toneRange};
    toneParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root.NumParameters = 2;
    root.pParameters = toneParameters;
    root.NumStaticSamplers = 1;
    root.pStaticSamplers = &sampler;
    if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature,
                                           &errors)) ||
        FAILED(device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                            signature->GetBufferSize(),
                                            IID_PPV_ARGS(&toneRootSignature_))))
      return false;
    if (FAILED(D3DCompile(scene_tonemap_hlsl_vert, sizeof(scene_tonemap_hlsl_vert),
                          "NexoraToneVertex", nullptr, nullptr, "toneVertexMain", "vs_5_0", 0, 0,
                          &vertex, &errors)) ||
        FAILED(D3DCompile(scene_tonemap_hlsl_frag, sizeof(scene_tonemap_hlsl_frag),
                          "NexoraToneFragment", nullptr, nullptr, "toneFragmentMain", "ps_5_0", 0,
                          0, &pixel, &errors)))
      return false;
    pipeline.pRootSignature = toneRootSignature_.Get();
    pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    const D3D12_INPUT_ELEMENT_DESC toneInputs[]{{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
                                                 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                                                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                                                 2 * sizeof(float),
                                                 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    pipeline.InputLayout = {toneInputs, 2};
    pipeline.DepthStencilState.DepthEnable = FALSE;
    pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    return SUCCEEDED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&tonePipeline_)));
  }
  bool RecordShadow(const SceneDrawData &draw, D3D12_GPU_VIRTUAL_ADDRESS geometry,
                    D3D12_GPU_VIRTUAL_ADDRESS constants, std::size_t vertexSize,
                    std::size_t indexSize, std::size_t instanceOffset, std::size_t instanceCount,
                    D3D12_GPU_VIRTUAL_ADDRESS materials, std::size_t materialStride) {
    const auto resolution = draw.shadow->resolution;
    D3D12_RESOURCE_DESC descriptor{};
    descriptor.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    descriptor.Width = descriptor.Height = resolution;
    descriptor.DepthOrArraySize = descriptor.MipLevels = 1;
    descriptor.SampleDesc.Count = 1;
    descriptor.Format = DXGI_FORMAT_R32_FLOAT;
    descriptor.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_R32_FLOAT;
    clear.Color[0] = 1;
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &descriptor,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
                                                IID_PPV_ARGS(&shadowColors_[frame_]))))
      return false;
    descriptor.Format = DXGI_FORMAT_D32_FLOAT;
    descriptor.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil = {1, 0};
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &descriptor,
                                                D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                                                IID_PPV_ARGS(&shadowDepths_[frame_]))))
      return false;
    auto rtv = sceneRtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += SIZE_T(frames_ + frame_) * increment_;
    device_->CreateRenderTargetView(shadowColors_[frame_].Get(), nullptr, rtv);
    auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    dsv.ptr += SIZE_T(frames_ + frame_) * dsvIncrement_;
    device_->CreateDepthStencilView(shadowDepths_[frame_].Get(), nullptr, dsv);
    auto srv = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
    srv.ptr += SIZE_T(kMaximumFrames + frame_) * uiDescriptorIncrement_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R32_FLOAT;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(shadowColors_[frame_].Get(), &view, srv);
    const float color[4]{1, 1, 1, 1};
    commands_->ClearRenderTargetView(rtv, color, 0, nullptr);
    commands_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
    commands_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commands_->SetGraphicsRootSignature(scenePbrRootSignature_.Get());
    commands_->SetPipelineState(shadowPipeline_.Get());
    commands_->SetGraphicsRootConstantBufferView(0, constants);
    const D3D12_VIEWPORT viewport{
        0, 0, static_cast<float>(resolution), static_cast<float>(resolution), 0, 1};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(resolution), static_cast<LONG>(resolution)};
    commands_->RSSetViewports(1, &viewport);
    commands_->RSSetScissorRects(1, &scissor);
    const D3D12_VERTEX_BUFFER_VIEW vertices[]{
        {geometry, static_cast<UINT>(vertexSize), sizeof(SceneVertex)},
        {geometry + instanceOffset, static_cast<UINT>(instanceCount * sizeof(SceneInstanceUpload)),
         sizeof(SceneInstanceUpload)}};
    const D3D12_INDEX_BUFFER_VIEW indices{geometry + vertexSize, static_cast<UINT>(indexSize),
                                          DXGI_FORMAT_R16_UINT};
    commands_->IASetVertexBuffers(0, 2, vertices);
    commands_->IASetIndexBuffer(&indices);
    commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const SceneMeshBatch whole{0, static_cast<std::uint32_t>(draw.indices.size()), 0,
                               static_cast<std::uint32_t>(instanceCount), 0};
    const auto batches =
        draw.batches.empty() ? std::span<const SceneMeshBatch>(&whole, 1) : draw.batches;
    ID3D12DescriptorHeap *heaps[]{uiDescriptors_.Get()};
    commands_->SetDescriptorHeaps(1, heaps);
    for (const auto &batch : batches) {
      const auto material = ResolveSceneMaterial(draw, batch.materialIndex);
      if (!material.castsShadow)
        continue;
      diagnostics_.sceneShadowInstances += batch.instanceCount;
      commands_->SetGraphicsRootConstantBufferView(
          1, materials + batch.materialIndex * materialStride + 256);
      auto handle = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
      const auto id = material.textureId ? material.textureId : UINT64_MAX;
      handle.ptr += UINT64(sceneTextures_.at(id).srgbDescriptor) * uiDescriptorIncrement_;
      commands_->SetGraphicsRootDescriptorTable(2, handle);
      commands_->DrawIndexedInstanced(batch.indexCount, batch.instanceCount, batch.firstIndex, 0,
                                      batch.firstInstance);
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {shadowColors_[frame_].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                          D3D12_RESOURCE_STATE_RENDER_TARGET,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    commands_->ResourceBarrier(1, &barrier);
    return true;
  }
  bool EnsureSceneUpload(std::size_t required) {
    if (sceneUploadCapacity_[frame_] >= required)
      return true;
    sceneUploadCapacity_[frame_] = 4096;
    while (sceneUploadCapacity_[frame_] < required)
      sceneUploadCapacity_[frame_] *= 2;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sceneUploadCapacity_[frame_];
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return SUCCEEDED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                      IID_PPV_ARGS(&sceneUploads_[frame_])));
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
  bool UploadUiTexture(const UiTextureUpload &upload, bool scene = false,
                       std::uint32_t mipLevels = 1, bool linear = false) {
    auto &textures = linear ? linearSceneTextures_ : scene ? sceneTextures_ : uiTextures_;
    if (upload.textureId == 0 || upload.width == 0 || upload.height == 0 ||
        upload.rowPitch != upload.width * (linear ? 8U : 4U) ||
        upload.pixels.size() !=
            (linear ? SceneLinearTextureByteSize(upload.width, upload.height, mipLevels)
                    : SceneRgbaTextureByteSize(upload.width, upload.height, mipLevels)))
      return false;
    const UINT descriptorCount = scene && !linear ? 2U : 1U;
    if (freeUiDescriptors_.size() + 4096U - nextUiDescriptor_ < descriptorCount)
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
    texture.MipLevels = static_cast<UINT16>(mipLevels);
    texture.Format = linear  ? DXGI_FORMAT_R16G16B16A16_FLOAT
                     : scene ? DXGI_FORMAT_R8G8B8A8_TYPELESS
                             : DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count = 1;
    ComPtr<ID3D12Resource> resource;
    if (FAILED(device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texture,
                                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                IID_PPV_ARGS(&resource))))
      return false;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mipLevels);
    std::vector<UINT> rowCounts(mipLevels);
    std::vector<UINT64> rowSizes(mipLevels);
    UINT64 totalBytes{};
    device_->GetCopyableFootprints(&texture, 0, mipLevels, 0, footprints.data(), rowCounts.data(),
                                   rowSizes.data(), &totalBytes);
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC stagingDesc{};
    stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    stagingDesc.Width = totalBytes;
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
    std::size_t sourceOffset = 0;
    for (std::uint32_t level = 0; level < mipLevels; ++level) {
      const auto &footprint = footprints[level];
      for (UINT row = 0; row < rowCounts[level]; ++row)
        std::memcpy(static_cast<std::byte *>(mapped) + footprint.Offset +
                        static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                    upload.pixels.data() + sourceOffset +
                        static_cast<std::size_t>(row) * rowSizes[level],
                    static_cast<std::size_t>(rowSizes[level]));
      sourceOffset += static_cast<std::size_t>(rowSizes[level]) * rowCounts[level];
    }
    staging->Unmap(0, nullptr);
    for (std::uint32_t level = 0; level < mipLevels; ++level) {
      D3D12_TEXTURE_COPY_LOCATION destination{};
      destination.pResource = resource.Get();
      destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      destination.SubresourceIndex = level;
      D3D12_TEXTURE_COPY_LOCATION source{};
      source.pResource = staging.Get();
      source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      source.PlacedFootprint = footprints[level];
      commands_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                          D3D12_RESOURCE_STATE_COPY_DEST,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    commands_->ResourceBarrier(1, &barrier);
    const auto allocateDescriptor = [this]() {
      if (freeUiDescriptors_.empty())
        return nextUiDescriptor_++;
      const auto available_descriptor = freeUiDescriptors_.back();
      freeUiDescriptors_.pop_back();
      return available_descriptor;
    };
    const UINT descriptor = allocateDescriptor();
    if (const auto previous = textures.find(upload.textureId); previous != textures.end()) {
      retired_[frame_].push_back(previous->second.resource);
      retiredUiDescriptors_[frame_].push_back(previous->second.descriptor);
      if (previous->second.srgbDescriptor != previous->second.descriptor)
        retiredUiDescriptors_[frame_].push_back(previous->second.srgbDescriptor);
    }
    auto cpu = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(descriptor) * uiDescriptorIncrement_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = linear ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = mipLevels;
    device_->CreateShaderResourceView(resource.Get(), &view, cpu);
    UINT srgbDescriptor = descriptor;
    if (scene && !linear) {
      srgbDescriptor = allocateDescriptor();
      cpu = uiDescriptors_->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += static_cast<SIZE_T>(srgbDescriptor) * uiDescriptorIncrement_;
      view.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      device_->CreateShaderResourceView(resource.Get(), &view, cpu);
    }
    textures[upload.textureId] = {resource, descriptor, srgbDescriptor, mipLevels};
    retired_[frame_].push_back(staging);
    if (scene)
      ++diagnostics_.sceneTextureUploads;
    else
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
    return RebuildDepthBuffers();
  }
  bool RebuildDepthBuffers() {
    if (!dsvHeap_)
      return true;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width_;
    desc.Height = height_;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    for (UINT i = 0; i < frames_; ++i) {
      depthBuffers_[i].Reset();
      if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                  D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                                                  IID_PPV_ARGS(&depthBuffers_[i]))))
        return false;
      auto d = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
      d.ptr += SIZE_T(i) * dsvIncrement_;
      device_->CreateDepthStencilView(depthBuffers_[i].Get(), nullptr, d);
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
  bool sceneDrawn_{}, sceneOffscreen_{}, sceneComposited_{}, uiDrawn_{}, compositeDrawn_{};
  std::atomic<uint32_t> pendingWidth_{}, pendingHeight_{};
  std::atomic_bool dirty_{};
  HANDLE event_{};
  UINT increment_{};
  UINT uiDescriptorIncrement_{};
  UINT nextUiDescriptor_{4 * kMaximumFrames}; // Fence-owned HDR SRVs reserve the first frame slots.
  std::vector<UINT> freeUiDescriptors_;
  std::array<std::vector<UINT>, kMaximumFrames> retiredUiDescriptors_;
  uint64_t fenceValue_{};
  std::array<uint64_t, kMaximumFrames> fenceValues_{};
  SurfaceDiagnostics diagnostics_{};
  ComPtr<IDXGIFactory6> factory_;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12Fence> fence_;
  ComPtr<ID3D12DescriptorHeap> heap_;
  ComPtr<ID3D12DescriptorHeap> sceneRtvHeap_;
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> sceneColors_, refractionColors_,
      reflectionColors_;
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> shadowColors_, shadowDepths_;
  ComPtr<ID3D12PipelineState> shadowPipeline_;
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
  std::unordered_map<std::uint64_t, UiTexture> sceneTextures_;
  std::unordered_map<std::uint64_t, UiTexture> linearSceneTextures_;
  ComPtr<IDXGISwapChain3> swapchain_;
  ComPtr<ID3D12DescriptorHeap> dsvHeap_;
  UINT dsvIncrement_{};
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> depthBuffers_;
  ComPtr<ID3D12RootSignature> sceneRootSignature_;
  ComPtr<ID3D12PipelineState> scenePipeline_;
  ComPtr<ID3D12PipelineState> scenePbrPipeline_;
  ComPtr<ID3D12PipelineState> sceneHdrPipeline_, sceneHdrBlendPipeline_, tonePipeline_;
  ComPtr<ID3D12RootSignature> toneRootSignature_;
  std::array<std::size_t, kMaximumFrames> toneOffsets_{};
  bool sceneHdr_{};
  float sceneExposure_ = 1.0F;
  SceneBloom sceneBloom_{0, 1, 12};
  SceneColorGrade sceneColorGrade_{};
  SceneDepthOfField sceneDepthOfField_{10, 0, 12};
  bool sceneAntiAliasing_ = false;
  ComPtr<ID3D12RootSignature> scenePbrRootSignature_;
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> sceneUploads_;
  std::array<std::size_t, kMaximumFrames> sceneUploadCapacity_{};
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
