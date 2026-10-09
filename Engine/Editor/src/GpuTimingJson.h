#pragma once
#include "Nexora/Editor/EditorProduction.h"
#include "ProfileJson.h"

namespace nexora::editor::detail {
inline std::string_view GpuTimingSourceName(GpuProfileSource source) noexcept {
  switch (source) {
  case GpuProfileSource::VulkanTimestamps:
    return "vulkan_timestamps";
  case GpuProfileSource::Dx12Timestamps:
    return "dx12_timestamps";
  case GpuProfileSource::MetalCommandBuffer:
    return "metal_command_buffer";
  default:
    return {};
  }
}
class GpuTimingJsonReader final : private ProfileJsonReader {
public:
  explicit GpuTimingJsonReader(std::string_view bytes) : ProfileJsonReader(bytes) {}
  std::optional<GpuTimingCapture> Read(std::string_view project_id) {
    GpuTimingCapture capture;
    std::uint64_t count = 0;
    const std::array<std::string_view, 12> keys{"schema",        "source",
                                                "metric",        "scope",
                                                "unit",          "export_project_uuid",
                                                "sample_count",  "older_samples_dropped",
                                                "timing_source", "software_rasterizer",
                                                "sequence_axis", "samples"};
    if (!Object(keys,
                [&](std::size_t field) {
                  switch (field) {
                  case 0:
                    return Literal("1");
                  case 1:
                    return Text("NexoraEditor");
                  case 2:
                    return Text("completed_gpu_timing");
                  case 3:
                    return Text("native_command_buffer_interval");
                  case 4:
                    return Text("milliseconds");
                  case 5:
                    return Text(project_id);
                  case 6: {
                    const auto token = Number();
                    return token && Integer(*token, count) && count > 0 && count <= 600;
                  }
                  case 7:
                    return StringInteger(capture.older_samples_dropped);
                  case 8: {
                    const auto value = String();
                    for (const auto source :
                         {GpuProfileSource::VulkanTimestamps, GpuProfileSource::Dx12Timestamps,
                          GpuProfileSource::MetalCommandBuffer})
                      if (value && *value == GpuTimingSourceName(source)) {
                        capture.source = source;
                        return true;
                      }
                    return false;
                  }
                  case 9:
                    Space();
                    if (remaining_.starts_with("true")) {
                      capture.software_rasterizer = true;
                      return Literal("true");
                    }
                    return Literal("false");
                  case 10:
                    return Text("native_completed_submission_id");
                  case 11:
                    return Samples(capture);
                  default:
                    return false;
                  }
                }) ||
        count != capture.samples.size() ||
        !ValidateGpuTimingSamples(capture.source, capture.samples) || !End())
      return std::nullopt;
    return capture;
  }

private:
  bool Samples(GpuTimingCapture &capture) {
    if (!Literal("["))
      return false;
    const std::array<std::string_view, 2> keys{"submission", "milliseconds"};
    do {
      if (capture.samples.size() == 600)
        return false;
      GpuProfileSample sample{};
      if (!Object(keys, [&](std::size_t field) {
            if (field == 0)
              return StringInteger(sample.submission);
            Space();
            if (remaining_.starts_with("null"))
              return Literal("null");
            const auto token = Number();
            if (!token)
              return false;
            double measured = 0;
            const auto result =
                std::from_chars(token->data(), token->data() + token->size(), measured);
            if (result.ec != std::errc{} || result.ptr != token->data() + token->size())
              return false;
            sample.milliseconds = measured;
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
