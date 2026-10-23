#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayAssets.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
extern "C" int32_t NexoraGameModuleLoad(uint32_t, NexoraGameModuleV3 *);
extern "C" int32_t NexoraDialogueControl(void *, uint32_t, uint32_t, float);
using namespace nexora;
struct Host {
  runtime::GameplayServices services;
  unsigned allocated{}, freed{};
};
struct Sink {
  bool fail{};
  std::array<float, 8192> samples{};
  static bool Write(void *p, const float *v, uint32_t n, uint32_t rate) {
    auto &s = *static_cast<Sink *>(p);
    assert(rate == 48000 && n <= 4096);
    std::copy_n(v, n * 2, s.samples.begin());
    return !s.fail;
  }
};
void Near(float a, float b) { assert(std::abs(a - b) < .00001F); }
int main() {
  runtime::GameplayAudio audio(48000, 8, {8, 32ULL << 20, 48ULL << 20});
  Sink sink;
  audio.SetOutput(Sink::Write, &sink);
  // Exact PCM16 [.25,.5,.75,.25], long enough to observe paused/resumed cursor and explicit replay.
  const unsigned char wav[]{'R', 'I', 'F', 'F',  44, 0, 0,    0, 'W',  'A', 'V',  'E',  'f',
                            'm', 't', ' ', 16,   0,  0, 0,    1, 0,    1,   0,    0x80, 0xbb,
                            0,   0,   0,   0x77, 1,  0, 2,    0, 16,   0,   'd',  'a',  't',
                            'a', 8,   0,   0,    0,  0, 0x20, 0, 0x40, 0,   0x60, 0,    0x20};
  const auto source = std::as_bytes(std::span(wav));
  twod::GameplayContent content;
  twod::GameplayAssets files(
      content, {{3, source.size(), "good"}, {3, 3, "corrupt"}, {3, source.size(), "missing"}},
      [&](const auto &f) {
        if (f.path == "missing")
          throw std::runtime_error("missing fixture WAV");
        if (f.path == "corrupt")
          return twod::Bytes(3, std::byte{0});
        return twod::Bytes(source.begin(), source.end());
      });
  Host context;
  context.services.audio = audio.Api();
  context.services.audio_state = audio.StateApi();
  context.services.assets.struct_size = sizeof(context.services.assets);
  context.services.assets.abi_version = 1;
  assert(files.GetApi(&context.services.assets) == 0);
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = 3;
  host.context = &context;
  host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API;
  host.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *out) {
    return static_cast<Host *>(p)->services.GetApi(out);
  };
  host.allocate = [](void *p, uint64_t, uint64_t n, uint64_t align) -> void * {
    ++static_cast<Host *>(p)->allocated;
    return ::operator new(n, std::align_val_t(align));
  };
  host.deallocate = [](void *p, uint64_t, void *v, uint64_t, uint64_t align) {
    ++static_cast<Host *>(p)->freed;
    ::operator delete(v, std::align_val_t(align));
  };
  NexoraGameModuleV3 module{};
  module.struct_size = sizeof(module);
  assert(NexoraGameModuleLoad(3, &module) == 0);
  void *state{};
  assert(module.create(&state, &host) == 0 && module.on_start(state) == 0);
  auto command = [&](uint32_t kind, uint32_t arg = 0, float value = 0) {
    return NexoraDialogueControl(state, kind, arg, value);
  };
  assert(command(0) ==
         0); // manual diagnostic control; the standalone module also has an auto workload
  auto tick = [&] {
    files.Tick();
    assert(module.update(state, 1.0 / 60) == 0);
  };
  auto wait = [&](int phase) {
    for (unsigned n = 0; n < 5000 && command(5) == 1; ++n) {
      tick();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(command(5) == phase);
  };
  auto usage = [&] {
    NexoraAudioUsageV1 u{};
    u.struct_size = sizeof(u);
    auto a = audio.StateApi();
    assert(a.get_usage(a.context, &u) == 0);
    return u;
  };
  const std::array<float, 1> music{.125F};
  assert(audio.RegisterPcm(1, 48000, 1, music));
  auto api = audio.Api();
  std::uint64_t bgm{};
  const NexoraAudioPlayDesc desc{1, 1, 1, 1};
  assert(api.play(api.context, &desc, &bgm) == 0);
  wait(2);
  assert(audio.Pump(1));
  Near(sink.samples[0], .375F);
  assert(command(2, 0, .5F) == 0 && command(3, 1) == 0 && audio.Pump(2));
  Near(sink.samples[0], .125F);
  assert(command(3) == 0 && command(2, 1, .5F) == 0 && audio.Pump(1));
  Near(sink.samples[0], .125F);
  assert(command(2, 0, .5F) == 0 && audio.Pump(1));
  Near(sink.samples[0], .5F);
  assert(command(2, 0, -1) == -1); // invalid settings do not mutate gain
  assert(command(4) == 0 && audio.Pump(1));
  Near(sink.samples[0], .125F);
  assert(usage().active_bytes == 4 && usage().retired_bytes == 0 && usage().active_voices == 1);
  assert(command(2, 0, 1) == 0 && command(1) == 0);
  wait(2);
  assert(audio.Pump(1));
  Near(sink.samples[0], .375F);
  sink.fail = true;
  assert(!audio.Pump(1));
  tick();
  assert(command(5) == 4 && usage().active_bytes == 4);
  sink.fail = false;
  audio.SetOutput(Sink::Write, &sink);
  assert(command(1) == 0);
  wait(2);
  assert(audio.Pump(1));
  Near(sink.samples[0], .375F); // explicit recovery/replay starts at the first sample
  for (auto index : {1U, 2U, 0xffffffffU}) {
    assert(command(1, index) == 0);
    wait(4);
    assert(usage().active_bytes == 4);
  }
  assert(command(1) == 0);
  wait(2);
  assert(audio.Pump(4));
  tick();
  assert(command(5) == 3 && usage().active_bytes == 4);
  // Cancel in-flight IO and immediately change chapter; no stale registration may publish later.
  assert(command(1) == 0 && command(1, 2) == 0);
  wait(4);
  assert(command(4) == 0);
  audio.CloseOutput();
  assert(command(1) == 0);
  wait(4);
  assert(usage().active_bytes == 4);
  module.on_stop(state);
  module.destroy(state);
  assert(context.allocated == context.freed);
  for (unsigned i = 0; i < 100; ++i) {
    files.Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  NexoraAssetUsageV1 resident{};
  resident.struct_size = sizeof(resident);
  assert(context.services.assets.get_usage(context.services.assets.context, &resident) == 0 &&
         resident.staging_bytes == 0 && resident.requests == 0);
  files.Shutdown();
  assert(api.stop_all(api.context) == 0 && audio.RemoveAsset(1));
  assert(usage().active_bytes == 0 && usage().retired_bytes == 0 && usage().active_voices == 0);
  auto old = host;
  old.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *out) {
    out->struct_size = offsetof(NexoraGameplayServicesApiV1, get_audio_state_api);
    return static_cast<Host *>(p)->services.GetApi(out);
  };
  state = nullptr;
  assert(module.create(&state, &old) == -2 && state == nullptr &&
         context.allocated == context.freed);
  std::cout
      << "SP-05 Zig actual PCM: independent voice gain/mute, paused cursor, skip/replay, "
         "missing/corrupt lines, sink failure/reopen, cancellation and old-host rejection passed\n";
}
