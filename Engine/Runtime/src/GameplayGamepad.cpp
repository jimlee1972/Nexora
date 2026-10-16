#include "Nexora/Runtime/GameplayGamepad.h"
#include "GameplayXInputState.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#if defined(__linux__)
#include <fcntl.h>
#include <linux/joystick.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <xinput.h>
#endif
namespace nexora::runtime {
#if defined(_WIN32)
struct NativeGameplayGamepad::WindowsState {
  HMODULE library{};
  using Read = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
  Read read{};
  WindowsState() {
    library = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library)
      library = LoadLibraryExW(L"xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (library) {
      const auto function = GetProcAddress(library, "XInputGetState");
      static_assert(sizeof(function) == sizeof(read));
      std::memcpy(&read, &function, sizeof(read));
    }
  }
  ~WindowsState() {
    if (library)
      FreeLibrary(library);
  }
};
NativeGameplayGamepad::NativeGameplayGamepad()
    : windows_(std::make_unique<WindowsState>()), owner_(std::this_thread::get_id()) {}
#else
NativeGameplayGamepad::NativeGameplayGamepad() : owner_(std::this_thread::get_id()) {}
#endif
NativeGameplayGamepad::~NativeGameplayGamepad() {
#if defined(__linux__)
  for (int descriptor : descriptors_)
    if (descriptor >= 0)
      close(descriptor);
#endif
}
bool NativeGameplayGamepad::Poll(GameplayInput &input, std::uint32_t user) {
  if (owner_ != std::this_thread::get_id() || user >= 8)
    return false;
#if defined(__linux__)
  const auto now = std::chrono::steady_clock::now();
  if (now >= next_scan_) {
    next_scan_ = now + std::chrono::seconds(1);
    for (std::size_t id = 0; id < descriptors_.size(); ++id) {
      if (descriptors_[id] < 0) {
        const auto path = "/dev/input/js" + std::to_string(id);
        descriptors_[id] = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      }
    }
  }
  for (std::uint32_t id = 0; id < descriptors_.size(); ++id) {
    auto &descriptor = descriptors_[id];
    if (descriptor < 0)
      continue;
    // Bound work even if a malfunctioning device floods the queue.
    for (unsigned count = 0; count < 256; ++count) {
      js_event event{};
      const auto bytes = read(descriptor, &event, sizeof(event));
      if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        break;
      if (bytes != sizeof(event)) {
        close(descriptor);
        descriptor = -1;
        input.Disconnect(3, id);
        break;
      }
      const auto type = event.type & ~JS_EVENT_INIT;
      if (type == JS_EVENT_BUTTON && event.number < 32)
        input.Submit(user, 3, id, event.number, event.value ? 1.0F : 0.0F);
      else if (type == JS_EVENT_AXIS && event.number < 16)
        input.Submit(user, 3, id, 32U + event.number,
                     std::clamp(static_cast<float>(event.value) / 32767.0F, -1.0F, 1.0F));
    }
  }
  return true;
#elif defined(_WIN32)
  if (!windows_->read)
    return false;
  for (std::uint32_t id = 0; id < 4; ++id) {
    XINPUT_STATE state{};
    if (windows_->read(id, &state) != ERROR_SUCCESS) {
      if (!detail::SubmitXInputSample(input, user, id, nullptr))
        return false;
      continue;
    }
    const auto &pad = state.Gamepad;
    const detail::XInputSample sample{pad.wButtons,
                                      {pad.sThumbLX, pad.sThumbLY, pad.sThumbRX, pad.sThumbRY},
                                      {pad.bLeftTrigger, pad.bRightTrigger}};
    // Do not skip unchanged packets: focus recovery needs a fresh full snapshot.
    if (!detail::SubmitXInputSample(input, user, id, &sample))
      return false;
  }
  return true;
#else
  (void)input;
  return false;
#endif
}
} // namespace nexora::runtime
