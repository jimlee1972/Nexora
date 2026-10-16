#include "Nexora/Runtime/GameplayAudio.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void Check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::uint32_t frames = 48000;
    bool expectUnavailable{};
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg(argv[i]);
      if (arg == "--expect-unavailable")
        expectUnavailable = true;
      else if (arg == "--frames" && i + 1 < argc) {
        const std::string_view value(argv[++i]);
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
        Check(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && frames &&
                  frames <= 480000,
              "frames must be in 1..480000");
      } else
        throw std::runtime_error("usage: NexoraTwoDAudioProbe [--frames N] [--expect-unavailable]");
    }
    nexora::runtime::GameplayAudio audio;
    const auto api = audio.Api();
    std::array<float, 4800> tone{};
    for (std::size_t i = 0; i < tone.size(); ++i)
      tone[i] = .1f * std::sin(static_cast<float>(i) * 6.28318530718f * 440.f / 48000.f);
    Check(audio.RegisterPcm(1, 48000, 1, tone), "tone registration");
    std::string error;
    const bool opened = audio.OpenDefaultOutput(error);
    std::uint32_t available{};
    Check(api.output_available(api.context, &available) == 0 && available == (opened ? 1u : 0u),
          "output availability disagrees with device open");
    NexoraAudioPlayDesc desc{1, NEXORA_AUDIO_BUS_SFX, 1, 1};
    std::uint64_t voice{};
    if (expectUnavailable) {
      Check(!opened && !error.empty(), "expected unavailable device");
      Check(api.play(api.context, &desc, &voice) == -2 && !audio.Pump(1),
            "unavailable output accepted playback");
      audio.CloseOutput();
      audio.CloseOutput();
      std::cout << "{\"device_output\":\"UNAVAILABLE\",\"audible_acceptance\":\"NOT VERIFIED\"}\n";
      std::cerr << error << '\n';
      return 0;
    }
    Check(opened, error.c_str());
    Check(error.empty(), "successful device open retained an error");
    Check(api.play(api.context, &desc, &voice) == 0, "device play");
    std::uint32_t submitted{};
    while (submitted < frames) {
      const auto count = std::min(4096u, frames - submitted);
      Check(audio.Pump(count), "device write failed");
      submitted += count;
    }
    Check(api.stop_all(api.context) == 0, "voice shutdown");
    audio.CloseOutput();
    Check(api.output_available(api.context, &available) == 0 && !available,
          "closed output remains available");
    Check(api.play(api.context, &desc, &voice) == -2, "closed output accepted playback");
    // Reopen exercises release of the prior header/event/device/library generation.
    const bool reopened = audio.OpenDefaultOutput(error);
    Check(reopened, error.c_str());
    Check(api.play(api.context, &desc, &voice) == 0 && audio.Pump(1), "reopened output");
    Check(api.stop_all(api.context) == 0, "reopened voice shutdown");
    audio.CloseOutput();
    audio.CloseOutput();
    std::cout << "{\"device_output\":\"PASS\",\"sample_rate\":48000,\"submitted_frames\":"
              << submitted + 1 << ",\"reopened\":true,\"audible_acceptance\":\"NOT VERIFIED\"}\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
