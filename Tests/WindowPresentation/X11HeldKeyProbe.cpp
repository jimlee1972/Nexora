#include "Nexora/Window/Window.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

int main() {
  using namespace Nexora::Window;
  auto windows = CreateWindowSystem();
  if (!windows)
    return 2;
  const auto main = windows->Create({"Nexora X11 held key probe", 320, 200});
  const auto other = windows->Create({"Nexora X11 focus target", 160, 100});
  if (!main || !other)
    return 3;
  const auto xid = reinterpret_cast<std::uintptr_t>(windows->NativeHandle(main.handle));
  const auto other_xid = reinterpret_cast<std::uintptr_t>(windows->NativeHandle(other.handle));
  std::printf("{\"kind\":\"ready\",\"window\":%llu,\"other\":%llu,\"handle\":%llu,\"other_handle\":"
              "%llu,\"key_n\":%d,\"key_m\":%d}\n",
              static_cast<unsigned long long>(xid), static_cast<unsigned long long>(other_xid),
              static_cast<unsigned long long>(main.handle.value),
              static_cast<unsigned long long>(other.handle.value), static_cast<int>(Key::N),
              static_cast<int>(Key::M));
  std::fflush(stdout);
  unsigned down = 0, up = 0, text = 0, focused = 0, unfocused = 0;
  unsigned m_down = 0, m_up = 0;
  bool close = false, destroyed = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
  while (!close && std::chrono::steady_clock::now() < deadline) {
    for (const auto &event : windows->PumpEvents()) {
      std::printf("{\"kind\":\"event\",\"window\":%llu,\"type\":%u,\"value0\":%d,"
                  "\"value1\":%d,\"timestamp_ns\":%llu}\n",
                  static_cast<unsigned long long>(event.window.value),
                  static_cast<unsigned>(event.type), event.value0, event.value1,
                  static_cast<unsigned long long>(event.timestampNanoseconds));
      if (event.window != main.handle)
        continue;
      if (event.type == WindowEventType::Key && event.value0 == static_cast<int>(Key::N)) {
        if (event.value1)
          ++down;
        else
          ++up;
      } else if (event.type == WindowEventType::Key && event.value0 == static_cast<int>(Key::M)) {
        if (event.value1)
          ++m_down;
        else
          ++m_up;
      } else if (event.type == WindowEventType::Text && event.value0 == 'n') {
        ++text;
      } else if (event.type == WindowEventType::FocusChanged) {
        if (event.value0)
          ++focused;
        else
          ++unfocused;
      } else if (event.type == WindowEventType::CloseRequested) {
        close = true;
      } else if (event.type == WindowEventType::WindowDestroyed) {
        destroyed = true;
        close = true;
      }
    }
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const bool native_alive_at_close = windows->NativeHandle(main.handle) != nullptr;
  const auto other_result = windows->Destroy(other.handle);
  const auto main_result = windows->Destroy(main.handle);
  std::printf("{\"kind\":\"summary\",\"down\":%u,\"up\":%u,\"text\":%u,\"focus\":%u,"
              "\"blur\":%u,\"close\":%u,\"destroyed_event\":%u,\"alive_at_close\":%u,"
              "\"destroy_ok\":%u,\"m_down\":%u,\"m_up\":%u}\n",
              down, up, text, focused, unfocused, unsigned(close), unsigned(destroyed),
              unsigned(native_alive_at_close),
              unsigned(other_result == WindowError::None && main_result == WindowError::None),
              m_down, m_up);
  std::fflush(stdout);
  return close && !destroyed && native_alive_at_close && other_result == WindowError::None &&
                 main_result == WindowError::None
             ? 0
             : 4;
}
