// Entity parenting (Unity conventions): local transforms under a parent, world transform and
// matrix, keep-world reparenting, cycle and cross-scene rejection, batch atomicity, cascading
// destruction, snapshot version 3, editor undo, play apply-back, and GameWorld binding rules.

#include "Nexora/Game/GameWorld.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/Runtime.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace nexora;
using runtime::Id;
using runtime::Transform;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool Near(double a, double b, double epsilon = 1e-9) { return std::abs(a - b) <= epsilon; }

bool SamePose(const Transform &a, const Transform &b, double epsilon = 1e-9) {
  // q and -q are the same rotation.
  const double sign = a.qx * b.qx + a.qy * b.qy + a.qz * b.qz + a.qw * b.qw < 0.0 ? -1.0 : 1.0;
  return Near(a.x, b.x, epsilon) && Near(a.y, b.y, epsilon) && Near(a.z, b.z, epsilon) &&
         Near(a.qx, sign * b.qx, epsilon) && Near(a.qy, sign * b.qy, epsilon) &&
         Near(a.qz, sign * b.qz, epsilon) && Near(a.qw, sign * b.qw, epsilon) &&
         Near(a.sx, b.sx, epsilon) && Near(a.sy, b.sy, epsilon) && Near(a.sz, b.sz, epsilon);
}

constexpr double kHalfSqrt2 = 0.70710678118654752440;

// Position (10, 0, 0), 90 degrees about +Y, uniform scale 2.
Transform ParentPose() {
  Transform transform{10.0, 0.0, 0.0};
  transform.qy = kHalfSqrt2;
  transform.qw = kHalfSqrt2;
  transform.sx = transform.sy = transform.sz = 2.0;
  return transform;
}

Id Create(runtime::World &world, Id scene, Transform transform = {}) {
  auto &entity = world.CreateEntity(scene);
  entity.transform = transform;
  return entity.id;
}

bool Reparent(runtime::World &world, Id entity, Id parent, bool keep_world = true) {
  runtime::WorldCommandBuffer commands;
  commands.SetParent(entity, parent, keep_world);
  return commands.Apply(world);
}

void TestMath() {
  const auto parent = ParentPose();
  Transform child{1.0, 0.0, 0.0};
  const auto world = runtime::ComposeTransforms(parent, child);
  // +X rotated 90 degrees about +Y is -Z; scaled by 2 and offset by the parent position.
  Require(Near(world.x, 10.0) && Near(world.y, 0.0) && Near(world.z, -2.0) && Near(world.sx, 2.0),
          "ComposeTransforms must apply the parent scale, rotation, then translation");
  Require(SamePose(runtime::RelativeTransform(parent, world), child),
          "RelativeTransform must invert ComposeTransforms");
  const auto matrix = runtime::ToMatrix(world);
  Require(Near(matrix[12], world.x) && Near(matrix[13], world.y) && Near(matrix[14], world.z) &&
              Near(matrix[15], 1.0),
          "ToMatrix must be column-major with the translation in the last column");
}

void TestReparenting() {
  runtime::World world;
  const auto scene = world.LoadScene("Main");
  const auto parent = Create(world, scene, ParentPose());
  const auto kept = Create(world, scene, {10.0, 0.0, -2.0});
  const auto local = Create(world, scene, {1.0, 0.0, 0.0});

  Require(world.Parent(kept) == Id{0} && world.Children(parent).empty(),
          "a new entity must be a root");
  Require(Reparent(world, kept, parent, true), "keep-world reparenting failed");
  Require(world.Parent(kept) == parent && SamePose(*world.WorldTransform(kept), {10.0, 0.0, -2.0}),
          "keep-world reparenting must not move the entity");
  const auto *kept_entity = world.FindEntity(kept);
  Require(Near(kept_entity->transform.x, 1.0) && Near(kept_entity->transform.z, 0.0) &&
              Near(kept_entity->transform.sx, 0.5),
          "keep-world reparenting must re-express the pose in the parent's space");

  Require(Reparent(world, local, parent, false), "keep-local reparenting failed");
  Require(world.FindEntity(local)->transform == Transform{1.0, 0.0, 0.0} &&
              SamePose(*world.WorldTransform(local),
                       runtime::ComposeTransforms(ParentPose(), {1.0, 0.0, 0.0})),
          "keep-local reparenting must keep the local values and move with the parent");
  Require((world.Children(parent) == std::vector<Id>{kept, local}),
          "Children must list direct children in storage order");

  // Moving the parent moves its children; the local values stay.
  auto moved = ParentPose();
  moved.y = 5.0;
  runtime::WorldCommandBuffer move;
  move.SetTransform(parent, moved);
  Require(move.Apply(world) && Near(world.WorldTransform(local)->y, 5.0) &&
              world.FindEntity(local)->transform == Transform{1.0, 0.0, 0.0},
          "a child must follow its parent");

  Require(Reparent(world, kept, 0, true) && world.Parent(kept) == Id{0} &&
              SamePose(*world.WorldTransform(kept), {10.0, 5.0, -2.0}),
          "detaching with keep-world must leave the entity where it was");

  // Rejections leave the world untouched.
  const auto other_scene = world.LoadScene("Other");
  const auto stranger = Create(world, other_scene);
  Require(!Reparent(world, parent, parent), "self-parenting was accepted");
  Require(!Reparent(world, parent, local), "a cycle (parent under its own child) was accepted");
  Require(!Reparent(world, stranger, parent), "cross-scene parenting was accepted");
  Require(!Reparent(world, local, 999'999) && !Reparent(world, 999'999, parent),
          "a missing entity or parent was accepted");
  Require(world.Parent(parent) == Id{0} && world.Parent(local) == parent &&
              world.Parent(stranger) == Id{0},
          "a rejected reparent changed the hierarchy");
}

void TestWorldMatrixAndShear() {
  runtime::World world;
  const auto scene = world.LoadScene("Main");
  Transform stretched{};
  stretched.sx = 2.0;
  const auto parent = Create(world, scene, stretched);
  Transform turned{};
  turned.qz = kHalfSqrt2; // 90 degrees about +Z
  turned.qw = kHalfSqrt2;
  const auto child = Create(world, scene, turned);
  Require(Reparent(world, child, parent, false), "reparenting failed");

  // The exact matrix is scale(2,1,1) * rotZ(90): the child's X axis becomes +Y unscaled and its Y
  // axis becomes -X scaled by 2, which no translation/rotation/scale triple can express (shear).
  const auto exact = *world.WorldMatrix(child);
  Require(Near(exact[0], 0.0) && Near(exact[1], 1.0) && Near(exact[4], -2.0) && Near(exact[5], 0.0),
          "WorldMatrix must be the exact product of the local matrices");
  const auto lossy = runtime::ToMatrix(*world.WorldTransform(child));
  Require(Near(lossy[1], 2.0) && Near(lossy[4], -1.0),
          "WorldTransform is lossy under non-uniform parent scale (like Unity's lossyScale)");

  // Without non-uniform scale the two agree.
  const auto uniform = Create(world, scene, ParentPose());
  const auto grandchild = Create(world, scene, {1.0, 2.0, 3.0});
  Require(Reparent(world, grandchild, uniform, false), "reparenting failed");
  const auto product = *world.WorldMatrix(grandchild);
  const auto composed = runtime::ToMatrix(*world.WorldTransform(grandchild));
  for (std::size_t index = 0; index < product.size(); ++index)
    Require(Near(product[index], composed[index]),
            "WorldMatrix and WorldTransform must agree under uniform scale");
  Require(!world.WorldTransform(999'999) && !world.WorldMatrix(999'999),
          "a missing entity must have no world transform");
}

void TestBatchesAndCascade() {
  runtime::World world;
  const auto scene = world.LoadScene("Main");
  const auto a = Create(world, scene);
  const auto b = Create(world, scene);

  runtime::WorldCommandBuffer cycle;
  cycle.SetParent(a, b, false);
  cycle.SetParent(b, a, false);
  Require(!cycle.Apply(world) && world.Parent(a) == Id{0} && world.Parent(b) == Id{0},
          "a batch whose later command forms a cycle must be rejected as a whole");

  runtime::WorldCommandBuffer attach;
  attach.SetParent(b, a, false);
  attach.SetTransform(b, {1.0, 0.0, 0.0});
  Require(attach.Apply(world) && world.Parent(b) == a, "a valid batch was rejected");

  // Destroying the parent cascades, so a later command on the child in the same batch is invalid.
  runtime::WorldCommandBuffer stale;
  stale.DestroyEntity(a);
  stale.SetTransform(b, {2.0, 0.0, 0.0});
  Require(!stale.Apply(world) && world.FindEntity(a) && world.FindEntity(b) &&
              world.FindEntity(b)->transform == Transform{1.0, 0.0, 0.0},
          "a command on a cascaded-destroyed entity must reject the whole batch");

  // Detaching first in the same batch saves the child.
  runtime::WorldCommandBuffer detach_then_destroy;
  detach_then_destroy.SetParent(b, 0, true);
  detach_then_destroy.DestroyEntity(a);
  detach_then_destroy.SetTransform(b, {5.0, 5.0, 5.0});
  Require(detach_then_destroy.Apply(world) && !world.FindEntity(a) &&
              world.FindEntity(b)->transform == Transform{5.0, 5.0, 5.0} &&
              (std::vector<Id>(detach_then_destroy.LastDestroyed().begin(),
                               detach_then_destroy.LastDestroyed().end()) == std::vector<Id>{a}),
          "detach-then-destroy must keep the child");

  const auto root = Create(world, scene);
  const auto child = Create(world, scene);
  const auto grandchild = Create(world, scene);
  const auto sibling = Create(world, scene);
  Require(Reparent(world, child, root) && Reparent(world, grandchild, child) &&
              Reparent(world, sibling, root),
          "building the hierarchy failed");
  Require((world.Subtree(root) == std::vector<Id>{root, child, sibling, grandchild}),
          "Subtree must list the entity first and every parent before its children");
  runtime::WorldCommandBuffer destroy;
  destroy.DestroyEntity(root);
  Require(destroy.Apply(world) && !world.FindEntity(root) && !world.FindEntity(child) &&
              !world.FindEntity(grandchild) && !world.FindEntity(sibling) && world.FindEntity(b),
          "destroying a parent must destroy exactly its descendants");
  auto destroyed = std::vector<Id>(destroy.LastDestroyed().begin(), destroy.LastDestroyed().end());
  std::ranges::sort(destroyed);
  auto expected = std::vector<Id>{root, child, grandchild, sibling};
  std::ranges::sort(expected);
  Require(destroyed == expected, "LastDestroyed must report every cascaded entity");
}

std::string Record(Id id, Id parent) {
  return std::to_string(id) + " " + std::to_string(parent) +
         " 1 2 3 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
}

void TestSnapshotVersion3() {
  runtime::World world;
  const auto scene = world.LoadScene("Main");
  // The child is stored before its parent, so loading must accept forward references.
  const auto child = Create(world, scene, {1.0, 0.0, 0.0});
  const auto parent = Create(world, scene, ParentPose());
  const auto grandchild = Create(world, scene, {0.0, 1.0, 0.0});
  Require(Reparent(world, child, parent, false) && Reparent(world, grandchild, child, false),
          "building the hierarchy failed");
  const auto saved = world.SaveScene(scene);
  Require(saved && saved->starts_with("NEXORA_SCENE 3 "), "a save must write the version 3 format");

  runtime::World restored;
  const auto restored_scene = restored.LoadSceneSnapshot(*saved);
  Require(restored_scene && restored.Parent(child) == parent &&
              restored.Parent(grandchild) == child && restored.Parent(parent) == Id{0},
          "snapshot version 3 must round trip the hierarchy");
  Require(SamePose(*restored.WorldTransform(grandchild), *world.WorldTransform(grandchild)) &&
              restored.SaveScene(*restored_scene) == saved,
          "snapshot version 3 must round trip deterministically");

  runtime::World legacy;
  Require(legacy.LoadSceneSnapshot("NEXORA_SCENE 2 \"Old\" 0 2\n"
                                   "5 1 2 3 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n"
                                   "6 1 2 3 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n") &&
              legacy.Parent(5) == Id{0} && legacy.Parent(6) == Id{0},
          "a version 2 snapshot must load with every entity as a root");

  const auto rejects = [](const std::string &records, std::size_t count, const char *message) {
    runtime::World hostile;
    const auto text = "NEXORA_SCENE 3 \"Hostile\" 0 " + std::to_string(count) + "\n" + records;
    Require(!hostile.LoadSceneSnapshot(text).has_value(), message);
  };
  rejects(Record(5, 7), 1, "a parent outside the snapshot was accepted");
  rejects(Record(5, 5), 1, "a self-parented entity was accepted");
  rejects(Record(5, 6) + Record(6, 5), 2, "a two-entity cycle was accepted");
  rejects(Record(5, 7) + Record(6, 5) + Record(7, 6), 3, "a three-entity cycle was accepted");
  runtime::World valid;
  Require(valid.LoadSceneSnapshot("NEXORA_SCENE 3 \"Valid\" 0 2\n" + Record(5, 6) + Record(6, 0))
              .has_value(),
          "a valid version 3 snapshot was rejected");
}

void TestSceneEditorUndo() {
  runtime::World world;
  const auto scene = world.LoadScene("Main");
  runtime::SceneEditor editor(world);
  const auto parent = editor.CreateEntity(scene);
  const auto child = editor.CreateEntity(scene);
  const auto grandchild = editor.CreateEntity(scene);
  Require(editor.SetTransform(parent, ParentPose()) && editor.SetTransform(child, {3.0, 0.0, 0.0}),
          "editing failed");
  const auto depth = editor.UndoDepth();
  Require(editor.SetParent(child, parent, true) && world.Parent(child) == parent &&
              SamePose(*world.WorldTransform(child), {3.0, 0.0, 0.0}) &&
              editor.UndoDepth() == depth + 1,
          "an editor reparent must be undoable and keep the world pose");
  Require(editor.Undo() && world.Parent(child) == Id{0} &&
              world.FindEntity(child)->transform == Transform{3.0, 0.0, 0.0},
          "undoing a reparent must restore the parent and the exact local transform");
  Require(!editor.SetParent(parent, parent) && editor.UndoDepth() == depth,
          "a rejected reparent must not push an undo step");

  Require(editor.SetParent(child, parent, false) && editor.SetParent(grandchild, child, false),
          "building the hierarchy failed");
  const auto before = *world.WorldTransform(grandchild);
  Require(editor.DestroyEntity(scene, parent) && !world.FindEntity(parent) &&
              !world.FindEntity(child) && !world.FindEntity(grandchild),
          "destroying a parent in the editor must cascade");
  Require(editor.Undo() && world.Parent(child) == parent && world.Parent(grandchild) == child &&
              SamePose(*world.WorldTransform(grandchild), before),
          "undoing a cascaded destroy must restore the whole subtree");
}

void TestPlayApplyBack() {
  runtime::World world;
  const auto scene = world.LoadScene("Main");
  Require(world.Activate(scene), "activation failed");
  const auto parent = Create(world, scene, ParentPose());
  const auto child = Create(world, scene, {3.0, 0.0, 0.0});
  runtime::PlaySession play(world);
  Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
          "play failed to start");
  // Reparenting keeps the world pose but changes the local values; applying those local values
  // to the editor entity, which is still a root, would move it.
  Require(Reparent(*play.PlayWorld(), child, parent, true), "play reparent failed");
  Require(!play.Stop(runtime::ApplyBackPolicy::Transforms) &&
              play.LastApplyBackStatus() == runtime::ApplyBackStatus::Conflict &&
              world.Parent(child) == Id{0} &&
              world.FindEntity(child)->transform == Transform{3.0, 0.0, 0.0},
          "apply-back must report a reparented entity as a conflict, not move it");
}

void TestGameWorld() {
  using game::DeferredCommands;
  using game::EntitySpawnDescriptor;
  game::GameWorld world;
  const auto scene = world.LoadScene("Main");
  EntitySpawnDescriptor posed;
  posed.transform = ParentPose();
  const auto parent = world.SpawnEntity(scene, posed);
  EntitySpawnDescriptor voiced;
  voiced.audio = runtime::AudioVoice{701, 1.0F, false};
  const auto child = world.SpawnEntity(scene, voiced);
  Require(world.SetParent(child, parent) && world.GetParent(child) == parent &&
              world.GetParent(999'999) == std::nullopt &&
              SamePose(*world.GetWorldTransform(child), Transform{}),
          "GameWorld reparenting and queries failed");
  Require(world.PlayAudio(child) && world.ActiveAudioVoices() == 1, "audio failed to play");
  Require(world.DestroyEntity(parent) && !world.IsAlive(child) && world.ActiveAudioVoices() == 0,
          "a cascaded destroy must release the descendants' bindings");

  const auto deferred_parent = world.SpawnEntity(scene);
  voiced.audio = runtime::AudioVoice{702, 1.0F, false};
  const auto deferred_child = world.SpawnEntity(scene, voiced);
  Require(world.SetParent(deferred_child, deferred_parent) && world.PlayAudio(deferred_child),
          "setup failed");
  DeferredCommands commands;
  commands.DestroyEntity(deferred_parent);
  Require(world.Submit(commands) && !world.IsAlive(deferred_child) &&
              world.ActiveAudioVoices() == 0,
          "a deferred cascaded destroy must release the descendants' bindings");

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  const auto holder = world.SpawnEntity(scene);
  const auto actor = world.SpawnEntity(scene);
  Require(world.SetCharacter(actor, runtime::CharacterControllerConfig{}), "character failed");
  Require(!world.SetParent(actor, holder) && world.GetParent(actor) == runtime::Id{0} &&
              world.SetParent(actor, 0),
          "a character-controlled entity must stay a root in this phase");
  const auto attached = world.SpawnEntity(scene);
  Require(world.SetParent(attached, holder) &&
              !world.SetCharacter(attached, runtime::CharacterControllerConfig{}),
          "a parented entity must not gain a character controller in this phase");
#endif
}
} // namespace

int main() {
  try {
    TestMath();
    TestReparenting();
    TestWorldMatrixAndShear();
    TestBatchesAndCascade();
    TestSnapshotVersion3();
    TestSceneEditorUndo();
    TestPlayApplyBack();
    TestGameWorld();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
