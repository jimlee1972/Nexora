#pragma once

#include "Nexora/Core/Log.h"
#include "Nexora/Foundation/Types.h"
#include <deque>
#include <limits>
#include <string_view>

namespace nexora::core::detail {
inline void CountRejectedLog(std::uint64_t &counter) noexcept {
  if (counter != std::numeric_limits<std::uint64_t>::max())
    ++counter;
}
inline bool ValidLogText(std::string_view text, std::size_t budget) noexcept {
  return text.size() <= budget && text.find('\0') == std::string_view::npos &&
         foundation::IsValidUtf8(text);
}
// Prepare compact strings before mutation; allocation failure leaves ingress and counters intact.
inline bool AdmitLog(LogRecord record, std::size_t capacity, std::deque<LogRecord> &pending,
                     std::uint64_t &next_sequence, std::uint64_t &rejected) {
  if (!next_sequence || pending.size() >= capacity || record.level > LogLevel::Fatal ||
      !ValidLogText(record.category, AsyncLogService::kMaximumCategoryBytes) ||
      !ValidLogText(record.message, AsyncLogService::kMaximumMessageBytes)) {
    CountRejectedLog(rejected);
    return false;
  }
  LogRecord owned{record.timestamp,
                  record.level,
                  std::string(record.category.data(), record.category.size()),
                  std::string(record.message.data(), record.message.size()),
                  record.thread_id,
                  next_sequence};
  pending.push_back(std::move(owned));
  next_sequence = next_sequence == UINT64_MAX ? 0 : next_sequence + 1;
  return true;
}
} // namespace nexora::core::detail
