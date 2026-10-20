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
  // External elapsed time outside observed synchronous children; never CPU execution cost.
  // Async and ambiguous overlapping/coincident contexts remain unavailable.
  std::optional<double> self_duration_milliseconds{};
};
// Owning external Chrome complete, synchronous, nestable and legacy async intervals. They are
// neither frame timings nor GPU/RSS observations. Timestamps retain the external trace clock, not
// the host clock.
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
  // order. Nestable async b/e supports id2.local and global id/id2.global, category/scope pairing
  // and cross-thread/process global ends. Selection uses the begin process/thread/name.
  // Legacy S/F uses the same bounded identity lanes in a separate non-nestable domain,
  // keyed additionally by name; duplicate live named identities reject. Local/global and
  // numeric/string identities stay separate. T/p steps remain metadata, not intervals.
  // Self elapsed time uses all selected-thread X/B/E context, including nonmatching names.
  // Known B/E begin order resolves paired coincident bounds; crossing or other coincident
  // components remain unavailable without changing inclusive intervals or input acceptance.
  // Reconstruction is iterative, with at most one context record/stack slot per input event.
  // Unknown JSON metadata is validated and skipped within bounds. No IO or live capture
  // mutation.
  [[nodiscard]] static std::optional<ChromeTraceCapture>
  Import(std::string_view bytes, ChromeTraceSelection, std::string *error = nullptr);
  [[nodiscard]] static bool Validate(const ChromeTraceCapture &) noexcept;
};
} // namespace nexora::editor
