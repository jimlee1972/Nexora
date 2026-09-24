#if !defined(_WIN32)
#error "Win32Window.cpp is only built on Windows"
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Nexora/Window/Window.h"
#include <algorithm>
#include <chrono>
#include <imm.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <windowsx.h>

namespace Nexora::Window {
namespace {
Key TranslateKey(WPARAM key, LPARAM data) noexcept {
  if (key >= '0' && key <= '9')
    return static_cast<Key>(static_cast<int>(Key::Digit0) + key - '0');
  if (key >= 'A' && key <= 'Z')
    return static_cast<Key>(static_cast<int>(Key::A) + key - 'A');
  if (key >= VK_F1 && key <= VK_F12)
    return static_cast<Key>(static_cast<int>(Key::F1) + key - VK_F1);
  if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
    return static_cast<Key>(static_cast<int>(Key::Keypad0) + key - VK_NUMPAD0);
  switch (key) {
  case VK_TAB:
    return Key::Tab;
  case VK_LEFT:
    return Key::LeftArrow;
  case VK_RIGHT:
    return Key::RightArrow;
  case VK_UP:
    return Key::UpArrow;
  case VK_DOWN:
    return Key::DownArrow;
  case VK_PRIOR:
    return Key::PageUp;
  case VK_NEXT:
    return Key::PageDown;
  case VK_HOME:
    return Key::Home;
  case VK_END:
    return Key::End;
  case VK_INSERT:
    return Key::Insert;
  case VK_DELETE:
    return Key::Delete;
  case VK_BACK:
    return Key::Backspace;
  case VK_SPACE:
    return Key::Space;
  case VK_RETURN:
    return (data & (1LL << 24)) ? Key::KeypadEnter : Key::Enter;
  case VK_ESCAPE:
    return Key::Escape;
  case VK_OEM_7:
    return Key::Apostrophe;
  case VK_OEM_COMMA:
    return Key::Comma;
  case VK_OEM_MINUS:
    return Key::Minus;
  case VK_OEM_PERIOD:
    return Key::Period;
  case VK_OEM_2:
    return Key::Slash;
  case VK_OEM_1:
    return Key::Semicolon;
  case VK_OEM_PLUS:
    return Key::Equal;
  case VK_OEM_4:
    return Key::LeftBracket;
  case VK_OEM_5:
    return Key::Backslash;
  case VK_OEM_6:
    return Key::RightBracket;
  case VK_OEM_3:
    return Key::GraveAccent;
  case VK_CAPITAL:
    return Key::CapsLock;
  case VK_SCROLL:
    return Key::ScrollLock;
  case VK_NUMLOCK:
    return Key::NumLock;
  case VK_SNAPSHOT:
    return Key::PrintScreen;
  case VK_PAUSE:
    return Key::Pause;
  case VK_DECIMAL:
    return Key::KeypadDecimal;
  case VK_DIVIDE:
    return Key::KeypadDivide;
  case VK_MULTIPLY:
    return Key::KeypadMultiply;
  case VK_SUBTRACT:
    return Key::KeypadSubtract;
  case VK_ADD:
    return Key::KeypadAdd;
  case VK_LSHIFT:
    return Key::LeftShift;
  case VK_SHIFT:
    return ((data >> 16) & 0xff) == MapVirtualKeyW(VK_RSHIFT, MAPVK_VK_TO_VSC) ? Key::RightShift
                                                                               : Key::LeftShift;
  case VK_LCONTROL:
    return Key::LeftControl;
  case VK_CONTROL:
    return (data & (1LL << 24)) ? Key::RightControl : Key::LeftControl;
  case VK_LMENU:
    return Key::LeftAlt;
  case VK_MENU:
    return (data & (1LL << 24)) ? Key::RightAlt : Key::LeftAlt;
  case VK_LWIN:
    return Key::LeftSuper;
  case VK_RSHIFT:
    return Key::RightShift;
  case VK_RCONTROL:
    return Key::RightControl;
  case VK_RMENU:
    return Key::RightAlt;
  case VK_RWIN:
    return Key::RightSuper;
  case VK_APPS:
    return Key::Menu;
  default:
    return Key::Unknown;
  }
}

KeyModifiers CurrentModifiers() noexcept {
  unsigned modifiers = 0;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    modifiers |= static_cast<unsigned>(KeyModifiers::Control);
  if (GetKeyState(VK_SHIFT) & 0x8000)
    modifiers |= static_cast<unsigned>(KeyModifiers::Shift);
  if (GetKeyState(VK_MENU) & 0x8000)
    modifiers |= static_cast<unsigned>(KeyModifiers::Alt);
  if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
    modifiers |= static_cast<unsigned>(KeyModifiers::Super);
  return static_cast<KeyModifiers>(modifiers);
}
std::uint64_t Now() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}
std::wstring Utf8ToWide(std::string_view utf8) {
  if (utf8.empty())
    return {};
  const auto length =
      MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (length <= 0)
    return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
  return wide;
}
class Win32WindowSystem final : public IWindowSystem {
public:
  Win32WindowSystem() : owner_(std::this_thread::get_id()), instance_(GetModuleHandleW(nullptr)) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = &WndProc;
    wc.hInstance = instance_;
    wc.lpszClassName = L"Nexora.Window";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    atom_ = RegisterClassExW(&wc);
    if (!atom_ && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return;
    available_ = true;
  }
  ~Win32WindowSystem() override {
    for (auto &entry : windows_)
      DestroyWindow(entry.second);
    windows_.clear();
    if (atom_)
      UnregisterClassW(L"Nexora.Window", instance_);
  }
  std::thread::id OwnerThread() const noexcept override { return owner_; }
  WindowResult Create(const WindowDescriptor &d) override {
    if (!OnOwner())
      return {{}, WindowError::WrongThread};
    if (!available_ || d.width == 0 || d.height == 0)
      return {{}, WindowError::InvalidDescriptor};
    const auto id = next_++;
    std::wstring title = Utf8ToWide(d.title);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                  (d.resizable ? WS_THICKFRAME | WS_MAXIMIZEBOX : 0);
    RECT r{0, 0, static_cast<LONG>(d.width), static_cast<LONG>(d.height)};
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForSystem());
    auto hwnd =
        CreateWindowExW(0, L"Nexora.Window", title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                        r.right - r.left, r.bottom - r.top, nullptr, nullptr, instance_, this);
    if (!hwnd)
      return {{}, WindowError::PlatformFailure};
    windows_[id] = hwnd;
    SetPropW(hwnd, L"Nexora.Handle", reinterpret_cast<HANDLE>(static_cast<uintptr_t>(id)));
    if (d.initiallyVisible)
      ShowWindow(hwnd, SW_SHOW);
    return {{id}, WindowError::None};
  }
  WindowError Destroy(WindowHandle h) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    auto it = windows_.find(h.value);
    if (it == windows_.end())
      return WindowError::InvalidHandle;
    DestroyWindow(it->second);
    windows_.erase(it);
    EraseQueued(h);
    return WindowError::None;
  }
  WindowError Show(WindowHandle h, bool visible) override {
    auto hwnd = Find(h);
    if (!OnOwner())
      return WindowError::WrongThread;
    if (!hwnd)
      return WindowError::InvalidHandle;
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    return WindowError::None;
  }
  WindowError Resize(WindowHandle h, uint32_t w, uint32_t he) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    auto hwnd = Find(h);
    if (!hwnd || !w || !he)
      return hwnd ? WindowError::InvalidDescriptor : WindowError::InvalidHandle;
    RECT r{0, 0, (LONG)w, (LONG)he};
    auto style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForWindow(hwnd));
    return SetWindowPos(hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)
               ? WindowError::None
               : WindowError::PlatformFailure;
  }
  WindowError SetFullscreen(WindowHandle h, bool fullscreen) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    auto hwnd = Find(h);
    if (!hwnd)
      return WindowError::InvalidHandle;
    auto &state = fullscreen_[h.value];
    if (state == fullscreen)
      return WindowError::None;
    if (fullscreen) {
      GetWindowRect(hwnd, &windowedRects_[h.value]);
      SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
      const auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO info{sizeof(info)};
      if (!GetMonitorInfoW(monitor, &info))
        return WindowError::PlatformFailure;
      SetWindowPos(hwnd, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                   info.rcMonitor.right - info.rcMonitor.left,
                   info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED);
    } else {
      SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
      const auto r = windowedRects_[h.value];
      SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                   SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    state = fullscreen;
    return WindowError::None;
  }
  std::span<const WindowEvent> PumpEvents() override {
    pumped_.clear();
    if (!OnOwner())
      return pumped_;
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    for (auto &e : pending_) {
      if (e.type == WindowEventType::Resized && !pumped_.empty() && pumped_.back().type == e.type &&
          pumped_.back().window == e.window)
        pumped_.back() = e;
      else
        pumped_.push_back(e);
    }
    pending_.clear();
    return pumped_;
  }
  void *NativeHandle(WindowHandle h) const noexcept override { return Find(h); }
  WindowError SetImeCandidatePosition(WindowHandle handle, std::int32_t x,
                                      std::int32_t y) override {
    if (!OnOwner())
      return WindowError::WrongThread;
    const auto hwnd = Find(handle);
    if (!hwnd)
      return WindowError::InvalidHandle;
    const auto context = ImmGetContext(hwnd);
    if (!context)
      return WindowError::PlatformFailure;
    CANDIDATEFORM candidate{};
    candidate.dwStyle = CFS_CANDIDATEPOS;
    candidate.ptCurrentPos = {x, y};
    const bool positioned = ImmSetCandidateWindow(context, &candidate) != FALSE;
    ImmReleaseContext(hwnd, context);
    return positioned ? WindowError::None : WindowError::PlatformFailure;
  }

private:
  bool OnOwner() const { return std::this_thread::get_id() == owner_; }
  HWND Find(WindowHandle h) const {
    auto i = windows_.find(h.value);
    return i == windows_.end() ? nullptr : i->second;
  }
  void EraseQueued(WindowHandle h) {
    std::erase_if(pending_, [&](auto &e) { return e.window == h; });
    std::erase_if(pumped_, [&](auto &e) { return e.window == h; });
  }
  void Push(HWND hwnd, WindowEventType type, int v0 = 0, int v1 = 0, uint32_t w = 0, uint32_t h = 0,
            float scale = 1, KeyModifiers modifiers = KeyModifiers::None) {
    auto id = (uint64_t)(uintptr_t)GetPropW(hwnd, L"Nexora.Handle");
    if (id)
      pending_.push_back({{id}, type, Now(), w, h, scale, v0, v1, modifiers});
  }
  static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto *self = reinterpret_cast<Win32WindowSystem *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
      self = static_cast<Win32WindowSystem *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
      SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
    }
    if (!self)
      return DefWindowProcW(h, m, w, l);
    switch (m) {
    case WM_CLOSE:
      self->Push(h, WindowEventType::CloseRequested);
      return 0;
    case WM_SIZE:
      self->Push(h, WindowEventType::Resized, 0, 0, LOWORD(l), HIWORD(l));
      return 0;
    case WM_DPICHANGED: {
      auto *r = reinterpret_cast<RECT *>(l);
      SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      self->Push(h, WindowEventType::DpiChanged, 0, 0, 0, 0, float(HIWORD(w)) / 96.f);
      return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
      self->Push(h, WindowEventType::FocusChanged, m == WM_SETFOCUS);
      return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      self->Push(h, WindowEventType::Key, static_cast<int>(TranslateKey(w, l)),
                 (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) ? 1 : 0, 0, 0, 1.0F, CurrentModifiers());
      return 0;
    case WM_CHAR:
      self->Push(h, WindowEventType::Text, (int)w);
      return 0;
    case WM_IME_COMPOSITION:
      if (l & GCS_RESULTSTR) {
        auto imc = ImmGetContext(h);
        if (imc) {
          LONG bytes = ImmGetCompositionStringW(imc, GCS_RESULTSTR, nullptr, 0);
          if (bytes > 0) {
            std::wstring text(static_cast<std::size_t>(bytes) / 2, L'\0');
            ImmGetCompositionStringW(imc, GCS_RESULTSTR, text.data(), bytes);
            for (wchar_t c : text)
              self->Push(h, WindowEventType::Text, c);
          }
          ImmReleaseContext(h, imc);
        }
      }
      return 0;
    case WM_MOUSEMOVE:
      self->Push(h, WindowEventType::Pointer, GET_X_LPARAM(l), GET_Y_LPARAM(l));
      return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      self->Push(h, WindowEventType::PointerButton,
                 m == WM_LBUTTONDOWN || m == WM_LBUTTONUP   ? 0
                 : m == WM_RBUTTONDOWN || m == WM_RBUTTONUP ? 1
                                                            : 2,
                 m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MBUTTONDOWN);
      return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      self->Push(h, WindowEventType::Wheel, m == WM_MOUSEWHEEL ? GET_WHEEL_DELTA_WPARAM(w) : 0,
                 m == WM_MOUSEHWHEEL ? GET_WHEEL_DELTA_WPARAM(w) : 0);
      return 0;
    case WM_NCDESTROY:
      SetWindowLongPtrW(h, GWLP_USERDATA, 0);
      RemovePropW(h, L"Nexora.Handle");
      break;
    }
    return DefWindowProcW(h, m, w, l);
  }
  std::thread::id owner_;
  HINSTANCE instance_{};
  ATOM atom_{};
  bool available_{};
  uint64_t next_ = 1;
  std::unordered_map<uint64_t, HWND> windows_;
  std::unordered_map<uint64_t, bool> fullscreen_;
  std::unordered_map<uint64_t, RECT> windowedRects_;
  std::vector<WindowEvent> pending_, pumped_;
};
} // namespace
std::unique_ptr<IWindowSystem> CreateWindowSystem() {
  return std::make_unique<Win32WindowSystem>();
}
} // namespace Nexora::Window
