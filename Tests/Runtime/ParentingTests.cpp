// Entity parenting (Unity conventions): local transforms under a parent, world transform and
// matrix, keep-world reparenting, cycle and cross-scene rejection, batch atomicity, cascading
// destruction, snapshot version 3, editor undo, play apply-back, and GameWorld binding rules.

#include "Nexora/Game/GameWorld.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/Runtime.h"

#include <algorithm>
#include <chrono>
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

  // Two valid poses can still overflow when re-expressed relative to each other; the whole batch,
  // including the commands already applied before the reparent, must then be rolled back.
  const auto far = Create(world, scene, {1e308, 0.0, 0.0});
  const auto opposite = Create(world, scene, {-1e308, 0.0, 0.0});
  const auto *borrowed = world.FindEntity(b);
  runtime::WorldCommandBuffer overflow;
  overflow.SetTransform(b, {9.0, 9.0, 9.0});
  overflow.SetParent(far, opposite, true);
  Require(!overflow.Apply(world) && world.FindEntity(b) == borrowed && world.Parent(far) == Id{0} &&
              world.FindEntity(far)->transform == Transform{1e308, 0.0, 0.0} &&
              world.FindEntity(b)->transform == Transform{5.0, 5.0, 5.0},
          "an unrepresentable keep-world reparent must reject the whole batch");

  // Entity::parent is a public field; a cycle written directly bypasses validation, but traversal
  // must still terminate.
  const auto loop_a = Create(world, scene);
  const auto loop_b = Create(world, scene);
  for (auto &entity : const_cast<runtime::Scene *>(world.FindScene(scene))->entities) {
    if (entity.id == loop_a)
      entity.parent = loop_b;
    if (entity.id == loop_b)
      entity.parent = loop_a;
  }
  Require(world.Subtree(loop_a).size() == 2 && !world.WorldTransform(loop_a),
          "a corrupted cyclic hierarchy must not hang traversal");
  // Validating a reparent walks the new parent's ancestors; on a corrupted chain it must reject.
  const auto newcomer = Create(world, scene);
  Require(!Reparent(world, newcomer, loop_a) && world.Parent(newcomer) == Id{0},
          "reparenting under a corrupted cycle must be rejected, not hang");
  const auto dangling = Create(world, scene);
  for (auto &entity : const_cast<runtime::Scene *>(world.FindScene(scene))->entities)
    if (entity.id == dangling)
      entity.parent = 999'999;
  Require(!Reparent(world, newcomer, dangling),
          "reparenting under a dangling chain must be rejected");
}

void TestSiblingOrder() {
  runtime::World world;
  const auto scene = world.LoadScene("Order");
  const auto parent = Create(world, scene);
  const auto a = Create(world, scene);
  const auto b = Create(world, scene);
  const auto c = Create(world, scene);
  for (const auto child : {a, b, c})
    Require(Reparent(world, child, parent, false), "setup failed");
  Require((world.Children(parent) == std::vector<Id>{a, b, c}) && world.SiblingIndex(c) == 2u,
          "a reparented entity must become its new parent's last child");
  // Attaching in the opposite order of creation must still append: the order follows SetParent.
  const auto late_parent = Create(world, scene);
  const auto x = Create(world, scene);
  const auto y = Create(world, scene);
  Require(Reparent(world, y, late_parent, false) && Reparent(world, x, late_parent, false) &&
              (world.Children(late_parent) == std::vector<Id>{y, x}),
          "reparenting must append to the new parent's children, not keep the creation order");

  runtime::WorldCommandBuffer first;
  first.SetSiblingIndex(c, 0);
  Require(first.Apply(world) && (world.Children(parent) == std::vector<Id>{c, a, b}) &&
              world.SiblingIndex(a) == 1u,
          "SetSiblingIndex must move an entity among its siblings");
  runtime::WorldCommandBuffer clamp;
  clamp.SetSiblingIndex(c, 99);
  Require(clamp.Apply(world) && (world.Children(parent) == std::vector<Id>{a, b, c}),
          "an index past the end must make the entity the last sibling");
  // Roots are siblings too.
  const auto other_root = Create(world, scene);
  runtime::WorldCommandBuffer root_order;
  root_order.SetSiblingIndex(other_root, 0);
  Require(root_order.Apply(world) && world.SiblingIndex(other_root) == 0u &&
              world.SiblingIndex(parent) == 1u,
          "roots must be ordered among the scene's roots");
  // A batch with an invalid command applies nothing, ordering included.
  runtime::WorldCommandBuffer rejected;
  rejected.SetSiblingIndex(b, 0);
  rejected.SetSiblingIndex(999'999, 0);
  Require(!rejected.Apply(world) && (world.Children(parent) == std::vector<Id>{a, b, c}),
          "a rejected batch must not reorder anything");
  // The order is the storage order, so snapshots keep it.
  runtime::WorldCommandBuffer shuffle;
  shuffle.SetSiblingIndex(c, 0);
  shuffle.SetSiblingIndex(a, 2);
  Require(shuffle.Apply(world) && (world.Children(parent) == std::vector<Id>{c, b, a}),
          "shuffle failed");
  runtime::World restored;
  Require(restored.LoadSceneSnapshot(*world.SaveScene(scene)) &&
              (restored.Children(parent) == std::vector<Id>{c, b, a}) &&
              restored.SiblingIndex(other_root) == 0u,
          "a snapshot must keep the sibling order");
}

void TestLongChainsStayLinear() {
  // One long chain is the worst case for per-entity ancestor walks and per-node child scans: both
  // would be quadratic (billions of steps here); loading and destroying it must stay linear.
  constexpr std::size_t kChain = 50'000;
  runtime::World world;
  const auto scene = world.LoadScene("Chain");
  Id previous{};
  for (std::size_t index = 0; index < kChain; ++index) {
    auto &entity = world.CreateEntity(scene);
    entity.parent = previous;
    previous = entity.id;
  }
  const auto root = world.FindScene(scene)->entities.front().id;
  const auto saved = world.SaveScene(scene);
  Require(saved.has_value(), "the chain could not be saved");
  const auto started = std::chrono::steady_clock::now();
  runtime::World restored;
  Require(restored.LoadSceneSnapshot(*saved).has_value(), "the chain snapshot was rejected");
  runtime::WorldCommandBuffer destroy;
  destroy.DestroyEntity(root);
  Require(destroy.Apply(restored) && destroy.LastDestroyed().size() == kChain,
          "destroying the chain root must destroy the whole chain");
  Require(std::chrono::steady_clock::now() - started < std::chrono::seconds(10),
          "loading or destroying a long chain is not linear");
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

#if NEXORA_EDITOR_SDK_ENABLED
// SceneEditor and PlaySession exist only with the Editor SDK feature.
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

  // Sibling order is undoable, and a Hierarchy drag (Move) is one undo step.
  const auto first = editor.CreateEntity(scene);
  const auto second = editor.CreateEntity(scene);
  Require(editor.SetParent(first, parent, false) && editor.SetParent(second, parent, false) &&
              (world.Children(parent) == std::vector<Id>{first, second}),
          "order setup failed");
  Require(editor.SetSiblingIndex(second, 0) &&
              (world.Children(parent) == std::vector<Id>{second, first}) && editor.Undo() &&
              (world.Children(parent) == std::vector<Id>{first, second}),
          "undoing SetSiblingIndex must restore the order");
  const auto move_depth = editor.UndoDepth();
  Require(editor.Move(child, parent, 0) && world.Parent(child) == parent &&
              (world.Children(parent) == std::vector<Id>{child, first, second}) &&
              editor.UndoDepth() == move_depth + 1,
          "Move must reparent and order as one undo step");
  Require(editor.Undo() && world.Parent(child) == Id{0} &&
              world.FindEntity(child)->transform == Transform{3.0, 0.0, 0.0} &&
              (world.Children(parent) == std::vector<Id>{first, second}),
          "undoing Move must restore the parent, transform, and order");
  Require(editor.SetParent(first, 0, true) && editor.Undo() &&
              (world.Children(parent) == std::vector<Id>{first, second}),
          "undoing a reparent must restore the sibling position");
  Require(editor.DestroyEntity(scene, first) && editor.Undo() &&
              (world.Children(parent) == std::vector<Id>{first, second}),
          "undoing a destroy must restore the sibling position");

  Require(editor.SetParent(child, parent, false) && editor.SetParent(grandchild, child, false),
          "building the hierarchy failed");
  const auto before = *world.WorldTransform(grandchild);
  Require(editor.DestroyEntity(scene, parent) && !world.FindEntity(parent) &&
              !world.FindEntity(child) && !world.FindEntity(grandchild),
          "destroying a parent in the editor must cascade");
  Require(editor.Undo() && world.Parent(child) == parent && world.Parent(grandchild) == child &&
              SamePose(*world.WorldTransform(grandchild), before),
          "undoing a cascaded destroy must restore the whole subtree");

  // If the subtree's outside parent disappears before the undo, the subtree root comes back as a
  // root at the world pose it had, never under a dangling parent.
  const auto child_world = *world.WorldTransform(child);
  Require(editor.DestroyEntity(scene, child), "destroying the child failed");
  runtime::WorldCommandBuffer remove_parent;
  remove_parent.DestroyEntity(parent);
  Require(remove_parent.Apply(world), "removing the parent failed");
  Require(editor.Undo() && world.Parent(child) == Id{0} && world.Parent(grandchild) == child &&
              SamePose(*world.WorldTransform(child), child_world) &&
              SamePose(*world.WorldTransform(grandchild), before),
          "an orphaned subtree must be restored as a root at its world pose");
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

  // Reparenting with keep-local leaves the local values unchanged, but it is still a conflict.
  runtime::PlaySession keep_local(world);
  Require(keep_local.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }) &&
              Reparent(*keep_local.PlayWorld(), child, parent, false),
          "play setup failed");
  Require(!keep_local.Stop(runtime::ApplyBackPolicy::Transforms) &&
              keep_local.LastApplyBackStatus() == runtime::ApplyBackStatus::Conflict,
          "apply-back must report a reparent even when the local transform is unchanged");
}
#endif

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
  // A character controller works in world space, also under a parent (Unity's CharacterController
  // on a child): it starts each move from the transform's current world position, so a moving
  // parent carries it, and the result is stored relative to the parent.
  EntitySpawnDescriptor platform_descriptor;
  platform_descriptor.transform = ParentPose(); // turned 90 degrees about +Y, scale 2
  const auto platform = world.SpawnEntity(scene, platform_descriptor);
  EntitySpawnDescriptor rider_descriptor;
  rider_descriptor.transform = {10.0, 1.0, -2.0};
  const auto rider = world.SpawnEntity(scene, rider_descriptor);
  Require(world.SetParent(rider, platform) &&
              world.SetCharacter(rider, runtime::CharacterControllerConfig{}),
          "a parented entity must accept a character controller");
  Require(Near(world.GetCharacter(rider)->position.x, 10.0) &&
              Near(world.GetCharacter(rider)->position.z, -2.0),
          "a character must start from the world position, not the local one");
  runtime::CharacterInput forward;
  forward.move_x = 1.0;
  Require(world.TickCharacter(rider, forward, 0.1).has_value(), "the rider failed to tick");
  const auto state = *world.GetCharacter(rider);
  const auto rider_world = *world.GetWorldTransform(rider);
  Require(Near(rider_world.x, state.position.x) && Near(rider_world.y, state.position.y) &&
              Near(rider_world.z, state.position.z) && world.GetParent(rider) == platform,
          "the controller's world position must be stored relative to the parent");
  // Raise the platform; the next (tiny) step starts from the carried position.
  auto raised = ParentPose();
  raised.y = 5.0;
  Require(world.SetTransform(platform, raised), "moving the platform failed");
  Require(world.TickCharacter(rider, {}, 0.001).has_value() &&
              std::abs(world.GetWorldTransform(rider)->y - (rider_world.y + 5.0)) < 0.05,
          "a moving parent must carry its character along");
  // Detaching keeps the world pose and the controller keeps working.
  Require(world.SetParent(rider, 0) && world.TickCharacter(rider, forward, 0.1).has_value() &&
              world.GetParent(rider) == runtime::Id{0},
          "a detached character must keep ticking as a root");

  // Under a sheared hierarchy (a non-uniformly scaled ancestor of a rotated parent) the world TRS
  // is only approximate; the controller must use the exact matrix translation.
  EntitySpawnDescriptor stretched;
  stretched.transform.sx = 2.0;
  const auto stretcher = world.SpawnEntity(scene, stretched);
  EntitySpawnDescriptor turned;
  turned.transform.qz = turned.transform.qw = kHalfSqrt2; // 90 degrees about +Z
  const auto turner = world.SpawnEntity(scene, turned);
  EntitySpawnDescriptor sheared_descriptor;
  sheared_descriptor.transform = {1.0, 0.0, 0.0};
  const auto sheared = world.SpawnEntity(scene, sheared_descriptor);
  Require(world.SetParent(turner, stretcher, false) && world.SetParent(sheared, turner, false) &&
              world.SetCharacter(sheared, runtime::CharacterControllerConfig{}),
          "sheared hierarchy setup failed");
  Require(Near(world.GetCharacter(sheared)->position.y, 1.0) &&
              Near(world.GetWorldTransform(sheared)->y, 2.0),
          "a character under shear must start at the exact matrix position, not the lossy TRS");
  Require(world.TickCharacter(sheared, {}, 0.01).has_value(), "the sheared character failed");
  const auto exact = *world.InternalWorld().WorldMatrix(sheared);
  Require(Near(exact[12], world.GetCharacter(sheared)->position.x) &&
              Near(exact[13], world.GetCharacter(sheared)->position.y),
          "a character under shear must be stored so its exact world position is the controller's");

  // Writing the position from outside is a teleport (no ground, velocity reset); a moving parent
  // or a keep-world reparent is not.
  EntitySpawnDescriptor faller_descriptor;
  faller_descriptor.transform = {50.0, 100.0, 0.0};
  const auto faller = world.SpawnEntity(scene, faller_descriptor);
  Require(world.SetCharacter(faller, runtime::CharacterControllerConfig{}), "faller failed");
  for (int step = 0; step < 3; ++step)
    Require(world.TickCharacter(faller, {}, 0.1).has_value(), "falling failed");
  const auto falling_speed = world.GetCharacter(faller)->velocity.y;
  Require(falling_speed < -1.0, "the character must be falling before the checks");
  const auto carrier = world.SpawnEntity(scene);
  Require(world.SetParent(faller, carrier) && world.TickCharacter(faller, {}, 0.001).has_value() &&
              world.GetCharacter(faller)->velocity.y < falling_speed + 0.1,
          "a keep-world reparent must not count as a teleport");
  auto lifted = *world.GetEntity(carrier);
  lifted.transform.y = 20.0;
  Require(world.SetTransform(carrier, lifted.transform) &&
              world.TickCharacter(faller, {}, 0.001).has_value() &&
              world.GetCharacter(faller)->velocity.y < falling_speed + 0.1,
          "a moving parent must not count as a teleport");
  Require(world.SetTransform(faller, {0.0, 200.0, 0.0}) &&
              world.TickCharacter(faller, {}, 0.001).has_value() &&
              std::abs(world.GetCharacter(faller)->velocity.y) < 0.1,
          "writing the position from outside must be a teleport that resets the velocity");

  // A move that cannot be stored (a parent scale too small to invert) changes nothing.
  EntitySpawnDescriptor tiny_descriptor;
  tiny_descriptor.transform.sx = tiny_descriptor.transform.sy = tiny_descriptor.transform.sz =
      1e-200;
  const auto tiny = world.SpawnEntity(scene, tiny_descriptor);
  const auto stuck = world.SpawnEntity(scene);
  Require(world.SetParent(stuck, tiny) &&
              world.SetCharacter(stuck, runtime::CharacterControllerConfig{}),
          "tiny parent setup failed");
  const auto stuck_before = *world.GetCharacter(stuck);
  const auto stuck_local = world.GetEntity(stuck)->transform;
  Require(!world.TickCharacter(stuck, forward, 0.1).has_value() &&
              world.GetCharacter(stuck)->position == stuck_before.position &&
              world.GetCharacter(stuck)->velocity == stuck_before.velocity &&
              world.GetEntity(stuck)->transform == stuck_local,
          "a move that cannot be stored must leave the character and the entity unchanged");
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
    TestSiblingOrder();
    TestLongChainsStayLinear();
#if NEXORA_EDITOR_SDK_ENABLED
    TestSceneEditorUndo();
    TestPlayApplyBack();
#endif
    TestGameWorld();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
