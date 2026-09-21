#pragma once

#include "Nexora/Runtime/Api.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexora::runtime::presentation {

using ResourceId = std::uint64_t;

struct Vec3 final {
  float x{}, y{}, z{};
  friend bool operator==(const Vec3 &, const Vec3 &) = default;
};

struct Joint final {
  std::int32_t parent{-1};
  std::string name;
};

class NEXORA_RUNTIME_API Skeleton final {
public:
  bool Build(std::vector<Joint> joints);
  [[nodiscard]] std::size_t JointCount() const noexcept { return joints_.size(); }

private:
  std::vector<Joint> joints_;
};

struct TranslationKey final {
  float time{};
  Vec3 value{};
};
struct AnimationTrack final {
  std::size_t joint{};
  std::vector<TranslationKey> translations;
};
struct AnimationAsset final {
  ResourceId id{};
  float duration{};
  bool looping{};
  std::vector<AnimationTrack> tracks;
};
struct AnimationPose final {
  std::vector<Vec3> translations;
  Vec3 root_motion{};
};

class NEXORA_RUNTIME_API AnimationGraph final {
public:
  bool SetSkeleton(Skeleton skeleton);
  bool AddClip(AnimationAsset clip);
  bool Play(ResourceId clip, float blend_seconds = 0.0F);
  [[nodiscard]] AnimationPose Update(float seconds);
  [[nodiscard]] ResourceId ActiveClip() const noexcept { return active_; }

private:
  [[nodiscard]] AnimationPose Sample(ResourceId clip, float time) const;
  Skeleton skeleton_;
  std::unordered_map<ResourceId, AnimationAsset> clips_;
  ResourceId active_{}, previous_{};
  float time_{}, previous_time_{}, blend_duration_{}, blend_time_{};
};

class NEXORA_RUNTIME_API SkinningPalette final {
public:
  bool Upload(std::span<const std::array<float, 16>> matrices);
  [[nodiscard]] std::size_t MatrixCount() const noexcept { return matrices_.size(); }
  [[nodiscard]] std::span<const std::array<float, 16>> Matrices() const noexcept {
    return matrices_;
  }

private:
  std::vector<std::array<float, 16>> matrices_;
};

class NEXORA_RUNTIME_API ResidencyTracker final {
public:
  bool Acquire(ResourceId resource);
  bool Release(ResourceId resource);
  [[nodiscard]] bool Resident(ResourceId resource) const;
  [[nodiscard]] std::size_t References(ResourceId resource) const;

private:
  std::unordered_map<ResourceId, std::size_t> references_;
};

struct AudioEvent final {
  ResourceId resource{};
  std::string bus{"Master"};
  float gain{1.0F};
  bool streaming{};
};
class NEXORA_RUNTIME_API AudioEngine final {
public:
  AudioEngine(std::size_t voice_limit, ResidencyTracker &residency)
      : voice_limit_(voice_limit), residency_(residency) {}
  bool SetBusGain(std::string bus, float gain);
  [[nodiscard]] std::optional<std::uint64_t> Play(const AudioEvent &event);
  bool Stop(std::uint64_t voice);
  [[nodiscard]] float EffectiveGain(std::uint64_t voice) const;
  [[nodiscard]] std::size_t ActiveVoices() const noexcept { return voices_.size(); }

private:
  struct Voice final {
    std::uint64_t id{};
    AudioEvent event;
  };
  std::size_t voice_limit_{};
  std::uint64_t next_voice_{1};
  ResidencyTracker &residency_;
  std::unordered_map<std::string, float> buses_{{"Master", 1.0F}};
  std::vector<Voice> voices_;
};

enum class ParticleRenderer { Sprite, Mesh, Trail };
struct ParticleSpawn final {
  Vec3 position{}, velocity{};
  float lifetime{1.0F};
};
class NEXORA_RUNTIME_API ParticleSystem final {
public:
  ParticleSystem(std::size_t capacity, ParticleRenderer renderer)
      : capacity_(capacity), renderer_(renderer) {}
  bool Spawn(const ParticleSpawn &particle);
  void Update(float seconds);
  [[nodiscard]] std::size_t Count() const noexcept { return ages_.size(); }
  [[nodiscard]] ParticleRenderer Renderer() const noexcept { return renderer_; }

private:
  std::size_t capacity_{};
  ParticleRenderer renderer_{};
  std::vector<Vec3> positions_, velocities_;
  std::vector<float> ages_, lifetimes_;
};

struct DecodedVideoFrame final {
  std::uint64_t sequence{};
  double presentation_time{};
  ResourceId texture{};
};
struct SubtitleCue final {
  double begin{}, end{};
  std::string text;
};
class NEXORA_RUNTIME_API VideoPlayer final {
public:
  explicit VideoPlayer(std::size_t decoded_capacity) : capacity_(decoded_capacity) {}
  bool SubmitDecoded(DecodedVideoFrame frame);
  void Seek(double time);
  [[nodiscard]] std::optional<DecodedVideoFrame> Tick(double audio_clock);
  bool AddSubtitle(SubtitleCue cue);
  [[nodiscard]] std::string_view Subtitle(double clock) const;
  [[nodiscard]] ResourceId VideoTexture() const noexcept { return current_texture_; }
  [[nodiscard]] std::size_t QueuedFrames() const noexcept { return frames_.size(); }

private:
  std::size_t capacity_{};
  std::uint64_t last_sequence_{}, current_texture_{};
  double seek_time_{};
  std::deque<DecodedVideoFrame> frames_;
  std::vector<SubtitleCue> subtitles_;
};

} // namespace nexora::runtime::presentation
