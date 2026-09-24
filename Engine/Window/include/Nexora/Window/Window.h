#pragma once

#include "Nexora/Window/Api.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <thread>

namespace Nexora::Window {

struct WindowHandle final {
  std::uint64_t value = 0;

  [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
  friend constexpr bool operator==(WindowHandle, WindowHandle) noexcept = default;
};

struct WindowDescriptor final {
  std::string_view title = "Nexora";
  std::uint32_t width = 1280;
  std::uint32_t height = 720;
  bool resizable = true;
  bool initiallyVisible = true;
};

enum class WindowEventType : std::uint8_t {
  CloseRequested,
  Resized,
  DpiChanged,
  FocusChanged,
  Key,
  Text,
  Pointer,
  PointerButton,
  Wheel,
  DisplayChanged,
};

// Platform-independent physical keys used by UI and gameplay input consumers. Native window
// implementations translate their key codes before publishing WindowEvent::Key.
enum class Key : std::int32_t {
  Unknown = 0,
  Tab,
  LeftArrow,
  RightArrow,
  UpArrow,
  DownArrow,
  PageUp,
  PageDown,
  Home,
  End,
  Insert,
  Delete,
  Backspace,
  Space,
  Enter,
  Escape,
  Apostrophe,
  Comma,
  Minus,
  Period,
  Slash,
  Semicolon,
  Equal,
  LeftBracket,
  Backslash,
  RightBracket,
  GraveAccent,
  CapsLock,
  ScrollLock,
  NumLock,
  PrintScreen,
  Pause,
  Keypad0,
  Keypad1,
  Keypad2,
  Keypad3,
  Keypad4,
  Keypad5,
  Keypad6,
  Keypad7,
  Keypad8,
  Keypad9,
  KeypadDecimal,
  KeypadDivide,
  KeypadMultiply,
  KeypadSubtract,
  KeypadAdd,
  KeypadEnter,
  KeypadEqual,
  LeftShift,
  LeftControl,
  LeftAlt,
  LeftSuper,
  RightShift,
  RightControl,
  RightAlt,
  RightSuper,
  Menu,
  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12
};

enum class KeyModifiers : std::uint8_t { None = 0, Control = 1, Shift = 2, Alt = 4, Super = 8 };

struct WindowEvent final {
  WindowHandle window;
  WindowEventType type = WindowEventType::CloseRequested;
  std::uint64_t timestampNanoseconds = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float scale = 1.0F;
  std::int32_t value0 = 0;
  std::int32_t value1 = 0;
  KeyModifiers modifiers = KeyModifiers::None;
};

enum class WindowError : std::uint8_t {
  None,
  Unsupported,
  InvalidDescriptor,
  InvalidHandle,
  WrongThread,
  PlatformFailure,
};

struct WindowResult final {
  WindowHandle handle;
  WindowError error = WindowError::None;

  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return error == WindowError::None;
  }
};

// The application owns IWindowSystem and every handle it creates. All methods must be called on
// OwnerThread(). PumpEvents returns a borrowed view valid until the next pump or destruction.
// Implementations coalesce consecutive pending Resized events for the same window.
class NEXORA_WINDOW_API IWindowSystem {
public:
  virtual ~IWindowSystem() = default;
  [[nodiscard]] virtual std::thread::id OwnerThread() const noexcept = 0;
  [[nodiscard]] virtual WindowResult Create(const WindowDescriptor &descriptor) = 0;
  [[nodiscard]] virtual WindowError Destroy(WindowHandle window) = 0;
  [[nodiscard]] virtual WindowError Show(WindowHandle window, bool visible) = 0;
  [[nodiscard]] virtual WindowError Resize(WindowHandle window, std::uint32_t clientWidth,
                                           std::uint32_t clientHeight) = 0;
  [[nodiscard]] virtual WindowError SetFullscreen(WindowHandle window, bool fullscreen) = 0;
  [[nodiscard]] virtual std::span<const WindowEvent> PumpEvents() = 0;
  [[nodiscard]] virtual WindowError SetImeCandidatePosition(WindowHandle window, std::int32_t x,
                                                            std::int32_t y) = 0;
  // Opaque native identity for presentation adapters; nullptr for unsupported handles.
  [[nodiscard]] virtual void *NativeHandle(WindowHandle window) const noexcept = 0;
};

// Returns the native platform implementation, or nullptr on unsupported platforms.
[[nodiscard]] NEXORA_WINDOW_API std::unique_ptr<IWindowSystem> CreateWindowSystem();

} // namespace Nexora::Window
