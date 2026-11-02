#include "Nexora/Runtime/GameplayAudio.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

namespace {
struct Capture {
  std::vector<float> samples;
  bool fail{};
  unsigned calls{};
  static bool Write(void *ctx, const float *samples, std::uint32_t frames, std::uint32_t rate) {
    auto &self = *static_cast<Capture *>(ctx);
    ++self.calls;
    assert(rate == 48000);
    self.samples.assign(samples, samples + frames * 2);
    return !self.fail;
  }
};
void Near(float actual, float expected) { assert(std::abs(actual - expected) < .00001f); }
NexoraAudioUsageV1 Usage(nexora::runtime::GameplayAudio &audio) {
  auto api = audio.StateApi();
  NexoraAudioUsageV1 result{};
  result.struct_size = sizeof(result);
  assert(api.get_usage(api.context, &result) == 0);
  return result;
}
bool SameUsage(const NexoraAudioUsageV1 &a, const NexoraAudioUsageV1 &b) {
  return a.active_assets == b.active_assets && a.active_voices == b.active_voices &&
         a.retired_generations == b.retired_generations && a.active_bytes == b.active_bytes &&
         a.retired_bytes == b.retired_bytes && a.peak_live_bytes == b.peak_live_bytes &&
         a.active_limit == b.active_limit && a.live_limit == b.live_limit &&
         a.asset_limit == b.asset_limit;
}
void ReloadMixer() {
  using nexora::runtime::GameplayAudio;
  GameplayAudio live(48000, 4), reference(48000, 4), foreign(48000, 4);
  Capture output, expected;
  live.SetOutput(Capture::Write, &output);
  reference.SetOutput(Capture::Write, &expected);
  const std::array<float, 4> original{.1F, .2F, .3F, .4F};
  const std::array<float, 2> replacement{.6F, .7F};
  std::uint64_t first{}, second{};
  for (auto *owner : {&live, &reference}) {
    auto api = owner->Api();
    const NexoraAudioPlayDesc music{10, NEXORA_AUDIO_BUS_MUSIC, 1, .5F};
    const NexoraAudioPlayDesc sfx{10, NEXORA_AUDIO_BUS_SFX, 1, .25F};
    assert(owner->RegisterPcm(10, 24000, 1, original) &&
           api.play(api.context, &music, &first) == 0 &&
           owner->RegisterPcm(10, 48000, 1, replacement) &&
           api.play(api.context, &sfx, &second) == 0 && owner->Pump(3));
  }
  assert(!live.SwapReloadMixer(foreign));
  const std::array<float, 16> extra{.125F};
  for (unsigned generation = 1; generation <= 32; ++generation) {
    const auto before = Usage(live);
    auto discarded = live.ForkForReload();
    assert(discarded && SameUsage(before, Usage(*discarded)) &&
           discarded->RegisterPcm(999, 48000, 1, extra));
    auto rejected = discarded->Api();
    assert(rejected.stop_all(rejected.context) == 0 && SameUsage(before, Usage(live)));
    discarded.reset();
    assert(SameUsage(before, Usage(live)));

    auto candidate = live.ForkForReload();
    assert(candidate && !candidate->Pump(1)); // No fake submission/clock advance in a branch.
    std::thread wrong([&] { assert(!live.ForkForReload() && !live.SwapReloadMixer(*candidate)); });
    wrong.join();
    auto api = candidate->Api();
    auto baseline = reference.Api();
    const auto calls = output.calls;
    const auto paused = generation % 2;
    const auto gain = generation % 3 == 0 ? .25F : .5F;
    assert(api.set_paused(api.context, second, paused) == 0 &&
           baseline.set_paused(baseline.context, second, paused) == 0 &&
           api.set_gain(api.context, first, gain) == 0 &&
           baseline.set_gain(baseline.context, first, gain) == 0 &&
           candidate->RegisterPcm(99, 48000, 1, extra) &&
           reference.RegisterPcm(99, 48000, 1, extra));
    const NexoraAudioPlayDesc transient{99, NEXORA_AUDIO_BUS_SFX, 0, 1};
    std::uint64_t staged_voice{}, expected_voice{};
    assert(api.play(api.context, &transient, &staged_voice) == 0 &&
           baseline.play(baseline.context, &transient, &expected_voice) == 0 &&
           staged_voice == expected_voice && staged_voice > second &&
           api.stop(api.context, staged_voice) == 0 &&
           baseline.stop(baseline.context, expected_voice) == 0 && output.calls == calls &&
           SameUsage(before, Usage(live)));
    assert(live.SwapReloadMixer(*candidate) && SameUsage(Usage(live), Usage(reference)));
    // The Editor must redirect old generation proxies to this retired mixer before cleanup;
    // a provider's raw context intentionally stays attached to that provider object.
    auto retired = candidate->Api();
    assert(retired.stop_all(retired.context) == 0 && !candidate->Pump(1) &&
           SameUsage(Usage(live), Usage(reference)) && output.calls == calls);
    assert(live.Pump(7) && reference.Pump(7) && output.calls == calls + 1 &&
           output.samples.size() == expected.samples.size());
    for (std::size_t index = 0; index < output.samples.size(); ++index)
      Near(output.samples[index], expected.samples[index]);
  }
  GameplayAudio unavailable;
  auto branch = unavailable.ForkForReload();
  assert(branch);
  auto api = branch->Api();
  std::uint32_t available = 1;
  assert(api.output_available(api.context, &available) == 0 && !available && !branch->Pump(1));
}
} // namespace
int main() {
  ReloadMixer();
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
  // No caller bytes retained.
  std::fill(copied_wave.begin(), copied_wave.end(), static_cast<unsigned char>(0));
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
