#pragma once
#include "Nexora/Editor/Api.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor {
struct ChromeTraceSelection final {
  std::uint64_t process{}, thread{};
  std::string event_name;
};
struct ExternalTraceInterval final {
  std::uint64_t sequence{};
  double start_microseconds{}, duration_milliseconds{};
};
// Owning external Chrome complete, synchronous and nestable async intervals. They are neither frame
// timings nor GPU/RSS observations. Timestamps retain the external trace clock, not the host clock.
struct ChromeTraceCapture final {
  ChromeTraceSelection selection;
  std::vector<ExternalTraceInterval> samples;
  std::uint64_t older_samples_dropped{};
};
class NEXORA_EDITOR_API ChromeTraceImporter final {
public:
  static constexpr std::size_t kMaximumBytes = 4 * 1024 * 1024;
  static constexpr std::size_t kMaximumEvents = 32768;
  static constexpr std::size_t kMaximumSamples = 600;
  static constexpr std::size_t kMaximumDepth = 32;
  static constexpr std::size_t kMaximumStringBytes = 65536;
  static constexpr std::size_t kMaximumOpenScopes = 128;
  // Serialized read-only planning. Wrapped traceEvents objects and bare event arrays are
  // supported; X and synchronous B/E events use Chrome microsecond timestamps/durations.
  // Selected-thread B/E scopes must be balanced, ordered and LIFO; newest retention uses begin
  // order. Nestable async b/e uses process-local id2.local/category/scope with cross-thread ends;
  // selection uses the begin thread/name. global id/id2.global and legacy S/F are unsupported.
  // Unknown JSON metadata is validated and skipped within bounds. No IO or live capture
  // mutation.
  [[nodiscard]] static std::optional<ChromeTraceCapture>
  Import(std::string_view bytes, ChromeTraceSelection, std::string *error = nullptr);
  [[nodiscard]] static bool Validate(const ChromeTraceCapture &) noexcept;
};
} // namespace nexora::editor
