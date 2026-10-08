#pragma once

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace nexora::core::detail {
// Linux statm begins with virtual and resident page counts. Bound the complete OS response
// before calling this parser; consume whole decimal tokens and reject byte-count overflow.
inline std::optional<std::uint64_t> ResidentBytesFromStatm(std::string_view text,
                                                           std::uint64_t page_bytes) noexcept {
  if (page_bytes == 0 || text.find('\0') != std::string_view::npos)
    return std::nullopt;
  std::uint64_t resident_pages = 0;
  for (int field = 0; field < 2; ++field) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
      return std::nullopt;
    text.remove_prefix(first);
    const auto end = text.find_first_of(" \t\r\n");
    const auto token = text.substr(0, end);
    std::uint64_t pages = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), pages);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
      return std::nullopt;
    if (field == 1)
      resident_pages = pages;
    text.remove_prefix(token.size());
  }
  if (resident_pages > std::numeric_limits<std::uint64_t>::max() / page_bytes)
    return std::nullopt;
  return resident_pages * page_bytes;
}
} // namespace nexora::core::detail
