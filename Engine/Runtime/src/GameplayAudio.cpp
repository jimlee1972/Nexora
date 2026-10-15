#include "Nexora/Runtime/GameplayAudio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>
#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace nexora::runtime {
namespace {
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
  struct Sample {
    std::uint32_t rate{}, channels{};
    std::vector<float> pcm;
  };
  struct Voice {
    std::uint64_t id{};
    std::shared_ptr<const Sample> sample;
    double cursor{};
    std::uint32_t bus{};
    bool loop{}, paused{};
    float gain{};
  };
  GameplayAudio *owner;
  std::uint32_t rate;
  std::vector<Voice> voices;
  std::uint64_t next_id{1};
  std::unordered_map<std::uint64_t, std::shared_ptr<const Sample>> assets;
  std::array<float, 3> gains{1, 1, 1};
  std::array<float, 8192> mix{};
  Sink sink{};
  void *sink_context{};
#if defined(__linux__)
  void *library{}, *device{};
  using Open = int (*)(void **, const char *, int, int);
  using Params = int (*)(void *, int, int, unsigned, unsigned, int, unsigned);
  using Write = long (*)(void *, const void *, unsigned long);
  using Recover = int (*)(void *, int, int);
  using Close = int (*)(void *);
  Write write{};
  Recover recover{};
  Close close{};
#endif
  Impl(GameplayAudio *host, std::uint32_t output_rate, std::uint32_t maximum)
      : owner(host), rate(output_rate), voices(maximum) {}
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
    auto &self = *static_cast<Impl *>(ctx);
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
                 : -1;
    } catch (...) {
      return -3;
    }
  }
  static std::int32_t Remove(void *ctx, std::uint64_t asset) {
    if (!ctx || !asset)
      return -1;
    return static_cast<Impl *>(ctx)->assets.erase(asset) ? 0 : -3;
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
    : impl_(std::make_unique<Impl>(this, rate >= 8000 && rate <= 192000 ? rate : 48000,
                                   std::min(maximum, 4096u))) {}
GameplayAudio::~GameplayAudio() { CloseOutput(); }
bool GameplayAudio::RegisterPcm(std::uint64_t asset, std::uint32_t rate, std::uint32_t channels,
                                std::span<const float> samples) {
  if (!asset || rate < 8000 || rate > 192000 || (channels != 1 && channels != 2) ||
      samples.empty() || samples.size() % channels)
    return false;
  if (!std::all_of(samples.begin(), samples.end(),
                   [](float f) { return std::isfinite(f) && f >= -1 && f <= 1; }))
    return false;
  auto sample = std::make_shared<Impl::Sample>();
  sample->rate = rate;
  sample->channels = channels;
  sample->pcm.assign(samples.begin(), samples.end());
  impl_->assets[asset] = std::move(sample);
  return true;
}
bool GameplayAudio::RegisterWave(std::uint64_t asset, std::span<const std::byte> bytes) {
  if (bytes.size() < 12 || !Tag(bytes.data(), "RIFF") || !Tag(bytes.data() + 8, "WAVE") ||
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
  return RegisterPcm(asset, rate, channels, samples);
}
bool GameplayAudio::RemoveAsset(std::uint64_t asset) { return impl_->assets.erase(asset) != 0; }
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
  if (impl_->library)
    dlclose(impl_->library);
  impl_->library = nullptr;
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
  auto open = reinterpret_cast<Impl::Open>(dlsym(s.library, "snd_pcm_open"));
  auto params = reinterpret_cast<Impl::Params>(dlsym(s.library, "snd_pcm_set_params"));
  s.write = reinterpret_cast<Impl::Write>(dlsym(s.library, "snd_pcm_writei"));
  s.recover = reinterpret_cast<Impl::Recover>(dlsym(s.library, "snd_pcm_recover"));
  s.close = reinterpret_cast<Impl::Close>(dlsym(s.library, "snd_pcm_close"));
  if (!open || !params || !s.write || !s.recover || !s.close ||
      open(&s.device, "default", 0, 0) < 0) {
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
