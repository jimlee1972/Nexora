#if !defined(_WIN32)
#error "Dx12Surface.cpp is only built on Windows"
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Nexora/Presentation/Surface.h"
#include <array>
#include <atomic>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include <mutex>
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
    valid_ = CreateSwapchain(width_, height_);
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
    swapchain_.Reset();
    destroyed_ = true;
    if (event_) {
      CloseHandle(event_);
      event_ = nullptr;
    }
    return SurfaceStatus::Ready;
  }

private:
  bool OnThread() const { return std::this_thread::get_id() == renderThread_; }
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
  uint64_t fenceValue_{};
  std::array<uint64_t, kMaximumFrames> fenceValues_{};
  SurfaceDiagnostics diagnostics_{};
  ComPtr<IDXGIFactory6> factory_;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12Fence> fence_;
  ComPtr<ID3D12DescriptorHeap> heap_;
  ComPtr<ID3D12GraphicsCommandList> commands_;
  std::array<ComPtr<ID3D12CommandAllocator>, kMaximumFrames> allocators_;
  std::array<ComPtr<ID3D12Resource>, kMaximumFrames> buffers_;
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
