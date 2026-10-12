#pragma once

#include "Nexora/Editor/BuildProcess.h"
#include "Nexora/Runtime/EditorSdk.h"
#include <algorithm>
#include <chrono>
#include <limits>

namespace nexora::editor::preview {
// Serialized native-owner adapter. The Console outlives it. No process, UI or document borrow
// escapes; records own escaped bytes. Poll consumes at most 16 KiB raw bytes plus one loss record.
class BuildConsoleIngress final {
public:
  explicit BuildConsoleIngress(runtime::RuntimeConsole &console) noexcept : console_(console) {}
  bool Poll(const BuildProcessSnapshot &snapshot, std::uint64_t current_scope) {
    if (!current_scope || snapshot.scope != current_scope || !snapshot.operation ||
        snapshot.phase == BuildProcessPhase::Stale)
      return true;
    if (snapshot.output.size() > BuildProcess::kMaximumOutputBytes ||
        snapshot.dropped_output_bytes >
            std::numeric_limits<std::uint64_t>::max() - snapshot.output.size())
      return false;
    if (static_cast<unsigned>(snapshot.phase) >
            static_cast<unsigned>(BuildProcessPhase::Unsupported) ||
        (scope_ == current_scope && snapshot.operation < operation_))
      return false;
    const auto total = snapshot.dropped_output_bytes + snapshot.output.size();
    const bool changed = scope_ != current_scope || operation_ != snapshot.operation;
    if (!changed && total < consumed_)
      return false;
    if (changed) {
      scope_ = current_scope;
      operation_ = snapshot.operation;
      consumed_ = 0;
    }
    if (snapshot.dropped_output_bytes > consumed_) {
      const auto missed = snapshot.dropped_output_bytes - consumed_;
      Push(runtime::RuntimeLogSeverity::Warning,
           "Build output lost " + std::to_string(missed) + " raw bytes before Console ingestion.");
      SaturatingAdd(missing_bytes_, missed);
      consumed_ = snapshot.dropped_output_bytes;
    }
    std::size_t budget = 16 * 1024;
    while (consumed_ < total && budget) {
      const auto begin = static_cast<std::size_t>(consumed_ - snapshot.dropped_output_bytes);
      const auto count = std::min({std::size_t{4096}, budget, snapshot.output.size() - begin});
      Push(runtime::RuntimeLogSeverity::Info,
           Escape(std::string_view(snapshot.output).substr(begin, count)));
      consumed_ += count;
      SaturatingAdd(consumed_bytes_, count);
      budget -= count;
    }
    return true;
  }
  [[nodiscard]] std::uint64_t ForwardedRecords() const noexcept { return forwarded_records_; }
  [[nodiscard]] std::uint64_t ConsumedRawBytes() const noexcept { return consumed_bytes_; }
  [[nodiscard]] std::uint64_t MissingRawBytes() const noexcept { return missing_bytes_; }

private:
  static void SaturatingAdd(std::uint64_t &value, std::uint64_t count) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
  }
  static std::string Escape(std::string_view bytes) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 4);
    for (const unsigned char byte : bytes) {
      if (byte >= 32 && byte < 127 && byte != '\\' && byte != '"')
        result.push_back(static_cast<char>(byte));
      else {
        result += "\\x";
        result.push_back(hex[byte >> 4]);
        result.push_back(hex[byte & 15]);
      }
    }
    return result;
  }
  void Push(runtime::RuntimeLogSeverity severity, std::string message) {
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    if (console_.Push({0, severity, "BuildOutput",
                       timestamp > 0 ? static_cast<std::uint64_t>(timestamp) : 0,
                       "BuildProcess scope=" + std::to_string(scope_) +
                           " operation=" + std::to_string(operation_),
                       std::move(message)}))
      SaturatingAdd(forwarded_records_, 1);
  }
  runtime::RuntimeConsole &console_;
  std::uint64_t scope_{}, operation_{}, consumed_{};
  std::uint64_t forwarded_records_{}, consumed_bytes_{}, missing_bytes_{};
};
} // namespace nexora::editor::preview
