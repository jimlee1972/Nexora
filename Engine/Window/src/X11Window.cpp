#if !defined(__linux__)
#error "X11Window.cpp is only built on Linux"
#endif

#include "Nexora/Window/Window.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#undef None
#include <algorithm>
#include <array>
#include <chrono>
#include <clocale>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Nexora::Window {
namespace {
Key TranslateKey(KeySym symbol) noexcept {
  if (symbol >= XK_0 && symbol <= XK_9)
    return static_cast<Key>(static_cast<int>(Key::Digit0) + symbol - XK_0);
  if (symbol >= XK_A && symbol <= XK_Z)
    return static_cast<Key>(static_cast<int>(Key::A) + symbol - XK_A);
  if (symbol >= XK_a && symbol <= XK_z)
    return static_cast<Key>(static_cast<int>(Key::A) + symbol - XK_a);
  if (symbol >= XK_F1 && symbol <= XK_F12)
    return static_cast<Key>(static_cast<int>(Key::F1) + symbol - XK_F1);
  if (symbol >= XK_KP_0 && symbol <= XK_KP_9)
    return static_cast<Key>(static_cast<int>(Key::Keypad0) + symbol - XK_KP_0);
#define NEXORA_X11_KEY(x, key)                                                                     \
  case XK_##x:                                                                                     \
    return Key::key
  switch (symbol) {
    NEXORA_X11_KEY(Tab, Tab);
    NEXORA_X11_KEY(Left, LeftArrow);
    NEXORA_X11_KEY(Right, RightArrow);
    NEXORA_X11_KEY(Up, UpArrow);
    NEXORA_X11_KEY(Down, DownArrow);
    NEXORA_X11_KEY(Prior, PageUp);
    NEXORA_X11_KEY(Next, PageDown);
    NEXORA_X11_KEY(Home, Home);
    NEXORA_X11_KEY(End, End);
    NEXORA_X11_KEY(Insert, Insert);
    NEXORA_X11_KEY(Delete, Delete);
    NEXORA_X11_KEY(BackSpace, Backspace);
    NEXORA_X11_KEY(space, Space);
    NEXORA_X11_KEY(Return, Enter);
    NEXORA_X11_KEY(Escape, Escape);
    NEXORA_X11_KEY(apostrophe, Apostrophe);
    NEXORA_X11_KEY(comma, Comma);
    NEXORA_X11_KEY(minus, Minus);
    NEXORA_X11_KEY(period, Period);
    NEXORA_X11_KEY(slash, Slash);
    NEXORA_X11_KEY(semicolon, Semicolon);
    NEXORA_X11_KEY(equal, Equal);
    NEXORA_X11_KEY(bracketleft, LeftBracket);
    NEXORA_X11_KEY(backslash, Backslash);
    NEXORA_X11_KEY(bracketright, RightBracket);
    NEXORA_X11_KEY(grave, GraveAccent);
    NEXORA_X11_KEY(Caps_Lock, CapsLock);
    NEXORA_X11_KEY(Scroll_Lock, ScrollLock);
    NEXORA_X11_KEY(Num_Lock, NumLock);
    NEXORA_X11_KEY(Print, PrintScreen);
    NEXORA_X11_KEY(Pause, Pause);
    NEXORA_X11_KEY(KP_Decimal, KeypadDecimal);
    NEXORA_X11_KEY(KP_Divide, KeypadDivide);
    NEXORA_X11_KEY(KP_Multiply, KeypadMultiply);
    NEXORA_X11_KEY(KP_Subtract, KeypadSubtract);
    NEXORA_X11_KEY(KP_Add, KeypadAdd);
    NEXORA_X11_KEY(KP_Enter, KeypadEnter);
    NEXORA_X11_KEY(KP_Equal, KeypadEqual);
    NEXORA_X11_KEY(Shift_L, LeftShift);
    NEXORA_X11_KEY(Control_L, LeftControl);
    NEXORA_X11_KEY(Alt_L, LeftAlt);
    NEXORA_X11_KEY(Super_L, LeftSuper);
    NEXORA_X11_KEY(Shift_R, RightShift);
    NEXORA_X11_KEY(Control_R, RightControl);
    NEXORA_X11_KEY(Alt_R, RightAlt);
    NEXORA_X11_KEY(Super_R, RightSuper);
    NEXORA_X11_KEY(Menu, Menu);
  default:
    return Key::Unknown;
  }
#undef NEXORA_X11_KEY
}

KeyModifiers TranslateModifiers(unsigned state) noexcept {
  unsigned result = 0;
  if (state & ControlMask)
    result |= static_cast<unsigned>(KeyModifiers::Control);
  if (state & ShiftMask)
    result |= static_cast<unsigned>(KeyModifiers::Shift);
  if (state & Mod1Mask)
    result |= static_cast<unsigned>(KeyModifiers::Alt);
  if (state & Mod4Mask)
    result |= static_cast<unsigned>(KeyModifiers::Super);
  return static_cast<KeyModifiers>(result);
}

using XErrorHandler = int (*)(Display *, XErrorEvent *);
XErrorHandler g_previous_error_handler = nullptr;

// Only BadWindow is tolerated, and only while DestroyWindowIfPresent installs this handler;
// every other protocol error still reaches the previously installed handler.
int IgnoreBadWindow(Display *display, XErrorEvent *error) {
  if (error->error_code == BadWindow)
    return 0;
  return g_previous_error_handler != nullptr ? g_previous_error_handler(display, error) : 0;
}

// The server (or another client such as xdotool) may already have destroyed the window by the time
// we do, and Vulkan can report the lost surface before the DestroyNotify has been pumped. A second
// XDestroyWindow would otherwise raise BadWindow, whose default handler aborts the process.
void DestroyWindowIfPresent(Display *display, ::Window window) {
  g_previous_error_handler = XSetErrorHandler(IgnoreBadWindow);
  XDestroyWindow(display, window);
  XSync(display, False);
  XSetErrorHandler(g_previous_error_handler);
  g_previous_error_handler = nullptr;
}

std::uint64_t Now() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}

void AppendUtf8Text(std::vector<WindowEvent> &events, WindowHandle window, std::uint64_t timestamp,
                    std::string_view text) {
  for (std::size_t offset = 0; offset < text.size();) {
    const auto lead = static_cast<std::uint8_t>(text[offset]);
    std::uint32_t codepoint = 0;
    std::size_t length = 0;
    if (lead < 0x80U) {
      codepoint = lead;
      length = 1;
    } else if ((lead & 0xe0U) == 0xc0U) {
      codepoint = lead & 0x1fU;
      length = 2;
    } else if ((lead & 0xf0U) == 0xe0U) {
      codepoint = lead & 0x0fU;
      length = 3;
    } else if ((lead & 0xf8U) == 0xf0U) {
      codepoint = lead & 0x07U;
      length = 4;
    } else {
      ++offset;
      continue;
    }
    if (offset + length > text.size())
      break;
    bool valid = true;
    for (std::size_t continuation = 1; continuation < length; ++continuation) {
      const auto byte = static_cast<std::uint8_t>(text[offset + continuation]);
      if ((byte & 0xc0U) != 0x80U) {
        valid = false;
        break;
      }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
    }
    const bool overlong = (length == 2 && codepoint < 0x80U) ||
                          (length == 3 && codepoint < 0x800U) ||
                          (length == 4 && codepoint < 0x10000U);
    if (!valid || overlong || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
      ++offset;
      continue;
    }
    offset += length;
    if (codepoint < 0x20U || codepoint == 0x7fU)
      continue;
    WindowEvent event{window, WindowEventType::Text, timestamp};
    event.value0 = static_cast<std::int32_t>(codepoint);
    events.push_back(event);
  }
}

class X11WindowSystem final : public IWindowSystem {
public:
  X11WindowSystem() : owner_(std::this_thread::get_id()) {
    static_cast<void>(std::setlocale(LC_CTYPE, ""));
    display_ = XOpenDisplay(nullptr);
    if (display_) {
      closeAtom_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
      static_cast<void>(XSetLocaleModifiers(""));
      inputMethod_ = XOpenIM(display_, nullptr, nullptr, nullptr);
    }
  }
  ~X11WindowSystem() override {
    if (!display_)
      return;
    for (const auto &[window, input] : inputContexts_)
      XDestroyIC(input);
    inputContexts_.clear();
    for (const auto &[id, window] : windows_)
      if (!gone_.contains(window))
        DestroyWindowIfPresent(display_, window);
    if (inputMethod_)
      XCloseIM(inputMethod_);
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
    long event_mask = StructureNotifyMask | FocusChangeMask | KeyPressMask | KeyReleaseMask |
                      PointerMotionMask | ButtonPressMask | ButtonReleaseMask;
    XSelectInput(display_, window, event_mask);
    XSetWMProtocols(display_, window, &closeAtom_, 1);
    if (inputMethod_) {
      if (auto input = XCreateIC(inputMethod_, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                 XNClientWindow, window, XNFocusWindow, window, nullptr)) {
        inputContexts_[window] = input;
        long filter_events = 0;
        if (XGetICValues(input, XNFilterEvents, &filter_events, nullptr) == nullptr) {
          event_mask |= filter_events;
          XSelectInput(display_, window, event_mask);
        }
      }
    }
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
    DestroyInputContext(found->second);
    // A window the server already destroyed (see DestroyNotify below) must not be destroyed again:
    // XDestroyWindow on it raises a BadWindow protocol error that aborts the process.
    if (!gone_.erase(found->second))
      DestroyWindowIfPresent(display_, found->second);
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
      if (XFilterEvent(&native, native.xany.window))
        continue;
      const auto found = reverse_.find(native.xany.window);
      if (found == reverse_.end())
        continue;
      WindowEvent event{found->second, WindowEventType::CloseRequested, Now()};
      bool emit = true;
      switch (native.type) {
      case ClientMessage:
        emit = static_cast<Atom>(native.xclient.data.l[0]) == closeAtom_;
        break;
      case DestroyNotify:
        // The window was destroyed behind our back (a client such as xdotool, or the server), so
        // no WM_DELETE_WINDOW will ever arrive. Report it as a close request so the owner stops.
        emit = native.xdestroywindow.event == native.xdestroywindow.window;
        if (emit) {
          gone_.insert(native.xdestroywindow.window);
          DestroyInputContext(native.xdestroywindow.window);
        }
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
        if (const auto input = inputContexts_.find(native.xfocus.window);
            input != inputContexts_.end()) {
          if (native.type == FocusIn)
            XSetICFocus(input->second);
          else
            XUnsetICFocus(input->second);
        }
        break;
      case KeyPress:
      case KeyRelease:
        event.type = WindowEventType::Key;
        event.value0 = static_cast<std::int32_t>(TranslateKey(XLookupKeysym(&native.xkey, 0)));
        event.value1 = native.type == KeyPress;
        event.modifiers = TranslateModifiers(native.xkey.state);
        if (event.value1) {
          const auto key = static_cast<Key>(event.value0);
          unsigned modifiers = static_cast<unsigned>(event.modifiers);
          if (key == Key::LeftControl || key == Key::RightControl)
            modifiers |= static_cast<unsigned>(KeyModifiers::Control);
          if (key == Key::LeftShift || key == Key::RightShift)
            modifiers |= static_cast<unsigned>(KeyModifiers::Shift);
          if (key == Key::LeftAlt || key == Key::RightAlt)
            modifiers |= static_cast<unsigned>(KeyModifiers::Alt);
          if (key == Key::LeftSuper || key == Key::RightSuper)
            modifiers |= static_cast<unsigned>(KeyModifiers::Super);
          event.modifiers = static_cast<KeyModifiers>(modifiers);
        }
        if (native.type == KeyPress) {
          pending_.push_back(event);
          AppendUtf8Text(pending_, found->second, event.timestampNanoseconds,
                         LookupUtf8(native.xkey));
          emit = false;
        }
        break;
      case MotionNotify:
        event.type = WindowEventType::Pointer;
        event.value0 = native.xmotion.x;
        event.value1 = native.xmotion.y;
        break;
      case ButtonPress:
      case ButtonRelease:
        if (native.xbutton.button >= 4 && native.xbutton.button <= 7) {
          event.type = WindowEventType::Wheel;
          event.value0 = native.xbutton.button == 6 ? -120 : native.xbutton.button == 7 ? 120 : 0;
          event.value1 = native.xbutton.button == 4 ? 120 : native.xbutton.button == 5 ? -120 : 0;
          emit = native.type == ButtonPress;
        } else {
          event.type = WindowEventType::PointerButton;
          event.value0 = static_cast<std::int32_t>(native.xbutton.button - 1U);
          event.value1 = native.type == ButtonPress;
        }
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
  WindowError SetImeCandidatePosition(WindowHandle handle, std::int32_t, std::int32_t) override {
    return Find(handle) ? WindowError::Unsupported : WindowError::InvalidHandle;
  }

private:
  bool OnOwner() const noexcept { return owner_ == std::this_thread::get_id(); }
  ::Window Find(WindowHandle handle) const noexcept {
    const auto found = windows_.find(handle.value);
    return found == windows_.end() ? 0 : found->second;
  }
  void DestroyInputContext(::Window window) noexcept {
    const auto found = inputContexts_.find(window);
    if (found == inputContexts_.end())
      return;
    XDestroyIC(found->second);
    inputContexts_.erase(found);
  }
  std::string LookupUtf8(XKeyPressedEvent &event) const {
    const auto input = inputContexts_.find(event.window);
    if (input != inputContexts_.end()) {
      std::array<char, 64> buffer{};
      KeySym symbol{};
      Status status{};
      int length = Xutf8LookupString(input->second, &event, buffer.data(),
                                     static_cast<int>(buffer.size()), &symbol, &status);
      if (status == XBufferOverflow && length > 0) {
        std::string grown(static_cast<std::size_t>(length), '\0');
        length = Xutf8LookupString(input->second, &event, grown.data(), length, &symbol, &status);
        if ((status == XLookupChars || status == XLookupBoth) && length > 0)
          return grown.substr(0, static_cast<std::size_t>(length));
        return {};
      }
      if ((status == XLookupChars || status == XLookupBoth) && length > 0)
        return {buffer.data(), static_cast<std::size_t>(length)};
      return {};
    }
    std::array<char, 64> buffer{};
    KeySym symbol{};
    const int length =
        XLookupString(&event, buffer.data(), static_cast<int>(buffer.size()), &symbol, nullptr);
    return length > 0 ? std::string(buffer.data(), static_cast<std::size_t>(length))
                      : std::string{};
  }
  std::thread::id owner_;
  Display *display_{};
  XIM inputMethod_{};
  Atom closeAtom_{};
  std::uint64_t next_ = 1;
  std::unordered_map<std::uint64_t, ::Window> windows_;
  std::unordered_map<::Window, WindowHandle> reverse_;
  std::unordered_map<::Window, XIC> inputContexts_;
  std::unordered_set<::Window> gone_; // destroyed by the server before our own Destroy
  std::vector<WindowEvent> pending_;
  std::vector<WindowEvent> pumped_;
};
} // namespace

std::unique_ptr<IWindowSystem> CreateWindowSystem() {
  auto result = std::make_unique<X11WindowSystem>();
  return result;
}
} // namespace Nexora::Window
