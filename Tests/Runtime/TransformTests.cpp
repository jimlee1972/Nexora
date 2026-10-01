// Rotation and scale on runtime::Transform: defaults, validation, normalization, snapshot v1/v2
// persistence, and position-only writers that must not reset rotation or scale.

#include "Nexora/Game/GameWorld.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/Runtime.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace nexora;
using runtime::Transform;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool Near(double a, double b, double epsilon = 1e-12) { return std::abs(a - b) <= epsilon; }

constexpr double kHalfSqrt2 = 0.70710678118654752440;

Transform Rotated() {
  Transform transform{1.0, 2.0, 3.0};
  transform.qy = kHalfSqrt2; // 90 degrees about +Y
  transform.qw = kHalfSqrt2;
  transform.sx = 1.0;
  transform.sy = -2.0; // negative scale mirrors the axis
  transform.sz = 3.0;
  return transform;
}

void TestDefaultsAndHelpers() {
  const Transform positional{4.0, 5.0, 6.0};
  Require(positional.qx == 0.0 && positional.qy == 0.0 && positional.qz == 0.0 &&
              positional.qw == 1.0 && positional.sx == 1.0 && positional.sy == 1.0 &&
              positional.sz == 1.0,
          "a position-only transform must default to identity rotation and unit scale");
  Require(runtime::IsValidTransform(positional), "the default transform must be valid");

  const auto moved = runtime::WithPosition(Rotated(), 9.0, 8.0, 7.0);
  Require(moved.x == 9.0 && moved.y == 8.0 && moved.z == 7.0 && moved.qy == Rotated().qy &&
              moved.sy == -2.0,
          "WithPosition must replace only the position");
}

void TestValidation() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  const auto inf = std::numeric_limits<double>::infinity();
  Require(runtime::IsValidTransform(Rotated()), "a negative scale must be valid (it mirrors)");

  const auto rejects = [](auto mutate, const char *message) {
    Transform transform = Rotated();
    mutate(transform);
    Require(!runtime::IsValidTransform(transform) && !runtime::NormalizedTransform(transform),
            message);
  };
  rejects([nan](Transform &t) { t.x = nan; }, "a NaN position was accepted");
  rejects([inf](Transform &t) { t.z = inf; }, "an infinite position was accepted");
  rejects([nan](Transform &t) { t.qx = nan; }, "a NaN rotation was accepted");
  rejects([inf](Transform &t) { t.qw = inf; }, "an infinite rotation was accepted");
  rejects([nan](Transform &t) { t.sy = nan; }, "a NaN scale was accepted");
  rejects([](Transform &t) { t.sx = 0.0; }, "a zero x scale was accepted");
  rejects([](Transform &t) { t.sy = 0.0; }, "a zero y scale was accepted");
  rejects([](Transform &t) { t.sz = 0.0; }, "a zero z scale was accepted");
  rejects([](Transform &t) { t.qx = t.qy = t.qz = t.qw = 0.0; }, "a zero quaternion was accepted");
  rejects([](Transform &t) { t.qx = t.qy = t.qz = t.qw = 1e200; },
          "a quaternion whose length overflows was accepted");

  Transform scaled = Rotated();
  scaled.qx = scaled.qy = scaled.qz = 0.0;
  scaled.qw = 5.0; // valid but not unit length
  Require(runtime::IsValidTransform(scaled), "a non-unit but usable quaternion must be valid");
  const auto normalized = runtime::NormalizedTransform(scaled);
  Require(normalized && Near(normalized->qw, 1.0) && normalized->sy == -2.0,
          "normalization must give a unit quaternion and leave everything else alone");
}

void TestCommandBuffer() {
  runtime::World world;
  const auto scene = world.LoadScene("Commands");
  const auto first = world.CreateEntity(scene).id;
  const auto second = world.CreateEntity(scene).id;

  runtime::WorldCommandBuffer ok;
  auto big = Rotated();
  big.qy = big.qw = 10.0 * kHalfSqrt2; // non-unit: must be normalized when applied
  ok.SetTransform(first, big);
  Require(ok.Apply(world), "a valid rotated, scaled transform was rejected");
  const auto *stored = world.FindEntity(first);
  Require(stored != nullptr && Near(stored->transform.qy, kHalfSqrt2) &&
              Near(stored->transform.qw, kHalfSqrt2) && stored->transform.sy == -2.0,
          "the stored rotation was not normalized or the scale was lost");

  // An invalid transform rejects the whole batch before anything changes.
  runtime::WorldCommandBuffer mixed;
  mixed.SetTransform(second, Transform{5.0, 5.0, 5.0});
  Transform bad = Rotated();
  bad.sx = 0.0;
  mixed.SetTransform(first, bad);
  Require(!mixed.Apply(world), "a batch containing an invalid transform was applied");
  Require(world.FindEntity(second)->transform == Transform{} &&
              world.FindEntity(first)->transform.sy == -2.0,
          "a rejected batch partially mutated the world");
}

void TestUndoRestoresFullTransform() {
  runtime::World world;
  const auto scene = world.LoadScene("Undo");
  runtime::SceneEditor editor(world);
  const auto entity = editor.CreateEntity(scene);
  Require(editor.SetTransform(entity, Rotated()), "the editor rejected a valid transform");
  Require(editor.SetTransform(entity, Transform{7.0, 7.0, 7.0}), "second edit failed");
  Require(editor.Undo() && Near(world.FindEntity(entity)->transform.qy, kHalfSqrt2) &&
              world.FindEntity(entity)->transform.sy == -2.0,
          "undo did not restore the full rotation and scale");
}

void TestSnapshotRoundTrip() {
  runtime::World world;
  const auto scene = world.LoadScene("Persist");
  const auto id = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(id, Rotated());
  Require(commands.Apply(world), "setup transform was rejected");

  const auto saved = world.SaveScene(scene);
  Require(saved && saved->starts_with("NEXORA_SCENE 2 "), "a save must write the version 2 format");

  runtime::World restored;
  const auto loaded = restored.LoadSceneSnapshot(*saved);
  Require(loaded.has_value(), "a version 2 snapshot did not load");
  const auto &entities = restored.FindScene(*loaded)->entities;
  Require(entities.size() == 1 && entities[0].transform == world.FindEntity(id)->transform,
          "rotation and scale did not survive a save/load round trip");
  Require(restored.SaveScene(*loaded) == saved, "version 2 save/load was not deterministic");
}

void TestVersion1StillLoads() {
  // Hand-written version 1 snapshot: id x y z camera light mesh fov near far intensity mesh shader.
  runtime::World world;
  const auto loaded =
      world.LoadSceneSnapshot("NEXORA_SCENE 1 \"legacy\" 0 1\n1 5 6 7 0 0 0 60 0.1 1000 1 0 0\n");
  Require(loaded.has_value(), "a version 1 snapshot no longer loads");
  const auto &transform = world.FindScene(*loaded)->entities.at(0).transform;
  Require(transform == Transform{5.0, 6.0, 7.0},
          "a version 1 entity must load with identity rotation and unit scale");
  const auto upgraded = world.SaveScene(*loaded);
  Require(upgraded && upgraded->starts_with("NEXORA_SCENE 2 "),
          "saving a loaded version 1 scene must upgrade it to version 2");
}

void TestHostileSnapshots() {
  const auto record = [](const std::string &transform_tokens) {
    return "NEXORA_SCENE 2 \"x\" 0 1\n1 0 0 0 " + transform_tokens + " 0 0 0 60 0.1 1000 1 0 0\n";
  };
  runtime::World world;
  Require(world.LoadSceneSnapshot(record("0 0 0 1 1 1 1")).has_value(),
          "the well-formed version 2 record used by these cases was rejected");
  Require(!world.LoadSceneSnapshot(record("0 0 0 1 0 1 1")).has_value(),
          "a zero scale was accepted from a snapshot");
  Require(!world.LoadSceneSnapshot(record("0 0 0 0 1 1 1")).has_value(),
          "a zero quaternion was accepted from a snapshot");
  Require(!world.LoadSceneSnapshot(record("nan 0 0 1 1 1 1")).has_value(),
          "a NaN rotation was accepted from a snapshot");
  Require(!world.LoadSceneSnapshot(record("1e999 0 0 1 1 1 1")).has_value(),
          "an overflowing rotation was accepted from a snapshot");
  Require(!world.LoadSceneSnapshot("NEXORA_SCENE 2 \"x\" 0 1\n1 0 0 0 0 0 0 1\n").has_value(),
          "a truncated version 2 record was accepted");
  Require(!world.LoadSceneSnapshot("NEXORA_SCENE 3 \"x\" 0 0\n").has_value(),
          "an unknown snapshot version was accepted");

  // A usable but non-unit rotation is normalized on load.
  runtime::World normalizing;
  const auto loaded = normalizing.LoadSceneSnapshot(record("0 0 0 4 1 1 1"));
  Require(loaded && Near(normalizing.FindScene(*loaded)->entities.at(0).transform.qw, 1.0),
          "a non-unit rotation was not normalized on load");
}

void TestSpawnAndBridge() {
  game::GameWorld world;
  const auto scene = world.LoadScene("Game");
  world.ActivateScene(scene);

  game::EntitySpawnDescriptor invalid;
  invalid.transform.sx = 0.0;
  bool threw = false;
  try {
    static_cast<void>(world.SpawnEntity(scene, invalid));
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  Require(threw, "spawning with a zero scale did not throw");
  Require(world.Query(scene, 0).empty(), "a rejected spawn left an entity behind");

  game::EntitySpawnDescriptor descriptor;
  descriptor.transform = Rotated();
  descriptor.transform.qy = descriptor.transform.qw = 3.0 * kHalfSqrt2; // non-unit
  const auto entity = world.SpawnEntity(scene, descriptor);
  Require(Near(world.GetEntity(entity)->transform.qw, kHalfSqrt2),
          "a spawned rotation was not normalized");

  // The Zig/C bridge carries a position only; writing it must keep rotation and scale.
  game::GameplayHostContext context{&world};
  const auto host = game::MakeHost(context);
  const game::GameplayTransformWire wire{9.0, 8.0, 7.0};
  Require(host.write_component(host.context, entity, game::TransformComponentType(), &wire,
                               sizeof(wire)) == 0,
          "the bridge rejected a position write");
  const auto after = world.GetEntity(entity)->transform;
  Require(after.x == 9.0 && after.y == 8.0 && after.z == 7.0 && after.sy == -2.0 &&
              Near(after.qy, kHalfSqrt2) && Near(after.qw, kHalfSqrt2),
          "a bridge position write reset the entity's rotation or scale");
}
} // namespace

int main() {
  try {
    TestDefaultsAndHelpers();
    TestValidation();
    TestCommandBuffer();
    TestUndoRestoresFullTransform();
    TestSnapshotRoundTrip();
    TestVersion1StillLoads();
    TestHostileSnapshots();
    TestSpawnAndBridge();
    std::cout << "Transform rotation/scale contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
