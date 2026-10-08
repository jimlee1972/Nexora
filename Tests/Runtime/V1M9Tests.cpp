#include "Nexora/Runtime/Presentation.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

void SyncSeekTests() {
  using namespace nexora::runtime::presentation;
  AnimationGraph graph;
  Require(!graph.Synchronize(0), "unbuilt graph accepted synchronization");
  Skeleton skeleton;
  Require(skeleton.Build({{-1, "root"}}) && graph.SetSkeleton(std::move(skeleton)),
          "skeleton setup failed");
  Require(graph.AddClip({10, 1, true, {{0, {{0, {}}, {1, {2, 0, 0}}}}}}) &&
              graph.AddClip({20, 2, true, {{0, {{0, {}}, {2, {8, 0, 0}}}}}}) &&
              graph.AddClip({30, 1, false, {}}),
          "graph clips rejected");
  Require(graph.Play(10) && graph.Play(20, 1), "transition failed");
  Require(graph.Synchronize(0.6F), "sync graph seek failed");
  const auto pose = graph.Update(0);
  Require(std::abs(pose.translations[0].x - 2.4F) < 1e-5F && pose.root_motion == Vec3{},
          "seek retained crossfade or published teleport root motion");
  for (const float invalid : {-1.0F, 2.0F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()})
    Require(!graph.Synchronize(invalid) && graph.Update(0).translations == pose.translations,
            "invalid seek changed graph");
  const auto moved = graph.Update(0.1F);
  Require(std::abs(moved.root_motion.x - 0.4F) < 1e-5F,
          "post-seek update lost ordinary root motion");
  Require(graph.Play(30) && !graph.Synchronize(0), "non-looping graph accepted sync seek");
}

int RunTests() {
  using namespace nexora::runtime::presentation;
  SyncSeekTests();
  Skeleton skeleton;
  Require(!skeleton.Build({{1, "cycle"}}), "skeleton accepted a forward parent");
  Require(skeleton.Build({{-1, "root"}, {0, "hand"}}), "valid skeleton was rejected");

  AnimationGraph graph;
  Require(graph.SetSkeleton(std::move(skeleton)), "skeleton was not installed");
  Require(graph.AddClip({1, 1.0F, true, {{0, {{0.0F, {}}, {1.0F, {2.0F, 0.0F, 0.0F}}}}}}),
          "walk clip was rejected");
  Require(graph.AddClip({2, 1.0F, true, {{0, {{0.0F, {}}, {1.0F, {6.0F, 0.0F, 0.0F}}}}}}),
          "run clip was rejected");
  Require(!graph.AddClip({3,
                          1.0F,
                          true,
                          {{0, {{0.0F, {}}, {1.0F, {1.0F, 0.0F, 0.0F}}}},
                           {0, {{0.0F, {}}, {1.0F, {2.0F, 0.0F, 0.0F}}}}}}),
          "animation accepted duplicate joint tracks");
  Require(graph.Play(1), "animation state did not start");
  const auto walk = graph.Update(0.25F);
  Require(std::abs(walk.translations[0].x - 0.5F) < 0.001F &&
              std::abs(walk.root_motion.x - 0.5F) < 0.001F,
          "clip sampling or root motion failed");
  Require(graph.Play(2, 0.5F), "animation transition did not start");
  const auto blended = graph.Update(0.25F);
  Require(blended.translations[0].x > 1.0F && blended.translations[0].x < 1.5F,
          "animation graph did not blend states");

  SkinningPalette palette;
  std::array<float, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  Require(palette.Upload(std::span{&identity, 1}) && palette.MatrixCount() == 1,
          "GPU skinning palette foundation failed");

  ResidencyTracker residency;
  AudioEngine audio{2, residency};
  Require(audio.SetBusGain("Music", 0.5F), "audio bus was rejected");
  const auto first = audio.Play({10, "Music", 0.8F, true});
  const auto second = audio.Play({10, "Master", 1.0F, false});
  Require(first && second && !audio.Play({11, "Master", 1.0F, false}), "voice bound failed");
  Require(residency.References(10) == 2 && std::abs(audio.EffectiveGain(*first) - 0.4F) < 0.001F,
          "audio residency or bus mixing failed");
  Require(audio.Stop(*first) && residency.Resident(10), "active audio resource was unloaded");
  Require(audio.Stop(*second) && !residency.Resident(10), "audio residency reference leaked");

  ParticleSystem particles{2, ParticleRenderer::Trail};
  Require(particles.Spawn({{}, {1, 0, 0}, 1.0F}) && particles.Spawn({{}, {0, 1, 0}, 2.0F}) &&
              !particles.Spawn({{}, {}, 1.0F}),
          "particle capacity was not enforced");
  particles.Update(1.0F);
  Require(particles.Count() == 1 && particles.Renderer() == ParticleRenderer::Trail,
          "particle SoA lifetime update failed");

  auto positions = particles.PositionSnapshot();
  Require(positions == std::vector<Vec3>{{0, 1, 0}},
          "particle position snapshot disagrees with integration");
  particles.Update(0.25F);
  Require(positions[0] == Vec3{0, 1, 0} && particles.PositionSnapshot()[0] == Vec3{0, 1.25F, 0},
          "particle snapshot lifetime was coupled to simulation storage");
  particles.Update(0.75F);
  Require(particles.PositionSnapshot().empty(), "expired particles remained in render snapshot");
  VideoPlayer video{2};
  Require(video.AddSubtitle({0.0, 1.0, "Hello"}) && video.Subtitle(0.5) == "Hello",
          "subtitle timing failed");
  Require(video.SubmitDecoded({1, 0.0, 100}) && video.SubmitDecoded({2, 0.04, 101}) &&
              !video.SubmitDecoded({3, 0.08, 102}),
          "decoded-frame back-pressure failed");
  const auto frame = video.Tick(0.04);
  Require(frame && frame->sequence == 2 && video.VideoTexture() == 101 && video.QueuedFrames() == 0,
          "A/V sync or VideoTexture publication failed");
  video.Seek(2.0);
  Require(!video.SubmitDecoded({1, 1.9, 200}) && video.SubmitDecoded({1, 2.0, 201}),
          "media seek did not invalidate stale decode output");

  Require(graph.Play(1), "looping animation did not restart");
  const auto looped = graph.Update(1.0F);
  Require(std::abs(looped.root_motion.x - 2.0F) < 0.001F,
          "looped animation root motion did not preserve cycle displacement");

  VideoPlayer ordered{3};
  Require(ordered.SubmitDecoded({1, 0.2, 300}) && !ordered.SubmitDecoded({2, 0.1, 301}),
          "video accepted out-of-order presentation timestamps");

  ParticleSystem baseline{10000, ParticleRenderer::Sprite};
  const auto begin = std::chrono::steady_clock::now();
  for (int index = 0; index < 10000; ++index)
    Require(baseline.Spawn({{}, {1, 0, 0}, 1.0F}), "particle baseline spawn failed");
  baseline.Update(0.01F);
  Require(baseline.Count() == 10000 &&
              std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
          "presentation performance baseline failed");
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
