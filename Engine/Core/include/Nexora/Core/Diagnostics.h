#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/Log.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string_view>
#include <utility>

namespace nexora::core {

// Process-wide diagnostics. Nested sessions share the first owner's directory/handlers.
// Construct/destroy on the application owner thread; join producers before the last destruction.
class NEXORA_CORE_API DiagnosticSession final {
public:
  explicit DiagnosticSession(std::string_view application = "NexoraEngine",
                             const std::filesystem::path &root = {});
  ~DiagnosticSession();
  DiagnosticSession(const DiagnosticSession &) = delete;
  DiagnosticSession &operator=(const DiagnosticSession &) = delete;
  [[nodiscard]] static std::filesystem::path Directory();
};

// Owner-thread scope identifying the native callback executing on the faulting thread.
class NEXORA_CORE_API DiagnosticScope final {
public:
  explicit DiagnosticScope(std::string_view context) noexcept;
  ~DiagnosticScope();
  DiagnosticScope(const DiagnosticScope &) = delete;
  DiagnosticScope &operator=(const DiagnosticScope &) = delete;

private:
  char previous_[256]{};
};
NEXORA_CORE_API void RefreshDiagnosticModules() noexcept;
// Latest bounded metadata, persisted outside the fatal handler in context.txt.
NEXORA_CORE_API void SetDiagnosticMetadata(std::string_view key, std::string_view value) noexcept;
NEXORA_CORE_API void SetDiagnosticMetadata(std::string_view key, std::uint64_t value) noexcept;

// Bounded, escaped single-line records; no exception escapes a gameplay C boundary.
// Writes directly to the OS before returning, independent of the async log worker.
NEXORA_CORE_API void WriteDiagnostic(LogLevel level, std::string_view category,
                                     std::string_view message) noexcept;
NEXORA_CORE_API void WriteGameplayDiagnostic(std::uint32_t level, const char *message,
                                             std::uint32_t length) noexcept;

// Keep the session alive through exception reporting and existing nonzero exit paths.
template <typename Function>
int RunDiagnosed(std::string_view application, Function &&run) noexcept {
  try {
    DiagnosticSession session(application);
    RefreshDiagnosticModules();
    try {
      const int result = std::forward<Function>(run)();
      if (result != 0)
        WriteDiagnostic(LogLevel::Error, application, "application exited with failure");
      return result;
    } catch (const std::exception &error) {
      WriteDiagnostic(LogLevel::Fatal, application, error.what());
      std::fprintf(stderr, "%s\n", error.what());
    } catch (...) {
      WriteDiagnostic(LogLevel::Fatal, application, "unhandled non-standard exception");
    }
  } catch (const std::exception &error) {
    std::fprintf(stderr, "Nexora diagnostics startup failed: %s\n", error.what());
  } catch (...) {
    std::fputs("Nexora diagnostics startup failed\n", stderr);
  }
  return 1;
}
} // namespace nexora::core
