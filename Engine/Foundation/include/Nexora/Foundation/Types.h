#pragma once
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace nexora::foundation {
using StringView = std::string_view;
using String = std::string;
using ByteBuffer = std::vector<std::byte>;
template <class T> using Span = std::span<T>;
enum class ErrorCode : std::uint32_t {
  None,
  InvalidArgument,
  NotFound,
  IoError,
  Cancelled,
  Conflict
};
template <class T> class Result final {
public:
  Result(T value) : value_(std::move(value)) {}
  Result(ErrorCode e) : error_(e) {}
  [[nodiscard]] bool HasValue() const { return error_ == ErrorCode::None; }
  explicit operator bool() const { return HasValue(); }
  [[nodiscard]] const T &Value() const { return value_; }
  [[nodiscard]] T &Value() { return value_; }
  [[nodiscard]] ErrorCode Error() const { return error_; }

private:
  T value_{};
  ErrorCode error_{ErrorCode::None};
};
struct Uuid final {
  std::uint64_t high{}, low{};
  friend constexpr bool operator==(Uuid, Uuid) = default;
  [[nodiscard]] constexpr bool IsNil() const { return high == 0 && low == 0; }
};
class Name final {
public:
  constexpr Name() = default;
  explicit constexpr Name(std::string_view text) : value_(Hash(text)) {}
  [[nodiscard]] constexpr std::uint64_t Value() const { return value_; }
  friend constexpr bool operator==(Name, Name) = default;

private:
  static constexpr std::uint64_t Hash(std::string_view s) {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
      h ^= c;
      h *= 1099511628211ULL;
    }
    return h;
  }
  std::uint64_t value_{};
};
[[nodiscard]] inline bool IsValidUtf8(StringView s) {
  std::size_t i = 0;
  while (i < s.size()) {
    const auto c = static_cast<unsigned char>(s[i]);
    std::size_t n = c < 0x80
                        ? 1
                        : (c >= 0xC2 && c <= 0xDF
                               ? 2
                               : (c >= 0xE0 && c <= 0xEF ? 3 : (c >= 0xF0 && c <= 0xF4 ? 4 : 0)));
    if (!n || i + n > s.size())
      return false;
    for (std::size_t j = 1; j < n; ++j)
      if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80)
        return false;
    if (n == 3) {
      auto d = static_cast<unsigned char>(s[i + 1]);
      if ((c == 0xE0 && d < 0xA0) || (c == 0xED && d >= 0xA0))
        return false;
    }
    if (n == 4) {
      auto d = static_cast<unsigned char>(s[i + 1]);
      if ((c == 0xF0 && d < 0x90) || (c == 0xF4 && d >= 0x90))
        return false;
    }
    i += n;
  }
  return true;
}
template <class T> [[nodiscard]] Result<T> ParseNumber(StringView text) {
  T result{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
  if (error != std::errc{} || end != text.data() + text.size())
    return ErrorCode::InvalidArgument;
  return result;
}
} // namespace nexora::foundation
