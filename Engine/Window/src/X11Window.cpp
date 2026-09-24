#if !defined(__linux__)
#error "X11Window.cpp is only built on Linux"
#endif

#include "Nexora/Window/Window.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef None
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nexora::Window {
namespace {
std::uint64_t Now() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}

class X11WindowSystem final : public IWindowSystem {
public:
  X11WindowSystem() : owner_(std::this_thread::get_id()), display_(XOpenDisplay(nullptr)) {
    if (display_)
      closeAtom_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
  }
  ~X11WindowSystem() override {
    if (!display_)
      return;
    for (const auto &[id, window] : windows_)
      XDestroyWindow(display_, window);
    XCloseDisplay(display_);
  }
  std::thread::id OwnerThread() const noexcept override { return owner_; }
  WindowResult Create(const WindowDescriptor &descriptor) override {
    if (!OnOwner())
      return {{}, WindowError::WrongThread};
    if (!display_)
      return {{}, WindowError::Unsupported};
    if (!descriptor.width || !descriptor.height)
      return {{}, WindowError::InvalidDescriptor};
    const auto window = XCreateSimpleWindow(display_, DefaultRootWindow(display_), 0, 0,
                                            descriptor.width, descriptor.height, 0, 0, 0);
    if (!window)
      return {{}, WindowError::PlatformFailure};
    const std::string title(descriptor.title);
    XStoreName(display_, window, title.c_str());
    XSelectInput(display_, window,
                 StructureNotifyMask | FocusChangeMask | KeyPressMask | KeyReleaseMask |
                     PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    XSetWMProtocols(display_, window, &closeAtom_, 1);
    if (descriptor.initiallyVisible)
      XMapWindow(display_, window);
    const WindowHandle handle{next_++};
    windows_[handle.value] = window;
    reverse_[window] = handle;
    XFlush(display_);
    return {handle, WindowError::None};
  }
  WindowError Destroy(WindowHandle handle) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    const auto found = windows_.find(handle.value);
    if (found == windows_.end())
      return WindowError::InvalidHandle;
    reverse_.erase(found->second);
    XDestroyWindow(display_, found->second);
    windows_.erase(found);
    std::erase_if(pending_, [handle](const auto &event) { return event.window == handle; });
    std::erase_if(pumped_, [handle](const auto &event) { return event.window == handle; });
    return WindowError::None;
  }
  WindowError Show(WindowHandle handle, bool visible) override {
    const auto window = Find(handle);
    if (!OnOwner())
      return WindowError::WrongThread;
    if (!window)
      return WindowError::InvalidHandle;
    visible ? XMapWindow(display_, window) : XUnmapWindow(display_, window);
    XFlush(display_);
    return WindowError::None;
  }
  WindowError Resize(WindowHandle handle, std::uint32_t width, std::uint32_t height) override {
    const auto window = Find(handle);
    if (!OnOwner())
      return WindowError::WrongThread;
    if (!window)
      return WindowError::InvalidHandle;
    if (!width || !height)
      return WindowError::InvalidDescriptor;
    XResizeWindow(display_, window, width, height);
    XFlush(display_);
    return WindowError::None;
  }
  WindowError SetFullscreen(WindowHandle handle, bool fullscreen) override {
    const auto window = Find(handle);
    if (!OnOwner())
      return WindowError::WrongThread;
    if (!window)
      return WindowError::InvalidHandle;
    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display_, "_NET_WM_STATE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = fullscreen ? 1 : 0;
    event.xclient.data.l[1] =
        static_cast<long>(XInternAtom(display_, "_NET_WM_STATE_FULLSCREEN", False));
    XSendEvent(display_, DefaultRootWindow(display_), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display_);
    return WindowError::None;
  }
  std::span<const WindowEvent> PumpEvents() override {
    pumped_.clear();
    if (!OnOwner() || !display_)
      return pumped_;
    while (XPending(display_)) {
      XEvent native{};
      XNextEvent(display_, &native);
      const auto found = reverse_.find(native.xany.window);
      if (found == reverse_.end())
        continue;
      WindowEvent event{found->second, WindowEventType::CloseRequested, Now()};
      bool emit = true;
      switch (native.type) {
      case ClientMessage:
        emit = static_cast<Atom>(native.xclient.data.l[0]) == closeAtom_;
        break;
      case ConfigureNotify:
        event.type = WindowEventType::Resized;
        event.width = static_cast<std::uint32_t>(native.xconfigure.width);
        event.height = static_cast<std::uint32_t>(native.xconfigure.height);
        break;
      case FocusIn:
      case FocusOut:
        event.type = WindowEventType::FocusChanged;
        event.value0 = native.type == FocusIn;
        break;
      case KeyPress:
      case KeyRelease:
        event.type = WindowEventType::Key;
        event.value0 = native.xkey.keycode;
        event.value1 = native.type == KeyPress;
        break;
      case MotionNotify:
        event.type = WindowEventType::Pointer;
        event.value0 = native.xmotion.x;
        event.value1 = native.xmotion.y;
        break;
      case ButtonPress:
        event.type = WindowEventType::Wheel;
        event.value1 = native.xbutton.button == 4 ? 120 : native.xbutton.button == 5 ? -120 : 0;
        emit = event.value1 != 0;
        break;
      default:
        emit = false;
      }
      if (emit)
        pending_.push_back(event);
    }
    for (const auto &event : pending_) {
      if (event.type == WindowEventType::Resized && !pumped_.empty() &&
          pumped_.back().type == event.type && pumped_.back().window == event.window)
        pumped_.back() = event;
      else
        pumped_.push_back(event);
    }
    pending_.clear();
    return pumped_;
  }
  void *NativeHandle(WindowHandle handle) const noexcept override {
    return reinterpret_cast<void *>(Find(handle));
  }

private:
  bool OnOwner() const noexcept { return owner_ == std::this_thread::get_id(); }
  ::Window Find(WindowHandle handle) const noexcept {
    const auto found = windows_.find(handle.value);
    return found == windows_.end() ? 0 : found->second;
  }
  std::thread::id owner_;
  Display *display_{};
  Atom closeAtom_{};
  std::uint64_t next_ = 1;
  std::unordered_map<std::uint64_t, ::Window> windows_;
  std::unordered_map<::Window, WindowHandle> reverse_;
  std::vector<WindowEvent> pending_;
  std::vector<WindowEvent> pumped_;
};
} // namespace

std::unique_ptr<IWindowSystem> CreateWindowSystem() {
  auto result = std::make_unique<X11WindowSystem>();
  return result;
}
} // namespace Nexora::Window
