#include "Nexora/Core/ProcessMemory.h"
#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayServices.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
using namespace nexora;
namespace {
struct Sink {
  bool fail{};
  std::vector<float> pcm;
  static bool Write(void *p, const float *v, std::uint32_t frames, std::uint32_t) {
    auto &s = *static_cast<Sink *>(p);
    s.pcm.assign(v, v + frames * 2);
    return !s.fail;
  }
};
NexoraAudioUsageV1 Usage(runtime::GameplayAudio &a) {
  auto api = a.StateApi();
  NexoraAudioUsageV1 out{};
  out.struct_size = sizeof(out);
  assert(api.get_usage(api.context, &out) == 0);
  return out;
}
void Near(float a, float b) { assert(std::abs(a - b) < 0.00001F); }
} // namespace
int main() {
  runtime::GameplayAudio a(48000, 4, {2, 64, 96});
  Sink sink;
  a.SetOutput(Sink::Write, &sink);
  const std::array<float, 8> pcm{.25F, .25F, .25F, .25F, .25F, .25F, .25F, .25F};
  assert(a.RegisterPcm(1, 48000, 1, pcm));
  auto api = a.Api();
  std::uint64_t voice{};
  NexoraAudioPlayDesc d{1, 2, 1, 1};
  assert(api.play(api.context, &d, &voice) == 0);
  assert(a.RemoveAsset(1));
  auto u = Usage(a);
  assert(u.active_bytes == 0 && u.retired_bytes == 32 && u.retired_generations == 1 &&
         u.active_voices == 1);
  assert(a.RegisterPcm(1, 48000, 1, pcm) && a.RegisterPcm(2, 48000, 1, pcm));
  assert(!a.RegisterPcm(3, 48000, 1, pcm)); // count/active/live all bounded
  assert(!a.RegisterPcm(1, 48000, 1, pcm)); // replacing must reserve the complete new generation
  assert(Usage(a).active_bytes == 64 && Usage(a).retired_bytes == 32);
  assert(a.Pump(1));
  Near(sink.pcm[0], .25F);
  assert(api.stop(api.context, voice) == 0 && Usage(a).retired_bytes == 0);
  assert(a.RegisterPcm(1, 48000, 1, pcm));
  assert(Usage(a).peak_live_bytes == 96);
  assert(a.RemoveAsset(1) && a.RemoveAsset(2) && Usage(a).active_bytes == 0);
  // ABI capacity errors are distinct from malformed WAVE and preserve the old registry.
  runtime::GameplayAudio tiny(48000, 1, {1, 4, 8});
  const std::array<float, 1> original{.5F};
  assert(tiny.RegisterPcm(7, 48000, 1, original));
  const unsigned char wave[]{'R',  'I',  'F', 'F', 40, 0,    0, 0, 'W', 'A',  'V', 'E',
                             'f',  'm',  't', ' ', 16, 0,    0, 0, 1,   0,    1,   0,
                             0x80, 0xbb, 0,   0,   0,  0x77, 1, 0, 2,   0,    16,  0,
                             'd',  'a',  't', 'a', 4,  0,    0, 0, 0,   0x20, 0,   0x20};
  auto tinyApi = tiny.Api();
  assert(tinyApi.register_wave(tinyApi.context, 7, wave, sizeof(wave)) == -3);
  assert(tinyApi.register_wave(tinyApi.context, 7, wave, sizeof(wave) - 1) == -1);
  assert(Usage(tiny).active_bytes == 4 && Usage(tiny).peak_live_bytes == 4);
  // Registry removal while paused/playing keeps exact samples; each old generation is counted once.
  assert(a.RegisterPcm(1, 48000, 1, pcm));
  assert(api.play(api.context, &d, &voice) == 0);
  assert(api.set_paused(api.context, voice, 1) == 0 && a.RemoveAsset(1));
  assert(a.Pump(4) && Usage(a).retired_bytes == 32);
  assert(api.stop_all(api.context) == 0 && Usage(a).retired_bytes == 0);
  // Independent voice volume: music is not changed by voice mute, skip or output failure.
  const std::array<float, 1> music{.125F};
  assert(a.RegisterPcm(1, 48000, 1, music) && a.RegisterPcm(2, 48000, 1, pcm));
  std::uint64_t bgm{}, line{};
  NexoraAudioPlayDesc m{1, 1, 1, 1}, l{2, 2, 1, .5F};
  assert(api.play(api.context, &m, &bgm) == 0 && api.play(api.context, &l, &line) == 0);
  assert(a.Pump(1));
  Near(sink.pcm[0], .25F);
  assert(api.set_gain(api.context, line, 0) == 0 && a.Pump(1));
  Near(sink.pcm[0], .125F);
  assert(api.set_gain(api.context, line, 1) == 0 && api.stop(api.context, line) == 0 && a.Pump(1));
  Near(sink.pcm[0], .125F);
  // A dedicated Voice bus remains audible with SFX muted, and vice versa.
  auto routing = a.StateApi();
  std::uint64_t spoken{}, sfx{};
  assert(api.play(api.context, &l, &sfx) == 0);
  assert(routing.play_voice(routing.context, 2, .5F, &spoken) == 0);
  assert(a.Pump(1));
  Near(sink.pcm[0], .375F);
  assert(api.set_bus_gain(api.context, 2, 0) == 0 && a.Pump(1));
  Near(sink.pcm[0], .25F);
  assert(routing.set_voice_bus_gain(routing.context, 0) == 0 && a.Pump(1));
  Near(sink.pcm[0], .125F);
  assert(api.set_bus_gain(api.context, 2, 1) == 0 && a.Pump(1));
  Near(sink.pcm[0], .25F);
  assert(routing.set_voice_bus_gain(routing.context, 1) == 0 && a.Pump(1));
  Near(sink.pcm[0], .375F);
  assert(api.stop(api.context, sfx) == 0 && api.stop(api.context, spoken) == 0);
  const NexoraAudioPlayDesc invalidBus{2, 3, 0, 1};
  std::uint64_t untouched = 123;
  assert(api.play(api.context, &invalidBus, &untouched) == -1 && untouched == 123); // V1 unchanged
  sink.fail = true;
  assert(!a.Pump(1));
  std::uint32_t available = 1;
  assert(api.output_available(api.context, &available) == 0 && !available);
  sink.fail = false;
  a.SetOutput(Sink::Write, &sink);
  assert(a.Pump(1));
  Near(sink.pcm[0], .125F);
  assert(api.stop_all(api.context) == 0 && a.RemoveAsset(1) && a.RemoveAsset(2));
  // Negotiation: original/SP01/SP02/SP03 hosts get complete prefixes and untouched canaries.
  runtime::GameplayServices services;
  services.audio = a.Api();
  services.audio_state = a.StateApi();
  alignas(NexoraGameplayServicesApiV1)
      std::array<std::byte, sizeof(NexoraGameplayServicesApiV1) + 16>
          bytes;
  for (auto size : {std::size_t(NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE),
                    offsetof(NexoraGameplayServicesApiV1, get_ui_draw_api),
                    offsetof(NexoraGameplayServicesApiV1, get_assets_api),
                    offsetof(NexoraGameplayServicesApiV1, get_audio_state_api),
                    sizeof(NexoraGameplayServicesApiV1)}) {
    bytes.fill(std::byte{0x5a});
    const std::uint32_t header[]{std::uint32_t(size), 1};
    std::memcpy(bytes.data(), header, sizeof(header));
    auto *table = reinterpret_cast<NexoraGameplayServicesApiV1 *>(bytes.data());
    assert(services.GetApi(table) == 0 && table->struct_size == size);
    for (auto i = size; i < bytes.size(); ++i)
      assert(bytes[i] == std::byte{0x5a});
  }
  auto stats = a.StateApi();
  NexoraAudioUsageV1 invalid{};
  invalid.struct_size = 1;
  invalid.active_bytes = 123;
  assert(stats.get_usage(stats.context, &invalid) == -2 && invalid.active_bytes == 123);
  int result{};
  std::thread thread([&] { result = stats.get_usage(stats.context, &invalid); });
  thread.join();
  assert(result == -3 && invalid.active_bytes == 123);
  // Include enough churn to fill ASan's default 256 MiB quarantine before comparing
  // equal RSS windows. Keep the growth bound; every round still requires zero live PCM.
  runtime::GameplayAudio large(48000, 4, {4, 32ULL << 20, 48ULL << 20});
  large.SetOutput(Sink::Write, &sink);
  std::vector<float> samples(4 * 1024 * 1024, .125F);
  std::uint64_t warm{}, tail{};
  for (unsigned round = 0; round < 48; ++round) {
    assert(large.RegisterPcm(99, 48000, 1, samples));
    auto play = large.Api();
    NexoraAudioPlayDesc desc{99, 2, 1, 1};
    assert(play.play(play.context, &desc, &voice) == 0 && large.RemoveAsset(99));
    assert(Usage(large).retired_bytes == 16ULL * 1024 * 1024 && large.Pump(64));
    assert(play.stop_all(play.context) == 0);
    auto usage = Usage(large);
    assert(!usage.active_bytes && !usage.retired_bytes);
    if (auto rss = core::CurrentProcessResidentBytes()) {
      if (round < 24)
        warm = std::max(warm, *rss);
      else
        tail = std::max(tail, *rss);
    }
  }
  if (warm && tail) {
    std::cerr << "PCM RSS warm=" << warm << " tail=" << tail << '\n';
    assert(tail <= warm + 32ULL * 1024 * 1024);
  } else
    std::cout << "PCM RSS unavailable\n";
  std::cout << "SP-05 PCM budgets, retained generations, sample/gain/failure recovery, services "
               "prefixes and RSS passed\n";
}
