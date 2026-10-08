#include "Nexora/Animation/SyncGroup.h"
#include "Nexora/Runtime/Presentation.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  try {
    using namespace nexora::runtime::presentation;
    nexora::animation::SyncGroup group;
    Require(group.SetMembers(std::vector<nexora::animation::SyncGroupMember>{
                {1, 1, 0, 1, {{"left", 0}, {"right", 0.5}}},
                {2, 2, 0, 0.5F, {{"left", 0.2}, {"right", 1}}}}),
            "group setup failed");
    AnimationGraph graphs[2];
    for (int i = 0; i < 2; ++i) {
      Skeleton skeleton;
      Require(skeleton.Build({{-1, "root"}}) && graphs[i].SetSkeleton(std::move(skeleton)),
              "skeleton setup failed");
      const float duration = static_cast<float>(i + 1);
      Require(graphs[i].AddClip({static_cast<ResourceId>(i + 1),
                                 duration,
                                 true,
                                 {{0, {{0, {}}, {duration, {duration * 2, 0, 0}}}}}}) &&
                  graphs[i].Play(static_cast<ResourceId>(i + 1)),
              "graph clip setup failed");
    }
    for (int tick = 0; tick < 32; ++tick) {
      if (tick == 8)
        Require(group.SetWeight(2, 2), "leader switch failed");
      const auto samples = group.Update(0.125);
      Require(samples.has_value(), "group tick failed");
      for (std::size_t i = 0; i < 2; ++i) {
        const auto &sample = (*samples)[i];
        Require(sample.clip == i + 1 && graphs[i].Synchronize(static_cast<float>(sample.time)),
                "group output could not drive matching graph");
        const auto pose = graphs[i].Update(0);
        Require(std::abs(pose.translations[0].x - sample.time * 2) < 1e-5 &&
                    pose.root_motion == Vec3{},
                "synchronized graph lost pose or published seek root motion");
      }
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
