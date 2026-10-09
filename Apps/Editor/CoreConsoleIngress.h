#pragma once

#include "Nexora/Core/Log.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/EditorSdk.h"
#include <stdexcept>

namespace nexora::editor::preview {
// Bind one producer and Console for the owner lifetime. Both outlive this adapter; no callbacks
// or UI/World references escape to the worker. Poll copies already-consumed records without Flush.
class CoreConsoleIngress final {
public:
  CoreConsoleIngress(core::AsyncLogService &producer, runtime::RuntimeConsole &console,
                     std::string source)
      : producer_(producer), console_(console), source_(CheckedSource(source)) {}
  CoreConsoleIngress(const CoreConsoleIngress &) = delete;
  CoreConsoleIngress &operator=(const CoreConsoleIngress &) = delete;
  void Poll() {
    const auto snapshot = producer_.SnapshotSince(cursor_);
    console_.ReportDropped(snapshot.rejected_records - rejected_);
    rejected_ = snapshot.rejected_records;
    for (const auto &record : snapshot.records) {
      const auto missed = record.sequence - cursor_ - 1;
      const auto timestamp =
          std::chrono::duration_cast<std::chrono::nanoseconds>(record.timestamp.time_since_epoch())
              .count();
      static_cast<void>(console_.Push({0, Severity(record.level), record.category,
                                       timestamp > 0 ? static_cast<std::uint64_t>(timestamp) : 0,
                                       source_, record.message}));
      // A failed owning allocation leaves the cursor/loss transaction retryable.
      console_.ReportDropped(missed);
      cursor_ = record.sequence;
      ++forwarded_;
    }
  }
  [[nodiscard]] std::uint64_t ForwardedCount() const noexcept { return forwarded_; }
  [[nodiscard]] static runtime::RuntimeLogSeverity Severity(core::LogLevel level) noexcept {
    switch (level) {
    case core::LogLevel::Trace:
    case core::LogLevel::Debug:
      return runtime::RuntimeLogSeverity::Trace;
    case core::LogLevel::Info:
      return runtime::RuntimeLogSeverity::Info;
    case core::LogLevel::Warning:
      return runtime::RuntimeLogSeverity::Warning;
    case core::LogLevel::Error:
      return runtime::RuntimeLogSeverity::Error;
    case core::LogLevel::Fatal:
      return runtime::RuntimeLogSeverity::Fatal;
    }
    return runtime::RuntimeLogSeverity::Error;
  }

private:
  static std::string CheckedSource(std::string_view source) {
    if (source.size() > runtime::RuntimeConsole::kMaxSourceBytes ||
        source.find('\0') != std::string_view::npos || !foundation::IsValidUtf8(source))
      throw std::invalid_argument("Console producer source must be bounded NUL-free UTF-8");
    return std::string(source.data(), source.size());
  }
  core::AsyncLogService &producer_;
  runtime::RuntimeConsole &console_;
  std::string source_;
  std::uint64_t cursor_{}, rejected_{}, forwarded_{};
};
} // namespace nexora::editor::preview
