#if !defined(__APPLE__)
#error "MetalSurface.mm is only built on Apple platforms"
#endif
#include "Nexora/Presentation/Surface.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <atomic>

namespace Nexora::Presentation {
namespace {
class MetalSurface final : public ISurface {
public:
  MetalSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows)
      : thread_(std::this_thread::get_id()), width_(descriptor.width), height_(descriptor.height) {
    auto *window = (__bridge NSWindow *)windows.NativeHandle(descriptor.window);
    device_ = MTLCreateSystemDefaultDevice();
    if (!window || !device_)
      return;
    layer_ = [CAMetalLayer layer];
    layer_.device = device_;
    layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer_.framebufferOnly = YES;
    layer_.displaySyncEnabled = descriptor.presentMode == PresentMode::VSync;
    layer_.drawableSize = CGSizeMake(width_, height_);
    [window contentView].wantsLayer = YES;
    [window contentView].layer = layer_;
    queue_ = [device_ newCommandQueue];
    diagnostics_.negotiatedPresentMode = descriptor.presentMode;
    valid_ = queue_ != nil;
  }
  ~MetalSurface() override { static_cast<void>(DrainAndDestroy()); }
  std::thread::id RenderThread() const noexcept override { return thread_; }
  SurfaceStatus NotifyWindowExtent(std::uint32_t width, std::uint32_t height) noexcept override {
    pendingWidth_.store(width);
    pendingHeight_.store(height);
    dirty_.store(true);
    return width && height ? SurfaceStatus::Ready : SurfaceStatus::ZeroExtent;
  }
  SurfaceStatus Acquire() override {
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    if (destroyed_)
      return SurfaceStatus::SurfaceLost;
    if (!valid_)
      return SurfaceStatus::Unsupported;
    if (dirty_.exchange(false)) {
      width_ = pendingWidth_.load();
      height_ = pendingHeight_.load();
      if (!width_ || !height_)
        return SurfaceStatus::ZeroExtent;
      layer_.drawableSize = CGSizeMake(width_, height_);
      ++diagnostics_.resizeGenerations;
    }
    drawable_ = [layer_ nextDrawable];
    if (!drawable_)
      return SurfaceStatus::OutOfDate;
    ++diagnostics_.acquiredFrames;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus Present() override {
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    if (!drawable_)
      return SurfaceStatus::OutOfDate;
    id<MTLCommandBuffer> commands = [queue_ commandBuffer];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable_.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.08, 0.16, 1.0);
    [[commands renderCommandEncoderWithDescriptor:pass] endEncoding];
    [commands presentDrawable:drawable_];
    [commands commit];
    drawable_ = nil;
    ++diagnostics_.presentedFrames;
    return SurfaceStatus::Ready;
  }
  SurfaceDiagnostics Diagnostics() const noexcept override { return diagnostics_; }
  SurfaceStatus DrainAndDestroy() override {
    if (destroyed_)
      return SurfaceStatus::Ready;
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    id<MTLCommandBuffer> barrier = [queue_ commandBuffer];
    [barrier commit];
    [barrier waitUntilCompleted];
    drawable_ = nil;
    layer_ = nil;
    queue_ = nil;
    device_ = nil;
    destroyed_ = true;
    return SurfaceStatus::Ready;
  }

private:
  std::thread::id thread_;
  std::uint32_t width_{}, height_{};
  std::atomic<std::uint32_t> pendingWidth_{}, pendingHeight_{};
  std::atomic_bool dirty_{};
  bool valid_{}, destroyed_{};
  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  CAMetalLayer *layer_ = nil;
  id<CAMetalDrawable> drawable_ = nil;
  SurfaceDiagnostics diagnostics_{};
};
} // namespace
std::unique_ptr<ISurface> CreateMetalSurface(const SurfaceDescriptor &descriptor,
                                             Window::IWindowSystem &windows) {
  return std::make_unique<MetalSurface>(descriptor, windows);
}
} // namespace Nexora::Presentation
