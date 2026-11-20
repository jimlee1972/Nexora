#pragma once

#include "Nexora/Editor/GameBuildProgress.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/EditorSdk.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nexora::editor::preview {
// Serialized native owner. The compiler already filters project/Play/source progress before
// this projection. Only owning diagnostics cross this boundary; the Console outlives it.
class ScriptBuildConsoleIngress final {
public:
  struct Context final {
    std::uint64_t project{}, play{}, source{};
    bool operator==(const Context &) const = default;
  };
  static constexpr auto kMaximumDiagnostics = GameBuildProgress::kMaximumDiagnostics;
  static constexpr auto kMaximumRemembered = 2 * kMaximumDiagnostics;

  explicit ScriptBuildConsoleIngress(runtime::RuntimeConsole &console) noexcept
      : console_(console) {}
  ScriptBuildConsoleIngress(const ScriptBuildConsoleIngress &) = delete;
  ScriptBuildConsoleIngress &operator=(const ScriptBuildConsoleIngress &) = delete;

  bool Poll(const GameBuildProgress &progress, Context observed, Context current) {
    if (!current.project || !current.play || !current.source || observed != current)
      return true;
    if (progress.stage > GameBuildStage::Cancelled ||
        progress.diagnostics.size() > kMaximumDiagnostics ||
        std::ranges::any_of(progress.diagnostics,
                            [](const auto &value) {
                              return value.severity > GameBuildDiagnosticSeverity::Error ||
                                     !Text(value.file) || !Text(value.message);
                            }) ||
        (context_.project == observed.project && context_.play == observed.play &&
         observed.source < context_.source))
      return false;

    // Prepare the next retained snapshot before publication. A failed publication may retain
    // old identities plus this snapshot's committed prefix (at most 64); retrying the same
    // observation cannot replay its prefix or lose old matches due to rolling-set eviction.
    std::vector<GameBuildDiagnostic> next;
    next.reserve(progress.diagnostics.size());
    for (const auto &value : progress.diagnostics)
      if (std::ranges::find(next, value) == next.end())
        next.push_back(Own(value));
    seen_.reserve(kMaximumRemembered);
    if (context_ != observed) {
      context_ = observed;
      seen_.clear();
      pending_suppressed_ = 0;
      pending_diagnostics_ = pending_output_ = false;
      reported_diagnostics_ = reported_output_ = false;
    }
    pending_diagnostics_ |= progress.diagnostics_truncated && !reported_diagnostics_;
    pending_output_ |= progress.output_truncated && !reported_output_;
    for (const auto &value : next) {
      if (std::ranges::find(seen_, value) != seen_.end())
        continue;
      auto remembered = Own(value);
      auto source = Source();
      if (!value.file.empty()) {
        source += " | ";
        source += value.file;
        if (value.line) {
          source += ':' + std::to_string(value.line);
          if (value.column)
            source += ':' + std::to_string(value.column);
        }
      }
      if (source.size() > runtime::RuntimeConsole::kMaxSourceBytes) {
        console_.ReportDropped(1);
        Add(suppressed_, 1);
        Add(pending_suppressed_, 1);
      } else {
        constexpr std::array levels{runtime::RuntimeLogSeverity::Info,
                                    runtime::RuntimeLogSeverity::Warning,
                                    runtime::RuntimeLogSeverity::Error};
        if (console_.Push({0, levels[static_cast<std::size_t>(value.severity)], "ScriptDiagnostic",
                           Timestamp(), std::move(source),
                           std::string(value.message.data(), value.message.size())}))
          Add(forwarded_, 1);
      }
      Remember(std::move(remembered));
    }
    // Successful calls retain exactly the current unique snapshot (at most 32 identities).
    seen_.swap(next);
    FlushStatus();
    return true;
  }

  [[nodiscard]] std::uint64_t ForwardedRecords() const noexcept { return forwarded_; }
  [[nodiscard]] std::uint64_t SuppressedDiagnostics() const noexcept { return suppressed_; }
  [[nodiscard]] std::size_t RememberedDiagnostics() const noexcept { return seen_.size(); }

private:
  static_assert(std::is_nothrow_move_constructible_v<GameBuildDiagnostic> &&
                std::is_nothrow_move_assignable_v<GameBuildDiagnostic>);
  static bool Text(std::string_view value) noexcept {
    return value.size() <= GameBuildProgress::kMaximumDiagnosticBytes &&
           value.find('\0') == value.npos && foundation::IsValidUtf8(value);
  }
  static GameBuildDiagnostic Own(const GameBuildDiagnostic &value) {
    // Do not retain producer reserve capacity.
    return {value.severity, std::string(value.file.data(), value.file.size()),
            std::string(value.message.data(), value.message.size()), value.line, value.column};
  }
  static void Add(std::uint64_t &value, std::uint64_t count) noexcept {
    value += std::min(count, UINT64_MAX - value);
  }
  static std::uint64_t Timestamp() {
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    return value > 0 ? static_cast<std::uint64_t>(value) : 0;
  }
  std::string Source() const {
    return "ScriptBuild project=" + std::to_string(context_.project) +
           " play=" + std::to_string(context_.play) + " source=" + std::to_string(context_.source);
  }
  void Remember(GameBuildDiagnostic value) noexcept {
    // All potentially throwing ownership work precedes Console publication; capacity was
    // reserved before it. Moving the committed identity must not allocate or throw.
    if (seen_.size() == kMaximumRemembered)
      seen_.erase(seen_.begin());
    seen_.push_back(std::move(value));
  }
  void FlushStatus() {
    if (!pending_suppressed_ && !pending_diagnostics_ && !pending_output_)
      return;
    std::string message = "Script build Console projection:";
    if (pending_suppressed_)
      message += " suppressed " + std::to_string(pending_suppressed_) + " diagnostic records.";
    if (pending_diagnostics_)
      message += " Upstream diagnostics truncated; omitted record count unknown.";
    if (pending_output_)
      message += " Upstream output truncated; omitted byte count unknown.";
    static_cast<void>(console_.Push({0, runtime::RuntimeLogSeverity::Warning, "ScriptBuildStatus",
                                     Timestamp(), Source(), std::move(message)}));
    pending_suppressed_ = 0;
    reported_diagnostics_ |= pending_diagnostics_;
    reported_output_ |= pending_output_;
    pending_diagnostics_ = pending_output_ = false;
  }
  runtime::RuntimeConsole &console_;
  Context context_;
  std::vector<GameBuildDiagnostic> seen_;
  std::uint64_t forwarded_{}, suppressed_{}, pending_suppressed_{};
  bool pending_diagnostics_{}, pending_output_{}, reported_diagnostics_{}, reported_output_{};
};
} // namespace nexora::editor::preview
