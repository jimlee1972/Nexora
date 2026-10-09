#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nexora::editor::detail {
// Exact bounded ASCII schema tokens. No recursion, DOM or unknown-value skipping.
// Callers bound file bytes, object keys and sample counts independently.
class ProfileJsonReader {
protected:
  explicit ProfileJsonReader(std::string_view bytes) : remaining_(bytes) {}
  void Space() {
    const auto first = remaining_.find_first_not_of(" \r\n\t");
    remaining_.remove_prefix(first == std::string_view::npos ? remaining_.size() : first);
  }
  bool Literal(std::string_view token) {
    Space();
    if (!remaining_.starts_with(token))
      return false;
    remaining_.remove_prefix(token.size());
    return true;
  }
  std::optional<std::string> String() {
    if (!Literal("\""))
      return std::nullopt;
    std::string text;
    while (!remaining_.empty()) {
      auto value = remaining_.front();
      remaining_.remove_prefix(1);
      if (value == '"')
        return text;
      if (value == '\\') {
        if (remaining_.empty())
          return std::nullopt;
        value = remaining_.front();
        remaining_.remove_prefix(1);
        // All schema strings are ASCII; accept their equivalent JSON escapes.
        if (value == 'u') {
          if (remaining_.size() < 4)
            return std::nullopt;
          unsigned scalar = 0;
          const auto hex = remaining_.substr(0, 4);
          const auto result = std::from_chars(hex.data(), hex.data() + 4, scalar, 16);
          if (result.ec != std::errc{} || result.ptr != hex.data() + 4 || scalar > 127 ||
              scalar < 32)
            return std::nullopt;
          remaining_.remove_prefix(4);
          value = static_cast<char>(scalar);
        } else if (value != '"' && value != '\\' && value != '/') {
          return std::nullopt;
        }
      }
      if (static_cast<unsigned char>(value) < 32 || static_cast<unsigned char>(value) > 127 ||
          text.size() == 128)
        return std::nullopt;
      text.push_back(value);
    }
    return std::nullopt;
  }
  bool Text(std::string_view expected) {
    const auto text = String();
    return text && *text == expected;
  }
  static bool Integer(std::string_view text, std::uint64_t &value) {
    if (text.empty() || (text.size() > 1 && text.front() == '0'))
      return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
  }
  bool StringInteger(std::uint64_t &value) {
    const auto text = String();
    return text && Integer(*text, value);
  }
  std::optional<std::string_view> Number() {
    Space();
    const auto bytes = remaining_;
    const auto digit = [&] {
      return !remaining_.empty() && remaining_.front() >= '0' && remaining_.front() <= '9';
    };
    if (remaining_.starts_with('-'))
      remaining_.remove_prefix(1);
    if (!digit())
      return std::nullopt;
    if (remaining_.front() == '0')
      remaining_.remove_prefix(1);
    else
      while (digit())
        remaining_.remove_prefix(1);
    if (remaining_.starts_with('.')) {
      remaining_.remove_prefix(1);
      if (!digit())
        return std::nullopt;
      while (digit())
        remaining_.remove_prefix(1);
    }
    if (remaining_.starts_with('e') || remaining_.starts_with('E')) {
      remaining_.remove_prefix(1);
      if (remaining_.starts_with('+') || remaining_.starts_with('-'))
        remaining_.remove_prefix(1);
      if (!digit())
        return std::nullopt;
      while (digit())
        remaining_.remove_prefix(1);
    }
    return bytes.substr(0, bytes.size() - remaining_.size());
  }
  template <std::size_t N, typename ReadField>
  bool Object(const std::array<std::string_view, N> &keys, ReadField read_field) {
    static_assert(N <= 32);
    if (!Literal("{"))
      return false;
    unsigned seen = 0;
    for (std::size_t entry = 0; entry < N; ++entry) {
      if (entry && !Literal(","))
        return false;
      const auto key = String();
      if (!key || !Literal(":"))
        return false;
      std::size_t field = 0;
      while (field < N && keys[field] != *key)
        ++field;
      if (field == N || (seen & (1U << field)) || !read_field(field))
        return false;
      seen |= 1U << field;
    }
    return Literal("}");
  }
  bool End() {
    Space();
    return remaining_.empty();
  }
  std::string_view remaining_;
};
} // namespace nexora::editor::detail
