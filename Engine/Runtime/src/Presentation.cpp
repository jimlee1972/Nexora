#include "Nexora/Runtime/Presentation.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace nexora::runtime::presentation {
namespace {
Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Scale(Vec3 value, float scale) { return {value.x * scale, value.y * scale, value.z * scale}; }
Vec3 Lerp(Vec3 a, Vec3 b, float alpha) { return Add(a, Scale(Subtract(b, a), alpha)); }
bool Finite(Vec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace

bool Skeleton::Build(std::vector<Joint> joints) {
  for (std::size_t index = 0; index < joints.size(); ++index)
    if (joints[index].name.empty() || joints[index].parent >= static_cast<std::int32_t>(index) ||
        joints[index].parent < -1)
      return false;
  joints_ = std::move(joints);
  return !joints_.empty();
}

bool AnimationGraph::SetSkeleton(Skeleton skeleton) {
  if (skeleton.JointCount() == 0)
    return false;
  skeleton_ = std::move(skeleton);
  clips_.clear();
  active_ = previous_ = 0;
  time_ = previous_time_ = 0.0F;
  return true;
}

bool AnimationGraph::AddClip(AnimationAsset clip) {
  if (clip.id == 0 || !std::isfinite(clip.duration) || clip.duration <= 0.0F ||
      skeleton_.JointCount() == 0 || clips_.contains(clip.id))
    return false;
  for (const auto &track : clip.tracks) {
    if (track.joint >= skeleton_.JointCount() || track.translations.empty())
      return false;
    float previous = -1.0F;
    for (const auto &key : track.translations) {
      if (!std::isfinite(key.time) || key.time < 0.0F || key.time > clip.duration ||
          key.time < previous || !Finite(key.value))
        return false;
      previous = key.time;
    }
  }
  return clips_.emplace(clip.id, std::move(clip)).second;
}

bool AnimationGraph::Play(ResourceId clip, float blend_seconds) {
  if (!clips_.contains(clip) || !std::isfinite(blend_seconds) || blend_seconds < 0.0F)
    return false;
  previous_ = active_;
  previous_time_ = time_;
  active_ = clip;
  time_ = 0.0F;
  blend_duration_ = previous_ == 0 ? 0.0F : blend_seconds;
  blend_time_ = 0.0F;
  return true;
}

AnimationPose AnimationGraph::Sample(ResourceId id, float time) const {
  AnimationPose pose;
  pose.translations.resize(skeleton_.JointCount());
  const auto found = clips_.find(id);
  if (found == clips_.end())
    return pose;
  const auto &clip = found->second;
  const float sample_time =
      clip.looping ? std::fmod(time, clip.duration) : std::min(time, clip.duration);
  for (const auto &track : clip.tracks) {
    const auto upper =
        std::ranges::upper_bound(track.translations, sample_time, {}, &TranslationKey::time);
    if (upper == track.translations.begin())
      pose.translations[track.joint] = upper->value;
    else if (upper == track.translations.end())
      pose.translations[track.joint] = track.translations.back().value;
    else {
      const auto &right = *upper;
      const auto &left = *(upper - 1);
      pose.translations[track.joint] =
          Lerp(left.value, right.value, (sample_time - left.time) / (right.time - left.time));
    }
  }
  return pose;
}

AnimationPose AnimationGraph::Update(float seconds) {
  if (active_ == 0 || !std::isfinite(seconds) || seconds < 0.0F)
    return Sample(active_, time_);
  const auto before = Sample(active_, time_);
  time_ += seconds;
  auto pose = Sample(active_, time_);
  if (!pose.translations.empty())
    pose.root_motion = Subtract(pose.translations.front(), before.translations.front());
  if (previous_ != 0 && blend_time_ < blend_duration_) {
    previous_time_ += seconds;
    blend_time_ += seconds;
    const auto old = Sample(previous_, previous_time_);
    const float alpha =
        blend_duration_ == 0.0F ? 1.0F : std::min(1.0F, blend_time_ / blend_duration_);
    for (std::size_t i = 0; i < pose.translations.size(); ++i)
      pose.translations[i] = Lerp(old.translations[i], pose.translations[i], alpha);
    if (alpha == 1.0F)
      previous_ = 0;
  }
  return pose;
}

bool SkinningPalette::Upload(std::span<const std::array<float, 16>> matrices) {
  if (matrices.empty() || std::ranges::any_of(matrices, [](const auto &matrix) {
        return std::ranges::any_of(matrix, [](float value) { return !std::isfinite(value); });
      }))
    return false;
  matrices_.assign(matrices.begin(), matrices.end());
  return true;
}

bool ResidencyTracker::Acquire(ResourceId resource) {
  if (resource == 0)
    return false;
  ++references_[resource];
  return true;
}
bool ResidencyTracker::Release(ResourceId resource) {
  const auto found = references_.find(resource);
  if (found == references_.end())
    return false;
  if (--found->second == 0)
    references_.erase(found);
  return true;
}
bool ResidencyTracker::Resident(ResourceId resource) const {
  return references_.contains(resource);
}
std::size_t ResidencyTracker::References(ResourceId resource) const {
  const auto found = references_.find(resource);
  return found == references_.end() ? 0 : found->second;
}

bool AudioEngine::SetBusGain(std::string bus, float gain) {
  if (bus.empty() || !std::isfinite(gain) || gain < 0.0F || gain > 1.0F)
    return false;
  buses_[std::move(bus)] = gain;
  return true;
}
std::optional<std::uint64_t> AudioEngine::Play(const AudioEvent &event) {
  if (event.resource == 0 || event.bus.empty() || !buses_.contains(event.bus) ||
      !std::isfinite(event.gain) || event.gain < 0.0F || voices_.size() >= voice_limit_ ||
      !residency_.Acquire(event.resource))
    return std::nullopt;
  const auto id = next_voice_++;
  voices_.push_back({id, event});
  return id;
}
bool AudioEngine::Stop(std::uint64_t voice) {
  const auto found = std::ranges::find(voices_, voice, &Voice::id);
  if (found == voices_.end())
    return false;
  residency_.Release(found->event.resource);
  voices_.erase(found);
  return true;
}
float AudioEngine::EffectiveGain(std::uint64_t voice) const {
  const auto found = std::ranges::find(voices_, voice, &Voice::id);
  if (found == voices_.end())
    return 0.0F;
  return found->event.gain * buses_.at(found->event.bus) * buses_.at("Master");
}

bool ParticleSystem::Spawn(const ParticleSpawn &particle) {
  if (ages_.size() >= capacity_ || !Finite(particle.position) || !Finite(particle.velocity) ||
      !std::isfinite(particle.lifetime) || particle.lifetime <= 0.0F)
    return false;
  positions_.push_back(particle.position);
  velocities_.push_back(particle.velocity);
  ages_.push_back(0.0F);
  lifetimes_.push_back(particle.lifetime);
  return true;
}
void ParticleSystem::Update(float seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0F)
    return;
  for (std::size_t i = ages_.size(); i-- > 0;) {
    ages_[i] += seconds;
    if (ages_[i] >= lifetimes_[i]) {
      positions_.erase(positions_.begin() + static_cast<std::ptrdiff_t>(i));
      velocities_.erase(velocities_.begin() + static_cast<std::ptrdiff_t>(i));
      ages_.erase(ages_.begin() + static_cast<std::ptrdiff_t>(i));
      lifetimes_.erase(lifetimes_.begin() + static_cast<std::ptrdiff_t>(i));
    } else
      positions_[i] = Add(positions_[i], Scale(velocities_[i], seconds));
  }
}

bool VideoPlayer::SubmitDecoded(DecodedVideoFrame frame) {
  if (capacity_ == 0 || frames_.size() >= capacity_ || frame.sequence <= last_sequence_ ||
      frame.texture == 0 || !std::isfinite(frame.presentation_time) ||
      frame.presentation_time < seek_time_)
    return false;
  last_sequence_ = frame.sequence;
  frames_.push_back(frame);
  return true;
}
void VideoPlayer::Seek(double time) {
  frames_.clear();
  last_sequence_ = 0;
  current_texture_ = 0;
  seek_time_ = std::isfinite(time) ? std::max(0.0, time) : 0.0;
}
std::optional<DecodedVideoFrame> VideoPlayer::Tick(double audio_clock) {
  if (!std::isfinite(audio_clock))
    return std::nullopt;
  std::optional<DecodedVideoFrame> result;
  while (!frames_.empty() && frames_.front().presentation_time <= audio_clock) {
    result = frames_.front();
    frames_.pop_front();
  }
  if (result)
    current_texture_ = result->texture;
  return result;
}
bool VideoPlayer::AddSubtitle(SubtitleCue cue) {
  if (!std::isfinite(cue.begin) || !std::isfinite(cue.end) || cue.begin < 0.0 ||
      cue.end <= cue.begin || cue.text.empty())
    return false;
  const auto position = std::ranges::lower_bound(subtitles_, cue.begin, {}, &SubtitleCue::begin);
  subtitles_.insert(position, std::move(cue));
  return true;
}
std::string_view VideoPlayer::Subtitle(double clock) const {
  if (!std::isfinite(clock))
    return {};
  const auto found = std::ranges::find_if(
      subtitles_, [clock](const auto &cue) { return cue.begin <= clock && clock < cue.end; });
  return found == subtitles_.end() ? std::string_view{} : std::string_view(found->text);
}

} // namespace nexora::runtime::presentation
