#if !defined(__APPLE__)
#error "CocoaWindow.mm is only built on Apple platforms"
#endif

#include "Nexora/Window/Window.h"
#import <Cocoa/Cocoa.h>

#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

@interface NexoraCloseDelegate : NSObject <NSWindowDelegate> {
@public
  BOOL requested;
}
@end

@implementation NexoraCloseDelegate
- (BOOL)windowShouldClose:(id)sender {
  (void)sender;
  requested = YES;
  return NO;
}
@end

namespace Nexora::Window {
namespace {
Key TranslateKey(unsigned short code) noexcept {
  // macOS hardware key codes are physical positions, independent of the active text layout.
  static constexpr std::array<Key, 128> keys{
      Key::A,
      Key::S,
      Key::D,
      Key::F,
      Key::H,
      Key::G,
      Key::Z,
      Key::X,
      Key::C,
      Key::V,
      Key::Unknown,
      Key::B,
      Key::Q,
      Key::W,
      Key::E,
      Key::R,
      Key::Y,
      Key::T,
      Key::Digit1,
      Key::Digit2,
      Key::Digit3,
      Key::Digit4,
      Key::Digit6,
      Key::Digit5,
      Key::Equal,
      Key::Digit9,
      Key::Digit7,
      Key::Minus,
      Key::Digit8,
      Key::Digit0,
      Key::RightBracket,
      Key::O,
      Key::U,
      Key::LeftBracket,
      Key::I,
      Key::P,
      Key::Enter,
      Key::L,
      Key::J,
      Key::Apostrophe,
      Key::K,
      Key::Semicolon,
      Key::Backslash,
      Key::Comma,
      Key::Slash,
      Key::N,
      Key::M,
      Key::Period,
      Key::Tab,
      Key::Space,
      Key::GraveAccent,
      Key::Backspace,
      Key::Unknown,
      Key::Escape,
      Key::RightSuper,
      Key::LeftSuper,
      Key::LeftShift,
      Key::CapsLock,
      Key::LeftAlt,
      Key::LeftControl,
      Key::RightShift,
      Key::RightAlt,
      Key::RightControl,
      Key::Unknown,
      Key::Unknown,
      Key::KeypadDecimal,
      Key::Unknown,
      Key::KeypadMultiply,
      Key::Unknown,
      Key::KeypadAdd,
      Key::Unknown,
      Key::NumLock,
      Key::Unknown,
      Key::Unknown,
      Key::Unknown,
      Key::KeypadDivide,
      Key::KeypadEnter,
      Key::Unknown,
      Key::KeypadSubtract,
      Key::Unknown,
      Key::Unknown,
      Key::KeypadEqual,
      Key::Keypad0,
      Key::Keypad1,
      Key::Keypad2,
      Key::Keypad3,
      Key::Keypad4,
      Key::Keypad5,
      Key::Keypad6,
      Key::Keypad7,
      Key::Unknown,
      Key::Keypad8,
      Key::Keypad9,
      Key::Unknown,
      Key::Unknown,
      Key::Unknown,
      Key::F5,
      Key::F6,
      Key::F7,
      Key::F3,
      Key::F8,
      Key::F9,
      Key::Unknown,
      Key::F11,
      Key::Unknown,
      Key::Unknown,
      Key::Unknown,
      Key::Unknown,
      Key::Unknown,
      Key::F10,
      Key::Unknown,
      Key::F12,
      Key::Unknown,
      Key::Unknown,
      Key::Insert,
      Key::Home,
      Key::PageUp,
      Key::Delete,
      Key::F4,
      Key::End,
      Key::F2,
      Key::PageDown,
      Key::F1,
      Key::LeftArrow,
      Key::RightArrow,
      Key::DownArrow,
      Key::UpArrow,
      Key::Unknown,
  };
  return code < keys.size() ? keys[code] : Key::Unknown;
}
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
    static std::once_flag launched;
    std::call_once(launched, [] { [NSApp finishLaunching]; });
  }
  ~CocoaWindowSystem() override {
    for (const auto &[id, window] : windows_) {
      [window setDelegate:nil];
      [window close];
      [delegates_.at(id) release];
    }
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
    [window setAcceptsMouseMovedEvents:YES];
    if (descriptor.initiallyVisible)
      [NSApp activateIgnoringOtherApps:YES];
    const WindowHandle handle{next_++};
    windows_[handle.value] = window;
    auto *delegate = [[NexoraCloseDelegate alloc] init];
    [window setDelegate:delegate];
    delegates_[handle.value] = delegate;
    return {handle, WindowError::None};
  }
  WindowError Destroy(WindowHandle handle) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    const auto found = windows_.find(handle.value);
    if (found == windows_.end())
      return WindowError::InvalidHandle;
    [found->second setDelegate:nil];
    [found->second close];
    [delegates_.at(handle.value) release];
    delegates_.erase(handle.value);
    extents_.erase(handle.value);
    scales_.erase(handle.value);
    focus_.erase(handle.value);
    modifierKeys_.erase(handle.value);
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
    const auto scale = window.backingScaleFactor;
    [window setContentSize:NSMakeSize(width / scale, height / scale)];
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
    @autoreleasepool {
      events_.clear();
      if (!OnOwner())
        return events_;
      NSEvent *event = nil;
      while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                         untilDate:[NSDate distantPast]
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES])) {
        TranslateEvent(event);
        [NSApp sendEvent:event];
      }
      for (const auto &[id, window] : windows_) {
        const auto size = [[window contentView] bounds].size;
        const auto scale = static_cast<float>(window.backingScaleFactor);
        const auto extent =
            window.isMiniaturized
                ? std::pair<std::uint32_t, std::uint32_t>{0, 0}
                : std::pair{static_cast<std::uint32_t>(std::round(size.width * scale)),
                            static_cast<std::uint32_t>(std::round(size.height * scale))};
        if (scales_[id] != scale) {
          scales_[id] = scale;
          WindowEvent dpi{{id}, WindowEventType::DpiChanged, Now()};
          dpi.scale = scale;
          events_.push_back(dpi);
        }
        const bool focused = window.isKeyWindow && NSApp.isActive;
        if (focus_[id] != focused) {
          focus_[id] = focused;
          if (!focused)
            modifierKeys_[id].clear();
          WindowEvent focus{{id}, WindowEventType::FocusChanged, Now()};
          focus.value0 = focused ? 1 : 0;
          events_.push_back(focus);
        }
        if (extents_[id] != extent) {
          extents_[id] = extent;
          events_.push_back({{id}, WindowEventType::Resized, Now(), extent.first, extent.second});
        }
        if (delegates_.at(id)->requested) {
          events_.push_back({{id}, WindowEventType::CloseRequested, Now()});
          delegates_.at(id)->requested = NO;
        }
      }
      return events_;
    }
  }
  void *NativeHandle(WindowHandle handle) const noexcept override { return Find(handle); }
  WindowError SetImeCandidatePosition(WindowHandle handle, std::int32_t, std::int32_t) override {
    return Find(handle) ? WindowError::Unsupported : WindowError::InvalidHandle;
  }

private:
  void TranslateEvent(NSEvent *native) {
    auto *window = native.window;
    if (!window)
      return;
    WindowHandle handle{};
    for (const auto &[id, candidate] : windows_)
      if (candidate == window) {
        handle = {id};
        break;
      }
    if (!handle.IsValid())
      return;
    WindowEvent event{handle, WindowEventType::Key, Now()};
    const auto flags = native.modifierFlags;
    std::uint8_t modifiers = 0;
    if (flags & NSEventModifierFlagControl)
      modifiers |= 1;
    if (flags & NSEventModifierFlagShift)
      modifiers |= 2;
    if (flags & NSEventModifierFlagOption)
      modifiers |= 4;
    if (flags & NSEventModifierFlagCommand)
      modifiers |= 8;
    event.modifiers = static_cast<KeyModifiers>(modifiers);
    switch (native.type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
      if (native.type == NSEventTypeKeyDown && native.isARepeat)
        return;
      event.value0 = static_cast<std::int32_t>(TranslateKey(native.keyCode));
      event.value1 = native.type == NSEventTypeKeyDown ? 1 : 0;
      if (event.value0 != static_cast<std::int32_t>(Key::Unknown))
        events_.push_back(event);
      return;
    case NSEventTypeFlagsChanged: {
      const auto key = TranslateKey(native.keyCode);
      NSEventModifierFlags family = 0;
      switch (key) {
      case Key::LeftControl:
      case Key::RightControl:
        family = NSEventModifierFlagControl;
        break;
      case Key::LeftShift:
      case Key::RightShift:
        family = NSEventModifierFlagShift;
        break;
      case Key::LeftAlt:
      case Key::RightAlt:
        family = NSEventModifierFlagOption;
        break;
      case Key::LeftSuper:
      case Key::RightSuper:
        family = NSEventModifierFlagCommand;
        break;
      case Key::CapsLock:
        family = NSEventModifierFlagCapsLock;
        break;
      default:
        return;
      }
      auto &held = modifierKeys_[handle.value];
      const bool down = (flags & family) && (key == Key::CapsLock || !held.contains(key));
      if (down)
        held.insert(key);
      else
        held.erase(key);
      event.value0 = static_cast<std::int32_t>(key);
      event.value1 = down ? 1 : 0;
      events_.push_back(event);
      return;
    }
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp: {
      auto *view = window.contentView;
      const auto point = [view convertPoint:native.locationInWindow fromView:nil];
      const auto bounds = view.bounds;
      const auto scale = window.backingScaleFactor;
      event.type = WindowEventType::Pointer;
      event.value0 = static_cast<std::int32_t>((point.x - bounds.origin.x) * scale);
      event.value1 =
          static_cast<std::int32_t>((bounds.size.height - (point.y - bounds.origin.y)) * scale);
      events_.push_back(event);
      if (native.type == NSEventTypeLeftMouseDown || native.type == NSEventTypeRightMouseDown ||
          native.type == NSEventTypeOtherMouseDown || native.type == NSEventTypeLeftMouseUp ||
          native.type == NSEventTypeRightMouseUp || native.type == NSEventTypeOtherMouseUp) {
        event.type = WindowEventType::PointerButton;
        event.value0 = static_cast<std::int32_t>(native.buttonNumber);
        event.value1 = native.type == NSEventTypeLeftMouseDown ||
                               native.type == NSEventTypeRightMouseDown ||
                               native.type == NSEventTypeOtherMouseDown
                           ? 1
                           : 0;
        events_.push_back(event);
      }
      return;
    }
    case NSEventTypeScrollWheel:
      event.type = WindowEventType::Wheel;
      event.value0 = static_cast<std::int32_t>(std::round(native.scrollingDeltaX * 120.0));
      event.value1 = static_cast<std::int32_t>(std::round(native.scrollingDeltaY * 120.0));
      events_.push_back(event);
      return;
    default:
      return;
    }
  }
  bool OnOwner() const noexcept { return owner_ == std::this_thread::get_id(); }
  NSWindow *Find(WindowHandle handle) const noexcept {
    const auto found = windows_.find(handle.value);
    return found == windows_.end() ? nil : found->second;
  }
  std::thread::id owner_;
  std::uint64_t next_ = 1;
  std::unordered_map<std::uint64_t, NSWindow *> windows_;
  std::unordered_map<std::uint64_t, NexoraCloseDelegate *> delegates_;
  std::unordered_map<std::uint64_t, std::pair<std::uint32_t, std::uint32_t>> extents_;
  std::unordered_map<std::uint64_t, float> scales_;
  std::unordered_map<std::uint64_t, bool> focus_;
  std::unordered_map<std::uint64_t, std::unordered_set<Key>> modifierKeys_;
  std::vector<WindowEvent> events_;
};
} // namespace
std::unique_ptr<IWindowSystem> CreateWindowSystem() {
  return std::make_unique<CocoaWindowSystem>();
}
} // namespace Nexora::Window
