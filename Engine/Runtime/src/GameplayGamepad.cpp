#include "Nexora/Runtime/GameplayGamepad.h"
#include "GameplayDevicePresence.h"
#include "GameplayDirectInputState.h"
#include "GameplayXInputState.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
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

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>
#endif
namespace nexora::runtime {
#if defined(_WIN32)
struct NativeGameplayGamepad::WindowsState {
  struct Joystick {
    IDirectInputDevice8W *device{};
    GUID instance{};
    bool dualshock4{}, seen{};
  };
  struct AxisRanges {
    IDirectInputDevice8W *device;
    bool valid{true};
  };
  HMODULE library{};
  IDirectInput8W *direct{};
  std::array<Joystick, 4> joysticks{};
  std::array<bool, 4> xinputConnected{};
  std::chrono::steady_clock::time_point nextScan{};
  using Read = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
  Read read{};
  using Write = DWORD(WINAPI *)(DWORD, XINPUT_VIBRATION *);
  Write write{};
  using Capabilities = DWORD(WINAPI *)(DWORD, DWORD, XINPUT_CAPABILITIES *);
  Capabilities capabilities{};
  uint32_t rumbleMask{};
  WindowsState() {
    (void)DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
                             reinterpret_cast<void **>(&direct), nullptr);
    library = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library)
      library = LoadLibraryExW(L"xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (library) {
      const auto function = GetProcAddress(library, "XInputGetState");
      static_assert(sizeof(function) == sizeof(read));
      std::memcpy(&read, &function, sizeof(read));
      const auto writer = GetProcAddress(library, "XInputSetState");
      static_assert(sizeof(writer) == sizeof(write));
      std::memcpy(&write, &writer, sizeof(write));
      const auto query = GetProcAddress(library, "XInputGetCapabilities");
      static_assert(sizeof(query) == sizeof(capabilities));
      std::memcpy(&capabilities, &query, sizeof(capabilities));
    }
  }
  ~WindowsState() {
    if (write) {
      XINPUT_VIBRATION zero{};
      for (DWORD id = 0; id < 4; ++id)
        (void)write(id, &zero);
    }
    for (auto &joystick : joysticks)
      Release(joystick);
    if (direct)
      direct->Release();
    if (library)
      FreeLibrary(library);
  }
  static void Release(Joystick &joystick) {
    if (joystick.device) {
      joystick.device->Unacquire();
      joystick.device->Release();
    }
    joystick = {};
  }
  static BOOL CALLBACK SetRange(const DIDEVICEOBJECTINSTANCEW *object, void *context) {
    auto &ranges = *static_cast<AxisRanges *>(context);
    DIPROPRANGE range{};
    range.diph = {sizeof(range), sizeof(range.diph), object->dwType, DIPH_BYID};
    range.lMin = -32768;
    range.lMax = 32767;
    ranges.valid = SUCCEEDED(ranges.device->SetProperty(DIPROP_RANGE, &range.diph));
    return ranges.valid ? DIENUM_CONTINUE : DIENUM_STOP;
  }
  static BOOL CALLBACK Enumerate(const DIDEVICEINSTANCEW *instance, void *context) {
    auto &self = *static_cast<WindowsState *>(context);
    for (auto &joystick : self.joysticks)
      if (joystick.device && joystick.instance == instance->guidInstance) {
        joystick.seen = true;
        return DIENUM_CONTINUE;
      }
    std::size_t slot{};
    while (slot < self.joysticks.size() &&
           (self.xinputConnected[slot] || self.joysticks[slot].device))
      ++slot;
    if (slot == self.joysticks.size())
      return DIENUM_CONTINUE;
    IDirectInputDevice8W *device{};
    if (FAILED(self.direct->CreateDevice(instance->guidInstance, &device, nullptr)))
      return DIENUM_CONTINUE;
    DIPROPGUIDANDPATH path{};
    path.diph = {sizeof(path), sizeof(path.diph), 0, DIPH_DEVICE};
    // XInput devices also enumerate through DirectInput. Never submit them twice.
    const bool duplicate = SUCCEEDED(device->GetProperty(DIPROP_GUIDANDPATH, &path.diph)) &&
                           (std::wcsstr(path.wszPath, L"IG_") || std::wcsstr(path.wszPath, L"ig_"));
    if (duplicate || FAILED(device->SetDataFormat(&c_dfDIJoystick2)) ||
        FAILED(device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
      device->Release();
      return DIENUM_CONTINUE;
    }
    AxisRanges ranges{device};
    if (FAILED(device->EnumObjects(SetRange, &ranges, DIDFT_AXIS)) || !ranges.valid) {
      device->Release();
      return DIENUM_CONTINUE;
    }
    const auto product = instance->guidProduct.Data1;
    const bool ps4 =
        LOWORD(product) == 0x054c && (HIWORD(product) == 0x05c4 || HIWORD(product) == 0x09cc);
    self.joysticks[slot] = {device, instance->guidInstance, ps4, true};
    std::fprintf(stderr,
                 "native_gamepad slot=%u backend=DirectInput vid=%04x pid=%04x dualshock4=%u\n",
                 static_cast<unsigned>(slot), LOWORD(product), HIWORD(product), unsigned(ps4));
    return DIENUM_CONTINUE;
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
        Disconnect(input, id);
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
  connected_ = 0;
  for (uint32_t id = 0; id < 4; ++id)
    if (descriptors_[id] >= 0) {
      connected_ |= 1u << id;
      (void)detail::SetGamepadPresence(observed_, disconnected_, id, true);
    }
  return true;
#elif defined(_WIN32)
  if (!windows_->read && !windows_->direct)
    return false;
  connected_ = 0;
  windows_->rumbleMask = 0;
  std::array<XINPUT_STATE, 4> states{};
  for (std::uint32_t id = 0; id < 4; ++id) {
    const bool connected = windows_->read && windows_->read(id, &states[id]) == ERROR_SUCCESS;
    auto &joystick = windows_->joysticks[id];
    if (connected && joystick.device) {
      WindowsState::Release(joystick);
      if (!Disconnect(input, id))
        return false;
    }
    if (windows_->xinputConnected[id] && !connected)
      if (!Disconnect(input, id))
        return false;
    windows_->xinputConnected[id] = connected;
    XINPUT_CAPABILITIES caps{};
    if (connected && windows_->write && windows_->capabilities &&
        windows_->capabilities(id, XINPUT_FLAG_GAMEPAD, &caps) == ERROR_SUCCESS &&
        (caps.Vibration.wLeftMotorSpeed || caps.Vibration.wRightMotorSpeed))
      windows_->rumbleMask |= 1u << id;
  }
  const auto now = std::chrono::steady_clock::now();
  if (windows_->direct && now >= windows_->nextScan) {
    windows_->nextScan = now + std::chrono::seconds(1);
    for (auto &joystick : windows_->joysticks)
      joystick.seen = false;
    const auto result = windows_->direct->EnumDevices(DI8DEVCLASS_GAMECTRL, WindowsState::Enumerate,
                                                      windows_.get(), DIEDFL_ATTACHEDONLY);
    if (SUCCEEDED(result))
      for (std::uint32_t id = 0; id < 4; ++id) {
        auto &joystick = windows_->joysticks[id];
        if (joystick.device && !joystick.seen) {
          WindowsState::Release(joystick);
          if (!Disconnect(input, id))
            return false;
        }
      }
  }
  for (std::uint32_t id = 0; id < 4; ++id) {
    auto &joystick = windows_->joysticks[id];
    if (joystick.device) {
      DIJOYSTATE2 state{};
      auto result = joystick.device->Poll();
      if (FAILED(result)) {
        // Acquisition can expose a zero-initialized cache before the first HID report.
        // Clear old controls now; consume a fresh snapshot on the following input frame.
        (void)joystick.device->Acquire();
        if (!Disconnect(input, id))
          return false;
        continue;
      }
      if (SUCCEEDED(result))
        result = joystick.device->GetDeviceState(sizeof(state), &state);
      if (FAILED(result)) {
        if (!Disconnect(input, id))
          return false;
        continue;
      }
      connected_ |= 1u << id;
      (void)detail::SetGamepadPresence(observed_, disconnected_, id, true);
      detail::DirectInputSample sample{};
      sample.axes = {state.lX,  state.lY,  state.lZ,           state.lRx,
                     state.lRy, state.lRz, state.rglSlider[0], state.rglSlider[1]};
      std::copy_n(state.rgbButtons, sample.buttons.size(), sample.buttons.begin());
      sample.pov = state.rgdwPOV[0];
      sample.dualshock4 = joystick.dualshock4;
      if (!detail::SubmitDirectInputSample(input, user, id, &sample))
        return false;
      continue;
    }
    if (!windows_->xinputConnected[id])
      continue;
    connected_ |= 1u << id;
    (void)detail::SetGamepadPresence(observed_, disconnected_, id, true);
    const auto &pad = states[id].Gamepad;
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
bool NativeGameplayGamepad::Disconnect(GameplayInput &input, uint32_t device) {
  (void)detail::SetGamepadPresence(observed_, disconnected_, device, false);
  return input.Disconnect(NEXORA_INPUT_DEVICE_GAMEPAD, device);
}
uint32_t NativeGameplayGamepad::RumbleMask() const noexcept {
#if defined(_WIN32)
  return windows_->rumbleMask & connected_;
#else
  return 0;
#endif
}
int32_t NativeGameplayGamepad::SetRumble(uint32_t device, float low, float high) {
  if (owner_ != std::this_thread::get_id())
    return -3;
  if (device >= 4 || !std::isfinite(low) || !std::isfinite(high) || low < 0 || high < 0 ||
      low > 1 || high > 1)
    return -1;
#if defined(_WIN32)
  if (!windows_->write || !windows_->xinputConnected[device] ||
      ((low || high) && !(windows_->rumbleMask & (1u << device))))
    return -2;
  XINPUT_VIBRATION value{static_cast<WORD>(std::lround(low * 65535.F)),
                         static_cast<WORD>(std::lround(high * 65535.F))};
  const auto result = windows_->write(device, &value);
  return result == ERROR_SUCCESS ? 0 : result == ERROR_DEVICE_NOT_CONNECTED ? -2 : -3;
#else
  return -2;
#endif
}
} // namespace nexora::runtime
