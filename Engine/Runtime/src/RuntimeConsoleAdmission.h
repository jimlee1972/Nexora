#pragma once

#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <limits>
#include <utility>

namespace nexora::runtime::detail {

// Internal admission transaction; the owning RuntimeConsole holds its mutex for the entire call.
// Keeping counters explicit permits deterministic exhaustion tests without a public state-reset
// API.
inline void CountConsoleDrop(std::uint64_t &dropped) noexcept {
  if (dropped != std::numeric_limits<std::uint64_t>::max())
    ++dropped;
}

inline bool ValidConsoleText(std::string_view text, std::size_t budget) noexcept {
  return text.size() <= budget && text.find('\0') == std::string_view::npos &&
         foundation::IsValidUtf8(text);
}

inline bool PushConsoleRecord(RuntimeLogRecord record, std::size_t capacity,
                              std::vector<RuntimeLogRecord> &records, std::uint64_t &next_sequence,
                              std::uint64_t &dropped) {
  const bool valid_severity =
      record.severity == RuntimeLogSeverity::Trace || record.severity == RuntimeLogSeverity::Info ||
      record.severity == RuntimeLogSeverity::Warning ||
      record.severity == RuntimeLogSeverity::Error || record.severity == RuntimeLogSeverity::Fatal;
  if (capacity == 0 || next_sequence == 0 || !valid_severity ||
      !ValidConsoleText(record.category, RuntimeConsole::kMaxCategoryBytes) ||
      !ValidConsoleText(record.source, RuntimeConsole::kMaxSourceBytes) ||
      !ValidConsoleText(record.message, RuntimeConsole::kMaxMessageBytes)) {
    CountConsoleDrop(dropped);
    return false;
  }

  // Rebuild strings instead of retaining producer-provided oversized reserve allocations. Failure
  // to allocate either the owning strings or vector leaves history and both counters unchanged.
  RuntimeLogRecord owned{next_sequence,
                         record.severity,
                         std::string(record.category.data(), record.category.size()),
                         record.timestamp_nanoseconds,
                         std::string(record.source.data(), record.source.size()),
                         std::string(record.message.data(), record.message.size())};
  records.push_back(std::move(owned));
  next_sequence =
      next_sequence == std::numeric_limits<std::uint64_t>::max() ? 0 : next_sequence + 1;
  if (records.size() > capacity) {
    records.erase(records.begin());
    CountConsoleDrop(dropped);
  }
  return true;
}

} // namespace nexora::runtime::detail
