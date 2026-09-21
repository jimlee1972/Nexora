#include "Nexora/Runtime/Presentation.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora::runtime::presentation;
  Skeleton skeleton;
  Require(!skeleton.Build({{{1}, "cycle"}}), "skeleton accepted a forward parent");
  Require(skeleton.Build({{-1, "root"}, {0, "hand"}}), "valid skeleton was rejected");

  AnimationGraph graph;
  Require(graph.SetSkeleton(std::move(skeleton)), "skeleton was not installed");
  Require(graph.AddClip({1, 1.0F, true, {{0, {{0.0F, {}}, {1.0F, {2.0F, 0.0F, 0.0F}}}}}}),
          "walk clip was rejected");
  Require(graph.AddClip({2, 1.0F, true, {{0, {{0.0F, {}}, {1.0F, {6.0F, 0.0F, 0.0F}}}}}}),
          "run clip was rejected");
  Require(!graph.AddClip({3, 1.0F, true,
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
  Require(ordered.SubmitDecoded({1, 0.2, 300}) &&
              !ordered.SubmitDecoded({2, 0.1, 301}),
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
