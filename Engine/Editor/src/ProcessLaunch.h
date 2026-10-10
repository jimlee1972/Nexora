#pragma once

#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>

namespace nexora::editor::detail {
// Serialize pipe creation through spawn across the built-in POSIX launchers. Darwin's older
// pipe API cannot atomically set CLOEXEC; other embedding launchers must use the same policy.
inline std::mutex &ProcessLaunchMutex() {
  static std::mutex mutex;
  return mutex;
}
inline int ProcessOutputPipe(int (&descriptors)[2]) {
#if defined(__linux__)
  return pipe2(descriptors, O_CLOEXEC);
#else
  if (pipe(descriptors))
    return -1;
  for (const auto descriptor : descriptors)
    if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
      const auto saved = errno;
      close(descriptors[0]);
      close(descriptors[1]);
      errno = saved;
      return -1;
    }
  return 0;
#endif
}
} // namespace nexora::editor::detail
