#include "Nexora/Runtime/Runtime.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
bool Near(double a, double b) {
  return std::abs(a - b) <= 1e-10 * std::max({1.0, std::abs(a), std::abs(b)});
}
std::string Small() {
  return "NEXORA_SCENE 3 \"Bulk poses\" 0 4\n"
         "40 20 5 -2 1 .1 .2 .3 .9 .75 -1.25 .5 0 0 0 60 .1 1000 1 0 0\n"
         "20 10 1 2 3 0 0 .4 .8 1.5 .25 2 0 0 0 60 .1 1000 1 0 0\n"
         "10 0 -3 4 5 0 .3 0 .95 2 3 -4 0 0 0 60 .1 1000 1 0 0\n"
         "30 0 3 -4 5 0 0 0 1 1 1 1 0 0 0 60 .1 1000 1 0 0\n";
}
void ExactAndOwning() {
  World world;
  const auto scene = world.LoadSceneSnapshot(Small());
  Require(scene.has_value(), "small shear fixture failed");
  const auto baseline = world.SaveScene(*scene);
  const auto poses = world.SceneWorldPoses(*scene);
  Require(poses && poses->size() == 4, "bulk snapshot missing");
  const auto &entities = world.FindScene(*scene)->entities;
  for (std::size_t i = 0; i < poses->size(); ++i) {
    const auto &actual = (*poses)[i];
    const auto expected = world.WorldTransform(actual.id);
    const auto matrix = world.WorldMatrix(actual.id);
    Require(actual.id == entities[i].id && expected && matrix,
            "bulk snapshot changed storage order or identity");
    const auto &a = actual.transform;
    const auto &b = *expected;
    Require(Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z) && Near(a.qx, b.qx) &&
                Near(a.qy, b.qy) && Near(a.qz, b.qz) && Near(a.qw, b.qw) && Near(a.sx, b.sx) &&
                Near(a.sy, b.sy) && Near(a.sz, b.sz),
            "bulk TRS differs from scalar under mirrored/sheared ancestry");
    for (std::size_t k = 0; k < matrix->size(); ++k)
      Require(Near(actual.matrix[k], (*matrix)[k]), "bulk affine matrix differs from scalar");
  }
  Require(world.SaveScene(*scene) == baseline, "bulk observation authored the scene");
  const auto original = poses->front().transform;
  WorldCommandBuffer commands;
  commands.SetTransform(40, {100, 200, 300});
  Require(commands.Apply(world) && poses->front().transform == original &&
              world.SceneWorldPoses(*scene)->front().transform != original,
          "bulk observations retained mutable World storage");
  Require(!world.SceneWorldPoses(999999), "missing scene accepted");
  const auto empty = world.LoadScene("Empty");
  Require(world.SceneWorldPoses(empty) && world.SceneWorldPoses(empty)->empty(),
          "valid empty scene rejected");
  Require(world.Activate(empty) && world.RequestUnload(empty) && !world.SceneWorldPoses(empty),
          "unloading scene accepted");
  world.EndFrame();
  Require(!world.SceneWorldPoses(empty), "unloaded scene accepted");
}
template <typename Corrupt> void Reject(Corrupt corrupt) {
  World world;
  const auto scene = world.LoadSceneSnapshot(Small());
  Require(scene.has_value(), "corrupt fixture failed");
  auto &entities = const_cast<Scene *>(world.FindScene(*scene))->entities;
  corrupt(entities);
  const auto baseline = world.SaveScene(*scene);
  Require(!world.SceneWorldPoses(*scene) && world.SaveScene(*scene) == baseline,
          "invalid bulk snapshot accepted or mutated its input");
}
void Invalid() {
  Reject([](auto &e) { e[0].id = e[1].id; });
  Reject([](auto &e) { e[0].id = 0; });
  Reject([](auto &e) { e[0].parent = 999999; });
  Reject([](auto &e) { e[0].parent = e[0].id; });
  Reject([](auto &e) { e[1].parent = e[0].id; });
  Reject([](auto &e) { e[0].transform.x = std::numeric_limits<double>::quiet_NaN(); });
  Reject([](auto &e) { e[0].transform.sy = 0; });
  Reject([](auto &e) {
    e[2].transform.sx = 1e308;
    e[1].transform.sx = 1e308;
  });
}
void Large(bool deep, bool reverse) {
  constexpr std::size_t count = 100000;
  std::string input = "NEXORA_SCENE 3 \"Large bulk poses\" 0 " + std::to_string(count) + '\n';
  for (std::size_t at = 0; at < count; ++at) {
    const auto i = reverse ? count - at - 1 : at;
    input += std::to_string(i + 100) + ' ' + std::to_string(deep && i ? i + 99 : 0) +
             " .000001 0 0 0 0 0 1 1 1 1 0 0 0 60 .1 1000 1 0 0\n";
  }
  World world;
  const auto scene = world.LoadSceneSnapshot(input);
  Require(scene.has_value(), "actual 100k scene failed to load");
  const auto poses = world.SceneWorldPoses(*scene);
  Require(poses && poses->size() == count, "actual 100k bulk poses missing");
  for (std::size_t at = 0; at < count; ++at) {
    const auto i = reverse ? count - at - 1 : at;
    const auto &pose = (*poses)[at];
    const auto expected = deep ? static_cast<double>(i + 1) * .000001 : .000001;
    Require(pose.id == i + 100 && Near(pose.transform.x, expected) &&
                Near(pose.matrix[12], expected) && pose.transform.y == 0 &&
                pose.transform.qw == 1 && pose.transform.sx == 1,
            "100k bulk result changed exact order or accumulated origins");
  }
}
} // namespace
int main() {
  try {
    ExactAndOwning();
    Invalid();
    Large(true, false);
    Large(true, true);
    Large(false, true);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
