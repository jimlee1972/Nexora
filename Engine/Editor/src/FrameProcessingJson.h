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
class FrameProcessingJsonReader final : private ProfileJsonReader {
public:
  explicit FrameProcessingJsonReader(std::string_view bytes) : ProfileJsonReader(bytes) {}

  std::optional<FrameProcessingCapture> Read(std::string_view project_id) {
    FrameProcessingCapture capture;
    std::uint64_t count = 0;
    const std::array<std::string_view, 11> keys{"schema",
                                                "source",
                                                "metric",
                                                "scope",
                                                "unit",
                                                "project_uuid",
                                                "sample_count",
                                                "older_frames_dropped",
                                                "gpu_timing_available",
                                                "memory_measurement_available",
                                                "samples"};
    if (!Object(keys,
                [&](std::size_t field) {
                  switch (field) {
                  case 0:
                    return Literal("1");
                  case 1:
                    return Text("NexoraEditor");
                  case 2:
                    return Text("editor_frame_processing_wall_ms");
                  case 3:
                    return Text("after_begin_frame_before_present");
                  case 4:
                    return Text("milliseconds");
                  case 5:
                    return Text(project_id);
                  case 6: {
                    auto token = Number();
                    return token && Integer(*token, count) && count > 0 && count <= 600;
                  }
                  case 7:
                    return StringInteger(capture.older_frames_dropped);
                  case 8:
                  case 9:
                    return Literal("false");
                  case 10:
                    return Samples(capture);
                  default:
                    return false;
                  }
                }) ||
        count != capture.samples.size())
      return std::nullopt;
    if (!End())
      return std::nullopt;
    return capture;
  }

private:
  bool Samples(FrameProcessingCapture &capture) {
    if (!Literal("["))
      return false;
    const std::array<std::string_view, 4> keys{"frame", "frame_processing_wall_ms", "gpu_ms",
                                               "memory_bytes"};
    do {
      if (capture.samples.size() == 600)
        return false;
      FrameSample sample{};
      if (!Object(keys,
                  [&](std::size_t field) {
                    if (field == 0)
                      return StringInteger(sample.frame);
                    if (field == 1) {
                      const auto token = Number();
                      if (!token)
                        return false;
                      const auto result = std::from_chars(
                          token->data(), token->data() + token->size(), sample.cpu_ms);
                      return result.ec == std::errc{} &&
                             result.ptr == token->data() + token->size() &&
                             std::isfinite(sample.cpu_ms) && sample.cpu_ms >= 0;
                    }
                    return Literal("null");
                  }) ||
          sample.frame == 0 ||
          (!capture.samples.empty() && sample.frame <= capture.samples.back().frame))
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
