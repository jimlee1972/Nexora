#include "Nexora/Editor/ChromeTrace.h"
#include "Nexora/Foundation/Types.h"
#include "ProfileJson.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
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
    if (!events || !End() || !open_scopes_.empty() || !async_scopes_.empty() ||
        capture_.samples.empty())
      return {};
    ComputeSelf();
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
  bool Sample(double start, double duration) {
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
  bool Event() {
    std::string phase, name;
    std::string_view id_token, category_token, scope_token;
    std::string_view id2_token;
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
          if (key == "id" || key == "id2" || key == "cat" || key == "scope") {
            Space();
            const auto before = remaining_;
            if (!Skip(2))
              return false;
            auto &token = key == "id"    ? id_token
                          : key == "id2" ? id2_token
                          : key == "cat" ? category_token
                                         : scope_token;
            token = before.substr(0, before.size() - remaining_.size());
            return true;
          }
          return Skip(2);
        }) ||
        !(seen & 1))
      return false;
    if (phase == "b" || phase == "e" || phase == "S" || phase == "F") {
      const bool legacy = phase == "S" || phase == "F";
      if ((seen & 31) != 31 || (seen & 32))
        return false;
      // Decode identity only for supported async events. Existing X/B/E metadata remains opaque.
      const auto tail = remaining_;
      AsyncIdentity identity;
      identity.legacy = legacy;
      const auto value = [&] {
        Space();
        identity.numeric = !remaining_.starts_with('"');
        if (identity.numeric) {
          std::uint64_t number{};
          if (!Identity(number))
            return false;
          identity.id = std::to_string(number);
        } else {
          auto decoded = ReadString();
          if (!decoded || decoded->empty() || decoded->size() > 256)
            return false;
          identity.id = std::move(*decoded);
        }
        return true;
      };
      if (!id_token.empty()) {
        remaining_ = id_token;
        if (!id2_token.empty() || !value() || !End())
          return false;
      } else {
        remaining_ = id2_token;
        bool identifier = false;
        if (!Members([&](const std::string &key) {
              if (identifier || (key != "local" && key != "global"))
                return false;
              identifier = true;
              identity.process_local = key == "local";
              // Unrelated process-local identities remain opaque validated metadata.
              if (identity.process_local && process != capture_.selection.process)
                return Skip(3);
              return value();
            }) ||
            !identifier || !End())
          return false;
      }
      if (identity.process_local && process != capture_.selection.process) {
        remaining_ = tail;
        return true;
      }
      const auto text = [&](std::string_view token, std::string &value) {
        if (token.empty())
          return true;
        remaining_ = token;
        auto decoded = ReadString();
        if (!decoded || decoded->size() > 256 || !End())
          return false;
        value = std::move(*decoded);
        return true;
      };
      if (!text(category_token, identity.category) || !text(scope_token, identity.scope))
        return false;
      remaining_ = tail;
      const auto previous =
          std::find_if(async_scopes_.rbegin(), async_scopes_.rend(), [&](const auto &scope) {
            return scope.identity == identity && (!legacy || scope.scope.name == name);
          });
      if (previous != async_scopes_.rend() && start < previous->last_time)
        return false;
      if (phase == "b" || phase == "S") {
        // Legacy S/F intervals are independent by name and cannot nest within one identity.
        if (legacy && previous != async_scopes_.rend())
          return false;
        if (open_scopes_.size() + async_scopes_.size() == ChromeTraceImporter::kMaximumOpenScopes)
          return false;
        const bool selected = process == capture_.selection.process &&
                              thread == capture_.selection.thread &&
                              name == capture_.selection.event_name;
        if (selected && !Sample(start, 0))
          return false;
        async_scopes_.push_back(
            {std::move(identity), {std::move(name), start, selected ? selected_ : 0}, start});
        return true;
      }
      // Nestable b/e is LIFO per identity; legacy S/F closes its independent named interval.
      // Independent IDs may finish in any order and on any thread/process.
      const auto begin = previous;
      if (begin == async_scopes_.rend() || begin->scope.name != name || start < begin->scope.start)
        return false;
      Complete(begin->scope, start);
      async_scopes_.erase(std::next(begin).base());
      const auto parent =
          std::find_if(async_scopes_.rbegin(), async_scopes_.rend(),
                       [&](const auto &scope) { return scope.identity == identity; });
      if (!legacy && parent != async_scopes_.rend())
        parent->last_time = start;
      return true;
    }
    if (phase != "X" && phase != "B" && phase != "E")
      return true;
    if ((phase == "X" && (seen != 63 || duration > maximum_exact_integer - start)) ||
        (phase == "B" && seen != 31) || (phase == "E" && ((seen & 29) != 29 || (seen & 32))))
      return false;
    if (process != capture_.selection.process || thread != capture_.selection.thread)
      return true;
    if (phase == "X") {
      const bool selected = name == capture_.selection.event_name;
      if (selected && !Sample(start, duration))
        return false;
      synchronous_.push_back({start, duration, 0, 0, selected ? selected_ : 0, events_, false});
      return true;
    }
    if (scope_time_ && start < *scope_time_)
      return false;
    scope_time_ = start;
    if (phase == "B") {
      if (open_scopes_.size() + async_scopes_.size() == ChromeTraceImporter::kMaximumOpenScopes)
        return false;
      const bool selected = name == capture_.selection.event_name;
      if (selected && !Sample(start, 0))
        return false;
      open_scopes_.push_back({std::move(name), start, selected ? selected_ : 0, events_});
      return true;
    }
    if (open_scopes_.empty())
      return false;
    const auto &begin = open_scopes_.back();
    if (((seen & 2) && name != begin.name) || start < begin.start)
      return false;
    Complete(begin, start);
    synchronous_.push_back(
        {begin.start, start - begin.start, 0, 0, begin.sequence, begin.origin, true});
    open_scopes_.pop_back();
    return true;
  }
  struct OpenScope final {
    std::string name;
    double start;
    std::uint64_t sequence;
    std::size_t origin{};
  };
  struct AsyncIdentity final {
    std::string id, category, scope;
    bool numeric{}, process_local{}, legacy{};
    bool operator==(const AsyncIdentity &) const = default;
  };
  struct AsyncScope final {
    AsyncIdentity identity;
    OpenScope scope;
    double last_time;
  };
  struct SynchronousScope final {
    double start, duration, children, self;
    std::uint64_t sequence;
    std::size_t origin;
    bool paired;
  };
  void ComputeSelf() {
    if (synchronous_.empty())
      return;
    std::ranges::sort(synchronous_, [](const auto &a, const auto &b) {
      if (a.start != b.start)
        return a.start < b.start;
      if (a.duration != b.duration)
        return a.duration > b.duration;
      return a.origin < b.origin;
    });
    std::vector<std::size_t> stack;
    stack.reserve(synchronous_.size());
    const auto close = [&] {
      auto &scope = synchronous_[stack.back()];
      if (scope.children > scope.duration)
        return false;
      scope.self = scope.duration - scope.children;
      stack.pop_back();
      if (!stack.empty())
        synchronous_[stack.back()].children += scope.duration;
      return true;
    };
    std::size_t first{}, farthest{};
    bool valid = true;
    const auto finish = [&](std::size_t end) {
      if (!valid) {
        stack.clear();
        return;
      }
      while (!stack.empty())
        if (!close()) {
          valid = false;
          stack.clear();
          return;
        }
      for (auto i = first; i < end; ++i) {
        const auto &scope = synchronous_[i];
        if (!scope.sequence)
          continue;
        auto &sample =
            capture_.samples[(scope.sequence - 1) % ChromeTraceImporter::kMaximumSamples];
        // Sorting context can visit evicted begins after a newer occupant; never overwrite it.
        if (sample.sequence == scope.sequence)
          sample.self_duration_milliseconds = scope.self / 1000.0;
      }
    };
    for (std::size_t i = 0; i < synchronous_.size(); ++i) {
      const auto &scope = synchronous_[i];
      const auto &last = synchronous_[farthest];
      // Compare relative starts and raw durations: adding a tiny X duration to a large
      // absolute timestamp could round away the entire interval or invent an overlap.
      if (i != first && scope.start - last.start >= last.duration) {
        finish(i);
        first = farthest = i;
        valid = true;
      } else if (scope.duration > last.duration - (scope.start - last.start)) {
        farthest = i;
      }
      if (!valid)
        continue;
      while (!stack.empty() &&
             scope.start - synchronous_[stack.back()].start >= synchronous_[stack.back()].duration)
        if (!close()) {
          valid = false;
          stack.clear();
          break;
        }
      if (!valid)
        continue;
      if (!stack.empty()) {
        const auto &parent = synchronous_[stack.back()];
        if (scope.duration > parent.duration - (scope.start - parent.start) ||
            (scope.start == parent.start && scope.duration == parent.duration &&
             (!scope.paired || !parent.paired))) {
          // One crossing/coincident scope makes this connected component ambiguous. Inclusive
          // intervals remain valid; disjoint components can still expose their own self time.
          valid = false;
          stack.clear();
          continue;
        }
      }
      stack.push_back(i);
    }
    finish(synchronous_.size());
  }
  void Complete(const OpenScope &begin, double start) {
    if (begin.sequence) {
      auto &sample = capture_.samples[(begin.sequence - 1) % ChromeTraceImporter::kMaximumSamples];
      // The begin can be evicted before its end; never overwrite the newer occupant of that slot.
      if (sample.sequence == begin.sequence)
        sample.duration_milliseconds = (start - begin.start) / 1000.0;
    }
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
  std::vector<OpenScope> open_scopes_;
  std::vector<AsyncScope> async_scopes_;
  std::vector<SynchronousScope> synchronous_;
  std::optional<double> scope_time_;
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
    *error = "Chrome trace is malformed, exceeds bounds, or has no ordered matching "
             "complete/balanced duration events.";
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
    if (sample.self_duration_milliseconds &&
        (!std::isfinite(*sample.self_duration_milliseconds) ||
         *sample.self_duration_milliseconds < 0 ||
         *sample.self_duration_milliseconds > sample.duration_milliseconds))
      return false;
    previous = sample.start_microseconds;
  }
  return true;
}
} // namespace nexora::editor
