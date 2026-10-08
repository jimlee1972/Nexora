#include "Nexora/Core/ProcessMemory.h"
#include "ProcessMemoryReading.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void VerifyParsing() {
  using nexora::core::detail::ResidentBytesFromStatm;
  Require(ResidentBytesFromStatm("100 42 9 0 0 0 0\n", 4096) == 42ULL * 4096,
          "resident count or page unit incorrect");
  Require(ResidentBytesFromStatm("100\t0\n", 16384) == 0,
          "zero observation was confused with unavailable");
  for (const auto *text : {"", "100", "-1 10", "+1 10", "100 -1", "100 +1", "100 42garbage",
                           "garbage 42", "100 18446744073709551616", "100 18446744073709551615"})
    Require(!ResidentBytesFromStatm(text, 4096), "invalid or overflowing statm accepted");
  Require(!ResidentBytesFromStatm(std::string("1 2\0private", 11), 4096) &&
              !ResidentBytesFromStatm("1 2", 0),
          "invalid response/page size accepted");
  Require(ResidentBytesFromStatm("1 18446744073709551615", 1) ==
              std::numeric_limits<std::uint64_t>::max(),
          "lossless page count rejected");
}
void VerifyRealObservation() {
  const auto before = nexora::core::CurrentProcessResidentBytes();
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
  Require(before && *before > 0, "native process resident memory unavailable");
#else
  Require(!before, "unsupported host fabricated a measurement");
#endif
#if defined(__linux__)
  constexpr std::size_t bytes = 16 * 1024 * 1024;
  struct ResidentMapping final {
    void *data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ~ResidentMapping() {
      if (data != MAP_FAILED)
        munmap(data, bytes);
    }
  } mapping;
  Require(mapping.data != MAP_FAILED, "native anonymous allocation failed");
  const auto page_bytes = sysconf(_SC_PAGESIZE);
  Require(page_bytes > 0, "native page size unavailable");
  auto *pages = static_cast<volatile unsigned char *>(mapping.data);
  for (std::size_t offset = 0; offset < bytes; offset += static_cast<std::size_t>(page_bytes))
    pages[offset] = 0x5A;
  const auto after = nexora::core::CurrentProcessResidentBytes();
  // Linux RSS counters may batch updates; require substantial real residency, not exact equality.
  Require(after && *after >= *before + bytes / 2,
          "touched anonymous pages did not affect current process resident memory");
  std::cout << "Native Linux current process RSS bytes: before=" << *before << " after=" << *after
            << " touched=" << bytes << '\n';
#endif
}
} // namespace
int main() {
  try {
    VerifyParsing();
    VerifyRealObservation();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
