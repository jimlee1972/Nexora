#pragma once

#include "Nexora/Editor/BuildProcess.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/EditorSdk.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nexora::editor::preview {
// Serialized native-owner projection of merged process text, not a compiler/artifact authority.
// The existing raw ingress remains independent. The Console outlives this adapter.
class BuildDiagnosticIngress final {
public:
  static constexpr std::size_t kRawBudget = 16 * 1024;
  static constexpr std::size_t kMaximumLineBytes = 4096;
  static constexpr std::size_t kMaximumRecordsPerPoll = 64;

  explicit BuildDiagnosticIngress(runtime::RuntimeConsole &console) noexcept : console_(console) {}
  BuildDiagnosticIngress(const BuildDiagnosticIngress &) = delete;
  BuildDiagnosticIngress &operator=(const BuildDiagnosticIngress &) = delete;

  bool Poll(const BuildProcessSnapshot &snapshot, std::uint64_t current_scope) {
    if (!current_scope || snapshot.scope != current_scope || !snapshot.operation ||
        snapshot.phase == BuildProcessPhase::Stale)
      return true;
    if (snapshot.output.size() > BuildProcess::kMaximumOutputBytes ||
        snapshot.dropped_output_bytes > UINT64_MAX - snapshot.output.size() ||
        snapshot.phase > BuildProcessPhase::Unsupported ||
        (scope_ == current_scope && snapshot.operation < operation_))
      return false;
    const auto total = snapshot.dropped_output_bytes + snapshot.output.size();
    const bool changed = scope_ != current_scope || operation_ != snapshot.operation;
    if (!changed && (total < observed_total_ || snapshot.dropped_output_bytes < observed_drop_ ||
                     (terminal_total_ && total != *terminal_total_)))
      return false;
    if (changed) {
      scope_ = current_scope;
      operation_ = snapshot.operation;
      consumed_ = observed_total_ = observed_drop_ = 0;
      pending_suppressed_ = pending_long_lines_ = 0;
      line_.clear();
      discard_prefix_ = false;
      terminal_total_.reset();
    }
    observed_total_ = total;
    observed_drop_ = snapshot.dropped_output_bytes;
    const bool terminal = snapshot.phase == BuildProcessPhase::Exited ||
                          snapshot.phase == BuildProcessPhase::Failed ||
                          snapshot.phase == BuildProcessPhase::Cancelled ||
                          snapshot.phase == BuildProcessPhase::Unsupported;
    if (terminal)
      terminal_total_ = total;
    if (snapshot.dropped_output_bytes > consumed_) {
      Add(missing_bytes_, snapshot.dropped_output_bytes - consumed_);
      consumed_ = snapshot.dropped_output_bytes;
      line_.clear();
      // The retained first byte may be in the middle of a diagnostic or UTF-8 scalar.
      discard_prefix_ = true;
    }
    std::size_t budget = kRawBudget, attempts = 0;
    while (consumed_ < total && budget) {
      const char byte =
          snapshot.output[static_cast<std::size_t>(consumed_ - snapshot.dropped_output_bytes)];
      if (byte == '\n') {
        if (!discard_prefix_)
          FinishLine(attempts);
        line_.clear();
        discard_prefix_ = false;
      } else if (!discard_prefix_) {
        if (line_.size() == kMaximumLineBytes) {
          line_.clear();
          discard_prefix_ = true;
          Add(long_lines_, 1);
          Add(pending_long_lines_, 1);
        } else {
          // Allocation failure keeps this byte and the current line retryable.
          line_.push_back(byte);
        }
      }
      ++consumed_;
      Add(consumed_bytes_, 1);
      --budget;
    }
    if (terminal && consumed_ == total && !discard_prefix_ && !line_.empty()) {
      FinishLine(attempts);
      line_.clear();
    }
    FlushStatus();
    return true;
  }

  [[nodiscard]] std::uint64_t ForwardedRecords() const noexcept { return forwarded_; }
  [[nodiscard]] std::uint64_t ConsumedRawBytes() const noexcept { return consumed_bytes_; }
  [[nodiscard]] std::uint64_t MissingRawBytes() const noexcept { return missing_bytes_; }
  [[nodiscard]] std::uint64_t SuppressedDiagnostics() const noexcept { return suppressed_; }
  [[nodiscard]] std::uint64_t OversizedLines() const noexcept { return long_lines_; }
  [[nodiscard]] std::size_t PendingLineBytes() const noexcept { return line_.size(); }

private:
  struct Diagnostic final {
    runtime::RuntimeLogSeverity severity;
    std::string_view location, message;
  };
  static void Add(std::uint64_t &value, std::uint64_t count) noexcept {
    value += std::min(count, UINT64_MAX - value);
  }
  static bool Coordinate(std::string_view text) noexcept {
    std::uint32_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
           value != 0;
  }
  static std::optional<Diagnostic> Parse(std::string_view text) {
    if (text.ends_with('\r'))
      text.remove_suffix(1);
    if (!foundation::IsValidUtf8(text) ||
        std::ranges::any_of(text, [](unsigned char byte) { return byte < 32 || byte == 127; }))
      return std::nullopt;
    using Severity = runtime::RuntimeLogSeverity;
    constexpr std::array markers{std::pair{std::string_view{": error:"}, Severity::Error},
                                 std::pair{std::string_view{": warning:"}, Severity::Warning},
                                 std::pair{std::string_view{": note:"}, Severity::Info}};
    std::size_t position = std::string_view::npos, marker_size = 0;
    Severity severity{};
    for (const auto &[marker, level] : markers) {
      if (const auto found = text.find(marker); found < position) {
        position = found;
        marker_size = marker.size();
        severity = level;
      }
    }
    if (position == std::string_view::npos)
      return std::nullopt;
    const auto location = text.substr(0, position);
    const auto column = location.rfind(':');
    if (column == std::string_view::npos || column == 0)
      return std::nullopt;
    const auto line = location.rfind(':', column - 1);
    if (line == std::string_view::npos || line == 0 ||
        !Coordinate(location.substr(line + 1, column - line - 1)) ||
        !Coordinate(location.substr(column + 1)))
      return std::nullopt;
    auto message = text.substr(position + marker_size);
    if (message.starts_with(' '))
      message.remove_prefix(1);
    return Diagnostic{severity, location, message};
  }
  std::string Source() const {
    return "BuildProcess scope=" + std::to_string(scope_) +
           " operation=" + std::to_string(operation_);
  }
  static std::uint64_t Timestamp() {
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    return value > 0 ? static_cast<std::uint64_t>(value) : 0;
  }
  void Suppress() {
    // These are known derived record units, never inferred from missing raw bytes.
    console_.ReportDropped(1);
    Add(suppressed_, 1);
    Add(pending_suppressed_, 1);
  }
  void FinishLine(std::size_t &attempts) {
    const auto diagnostic = Parse(line_);
    if (!diagnostic)
      return;
    if (attempts == kMaximumRecordsPerPoll) {
      Suppress();
      return;
    }
    auto source = Source();
    if (source.size() + 3 + diagnostic->location.size() >
        runtime::RuntimeConsole::kMaxSourceBytes) {
      Suppress();
      return;
    }
    source += " | ";
    source.append(diagnostic->location);
    if (console_.Push({0, diagnostic->severity, "BuildDiagnostic", Timestamp(), std::move(source),
                       std::string(diagnostic->message)}))
      Add(forwarded_, 1);
    ++attempts;
  }
  void FlushStatus() {
    if (!pending_suppressed_ && !pending_long_lines_)
      return;
    auto message = "Compiler diagnostic projection omitted " + std::to_string(pending_suppressed_) +
                   " parsed records and " + std::to_string(pending_long_lines_) +
                   " oversized lines; raw BuildOutput routing is unchanged.";
    static_cast<void>(
        console_.Push({0, runtime::RuntimeLogSeverity::Warning, "BuildDiagnosticStatus",
                       Timestamp(), Source(), std::move(message)}));
    pending_suppressed_ = pending_long_lines_ = 0;
  }
  runtime::RuntimeConsole &console_;
  std::uint64_t scope_{}, operation_{}, consumed_{}, observed_total_{}, observed_drop_{};
  std::optional<std::uint64_t> terminal_total_;
  std::string line_;
  bool discard_prefix_{};
  std::uint64_t forwarded_{}, consumed_bytes_{}, missing_bytes_{}, suppressed_{}, long_lines_{};
  std::uint64_t pending_suppressed_{}, pending_long_lines_{};
};
} // namespace nexora::editor::preview
