#include "Nexora/Runtime/GameplayAudio.h"
#include "GameplayAudioQueue.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>
#if defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mmsystem.h>
#endif

namespace nexora::runtime {
namespace {
#if defined(_WIN32)
template <typename Function> Function LoadWaveFunction(HMODULE library, const char *name) {
  const auto address = GetProcAddress(library, name);
  static_assert(sizeof(Function) == sizeof(address));
  Function result{};
  std::memcpy(&result, &address, sizeof(result));
  return result;
}
#endif
bool ValidGain(float gain) { return std::isfinite(gain) && gain >= 0 && gain <= 16; }
std::uint16_t U16(const std::byte *p) {
  return static_cast<std::uint16_t>(std::to_integer<unsigned>(p[0]) |
                                    (std::to_integer<unsigned>(p[1]) << 8));
}
std::uint32_t U32(const std::byte *p) {
  return U16(p) | (static_cast<std::uint32_t>(U16(p + 2)) << 16);
}
bool Tag(const std::byte *p, const char *s) { return std::memcmp(p, s, 4) == 0; }
} // namespace
struct GameplayAudio::Impl {
  struct Residency {
    std::uint64_t live{}, peak{};
    std::uint32_t generations{};
  };
  struct Sample {
    std::uint32_t rate{}, channels{};
    std::vector<float> pcm;
    std::shared_ptr<Residency> residency;
    Sample(std::uint32_t r, std::uint32_t c, std::vector<float> samples,
           std::shared_ptr<Residency> owner)
        : rate(r), channels(c), pcm(std::move(samples)), residency(std::move(owner)) {
      residency->live += pcm.size() * sizeof(float);
      residency->peak = std::max(residency->peak, residency->live);
      ++residency->generations;
    }
    ~Sample() {
      residency->live -= pcm.size() * sizeof(float);
      --residency->generations;
    }
  };
  struct Voice {
    std::uint64_t id{};
    std::shared_ptr<const Sample> sample;
    double cursor{};
    std::uint32_t bus{};
    bool loop{}, paused{};
    float gain{};
  };
  Limits limits;
  const std::thread::id thread{std::this_thread::get_id()};
  std::shared_ptr<Residency> residency{std::make_shared<Residency>()};
  std::uint64_t active_bytes{};
  int registration_result{-1};
  bool Admit(std::uint64_t asset, std::uint64_t bytes) {
    const auto old = assets.find(asset);
    const auto previous = old == assets.end() ? 0 : old->second->pcm.size() * sizeof(float);
    if ((old == assets.end() && assets.size() >= limits.assets) ||
        bytes > limits.active_bytes - (active_bytes - previous) ||
        bytes > limits.live_bytes - residency->live) {
      registration_result = -3;
      return false;
    }
    return true;
  }
  bool Publish(std::uint64_t asset, std::uint32_t sample_rate, std::uint32_t channels,
               std::vector<float> samples) {
    const auto old = assets.find(asset);
    const auto previous = old == assets.end() ? 0 : old->second->pcm.size() * sizeof(float);
    const auto bytes = samples.size() * sizeof(float);
    auto sample = std::make_shared<Sample>(sample_rate, channels, std::move(samples), residency);
    assets.insert_or_assign(asset, std::move(sample));
    active_bytes = active_bytes - previous + bytes;
    registration_result = 0;
    return true;
  }
  bool RemoveAsset(std::uint64_t asset) {
    const auto old = assets.find(asset);
    if (old == assets.end())
      return false;
    active_bytes -= old->second->pcm.size() * sizeof(float);
    assets.erase(old);
    return true;
  }
  static std::int32_t Usage(void *context, NexoraAudioUsageV1 *out) {
    if (!context || !out)
      return -1;
    auto &self = *static_cast<Impl *>(context);
    if (std::this_thread::get_id() != self.thread)
      return -3;
    if (out->struct_size < sizeof(*out))
      return -2;
    const auto voices = std::count_if(self.voices.begin(), self.voices.end(),
                                      [](const auto &v) { return v.id != 0; });
    *out = {sizeof(*out),
            static_cast<std::uint32_t>(self.assets.size()),
            static_cast<std::uint32_t>(voices),
            self.residency->generations - static_cast<std::uint32_t>(self.assets.size()),
            self.active_bytes,
            self.residency->live - self.active_bytes,
            self.residency->peak,
            self.limits.active_bytes,
            self.limits.live_bytes,
            self.limits.assets,
            0};
    return 0;
  }
  GameplayAudio *owner;
  std::uint32_t rate;
  std::vector<Voice> voices;
  std::uint64_t next_id{1};
  std::unordered_map<std::uint64_t, std::shared_ptr<const Sample>> assets;
  std::array<float, 4> gains{1, 1, 1, 1};
  std::array<float, 8192> mix{};
  Sink sink{};
  void *sink_context{};
#if defined(__linux__)
  void *library{}, *device{};
  using Open = int (*)(void **, const char *, int, int, void *);
  void *config{}, *config_update{};
  using UpdateConfig = int (*)(void **, void **, const char *);
  using FreeConfig = int (*)(void *);
  FreeConfig delete_config{}, free_update{};
  using Params = int (*)(void *, int, int, unsigned, unsigned, int, unsigned);
  using Write = long (*)(void *, const void *, unsigned long);
  using Recover = int (*)(void *, int, int);
  using Close = int (*)(void *);
  Write write{};
  Recover recover{};
  Close close{};
#elif defined(_WIN32)
  HMODULE library{};
  HWAVEOUT device{};
  HANDLE completed{};
  detail::GameplayAudioQueue queue;
  std::array<WAVEHDR, detail::GameplayAudioQueue::Slots> headers{};
  std::array<bool, detail::GameplayAudioQueue::Slots> prepared{};
  decltype(&waveOutOpen) open{};
  decltype(&waveOutPrepareHeader) prepare{};
  decltype(&waveOutWrite) write{};
  decltype(&waveOutReset) reset{};
  decltype(&waveOutUnprepareHeader) unprepare{};
  decltype(&waveOutClose) close{};
#endif
  Impl(GameplayAudio *host, std::uint32_t output_rate, std::uint32_t maximum, Limits budget)
      : limits(budget), owner(host), rate(output_rate), voices(maximum) {
    if (!limits.assets || limits.assets > 4096 || !limits.active_bytes ||
        limits.active_bytes > 1024ULL * 1024 * 1024 || limits.live_bytes < limits.active_bytes ||
        limits.live_bytes > 2ULL * 1024 * 1024 * 1024)
      throw std::invalid_argument("invalid PCM residency limits");
  }
  Voice *Find(std::uint64_t id) {
    if (!id)
      return nullptr;
    for (auto &v : voices)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  static std::int32_t Play(void *ctx, const NexoraAudioPlayDesc *d, std::uint64_t *out) {
    if (!ctx || !d || !out || !d->asset || d->bus < 1 || d->bus > 2 || d->loop > 1 ||
        !ValidGain(d->gain))
      return -1;
    return PlayInternal(*static_cast<Impl *>(ctx), d, out);
  }
  static std::int32_t PlayVoice(void *ctx, std::uint64_t asset, float gain, std::uint64_t *out) {
    if (!ctx || !asset || !out || !ValidGain(gain))
      return -1;
    if (std::this_thread::get_id() != static_cast<Impl *>(ctx)->thread)
      return -3;
    const NexoraAudioPlayDesc desc{asset, 3, 0, gain};
    return PlayInternal(*static_cast<Impl *>(ctx), &desc, out);
  }
  static std::int32_t VoiceBus(void *ctx, float gain) {
    if (!ctx || !ValidGain(gain))
      return -1;
    if (std::this_thread::get_id() != static_cast<Impl *>(ctx)->thread)
      return -3;
    static_cast<Impl *>(ctx)->gains[3] = gain;
    return 0;
  }
  static std::int32_t PlayInternal(Impl &self, const NexoraAudioPlayDesc *d, std::uint64_t *out) {
    if (!self.sink)
      return -2;
    auto sample = self.assets.find(d->asset);
    if (sample == self.assets.end() || self.next_id == std::numeric_limits<std::uint64_t>::max())
      return -3;
    for (auto &v : self.voices)
      if (!v.id) {
        v = {self.next_id++, sample->second, 0, d->bus, d->loop != 0, false, d->gain};
        *out = v.id;
        return 0;
      }
    return -3;
  }
  static std::int32_t Stop(void *ctx, std::uint64_t id) {
    if (!ctx || !id)
      return -1;
    auto *v = static_cast<Impl *>(ctx)->Find(id);
    if (!v)
      return -3;
    *v = {};
    return 0;
  }
  static std::int32_t Pause(void *ctx, std::uint64_t id, std::uint32_t paused) {
    if (!ctx || !id || paused > 1)
      return -1;
    auto *v = static_cast<Impl *>(ctx)->Find(id);
    if (!v)
      return -3;
    v->paused = paused != 0;
    return 0;
  }
  static std::int32_t Gain(void *ctx, std::uint64_t id, float gain) {
    if (!ctx || !id || !ValidGain(gain))
      return -1;
    auto *v = static_cast<Impl *>(ctx)->Find(id);
    if (!v)
      return -3;
    v->gain = gain;
    return 0;
  }
  static std::int32_t Bus(void *ctx, std::uint32_t bus, float gain) {
    if (!ctx || bus > 2 || !ValidGain(gain))
      return -1;
    static_cast<Impl *>(ctx)->gains[bus] = gain;
    return 0;
  }
  static std::int32_t Playing(void *ctx, std::uint64_t id, std::uint32_t *out) {
    if (!ctx || !id || !out)
      return -1;
    *out = static_cast<Impl *>(ctx)->Find(id) ? 1u : 0u;
    return 0;
  }
  static std::int32_t Available(void *ctx, std::uint32_t *out) {
    if (!ctx || !out)
      return -1;
    *out = static_cast<Impl *>(ctx)->sink ? 1u : 0u;
    return 0;
  }
  static std::int32_t Register(void *ctx, std::uint64_t asset, const void *bytes,
                               std::uint32_t size) {
    if (!ctx || !asset || !bytes || !size || size > 64u * 1024u * 1024u)
      return -1;
    try {
      return static_cast<Impl *>(ctx)->owner->RegisterWave(
                 asset, {static_cast<const std::byte *>(bytes), size})
                 ? 0
                 : static_cast<Impl *>(ctx)->registration_result;
    } catch (...) {
      return -3;
    }
  }
  static std::int32_t Remove(void *ctx, std::uint64_t asset) {
    if (!ctx || !asset)
      return -1;
    return static_cast<Impl *>(ctx)->RemoveAsset(asset) ? 0 : -3;
  }
  static std::int32_t StopAll(void *ctx) {
    if (!ctx)
      return -1;
    for (auto &v : static_cast<Impl *>(ctx)->voices)
      v = {};
    return 0;
  }
};
GameplayAudio::GameplayAudio(std::uint32_t rate, std::uint32_t maximum)
    : GameplayAudio(rate, maximum, Limits{}) {}
GameplayAudio::GameplayAudio(std::uint32_t rate, std::uint32_t maximum, Limits limits)
    : impl_(std::make_unique<Impl>(this, rate >= 8000 && rate <= 192000 ? rate : 48000,
                                   std::min(maximum, 4096u), limits)) {}
GameplayAudio::~GameplayAudio() { CloseOutput(); }
bool GameplayAudio::RegisterPcm(std::uint64_t asset, std::uint32_t rate, std::uint32_t channels,
                                std::span<const float> samples) {
  impl_->registration_result = -1;
  if (!asset || rate < 8000 || rate > 192000 || (channels != 1 && channels != 2) ||
      samples.empty() || samples.size() % channels)
    return false;
  if (!std::all_of(samples.begin(), samples.end(),
                   [](float f) { return std::isfinite(f) && f >= -1 && f <= 1; }))
    return false;
  if (!impl_->Admit(asset, samples.size_bytes()))
    return false;
  return impl_->Publish(asset, rate, channels, std::vector<float>(samples.begin(), samples.end()));
}
bool GameplayAudio::RegisterWave(std::uint64_t asset, std::span<const std::byte> bytes) {
  impl_->registration_result = -1;
  if (!asset || bytes.size() > 64ULL * 1024 * 1024 || bytes.size() < 12 ||
      !Tag(bytes.data(), "RIFF") || !Tag(bytes.data() + 8, "WAVE") ||
      U32(bytes.data() + 4) != bytes.size() - 8)
    return false;
  std::span<const std::byte> fmt, data;
  std::size_t offset = 12;
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 8)
      return false;
    const auto size = U32(bytes.data() + offset + 4);
    if (size > bytes.size() - offset - 8)
      return false;
    auto chunk = bytes.subspan(offset + 8, size);
    if (Tag(bytes.data() + offset, "fmt ")) {
      if (!fmt.empty())
        return false;
      fmt = chunk;
    }
    if (Tag(bytes.data() + offset, "data")) {
      if (!data.empty())
        return false;
      data = chunk;
    }
    offset += 8 + static_cast<std::size_t>(size);
    if (size & 1) {
      if (offset == bytes.size())
        return false;
      ++offset;
    }
  }
  if (fmt.size() < 16 || data.empty())
    return false;
  const auto format = U16(fmt.data()), channels = U16(fmt.data() + 2), bits = U16(fmt.data() + 14);
  const auto rate = U32(fmt.data() + 4);
  if ((channels != 1 && channels != 2) ||
      !((format == 1 && bits == 16) || (format == 3 && bits == 32)))
    return false;
  const auto alignment = static_cast<std::uint32_t>(channels * bits / 8);
  if (U16(fmt.data() + 12) != alignment ||
      static_cast<std::uint64_t>(rate) * alignment != U32(fmt.data() + 8) ||
      data.size() % alignment)
    return false;
  if (rate < 8000 || rate > 192000 ||
      !impl_->Admit(asset, data.size() / (bits / 8) * sizeof(float)))
    return false;
  std::vector<float> samples(data.size() / (bits / 8));
  for (std::size_t i = 0; i < samples.size(); ++i) {
    if (bits == 16) {
      const auto n = U16(data.data() + i * 2);
      samples[i] = static_cast<float>(n >= 32768 ? static_cast<int>(n) - 65536 : n) / 32768.f;
    } else {
      const auto n = U32(data.data() + i * 4);
      std::memcpy(&samples[i], &n, 4);
    }
  }
  if (!std::all_of(samples.begin(), samples.end(),
                   [](float f) { return std::isfinite(f) && f >= -1 && f <= 1; }))
    return false;
  return impl_->Publish(asset, rate, channels, std::move(samples));
}
bool GameplayAudio::RemoveAsset(std::uint64_t asset) { return impl_->RemoveAsset(asset); }
void GameplayAudio::SetOutput(Sink sink, void *context) {
  CloseOutput();
  impl_->sink = sink;
  impl_->sink_context = context;
}
void GameplayAudio::CloseOutput() {
  impl_->sink = nullptr;
  impl_->sink_context = nullptr;
#if defined(__linux__)
  if (impl_->device && impl_->close)
    impl_->close(impl_->device);
  impl_->device = nullptr;
  if (impl_->config)
    impl_->delete_config(impl_->config);
  impl_->config = nullptr;
  if (impl_->config_update)
    impl_->free_update(impl_->config_update);
  impl_->config_update = nullptr;
  if (impl_->library)
    dlclose(impl_->library);
  impl_->library = nullptr;
#elif defined(_WIN32)
  auto &s = *impl_;
  if (s.device) {
    // Reset returns pending buffers before the header/storage/event can expire.
    (void)s.reset(s.device);
    for (std::size_t i = 0; i < s.headers.size(); ++i)
      if (s.prepared[i])
        (void)s.unprepare(s.device, &s.headers[i], sizeof(WAVEHDR));
    (void)s.close(s.device);
  }
  s.device = nullptr;
  s.prepared = {};
  s.headers = {};
  s.queue.Reset();
  if (s.completed)
    CloseHandle(s.completed);
  s.completed = nullptr;
  if (s.library)
    FreeLibrary(s.library);
  s.library = nullptr;
#endif
}
bool GameplayAudio::OpenDefaultOutput(std::string &error) {
  CloseOutput();
#if defined(__linux__)
  auto &s = *impl_;
  s.library = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
  if (!s.library) {
    error = "ALSA library unavailable";
    return false;
  }
  auto open = reinterpret_cast<Impl::Open>(dlsym(s.library, "snd_pcm_open_lconf"));
  auto params = reinterpret_cast<Impl::Params>(dlsym(s.library, "snd_pcm_set_params"));
  s.write = reinterpret_cast<Impl::Write>(dlsym(s.library, "snd_pcm_writei"));
  s.recover = reinterpret_cast<Impl::Recover>(dlsym(s.library, "snd_pcm_recover"));
  s.close = reinterpret_cast<Impl::Close>(dlsym(s.library, "snd_pcm_close"));
  auto update_config =
      reinterpret_cast<Impl::UpdateConfig>(dlsym(s.library, "snd_config_update_r"));
  s.delete_config = reinterpret_cast<Impl::FreeConfig>(dlsym(s.library, "snd_config_delete"));
  s.free_update = reinterpret_cast<Impl::FreeConfig>(dlsym(s.library, "snd_config_update_free"));
  // Use an owner-local root and file-update record. snd_pcm_open instead populates
  // ALSA's global cache, which outlives PCM close and can leak when we dlclose.
  // Never free that global cache: unrelated ALSA clients may still own it.
  if (!open || !params || !s.write || !s.recover || !s.close || !update_config ||
      !s.delete_config || !s.free_update ||
      update_config(&s.config, &s.config_update, nullptr) < 0 || !s.config ||
      open(&s.device, "default", 0, 0, s.config) < 0) {
    error = "ALSA default output unavailable";
    CloseOutput();
    return false;
  }
  // ALSA stable enum values: FLOAT_LE=14, RW_INTERLEAVED=3.
  if (params(s.device, 14, 3, 2, s.rate, 1, 50000) < 0) {
    error = "ALSA stereo float output unsupported";
    CloseOutput();
    return false;
  }
  s.sink_context = &s;
  s.sink = [](void *ctx, const float *pcm, std::uint32_t frames, std::uint32_t) {
    auto &self = *static_cast<Impl *>(ctx);
    std::uint32_t sent = 0;
    unsigned recoveries = 0;
    while (sent < frames) {
      const auto count = self.write(self.device, pcm + sent * 2, frames - sent);
      if (count < 0) {
        if (++recoveries > 2 || self.recover(self.device, static_cast<int>(count), 1) < 0)
          return false;
      } else if (!count || static_cast<unsigned long>(count) > frames - sent)
        return false;
      else
        sent += static_cast<std::uint32_t>(count);
    }
    return true;
  };
  error.clear();
  return true;
#elif defined(_WIN32)
  auto &s = *impl_;
  // Load only the OS copy; never search the working directory for a device DLL.
  s.library = LoadLibraryExW(L"winmm.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!s.library) {
    error = "Windows wave output library unavailable";
    return false;
  }
  s.open = LoadWaveFunction<decltype(s.open)>(s.library, "waveOutOpen");
  s.prepare = LoadWaveFunction<decltype(s.prepare)>(s.library, "waveOutPrepareHeader");
  s.write = LoadWaveFunction<decltype(s.write)>(s.library, "waveOutWrite");
  s.reset = LoadWaveFunction<decltype(s.reset)>(s.library, "waveOutReset");
  s.unprepare = LoadWaveFunction<decltype(s.unprepare)>(s.library, "waveOutUnprepareHeader");
  s.close = LoadWaveFunction<decltype(s.close)>(s.library, "waveOutClose");
  if (!s.open || !s.prepare || !s.write || !s.reset || !s.unprepare || !s.close) {
    error = "Windows wave output entry points unavailable";
    CloseOutput();
    return false;
  }
  s.completed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!s.completed) {
    error = "Windows wave output event unavailable";
    CloseOutput();
    return false;
  }
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = s.rate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = 4;
  format.nAvgBytesPerSec = s.rate * format.nBlockAlign;
  const auto opened = s.open(&s.device, WAVE_MAPPER, &format,
                             reinterpret_cast<DWORD_PTR>(s.completed), 0, CALLBACK_EVENT);
  if (opened != MMSYSERR_NOERROR) {
    // No usable handle is owned after an unsuccessful open.
    s.device = nullptr;
    error = "Windows default stereo PCM output unavailable: " + std::to_string(opened);
    CloseOutput();
    return false;
  }
  for (std::size_t i = 0; i < s.headers.size(); ++i) {
    auto &header = s.headers[i];
    const auto buffer = s.queue.Buffer(i);
    header.lpData = reinterpret_cast<LPSTR>(buffer.data());
    header.dwBufferLength = static_cast<DWORD>(buffer.size_bytes());
    const auto prepared = s.prepare(s.device, &header, sizeof(header));
    if (prepared != MMSYSERR_NOERROR) {
      error = "Windows wave output buffer preparation failed: " + std::to_string(prepared);
      CloseOutput();
      return false;
    }
    s.prepared[i] = true;
  }
  s.sink_context = &s;
  s.sink = [](void *ctx, const float *pcm, std::uint32_t frames, std::uint32_t) {
    auto &self = *static_cast<Impl *>(ctx);
    const detail::GameplayAudioQueue::Driver driver{
        &self,
        [](void *p, std::size_t slot) {
          return (static_cast<Impl *>(p)->headers[slot].dwFlags & WHDR_INQUEUE) != 0;
        },
        [](void *p, std::size_t slot, std::uint32_t count) {
          auto &output = *static_cast<Impl *>(p);
          auto &header = output.headers[slot];
          // Prepared capacity is fixed; each submitted block may be shortened.
          header.dwBufferLength = count * 4;
          return output.write(output.device, &header, sizeof(header)) == MMSYSERR_NOERROR;
        },
        [](void *p, std::uint32_t milliseconds) {
          return WaitForSingleObject(static_cast<Impl *>(p)->completed, milliseconds) ==
                 WAIT_OBJECT_0;
        }};
    return self.queue.Write({pcm, frames * 2}, frames, driver);
  };
  error.clear();
  return true;
#else
  error = "No built-in audio output backend on this platform; supply a host sink";
  return false;
#endif
}
bool GameplayAudio::Pump(std::uint32_t frames) {
  auto &s = *impl_;
  if (!s.sink || !frames || frames > 4096)
    return false;
  std::fill_n(s.mix.begin(), frames * 2, 0.f);
  for (auto &v : s.voices) {
    if (!v.id || v.paused)
      continue;
    const auto &sample = *v.sample;
    const auto count = sample.pcm.size() / sample.channels;
    const double step = static_cast<double>(sample.rate) / s.rate;
    const float gain = v.gain * s.gains[0] * s.gains[v.bus];
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
      if (v.cursor >= static_cast<double>(count)) {
        if (!v.loop)
          break;
        v.cursor = std::fmod(v.cursor, static_cast<double>(count));
      }
      const auto index = static_cast<std::size_t>(v.cursor);
      for (std::size_t channel = 0; channel < 2; ++channel)
        s.mix[frame * 2 + channel] +=
            sample.pcm[index * sample.channels + (sample.channels == 1 ? 0 : channel)] * gain;
      v.cursor += step;
    }
    if (!v.loop && v.cursor >= static_cast<double>(count))
      v = {};
  }
  for (std::size_t i = 0; i < frames * 2; ++i)
    s.mix[i] = std::clamp(s.mix[i], -1.f, 1.f);
  if (!s.sink(s.sink_context, s.mix.data(), frames, s.rate)) {
    CloseOutput();
    return false;
  }
  return true;
}
NexoraGameplayAudioStateApiV1 GameplayAudio::StateApi() noexcept {
  return {sizeof(NexoraGameplayAudioStateApiV1),
          1,
          impl_.get(),
          Impl::Usage,
          Impl::PlayVoice,
          Impl::VoiceBus};
}
NexoraGameplayAudioApiV1 GameplayAudio::Api() noexcept {
  return {sizeof(NexoraGameplayAudioApiV1),
          NEXORA_GAMEPLAY_AUDIO_API_VERSION,
          impl_.get(),
          Impl::Play,
          Impl::Stop,
          Impl::Pause,
          Impl::Gain,
          Impl::Bus,
          Impl::Playing,
          Impl::Available,
          Impl::StopAll,
          Impl::Register,
          Impl::Remove};
}
} // namespace nexora::runtime
