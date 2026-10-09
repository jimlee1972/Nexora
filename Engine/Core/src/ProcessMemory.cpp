#include "Nexora/Core/ProcessMemory.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include "ProcessMemoryReading.h"
#include <array>
#include <cstdio>
#include <unistd.h>
#endif

namespace nexora::core {
std::optional<std::uint64_t> CurrentProcessResidentBytes() noexcept {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
    return std::nullopt;
  return static_cast<std::uint64_t>(counters.WorkingSetSize);
#elif defined(__APPLE__)
  mach_task_basic_info_data_t information{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&information),
                &count) != KERN_SUCCESS)
    return std::nullopt;
  return static_cast<std::uint64_t>(information.resident_size);
#elif defined(__linux__)
  const auto page_bytes = sysconf(_SC_PAGESIZE);
  if (page_bytes <= 0)
    return std::nullopt;
  auto *input = std::fopen("/proc/self/statm", "rb");
  if (!input)
    return std::nullopt;
  std::array<char, 256> bytes{};
  const auto count = std::fread(bytes.data(), 1, bytes.size(), input);
  const bool failed = std::ferror(input) != 0 || count == bytes.size();
  std::fclose(input);
  if (failed)
    return std::nullopt;
  return detail::ResidentBytesFromStatm({bytes.data(), count},
                                        static_cast<std::uint64_t>(page_bytes));
#else
  return std::nullopt;
#endif
}
} // namespace nexora::core
