#include "Nexora/Runtime/GameplayAudio.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace {
struct Capture {
  std::vector<float> samples;
  bool fail{};
  static bool Write(void *ctx, const float *samples, std::uint32_t frames, std::uint32_t rate) {
    auto &self = *static_cast<Capture *>(ctx);
    assert(rate == 48000);
    self.samples.assign(samples, samples + frames * 2);
    return !self.fail;
  }
};
void Near(float actual, float expected) { assert(std::abs(actual - expected) < .00001f); }
} // namespace
int main() {
  nexora::runtime::GameplayAudio audio(48000, 2);
  auto api = audio.Api();
  assert(api.abi_version == 1 && api.struct_size == sizeof(api));
  std::uint32_t available = 8;
  assert(api.output_available(api.context, &available) == 0 && available == 0);
  const std::array<float, 2> pcm{.5f, -.25f};
  assert(audio.RegisterPcm(10, 24000, 1, pcm));
  NexoraAudioPlayDesc desc{10, NEXORA_AUDIO_BUS_MUSIC, 0, 1};
  std::uint64_t voice = 99;
  assert(api.play(api.context, &desc, &voice) == -2 && voice == 99);
  Capture capture;
  audio.SetOutput(Capture::Write, &capture);
  assert(api.play(api.context, &desc, &voice) == 0);
  assert(audio.Pump(2));
  assert(capture.samples.size() == 4);
  Near(capture.samples[0], .5f);
  Near(capture.samples[1], .5f);
  Near(capture.samples[2], .5f);
  assert(api.set_paused(api.context, voice, 1) == 0);
  assert(audio.Pump(4));
  for (float f : capture.samples)
    Near(f, 0);
  assert(api.set_paused(api.context, voice, 0) == 0);
  assert(api.set_gain(api.context, voice, .5f) == 0);
  assert(api.set_bus_gain(api.context, NEXORA_AUDIO_BUS_MASTER, .5f) == 0);
  assert(api.set_bus_gain(api.context, NEXORA_AUDIO_BUS_MUSIC, .5f) == 0);
  assert(audio.Pump(3));
  Near(capture.samples[0], -.03125f);
  Near(capture.samples[2], -.03125f);
  Near(capture.samples[4], 0);
  assert(api.is_playing(api.context, voice, &available) == 0 && !available);
  assert(api.stop(api.context, voice) == -3);
  desc.loop = 1;
  desc.bus = NEXORA_AUDIO_BUS_SFX;
  assert(api.set_bus_gain(api.context, NEXORA_AUDIO_BUS_MASTER, 1) == 0);
  assert(api.play(api.context, &desc, &voice) == 0);
  assert(audio.RemoveAsset(10)); // Active voice retains the original asset.
  assert(audio.Pump(6));
  Near(capture.samples[0], .5f);
  Near(capture.samples[4], -.25f);
  Near(capture.samples[8], .5f);
  std::uint64_t missing = 55;
  assert(api.play(api.context, &desc, &missing) == -3 && missing == 55);
  assert(api.stop(api.context, voice) == 0);
  assert(api.set_gain(api.context, voice, 1) == -3);
  assert(api.set_bus_gain(api.context, 9, 1) == -1);
  assert(api.set_gain(api.context, 1, std::numeric_limits<float>::infinity()) == -1);
  const std::array<float, 2> invalid{0, std::numeric_limits<float>::quiet_NaN()};
  assert(!audio.RegisterPcm(10, 48000, 1, invalid));
  // A complete minimal PCM16 WAV, plus rejection without replacing good data.
  const unsigned char wav[] = {'R',  'I',  'F', 'F', 40, 0,    0, 0, 'W', 'A',  'V', 'E',
                               'f',  'm',  't', ' ', 16, 0,    0, 0, 1,   0,    1,   0,
                               0x80, 0xbb, 0,   0,   0,  0x77, 1, 0, 2,   0,    16,  0,
                               'd',  'a',  't', 'a', 4,  0,    0, 0, 0,   0x40, 0,   0xc0};
  const auto bytes = std::as_bytes(std::span(wav));
  std::vector<unsigned char> copied_wave(std::begin(wav), std::end(wav));
  assert(api.register_wave(api.context, 10, copied_wave.data(),
                           static_cast<std::uint32_t>(copied_wave.size())) == 0);
  std::fill(copied_wave.begin(), copied_wave.end(), 0); // No caller bytes retained.
  assert(api.register_wave(api.context, 10, nullptr, 1) == -1);
  assert(api.register_wave(api.context, 0, wav, sizeof(wav)) == -1);
  assert(api.register_wave(api.context, 10, wav, 64u * 1024u * 1024u + 1) == -1);
  assert(api.register_wave(api.context, 10, wav, sizeof(wav) - 1) == -1);
  assert(!audio.RegisterWave(10, bytes.first(bytes.size() - 1)));
  desc.loop = 0;
  assert(api.play(api.context, &desc, &voice) == 0);
  assert(audio.Pump(2));
  Near(capture.samples[0], .5f);
  Near(capture.samples[2], -.5f);
  desc.loop = 1;
  assert(api.play(api.context, &desc, &voice) == 0);
  std::uint64_t other{};
  assert(api.play(api.context, &desc, &other) == 0);
  assert(other != voice);
  assert(api.play(api.context, &desc, &missing) == -3);
  assert(api.remove_asset(api.context, 0) == -1);
  assert(api.remove_asset(api.context, 10) == 0);
  assert(api.remove_asset(api.context, 10) == -3);
  assert(audio.Pump(2)); // Registered WAV voices retain samples after removal.
  Near(capture.samples[0], 1);
  Near(capture.samples[2], -1);
  assert(api.stop_all(api.context) == 0);
  assert(api.is_playing(api.context, other, &available) == 0 && !available);
  capture.fail = true;
  assert(!audio.Pump(1));
  assert(api.output_available(api.context, &available) == 0 && !available);
  assert(api.play(api.context, &desc, &voice) == -2);
  assert(!audio.Pump(0) && !audio.Pump(4097));
  // A resampling step larger than a short loop must wrap by its full length.
  nexora::runtime::GameplayAudio fast;
  Capture fast_capture;
  fast.SetOutput(Capture::Write, &fast_capture);
  auto fast_api = fast.Api();
  const std::array<float, 3> short_loop{.1f, .2f, .3f};
  assert(fast.RegisterPcm(1, 192000, 1, short_loop));
  NexoraAudioPlayDesc fast_desc{1, NEXORA_AUDIO_BUS_SFX, 1, 1};
  assert(fast_api.play(fast_api.context, &fast_desc, &voice) == 0);
  assert(fast.Pump(4));
  Near(fast_capture.samples[0], .1f);
  Near(fast_capture.samples[2], .2f);
  Near(fast_capture.samples[4], .3f);
  Near(fast_capture.samples[6], .1f);
  assert(fast_api.stop_all(fast_api.context) == 0);
  const std::array<float, 2> stereo{.75f, -.75f};
  assert(fast.RegisterPcm(2, 48000, 2, stereo));
  fast_desc = {2, NEXORA_AUDIO_BUS_SFX, 1, 2};
  assert(fast_api.play(fast_api.context, &fast_desc, &voice) == 0);
  assert(fast.Pump(2));
  Near(fast_capture.samples[0], 1);
  Near(fast_capture.samples[1], -1);
  Near(fast_capture.samples[2], 1);
  Near(fast_capture.samples[3], -1);
  return 0;
}
