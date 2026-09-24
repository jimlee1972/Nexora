#if !defined(__APPLE__)
#error "CocoaWindow.mm is only built on Apple platforms"
#endif

#include "Nexora/Window/Window.h"
#import <Cocoa/Cocoa.h>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Nexora::Window {
namespace {
std::uint64_t Now() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}
class CocoaWindowSystem final : public IWindowSystem {
public:
  CocoaWindowSystem() : owner_(std::this_thread::get_id()) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  }
  ~CocoaWindowSystem() override {
    for (const auto &[id, window] : windows_)
      [window close];
  }
  std::thread::id OwnerThread() const noexcept override { return owner_; }
  WindowResult Create(const WindowDescriptor &descriptor) override {
    if (!OnOwner())
      return {{}, WindowError::WrongThread};
    if (!descriptor.width || !descriptor.height)
      return {{}, WindowError::InvalidDescriptor};
    auto style =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    if (descriptor.resizable)
      style |= NSWindowStyleMaskResizable;
    auto *window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, descriptor.width, descriptor.height)
                                    styleMask:style
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    if (!window)
      return {{}, WindowError::PlatformFailure};
    [window setTitle:[NSString stringWithUTF8String:std::string(descriptor.title).c_str()]];
    [window center];
    if (descriptor.initiallyVisible)
      [window makeKeyAndOrderFront:nil];
    const WindowHandle handle{next_++};
    windows_[handle.value] = window;
    return {handle, WindowError::None};
  }
  WindowError Destroy(WindowHandle handle) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    const auto found = windows_.find(handle.value);
    if (found == windows_.end())
      return WindowError::InvalidHandle;
    [found->second close];
    windows_.erase(found);
    return WindowError::None;
  }
  WindowError Show(WindowHandle handle, bool visible) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    auto *window = Find(handle);
    if (!window)
      return WindowError::InvalidHandle;
    visible ? [window makeKeyAndOrderFront:nil] : [window orderOut:nil];
    return WindowError::None;
  }
  WindowError Resize(WindowHandle handle, std::uint32_t width, std::uint32_t height) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    auto *window = Find(handle);
    if (!window)
      return WindowError::InvalidHandle;
    if (!width || !height)
      return WindowError::InvalidDescriptor;
    [window setContentSize:NSMakeSize(width, height)];
    return WindowError::None;
  }
  WindowError SetFullscreen(WindowHandle handle, bool fullscreen) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    auto *window = Find(handle);
    if (!window)
      return WindowError::InvalidHandle;
    const bool current = ([window styleMask] & NSWindowStyleMaskFullScreen) != 0;
    if (current != fullscreen)
      [window toggleFullScreen:nil];
    return WindowError::None;
  }
  std::span<const WindowEvent> PumpEvents() override {
    events_.clear();
    if (!OnOwner())
      return events_;
    NSEvent *event = nil;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES]))
      [NSApp sendEvent:event];
    for (const auto &[id, window] : windows_) {
      const auto size = [[window contentView] bounds].size;
      const auto extent = std::pair{static_cast<std::uint32_t>(size.width),
                                    static_cast<std::uint32_t>(size.height)};
      if (extents_[id] != extent) {
        extents_[id] = extent;
        events_.push_back({{id}, WindowEventType::Resized, Now(), extent.first, extent.second});
      }
      if (![window isVisible])
        events_.push_back({{id}, WindowEventType::CloseRequested, Now()});
    }
    return events_;
  }
  void *NativeHandle(WindowHandle handle) const noexcept override { return Find(handle); }
  WindowError SetImeCandidatePosition(WindowHandle handle, std::int32_t, std::int32_t) override {
    return Find(handle) ? WindowError::Unsupported : WindowError::InvalidHandle;
  }

private:
  bool OnOwner() const noexcept { return owner_ == std::this_thread::get_id(); }
  NSWindow *Find(WindowHandle handle) const noexcept {
    const auto found = windows_.find(handle.value);
    return found == windows_.end() ? nil : found->second;
  }
  std::thread::id owner_;
  std::uint64_t next_ = 1;
  std::unordered_map<std::uint64_t, NSWindow *> windows_;
  std::unordered_map<std::uint64_t, std::pair<std::uint32_t, std::uint32_t>> extents_;
  std::vector<WindowEvent> events_;
};
} // namespace
std::unique_ptr<IWindowSystem> CreateWindowSystem() {
  return std::make_unique<CocoaWindowSystem>();
}
} // namespace Nexora::Window
