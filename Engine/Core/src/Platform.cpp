#include "Nexora/Core/Platform.h"

#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>

#include <processthreadsapi.h>
#elif defined(__APPLE__)
#include <pthread.h>
#else
#include <pthread.h>
#endif

namespace nexora::core::platform {

std::size_t HardwareConcurrency() noexcept {
  const auto detected = std::thread::hardware_concurrency();
  return detected == 0 ? 1 : detected;
}

#if defined(_WIN32)
void SetCurrentThreadName(std::string_view name) noexcept {
  std::wstring wide(name.begin(), name.end());
  // SetThreadDescription is only available on Windows 10 1607+; failure is
  // a diagnostic no-op, never a hard error.
  SetThreadDescription(GetCurrentThread(), wide.c_str());
}
#elif defined(__APPLE__)
void SetCurrentThreadName(std::string_view name) noexcept {
  // macOS pthread_setname_np names only the calling thread and truncates
  // internally at 63 characters (MAXTHREADNAMESIZE - 1).
  std::string truncated(name.substr(0, 63));
  pthread_setname_np(truncated.c_str());
}
#else
void SetCurrentThreadName(std::string_view name) noexcept {
  // glibc caps thread names at 15 characters plus the null terminator.
  std::string truncated(name.substr(0, 15));
  pthread_setname_np(pthread_self(), truncated.c_str());
}
#endif

} // namespace nexora::core::platform
