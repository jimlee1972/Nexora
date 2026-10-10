#include "Nexora/Editor/ChromeTrace.h"
#include "Nexora/Foundation/Types.h"
#include "ProfileJson.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace nexora::editor {
namespace {
constexpr std::uint64_t maximum_exact_integer = 9007199254740991ULL;
bool ValidSelection(const ChromeTraceSelection &selection) {
  return selection.process <= maximum_exact_integer && selection.thread <= maximum_exact_integer &&
         !selection.event_name.empty() && selection.event_name.size() <= 256 &&
         foundation::IsValidUtf8(selection.event_name) &&
         !std::ranges::any_of(selection.event_name,
                              [](unsigned char c) { return c < 32 || c == 127; });
}

class Reader final : private detail::ProfileJsonReader {
public:
  Reader(std::string_view bytes, ChromeTraceSelection selection)
      : ProfileJsonReader(bytes), capture_{std::move(selection), {}, 0} {}
  std::optional<ChromeTraceCapture> Read() {
    Space();
    bool events = false;
    if (remaining_.starts_with('[')) {
      events = Events();
    } else if (!Members([&](const std::string &key) {
                 if (key == "traceEvents") {
                   events = Events();
                   return events;
                 }
                 return Skip(1);
               })) {
      return {};
    }
    if (!events || !End() || capture_.samples.empty())
      return {};
    if (capture_.samples.size() == ChromeTraceImporter::kMaximumSamples && next_)
      std::rotate(capture_.samples.begin(), capture_.samples.begin() + next_,
                  capture_.samples.end());
    return std::move(capture_);
  }

private:
  std::optional<unsigned> Hex() {
    if (remaining_.size() < 4)
      return {};
    unsigned scalar{};
    const auto token = remaining_.substr(0, 4);
    if (!std::ranges::all_of(token, [](unsigned char c) {
          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }))
      return {};
    const auto result = std::from_chars(token.data(), token.data() + 4, scalar, 16);
    if (result.ec != std::errc{} || result.ptr != token.data() + 4)
      return {};
    remaining_.remove_prefix(4);
    return scalar;
  }
  static void Utf8(std::string &text, unsigned scalar) {
    if (scalar < 128) {
      text.push_back(static_cast<char>(scalar));
    } else if (scalar < 2048) {
      text.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
      text.push_back(static_cast<char>(0x80 | (scalar & 63)));
    } else if (scalar < 65536) {
      text.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
      text.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 63)));
      text.push_back(static_cast<char>(0x80 | (scalar & 63)));
    } else {
      text.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
      text.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 63)));
      text.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 63)));
      text.push_back(static_cast<char>(0x80 | (scalar & 63)));
    }
  }
  std::optional<std::string> ReadString() {
    if (!Literal("\""))
      return {};
    std::string text;
    while (!remaining_.empty()) {
      auto c = static_cast<unsigned char>(remaining_.front());
      remaining_.remove_prefix(1);
      if (c == '"')
        return foundation::IsValidUtf8(text) ? std::optional{std::move(text)} : std::nullopt;
      if (c < 32)
        return {};
      if (c == '\\') {
        if (remaining_.empty())
          return {};
        c = static_cast<unsigned char>(remaining_.front());
        remaining_.remove_prefix(1);
        if (c == 'u') {
          auto scalar = Hex();
          if (!scalar)
            return {};
          if (*scalar >= 0xd800 && *scalar <= 0xdbff) {
            if (!remaining_.starts_with("\\u"))
              return {};
            remaining_.remove_prefix(2);
            const auto low = Hex();
            if (!low || *low < 0xdc00 || *low > 0xdfff)
              return {};
            *scalar = 65536 + ((*scalar - 0xd800) << 10) + (*low - 0xdc00);
          } else if (*scalar >= 0xdc00 && *scalar <= 0xdfff) {
            return {};
          }
          Utf8(text, *scalar);
        } else {
          switch (c) {
          case 'b':
            c = '\b';
            break;
          case 'f':
            c = '\f';
            break;
          case 'n':
            c = '\n';
            break;
          case 'r':
            c = '\r';
            break;
          case 't':
            c = '\t';
            break;
          case '"':
          case '\\':
          case '/':
            break;
          default:
            return {};
          }
          text.push_back(static_cast<char>(c));
        }
      } else {
        text.push_back(static_cast<char>(c));
      }
      if (text.size() > ChromeTraceImporter::kMaximumStringBytes)
        return {};
    }
    return {};
  }
  template <class Field> bool Members(Field field) {
    if (!Literal("{"))
      return false;
    Space();
    if (remaining_.starts_with('}'))
      return Literal("}");
    std::unordered_set<std::string> keys;
    do {
      const auto key = ReadString();
      if (!key || keys.size() == 64 || !keys.insert(*key).second || !Literal(":") || !field(*key))
        return false;
      Space();
      if (remaining_.starts_with('}'))
        return Literal("}");
    } while (Literal(","));
    return false;
  }
  bool Skip(std::size_t depth) {
    if (depth > ChromeTraceImporter::kMaximumDepth)
      return false;
    Space();
    if (remaining_.starts_with('"'))
      return ReadString().has_value();
    if (remaining_.starts_with('{'))
      return Members([&](const std::string &) { return Skip(depth + 1); });
    if (remaining_.starts_with('[')) {
      Literal("[");
      Space();
      if (remaining_.starts_with(']'))
        return Literal("]");
      std::size_t count{};
      do {
        if (++count > ChromeTraceImporter::kMaximumEvents || !Skip(depth + 1))
          return false;
        Space();
        if (remaining_.starts_with(']'))
          return Literal("]");
      } while (Literal(","));
      return false;
    }
    return Literal("true") || Literal("false") || Literal("null") || Number().has_value();
  }
  bool Identity(std::uint64_t &value) {
    Space();
    if (remaining_.starts_with('"')) {
      const auto token = ReadString();
      return token && Integer(*token, value) && value <= maximum_exact_integer;
    }
    const auto token = Number();
    return token && Integer(*token, value) && value <= maximum_exact_integer;
  }
  bool Time(double &value) {
    const auto token = Number();
    if (!token || token->size() > 128)
      return false;
    const auto result = std::from_chars(token->data(), token->data() + token->size(), value);
    return result.ec == std::errc{} && result.ptr == token->data() + token->size() &&
           std::isfinite(value) && value >= 0 && value <= maximum_exact_integer;
  }
  bool Event() {
    std::string phase, name;
    std::uint64_t process{}, thread{};
    double start{}, duration{};
    unsigned seen{};
    if (!Members([&](const std::string &key) {
          if (key == "ph" || key == "name") {
            auto text = ReadString();
            if (!text)
              return false;
            (key == "ph" ? phase : name) = std::move(*text);
            seen |= key == "ph" ? 1 : 2;
            return true;
          }
          if (key == "pid" || key == "tid") {
            seen |= key == "pid" ? 4 : 8;
            return Identity(key == "pid" ? process : thread);
          }
          if (key == "ts" || key == "dur") {
            seen |= key == "ts" ? 16 : 32;
            return Time(key == "ts" ? start : duration);
          }
          return Skip(2);
        }) ||
        !(seen & 1))
      return false;
    if (phase != "X")
      return true;
    if (seen != 63 || duration > maximum_exact_integer - start)
      return false;
    if (name != capture_.selection.event_name || process != capture_.selection.process ||
        thread != capture_.selection.thread)
      return true;
    if (selected_ && start < previous_start_)
      return false;
    previous_start_ = start;
    ExternalTraceInterval sample{++selected_, start, duration / 1000.0};
    if (capture_.samples.size() < ChromeTraceImporter::kMaximumSamples) {
      capture_.samples.push_back(sample);
    } else {
      capture_.samples[next_] = sample;
      next_ = (next_ + 1) % ChromeTraceImporter::kMaximumSamples;
      ++capture_.older_samples_dropped;
    }
    return true;
  }
  bool Events() {
    if (!Literal("["))
      return false;
    Space();
    if (remaining_.starts_with(']'))
      return Literal("]");
    do {
      if (++events_ > ChromeTraceImporter::kMaximumEvents || !Event())
        return false;
      Space();
      if (remaining_.starts_with(']'))
        return Literal("]");
    } while (Literal(","));
    return false;
  }
  ChromeTraceCapture capture_;
  std::uint64_t selected_{};
  std::size_t events_{}, next_{};
  double previous_start_{};
};
} // namespace
std::optional<ChromeTraceCapture> ChromeTraceImporter::Import(std::string_view bytes,
                                                              ChromeTraceSelection selection,
                                                              std::string *error) {
  if (error)
    error->clear();
  if (bytes.size() > kMaximumBytes || !ValidSelection(selection)) {
    if (error)
      *error = "Chrome trace input or process/thread/event selection exceeds supported bounds.";
    return {};
  }
  auto result = Reader(bytes, std::move(selection)).Read();
  if (!result && error)
    *error =
        "Chrome trace is malformed, exceeds bounds, or has no ordered matching complete events.";
  return result;
}

bool ChromeTraceImporter::Validate(const ChromeTraceCapture &capture) noexcept {
  if (!ValidSelection(capture.selection) || capture.samples.empty() ||
      capture.samples.size() > kMaximumSamples || capture.older_samples_dropped > kMaximumEvents ||
      capture.samples.size() > kMaximumEvents - capture.older_samples_dropped ||
      (capture.older_samples_dropped && capture.samples.size() != kMaximumSamples))
    return false;
  auto sequence = capture.older_samples_dropped;
  double previous{};
  for (const auto &sample : capture.samples) {
    if (sample.sequence != ++sequence || !std::isfinite(sample.start_microseconds) ||
        sample.start_microseconds < previous || sample.start_microseconds > maximum_exact_integer ||
        !std::isfinite(sample.duration_milliseconds) || sample.duration_milliseconds < 0 ||
        sample.duration_milliseconds > (maximum_exact_integer - sample.start_microseconds) / 1000.0)
      return false;
    previous = sample.start_microseconds;
  }
  return true;
}
} // namespace nexora::editor
