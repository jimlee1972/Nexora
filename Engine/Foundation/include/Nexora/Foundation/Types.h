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

  // Accepts canonical 8-4-4-4-12 hex with dashes, or the same 32 hex digits
  // without them; anything else (including an embedded NUL, which just fails
  // as a non-hex character) is ErrorCode::InvalidArgument.
  [[nodiscard]] static Result<Uuid> Parse(StringView text) {
    const auto hex_value = [](char c) -> int {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      return -1;
    };
    std::uint8_t bytes[16]{};
    std::size_t byte_index = 0;
    int high_nibble = -1;
    for (const char c : text) {
      if (byte_index >= 16)
        return ErrorCode::InvalidArgument;
      if (c == '-')
        continue;
      const int value = hex_value(c);
      if (value < 0)
        return ErrorCode::InvalidArgument;
      if (high_nibble < 0) {
        high_nibble = value;
      } else {
        bytes[byte_index++] = static_cast<std::uint8_t>((high_nibble << 4) | value);
        high_nibble = -1;
      }
    }
    if (byte_index != 16 || high_nibble >= 0)
      return ErrorCode::InvalidArgument;
    Uuid uuid;
    for (int k = 0; k < 8; ++k)
      uuid.high = (uuid.high << 8) | bytes[k];
    for (int k = 8; k < 16; ++k)
      uuid.low = (uuid.low << 8) | bytes[k];
    return uuid;
  }

  [[nodiscard]] String ToString() const {
    static constexpr char kHex[] = "0123456789abcdef";
    std::uint8_t bytes[16];
    for (int k = 0; k < 8; ++k)
      bytes[k] = static_cast<std::uint8_t>(high >> (8 * (7 - k)));
    for (int k = 0; k < 8; ++k)
      bytes[8 + k] = static_cast<std::uint8_t>(low >> (8 * (7 - k)));
    String out;
    out.reserve(36);
    for (int k = 0; k < 16; ++k) {
      out.push_back(kHex[bytes[k] >> 4]);
      out.push_back(kHex[bytes[k] & 0x0F]);
      if (k == 3 || k == 5 || k == 7 || k == 9)
        out.push_back('-');
    }
    return out;
  }
};
// The API-M2 generational-handle deliverable is `nexora::core::Handle<Tag>` /
// `HandlePool<Tag>` in Nexora/Core/Handle.h (index + generation, generation 0
// invalid), already used by EventBus, Timer, and the RHI backends. It is not
// duplicated here: Core depends on Foundation, not the other way around, and
// moving it down would mean rewriting every existing consumer's namespace for
// no behavioral change, which is a larger refactor than this pass calls for.
// Name intentionally stores only a 64-bit FNV-1a hash, not the original text:
// it is meant to be a cheap, ABI-stable StringId, not a debugging aid. Two
// distinct strings that hash to the same value are indistinguishable and will
// compare equal; this is an accepted, extremely low-probability trade-off of
// the hash-only design (see the API-M2 hash collision diagnostics test),
// not a bug to be fixed by widening Name to also carry the source string.
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
