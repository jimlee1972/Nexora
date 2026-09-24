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

struct WindowEvent final {
  WindowHandle window;
  WindowEventType type = WindowEventType::CloseRequested;
  std::uint64_t timestampNanoseconds = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float scale = 1.0F;
  std::int32_t value0 = 0;
  std::int32_t value1 = 0;
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
