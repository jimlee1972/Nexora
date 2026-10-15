#pragma once
#include "Nexora/Runtime/Api.h"
#include "nexora/gameplay_audio.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace nexora::runtime {
// All calls, including destruction, must be serialized on the owner thread.
// Sink callbacks must consume/copy the borrowed stereo samples synchronously.
class NEXORA_RUNTIME_API GameplayAudio final {
public:
  using Sink = bool (*)(void *, const float *, std::uint32_t frames, std::uint32_t sample_rate);
  explicit GameplayAudio(std::uint32_t sample_rate = 48000, std::uint32_t maximum_voices = 64);
  ~GameplayAudio();
  GameplayAudio(const GameplayAudio &) = delete;
  GameplayAudio &operator=(const GameplayAudio &) = delete;
  // Copies mono or stereo interleaved normalized finite samples. Active voices
  // retain their original sample generation if the asset is replaced/removed.
  bool RegisterPcm(std::uint64_t asset, std::uint32_t sample_rate, std::uint32_t channels,
                   std::span<const float> samples);
  // Strict little-endian RIFF/WAVE PCM16 mono/stereo or IEEE float32 input.
  bool RegisterWave(std::uint64_t asset, std::span<const std::byte> bytes);
  bool RemoveAsset(std::uint64_t asset);
  void SetOutput(Sink sink, void *context);
  // Linux ALSA default device, dynamically loaded. Fails honestly on other
  // platforms or when no output is available; an application may supply a sink.
  bool OpenDefaultOutput(std::string &error);
  void CloseOutput();
  // Mixes and submits up to 4096 stereo frames. No Zig callbacks or asset IO.
  // Failure disables output; play then returns unsupported until reconfigured.
  bool Pump(std::uint32_t frames);
  [[nodiscard]] NexoraGameplayAudioApiV1 Api() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace nexora::runtime
