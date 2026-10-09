#pragma once

#include "Nexora/Editor/EditorProduction.h"

#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace nexora::editor::detail {
// A bounded schema reader, not a general JSON DOM. No recursion or unknown-value skipping.
class ProcessMemoryJsonReader final {
public:
  explicit ProcessMemoryJsonReader(std::string_view bytes) : remaining_(bytes) {}

  std::optional<ProcessMemoryCapture> Read(std::string_view project_id) {
    ProcessMemoryCapture capture;
    std::uint64_t count = 0;
    const std::array<std::string_view, 11> keys{"schema",       "source",
                                                "metric",       "scope",
                                                "unit",         "export_project_uuid",
                                                "sample_count", "older_samples_dropped",
                                                "time_unit",    "time_origin",
                                                "samples"};
    if (!Object(keys,
                [&](std::size_t field) {
                  switch (field) {
                  case 0:
                    return Literal("1");
                  case 1:
                    return Text("NexoraEditor");
                  case 2:
                    return Text("process_resident_memory");
                  case 3:
                    return Text("current_process_including_shared_resident_pages");
                  case 4:
                    return Text("bytes");
                  case 5:
                    return Text(project_id);
                  case 6: {
                    auto token = Number();
                    return token && Integer(*token, count) && count > 0 && count <= 600;
                  }
                  case 7:
                    return StringInteger(capture.older_samples_dropped);
                  case 8:
                    return Text("milliseconds");
                  case 9:
                    return Text("first_observation_since_clear");
                  case 10:
                    return Samples(capture);
                  default:
                    return false;
                  }
                }) ||
        count != capture.samples.size() || !ValidateProcessMemorySamples(capture.samples))
      return std::nullopt;
    Space();
    if (!remaining_.empty())
      return std::nullopt;
    return capture;
  }

private:
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
  bool Samples(ProcessMemoryCapture &capture) {
    if (!Literal("["))
      return false;
    const std::array<std::string_view, 3> keys{"sequence", "elapsed_ms", "resident_bytes"};
    do {
      if (capture.samples.size() == 600)
        return false;
      ProcessMemorySample sample{};
      if (!Object(keys, [&](std::size_t field) {
            if (field == 0)
              return StringInteger(sample.sequence);
            if (field == 1) {
              const auto token = Number();
              if (!token)
                return false;
              const auto result =
                  std::from_chars(token->data(), token->data() + token->size(), sample.elapsed_ms);
              return result.ec == std::errc{} && result.ptr == token->data() + token->size();
            }
            Space();
            if (remaining_.starts_with("null"))
              return Literal("null");
            std::uint64_t bytes = 0;
            if (!StringInteger(bytes))
              return false;
            sample.resident_bytes = bytes;
            return true;
          }))
        return false;
      capture.samples.push_back(sample);
      Space();
      if (remaining_.starts_with(']'))
        return Literal("]");
    } while (Literal(","));
    return false;
  }
  std::string_view remaining_;
};
} // namespace nexora::editor::detail
