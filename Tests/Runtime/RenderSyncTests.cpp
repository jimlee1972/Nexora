// Parenting phase 4: the renderer sees each mesh renderer through its exact world matrix. Covers
// the matrix and bounds conversion, parent motion reaching descendants, shear, minimal dirty
// updates, ownership of visibility, removal paths, overflow rejection, and linear cost on deep
// chains.

#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Runtime/RenderSync.h"
#include "Nexora/Runtime/Runtime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using namespace nexora;
using runtime::Id;
using runtime::Transform;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

constexpr double kHalfSqrt2 = 0.70710678118654752440;
constexpr runtime::Id kMesh = 7;
constexpr runtime::Id kShader = 9;

Id Create(runtime::World &world, Id scene, Transform transform = {}, bool mesh = true) {
  auto &entity = world.CreateEntity(scene);
  entity.transform = transform;
  entity.mesh_renderer = mesh;
  entity.mesh_data = {kMesh, {kShader}};
  return entity.id;
}

bool Reparent(runtime::World &world, Id entity, Id parent, bool keep_world) {
  runtime::WorldCommandBuffer commands;
  commands.SetParent(entity, parent, keep_world);
  return commands.Apply(world);
}

bool Move(runtime::World &world, Id entity, Transform transform) {
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(entity, transform);
  return commands.Apply(world);
}

// Every mesh resolves to resource 3/4 with a unit sphere at the origin.
std::optional<runtime::RenderResourceBinding> Resolve(const runtime::MeshComponent &mesh) {
  if (mesh.mesh == 0 || mesh.material.shader == 0)
    return std::nullopt;
  return runtime::RenderResourceBinding{static_cast<std::uint32_t>(mesh.mesh) - 4U,
                                        static_cast<std::uint32_t>(mesh.material.shader) - 5U,
                                        {{0.0F, 0.0F, 0.0F}, 1.0F}};
}

renderer::GPUObjectDescriptor Object(const runtime::RenderSceneSync &sync,
                                     const renderer::GPUScene &scene, Id entity) {
  const auto handle = sync.Handle(entity);
  Require(handle.has_value(), "the entity must be mirrored");
  const auto descriptor = scene.Read(*handle);
  Require(descriptor.has_value(), "the mirrored object must be live");
  return *descriptor;
}

bool SameMatrix(const math::Matrix4 &a, const math::Matrix4 &b) { return a.values == b.values; }

void TestConversion() {
  Transform pose{10.0, 20.0, 30.0};
  pose.qy = kHalfSqrt2; // 90 degrees about +Y
  pose.qw = kHalfSqrt2;
  pose.sx = pose.sy = pose.sz = 2.0;
  const auto matrix = runtime::ToMatrix(pose);
  const auto render = runtime::ToRenderMatrix(matrix);
  Require(render(0, 3) == 10.0F && render(1, 3) == 20.0F && render(2, 3) == 30.0F &&
              render(3, 3) == 1.0F && render(3, 0) == 0.0F,
          "the translation must land in column 3 of the renderer's (row, column) matrix");
  for (std::size_t row = 0; row < 4; ++row)
    for (std::size_t column = 0; column < 4; ++column)
      Require(render(row, column) == static_cast<float>(matrix[column * 4 + row]),
              "every element must be the transposed index of the column-major matrix");
  // The renderer's own Compose agrees with the runtime matrix for the same pose.
  const auto composed =
      math::Compose({{10.0F, 20.0F, 30.0F},
                     {0.0F, static_cast<float>(kHalfSqrt2), 0.0F, static_cast<float>(kHalfSqrt2)},
                     {2.0F, 2.0F, 2.0F}});
  for (std::size_t index = 0; index < 16; ++index)
    Require(std::abs(composed.values[index] - render.values[index]) < 1e-5F,
            "the runtime matrix must match math::Compose for the same pose");

  const auto bounds = runtime::TransformBounds(matrix, {{1.0F, 0.0F, 0.0F}, 1.5F});
  // +X rotated 90 degrees about +Y is -Z; scaled by 2 and offset.
  Require(std::abs(bounds.center.x - 10.0F) < 1e-5F && std::abs(bounds.center.y - 20.0F) < 1e-5F &&
              std::abs(bounds.center.z - 28.0F) < 1e-5F,
          "the bounds center must follow the matrix");
  Require(bounds.radius >= 3.0F && bounds.radius < 3.0001F,
          "a uniform scale of 2 must double the radius, and only by a tiny rounding margin");
}

void TestShearedBoundsAreConservative() {
  // scale(3, 1, 1) * rotZ(45): a sheared linear part whose stretch is not any axis scale.
  Transform stretched{};
  stretched.sx = 3.0;
  Transform turned{};
  turned.qz = std::sin(0.3926990816987241); // 45 degrees about +Z
  turned.qw = std::cos(0.3926990816987241);
  const auto matrix =
      runtime::MultiplyMatrices(runtime::ToMatrix(stretched), runtime::ToMatrix(turned));
  const math::Sphere local{{0.5F, -0.25F, 0.0F}, 2.0F};
  const auto bounds = runtime::TransformBounds(matrix, local);
  double worst = 0.0;
  for (int i = 0; i < 64; ++i)
    for (int j = 0; j <= 32; ++j) {
      const double theta = 6.283185307179586 * i / 64.0, phi = 3.141592653589793 * j / 32.0;
      const double px = local.center.x + local.radius * std::sin(phi) * std::cos(theta);
      const double py = local.center.y + local.radius * std::sin(phi) * std::sin(theta);
      const double pz = local.center.z + local.radius * std::cos(phi);
      const double wx = matrix[0] * px + matrix[4] * py + matrix[8] * pz + matrix[12];
      const double wy = matrix[1] * px + matrix[5] * py + matrix[9] * pz + matrix[13];
      const double wz = matrix[2] * px + matrix[6] * py + matrix[10] * pz + matrix[14];
      worst = std::max(
          worst, std::hypot(wx - bounds.center.x, wy - bounds.center.y, wz - bounds.center.z));
    }
  Require(worst <= bounds.radius, "the sheared bounds must contain every transformed point");
  // The bound is tight: the largest stretch of scale(3,1,1)*rot is 3, not the 3*sqrt(3) a
  // Frobenius norm would give.
  Require(bounds.radius < 6.001F && worst > 5.99,
          "the radius must be the spectral norm, not a loose overestimate");
}

void TestParentMotionAndShear() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  const auto parent = Create(world, main_scene, {5.0, 0.0, 0.0}, false);
  const auto child = Create(world, main_scene, {1.0, 2.0, 3.0});
  Require(Reparent(world, child, parent, false), "reparenting failed");

  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  auto stats = *sync.Sync(world, gpu, Resolve, 1);
  Require(stats.created == 1 && sync.ObjectCount() == 1 && !sync.Handle(parent),
          "only mesh renderers are mirrored");
  auto object = Object(sync, gpu, child);
  Require(SameMatrix(object.current_transform, runtime::ToRenderMatrix(*world.WorldMatrix(child))),
          "the object transform must be the entity's world matrix");
  Require(object.current_transform(0, 3) == 6.0F,
          "the parent's offset must reach the rendered child (local x is 1)");
  Require(object.mesh_resource_index == 3 && object.material_resource_index == 4,
          "resource indices come from the resolver");
  (void)gpu.ExtractUpdates();
  gpu.CommitFrame();

  // Unchanged world: nothing to upload.
  stats = *sync.Sync(world, gpu, Resolve, 2);
  Require(stats.created == 0 && stats.updated == 0 && stats.destroyed == 0 &&
              gpu.ExtractUpdates().updates.empty(),
          "an unchanged world must produce no GPU updates");

  // Moving only the parent moves the rendered child.
  Require(Move(world, parent, {8.0, 0.0, 0.0}), "moving the parent failed");
  stats = *sync.Sync(world, gpu, Resolve, 3);
  Require(stats.updated == 1, "moving a parent must update its rendered descendant");
  const auto batch = gpu.ExtractUpdates();
  Require(batch.updates.size() == 1 &&
              renderer::HasDirtyFlag(batch.updates[0].dirty_flags,
                                     renderer::GPUSceneDirtyFlags::Transform) &&
              !renderer::HasDirtyFlag(batch.updates[0].dirty_flags,
                                      renderer::GPUSceneDirtyFlags::Resources),
          "a pure motion must dirty the transform but not the resources");
  object = Object(sync, gpu, child);
  Require(object.current_transform(0, 3) == 9.0F && object.previous_transform(0, 3) == 6.0F,
          "the motion history must keep last frame's world matrix");
  gpu.CommitFrame();

  // Non-uniform parent scale under a rotated child: the renderer gets the exact (sheared) matrix,
  // not the lossy composed transform.
  Transform stretched{};
  stretched.sx = 2.0;
  Require(Move(world, parent, stretched), "scaling the parent failed");
  Transform turned{};
  turned.qz = kHalfSqrt2;
  turned.qw = kHalfSqrt2;
  Require(Move(world, child, turned), "rotating the child failed");
  (void)sync.Sync(world, gpu, Resolve, 4);
  object = Object(sync, gpu, child);
  Require(object.current_transform(1, 0) == 1.0F && object.current_transform(0, 1) == -2.0F,
          "the rendered matrix must keep the shear (child X -> +Y unscaled, Y -> -X scaled by 2)");
  Require(object.world_bounds.radius >= 2.0F,
          "the bounds must use the largest stretch of the sheared matrix");
}

void TestOwnershipAndRemoval() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  const auto side_scene = world.LoadScene("Side");
  Require(world.Activate(main_scene) && world.Activate(side_scene), "activation failed");
  const auto root = Create(world, main_scene);
  const auto leaf = Create(world, main_scene, {0.0, 1.0, 0.0});
  Require(Reparent(world, leaf, root, false), "reparenting failed");
  const auto loner = Create(world, main_scene);
  const auto other = Create(world, side_scene);
  const auto inactive_scene = world.LoadScene("Inactive");
  const auto hidden = Create(world, inactive_scene);

  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  auto stats = *sync.Sync(world, gpu, Resolve, 1);
  Require(stats.created == 4 && !sync.Handle(hidden),
          "mesh renderers in every active scene, and none in an inactive one, are mirrored");

  // Another system's visibility choice survives a motion update.
  const auto loner_handle = *sync.Handle(loner);
  Require(gpu.UpdateVisibility(loner_handle, 0), "hiding failed");
  Require(Move(world, loner, {4.0, 0.0, 0.0}), "moving failed");
  (void)sync.Sync(world, gpu, Resolve, 2);
  Require(Object(sync, gpu, loner).visibility_flags == 0,
          "the sync must not overwrite visibility it does not own");

  // A changed material updates resources in place.
  {
    runtime::WorldCommandBuffer commands;
    commands.SetMeshRenderer(loner, runtime::MeshComponent{kMesh, {kShader + 1}});
    Require(commands.Apply(world), "changing the material failed");
  }
  (void)gpu.ExtractUpdates();
  stats = *sync.Sync(world, gpu, Resolve, 3);
  Require(stats.updated == 1 && *sync.Handle(loner) == loner_handle &&
              Object(sync, gpu, loner).material_resource_index == 5,
          "a material change must update resources on the same object");

  // Cascading destroy removes the whole subtree's objects; losing the mesh renderer removes one;
  // an unresolvable mesh is kept out.
  {
    runtime::WorldCommandBuffer commands;
    commands.DestroyEntity(root);
    commands.SetMeshRenderer(other, std::nullopt);
    Require(commands.Apply(world), "destroying failed");
  }
  stats = *sync.Sync(world, gpu, Resolve, 4);
  Require(stats.destroyed == 3 && !sync.Handle(root) && !sync.Handle(leaf) && !sync.Handle(other),
          "destroyed entities and removed mesh renderers must lose their objects");
  {
    runtime::WorldCommandBuffer commands;
    commands.SetMeshRenderer(loner, runtime::MeshComponent{0, {kShader}});
    Require(commands.Apply(world), "clearing the mesh failed");
  }
  stats = *sync.Sync(world, gpu, Resolve, 5);
  Require(stats.destroyed == 1 && sync.ObjectCount() == 0,
          "a mesh the resolver rejects must leave the GPU scene");
  Require(gpu.GetStatistics().active_object_count == 0, "no object may leak in the GPU scene");

  // Unloading a scene removes its objects; Release() removes the rest.
  const auto again = Create(world, side_scene);
  const auto kept = Create(world, main_scene);
  (void)sync.Sync(world, gpu, Resolve, 6);
  Require(sync.Handle(again) && sync.Handle(kept), "new mesh renderers must be mirrored");
  Require(world.RequestUnload(side_scene), "unload failed");
  stats = *sync.Sync(world, gpu, Resolve, 7);
  Require(stats.destroyed == 1 && !sync.Handle(again) && sync.Handle(kept),
          "a scene that stops being active must lose its objects");

  // An object destroyed behind the sync's back is mirrored again.
  Require(gpu.Destroy(*sync.Handle(kept), 8), "external destroy failed");
  stats = *sync.Sync(world, gpu, Resolve, 8);
  Require(stats.created == 1 && gpu.Read(*sync.Handle(kept)).has_value(),
          "a stale handle must be replaced by a new object");
  Require(sync.Release(gpu, 9), "releasing from the bound scene failed");
  Require(sync.ObjectCount() == 0 && gpu.GetStatistics().active_object_count == 0,
          "Release must destroy every object the sync created");
}

void TestOverflowIsRejected() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  // Each scale is valid, but the chain's product exceeds float range.
  Transform huge{};
  huge.sx = huge.sy = huge.sz = 1e30;
  const auto outer = Create(world, main_scene, huge, false);
  const auto inner = Create(world, main_scene, huge);
  Require(Reparent(world, inner, outer, false), "reparenting failed");
  const auto fine = Create(world, main_scene);

  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  auto stats = *sync.Sync(world, gpu, Resolve, 1);
  Require(stats.rejected == 1 && stats.created == 1 && !sync.Handle(inner) && sync.Handle(fine),
          "a world matrix outside float range must be rejected, not rendered as inf");

  // An object that becomes unrepresentable is removed, and comes back when it fits again.
  Require(Move(world, inner, {}), "shrinking failed");
  stats = *sync.Sync(world, gpu, Resolve, 2);
  Require(stats.created == 1 && sync.Handle(inner), "a representable pose must be mirrored");
  Require(Move(world, inner, huge), "growing failed");
  stats = *sync.Sync(world, gpu, Resolve, 3);
  Require(stats.rejected == 1 && stats.destroyed == 1 && !sync.Handle(inner),
          "an object whose pose overflows must leave the GPU scene");
}

void TestCorruptCycleIsBounded() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  const auto a = Create(world, main_scene);
  const auto b = Create(world, main_scene);
  const auto fine = Create(world, main_scene);
  // A direct field write bypasses validation; the sync must still terminate.
  const_cast<runtime::Entity *>(world.FindEntity(a))->parent = b;
  const_cast<runtime::Entity *>(world.FindEntity(b))->parent = a;
  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  const auto stats = *sync.Sync(world, gpu, Resolve, 1);
  Require(stats.rejected == 2 && stats.created == 1 && sync.Handle(fine),
          "a cyclic parent chain must be rejected without hanging");
}

void TestFloatMatrixBounds() {
  // A far-off mesh center cancelled by the translation: in double the world center is ~0, but the
  // float coefficients the GPU uses move it by a few hundredths, more than the tiny radius.
  Transform pose{};
  pose.sx = 1.0000095;
  pose.x = -1e6 * 1.0000095;
  const auto matrix = runtime::ToMatrix(pose);
  const math::Sphere local{{1.0e6F, 0.0F, 0.0F}, 0.01F};
  const auto bounds = runtime::TransformBounds(matrix, local);
  const auto render = runtime::ToRenderMatrix(matrix);
  // Evaluate sample points exactly as a float shader would.
  float worst = 0.0F;
  for (const float dx : {-1.0F, 0.0F, 1.0F})
    for (const float dy : {-1.0F, 0.0F, 1.0F}) {
      const float px = local.center.x + dx * local.radius, py = local.center.y + dy * local.radius;
      const float wx = render(0, 0) * px + render(0, 1) * py + render(0, 3);
      const float wy = render(1, 0) * px + render(1, 1) * py + render(1, 3);
      worst = std::max(worst, std::hypot(wx - bounds.center.x, wy - bounds.center.y));
    }
  Require(worst <= bounds.radius,
          "the bounds must contain the points the float matrix actually draws");
}

void TestCorruptCyclesStayLinear() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  constexpr std::size_t kRing = 20000;
  std::vector<Id> ring;
  for (std::size_t index = 0; index < kRing; ++index)
    ring.push_back(Create(world, main_scene));
  // One huge cycle written directly into the parent fields (in storage order, which is ring order).
  auto &entities = const_cast<runtime::Scene *>(world.FindScene(main_scene))->entities;
  for (std::size_t index = 0; index < kRing; ++index)
    entities[index].parent = ring[(index + 1) % kRing];
  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  const auto start = std::chrono::steady_clock::now();
  const auto stats = *sync.Sync(world, gpu, Resolve, 1);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  Require(stats.rejected == kRing && stats.created == 0, "every entity on the cycle is rejected");
  // Without remembering failed walks this is 400 million steps.
  Require(elapsed < std::chrono::seconds(2), "rejecting a large cycle must stay linear");
}

void TestSceneBindingAndOwnership() {
  static_assert(!std::is_copy_constructible_v<runtime::RenderSceneSync> &&
                    !std::is_copy_assignable_v<runtime::RenderSceneSync> &&
                    !std::is_move_assignable_v<runtime::RenderSceneSync> &&
                    std::is_nothrow_move_constructible_v<runtime::RenderSceneSync>,
                "a sync owns GPU objects: no copies, no ownership-dropping assignment");
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  const auto mesh = Create(world, main_scene);

  renderer::GPUScene first;
  renderer::GPUScene second;
  // The second scene holds an unrelated object in the slot/generation the sync will use.
  const auto unrelated = second.Create({});
  runtime::RenderSceneSync sync;
  Require(sync.Sync(world, first, Resolve, 1).has_value() && *sync.Handle(mesh) == unrelated,
          "the test needs colliding handles");
  Require(!sync.Sync(world, second, Resolve, 2).has_value() &&
              second.Read(unrelated)->visibility_flags ==
                  renderer::VisibilityFlags(renderer::GPUObjectVisibility::Visible) &&
              second.GetStatistics().active_object_count == 1,
          "a sync bound to one scene must not touch another scene's objects");
  Require(!sync.Release(second, 3) && second.Read(unrelated).has_value() && sync.ObjectCount() == 1,
          "releasing through the wrong scene must destroy nothing and keep ownership");

  // Moving hands the objects over; the source is left empty and unbound.
  runtime::RenderSceneSync moved(std::move(sync));
  Require(moved.ObjectCount() == 1 && sync.ObjectCount() == 0, "a move must transfer ownership");
  Require(sync.Sync(world, second, Resolve, 4).has_value(), "a moved-from sync must be unbound");
  Require(sync.Release(second, 5), "releasing the moved-from sync's own objects failed");
  Require(moved.Release(first, 6) && first.GetStatistics().active_object_count == 0,
          "the new owner must release the moved objects");
  Require(moved.Sync(world, second, Resolve, 7).has_value(),
          "a released sync can serve another scene");
}

void TestSceneReplacedAtSameAddress() {
  // Codex's case: a scene destroyed and rebuilt in place, or cleared, reuses both the address and
  // the slot/generation pairs, so only the scene's instance identity tells it apart.
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  const auto mesh = Create(world, main_scene);
  std::optional<renderer::GPUScene> gpu;
  gpu.emplace();
  runtime::RenderSceneSync sync;
  Require(sync.Sync(world, *gpu, Resolve, 1).has_value(), "the first sync failed");
  const auto owned = *sync.Handle(mesh);
  gpu.reset();
  gpu.emplace();
  const auto unrelated = gpu->Create({});
  Require(unrelated == owned, "the test needs the rebuilt scene to reuse the handle");
  Require(!sync.Sync(world, *gpu, Resolve, 2).has_value() && !sync.Release(*gpu, 3) &&
              gpu->Read(unrelated).has_value() && gpu->GetStatistics().active_object_count == 1,
          "a scene rebuilt at the same address must not be taken for the bound one");
  sync.Abandon();
  Require(sync.ObjectCount() == 0 && sync.Sync(world, *gpu, Resolve, 4).has_value(),
          "after Abandon the sync can serve the new scene");

  // Clear() is the same hazard within one scene object.
  gpu->Clear();
  Require(!sync.Sync(world, *gpu, Resolve, 5).has_value(),
          "a cleared scene must not be taken for the bound one");
  sync.Abandon();

  // Moving the bound scene keeps the binding: the identity moves with the contents.
  Require(sync.Sync(world, *gpu, Resolve, 6).has_value(), "re-syncing the cleared scene failed");
  renderer::GPUScene moved(std::move(*gpu));
  Require(sync.Sync(world, moved, Resolve, 7).has_value() && sync.Release(moved, 8) &&
              moved.GetStatistics().active_object_count == 0,
          "a moved scene must stay bound to the sync that owns its objects");
}

void TestManySmallCyclesStayLinear() {
  // Codex's case: 10,000 independent two-node cycles. Without per-walk revisit detection each walk
  // runs on until it has taken as many steps as the scene has entities.
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  constexpr std::size_t kPairs = 10000;
  std::vector<Id> ids;
  for (std::size_t index = 0; index < 2 * kPairs; ++index)
    ids.push_back(Create(world, main_scene));
  auto &entities = const_cast<runtime::Scene *>(world.FindScene(main_scene))->entities;
  for (std::size_t pair = 0; pair < kPairs; ++pair) {
    entities[2 * pair].parent = ids[2 * pair + 1];
    entities[2 * pair + 1].parent = ids[2 * pair];
  }
  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  const auto start = std::chrono::steady_clock::now();
  const auto stats = *sync.Sync(world, gpu, Resolve, 1);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  Require(stats.rejected == 2 * kPairs && stats.created == 0,
          "every entity on a small cycle is rejected");
  Require(elapsed < std::chrono::seconds(2), "rejecting many small cycles must stay linear");
}

void TestIntermediateOverflowIsUnbounded() {
  // Codex's case: the final coordinate is finite (2e38 + 2e38 - 3e38 = 1e38), but the GPU's float
  // partial sum 2e38 + 2e38 overflows to inf first.
  runtime::TransformMatrix matrix{1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1};
  const auto bounds = runtime::TransformBounds(matrix, {{2.0e38F, 2.0e38F, -3.0e38F}, 1.0F});
  Require(std::isinf(bounds.radius),
          "bounds whose float evaluation could overflow must be reported as unbounded");
  // Large but safe values stay bounded.
  const auto safe = runtime::TransformBounds(matrix, {{1.0e37F, 1.0e37F, -1.0e37F}, 1.0F});
  Require(std::isfinite(safe.radius), "values well inside the float range must stay bounded");
}

void TestDeepChainsMatchWorldMatrix() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  constexpr int kDepth = 4000;
  Transform step{0.25, 0.0, 0.0};
  step.qz = std::sin(0.005);
  step.qw = std::cos(0.005);
  std::vector<Id> chain;
  for (int index = 0; index < kDepth; ++index)
    chain.push_back(Create(world, main_scene, step));
  // Attach from the leaf end, in one batch, so storage order differs from hierarchy order.
  runtime::WorldCommandBuffer attach;
  for (int index = kDepth - 1; index > 0; --index)
    attach.SetParent(chain[index], chain[index - 1], false);
  Require(attach.Apply(world), "reparenting failed");

  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  const auto start = std::chrono::steady_clock::now();
  const auto stats = *sync.Sync(world, gpu, Resolve, 1);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  Require(stats.created == static_cast<std::size_t>(kDepth), "every link must be mirrored");
  for (const auto id : {chain.front(), chain[kDepth / 2], chain.back()})
    Require(SameMatrix(Object(sync, gpu, id).current_transform,
                       runtime::ToRenderMatrix(*world.WorldMatrix(id))),
            "the memoized matrices must equal WorldMatrix bit for bit");
  // A per-entity walk would be ~8 million matrix products; the memoized pass is 4000.
  Require(elapsed < std::chrono::seconds(2), "the sync must stay linear on a deep chain");
}

Id CreateCamera(runtime::World &world, Id scene, Transform transform) {
  auto &entity = world.CreateEntity(scene);
  entity.transform = transform;
  entity.camera = true;
  return entity.id;
}

// Clip-space position of a world point through a view.
std::array<float, 4> Clip(const renderer::GPUDrivenView &view, float x, float y, float z) {
  std::array<float, 4> result{};
  for (std::size_t row = 0; row < 4; ++row)
    result[row] = view.view_projection(row, 0) * x + view.view_projection(row, 1) * y +
                  view.view_projection(row, 2) * z + view.view_projection(row, 3);
  return result;
}

bool Inside(const renderer::GPUDrivenView &view, float x, float y, float z) {
  const auto clip = Clip(view, x, y, z);
  return clip[3] > 0.0F && std::abs(clip[0]) <= clip[3] && std::abs(clip[1]) <= clip[3] &&
         clip[2] >= 0.0F && clip[2] <= clip[3];
}

void RequireSameView(const renderer::GPUDrivenView &actual, const math::Matrix4 &expected,
                     const char *message) {
  for (std::size_t index = 0; index < 16; ++index)
    Require(std::abs(actual.view_projection.values[index] - expected.values[index]) < 1e-4F,
            message);
}

void TestCameraView() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  const auto camera = CreateCamera(world, main_scene, {0.0, 0.0, 10.0});
  const auto projection = math::PerspectiveRadians(math::Radians(60.0F), 2.0F, 0.1F, 1000.0F);
  auto view = runtime::CameraView(world, camera, 2.0F);
  Require(view.has_value(), "a camera entity must produce a view");
  RequireSameView(*view, projection * math::LookAt({0, 0, 10}, {0, 0, 0}),
                  "a root camera must look down its local -Z (right-handed), like LookAt");
  Require(Inside(*view, 0.0F, 0.0F, 0.0F) && !Inside(*view, 0.0F, 0.0F, 20.0F),
          "the origin is in front of the camera and z=20 is behind it");
  Require(view->camera_position.z == 10.0F &&
              view->maximum_distance == std::numeric_limits<float>::max(),
          "the view must carry the camera position, and leave the far plane to the frustum");

  // Under a moved, rotated, and scaled parent the camera follows the parent (position through the
  // exact matrix) and ignores scale, as Unity does.
  Transform rig{100.0, 0.0, 0.0};
  rig.qy = kHalfSqrt2; // 90 degrees about +Y: local -Z becomes world -X
  rig.qw = kHalfSqrt2;
  rig.sx = rig.sy = rig.sz = 3.0;
  auto &parent = world.CreateEntity(main_scene);
  parent.transform = rig;
  const auto rig_id = parent.id;
  Require(Reparent(world, camera, rig_id, false), "parenting the camera failed");
  view = runtime::CameraView(world, camera, 2.0F);
  Require(view.has_value(), "a parented camera must produce a view");
  // Local (0, 0, 10) under the rig is world (130, 0, 0), looking toward -X.
  RequireSameView(*view, projection * math::LookAt({130, 0, 0}, {0, 0, 0}),
                  "a parented camera must use its world matrix, with scale ignored");
  Require(Inside(*view, 100.0F, 0.0F, 0.0F) && !Inside(*view, 150.0F, 0.0F, 0.0F),
          "the parented camera must see along the parent's rotated -Z");
  Require(Move(world, rig_id, {0.0, 0.0, 0.0}), "moving the rig failed");
  view = runtime::CameraView(world, camera, 2.0F);
  Require(view->camera_position.z == 10.0F, "moving the parent must move the camera");

  // Invalid inputs.
  Require(!runtime::CameraView(world, camera, 0.0F) &&
              !runtime::CameraView(world, camera, std::nanf("")) &&
              !runtime::CameraView(world, rig_id, 1.0F) &&
              !runtime::CameraView(world, 999'999, 1.0F),
          "a bad aspect, a non-camera, or a missing entity must produce no view");
  const auto set_camera = [&](runtime::CameraComponent data) {
    runtime::WorldCommandBuffer commands;
    commands.SetCamera(camera, data);
    Require(commands.Apply(world), "setting camera data failed");
  };
  // The last three are valid doubles that degenerate as the renderer's floats.
  for (const auto &bad :
       {runtime::CameraComponent{0.0, 0.1, 10.0}, runtime::CameraComponent{180.0, 0.1, 10.0},
        runtime::CameraComponent{60.0, 0.0, 10.0}, runtime::CameraComponent{60.0, 1.0, 1.0},
        runtime::CameraComponent{179.999999, 0.1, 10.0},
        runtime::CameraComponent{60.0, 1e-50, 10.0},
        runtime::CameraComponent{60.0, 1.0, 1.0 + 1e-12}}) {
    set_camera(bad);
    Require(!runtime::CameraView(world, camera, 1.0F), "invalid camera data must produce no view");
  }
}

void TestCameraIgnoresParentScale() {
  // Codex's case: a parent stretched 2x along X, the camera turned 45 degrees about Y. The view
  // must look along the world rotation (45 degrees), not the skewed matrix axis (~63 degrees).
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Transform stretched{};
  stretched.sx = 2.0;
  auto &parent = world.CreateEntity(main_scene);
  parent.transform = stretched;
  const auto parent_id = parent.id;
  Transform turned{};
  turned.qy = std::sin(0.3926990816987241); // 45 degrees about +Y
  turned.qw = std::cos(0.3926990816987241);
  const auto camera = CreateCamera(world, main_scene, turned);
  Require(Reparent(world, camera, parent_id, false), "parenting the camera failed");
  const auto projection = math::PerspectiveRadians(math::Radians(60.0F), 1.0F, 0.1F, 1000.0F);
  const float s = static_cast<float>(kHalfSqrt2);
  auto view = runtime::CameraView(world, camera, 1.0F);
  Require(view.has_value(), "a camera under a stretched parent must produce a view");
  RequireSameView(*view, projection * math::LookAt({0, 0, 0}, {-s, 0, -s}),
                  "non-uniform parent scale must not skew the view direction");

  // A mirroring parent (negative Z scale) must not flip the view either.
  Transform mirrored{};
  mirrored.sz = -1.0;
  Require(Move(world, parent_id, mirrored), "mirroring the parent failed");
  view = runtime::CameraView(world, camera, 1.0F);
  Require(view.has_value(), "a camera under a mirrored parent must produce a view");
  RequireSameView(*view, projection * math::LookAt({0, 0, 0}, {-s, 0, -s}),
                  "negative parent scale must not flip the view direction");
}

void TestFarCornersAreNotDistanceCulled() {
  // Codex's case: fov 60, aspect 2, far 100. (80, 0, -90) lies inside the frustum near its far
  // corner, yet 120 away from the camera; a radial limit at the far plane would cull it.
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  world.CreateEntity(main_scene).light = true;
  const auto camera = CreateCamera(world, main_scene, {});
  {
    runtime::WorldCommandBuffer commands;
    commands.SetCamera(camera, runtime::CameraComponent{60.0, 0.1, 100.0});
    Require(commands.Apply(world), "setting camera data failed");
  }
  Create(world, main_scene, {80.0, 0.0, -90.0});
  if (!runtime::SceneRenderingEnabled())
    return;
  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  auto device = rhi::CreateValidationDevice();
  const auto layout = rhi::TrianglePipelineLayout();
  const auto pipeline = device->CreatePipeline(
      {layout.layout_hash, 0x5254, rhi::TextureFormat::Rgba8Unorm, "Far corner"});
  const rhi::TextureDescriptor descriptor{640, 320, rhi::TextureFormat::Rgba8Unorm,
                                          rhi::ResourceState::Present, "Far corner target"};
  const auto target = device->CreateTexture(descriptor);
  const auto frame =
      sync.RenderFrame(world, gpu, Resolve, 1, *device, target, descriptor, pipeline);
  Require(frame && frame->frame.visible_meshes == 1 && frame->culling.distance_rejected == 0,
          "an object inside the frustum's far corner must not be distance-culled");
  Require(sync.Release(gpu, 2), "releasing failed");
  device->DestroyTexture(target);
  device->DestroyPipeline(pipeline);
}

void TestCulledSceneFrame() {
  runtime::World world;
  const auto main_scene = world.LoadScene("Main");
  Require(world.Activate(main_scene), "activation failed");
  auto &sun = world.CreateEntity(main_scene);
  sun.light = true;
  const auto sun_id = sun.id;
  const auto rig = Create(world, main_scene, {}, false);
  const auto camera = CreateCamera(world, main_scene, {0.0, 0.0, 10.0});
  Require(Reparent(world, camera, rig, false), "parenting the camera failed");
  const auto holder = Create(world, main_scene, {}, false);
  const auto mesh = Create(world, main_scene);
  Require(Reparent(world, mesh, holder, false), "parenting the mesh failed");

  renderer::GPUScene gpu;
  runtime::RenderSceneSync sync;
  auto device = rhi::CreateValidationDevice();
  const auto layout = rhi::TrianglePipelineLayout();
  const auto pipeline = device->CreatePipeline(
      {layout.layout_hash, 0x5253, rhi::TextureFormat::Rgba8Unorm, "Render sync"});
  const rhi::TextureDescriptor descriptor{640, 320, rhi::TextureFormat::Rgba8Unorm,
                                          rhi::ResourceState::Present, "Render sync target"};
  const auto target = device->CreateTexture(descriptor);
  const auto render = [&](std::uint64_t fence) {
    return sync.RenderFrame(world, gpu, Resolve, fence, *device, target, descriptor, pipeline);
  };

  if (!runtime::SceneRenderingEnabled()) {
    Require(!render(1) && sync.Handle(mesh), "a compiled-out frame must still sync");
    return;
  }
  auto frame = render(1);
  Require(frame && frame->camera == camera && frame->sync.created == 1 &&
              frame->frame.visible_meshes == 1 && frame->culling.frustum_rejected == 0,
          "a mesh in front of the camera must be submitted");
  const auto draws_with_mesh = device->Diagnostics().draw_calls;
  Require(draws_with_mesh == 3, "shadow, forward, and post-process must draw");

  // Moving the mesh's parent behind the camera culls it; the frame still runs.
  Require(Move(world, holder, {0.0, 0.0, 50.0}), "moving the holder failed");
  frame = render(2);
  Require(frame && frame->sync.updated == 1 && frame->frame.visible_meshes == 0 &&
              frame->culling.frustum_rejected == 1,
          "a mesh moved behind the camera through its parent must be culled");
  Require(device->Diagnostics().draw_calls == draws_with_mesh + 1,
          "a culled-empty frame must clear without zero-instance draws");

  // Turning the camera's parent around brings it back into view.
  Transform turned{};
  turned.qy = 1.0; // 180 degrees about +Y
  turned.qw = 0.0;
  Require(Move(world, rig, turned), "turning the rig failed");
  frame = render(3);
  Require(frame && frame->frame.visible_meshes == 1,
          "a camera turned by its parent must see what is now in front of it");

  // Beyond the far plane is culled by distance.
  {
    runtime::WorldCommandBuffer commands;
    commands.SetCamera(camera, runtime::CameraComponent{60.0, 0.1, 5.0});
    Require(commands.Apply(world), "shortening the far plane failed");
  }
  frame = render(4);
  Require(frame && frame->frame.visible_meshes == 0,
          "a mesh beyond the far plane must not be submitted");

  // No light: rejected, but the GPU scene is still kept in sync.
  {
    runtime::WorldCommandBuffer commands;
    commands.SetLight(sun_id, std::nullopt);
    commands.DestroyEntity(holder);
    Require(commands.Apply(world), "removing the light failed");
  }
  Require(!render(5) && !sync.Handle(mesh) && gpu.GetStatistics().active_object_count == 0,
          "a frame without a light must be rejected after syncing");
  device->DestroyTexture(target);
  device->DestroyPipeline(pipeline);
}

} // namespace

int main() {
  try {
    TestConversion();
    TestShearedBoundsAreConservative();
    TestParentMotionAndShear();
    TestOwnershipAndRemoval();
    TestOverflowIsRejected();
    TestCorruptCycleIsBounded();
    TestCorruptCyclesStayLinear();
    TestFloatMatrixBounds();
    TestSceneBindingAndOwnership();
    TestSceneReplacedAtSameAddress();
    TestManySmallCyclesStayLinear();
    TestIntermediateOverflowIsUnbounded();
    TestDeepChainsMatchWorldMatrix();
    TestCameraView();
    TestCameraIgnoresParentScale();
    TestFarCornersAreNotDistanceCulled();
    TestCulledSceneFrame();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
