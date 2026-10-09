#pragma once

#include "Nexora/Editor/EditorProduction.h"
#include "ProfileJson.h"

#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace nexora::editor::detail {
// A bounded schema reader, not a general JSON DOM. No recursion or unknown-value skipping.
class ProcessMemoryJsonReader final : private ProfileJsonReader {
public:
  explicit ProcessMemoryJsonReader(std::string_view bytes) : ProfileJsonReader(bytes) {}

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
    if (!End())
      return std::nullopt;
    return capture;
  }

private:
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
};
} // namespace nexora::editor::detail
