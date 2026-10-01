// Parenting phase 4: the renderer sees each mesh renderer through its exact world matrix. Covers
// the matrix and bounds conversion, parent motion reaching descendants, shear, minimal dirty
// updates, ownership of visibility, removal paths, overflow rejection, and linear cost on deep
// chains.

#include "Nexora/Runtime/RenderSync.h"
#include "Nexora/Runtime/Runtime.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
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
  auto stats = sync.Sync(world, gpu, Resolve, 1);
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
  stats = sync.Sync(world, gpu, Resolve, 2);
  Require(stats.created == 0 && stats.updated == 0 && stats.destroyed == 0 &&
              gpu.ExtractUpdates().updates.empty(),
          "an unchanged world must produce no GPU updates");

  // Moving only the parent moves the rendered child.
  Require(Move(world, parent, {8.0, 0.0, 0.0}), "moving the parent failed");
  stats = sync.Sync(world, gpu, Resolve, 3);
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
  auto stats = sync.Sync(world, gpu, Resolve, 1);
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
  stats = sync.Sync(world, gpu, Resolve, 3);
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
  stats = sync.Sync(world, gpu, Resolve, 4);
  Require(stats.destroyed == 3 && !sync.Handle(root) && !sync.Handle(leaf) && !sync.Handle(other),
          "destroyed entities and removed mesh renderers must lose their objects");
  {
    runtime::WorldCommandBuffer commands;
    commands.SetMeshRenderer(loner, runtime::MeshComponent{0, {kShader}});
    Require(commands.Apply(world), "clearing the mesh failed");
  }
  stats = sync.Sync(world, gpu, Resolve, 5);
  Require(stats.destroyed == 1 && sync.ObjectCount() == 0,
          "a mesh the resolver rejects must leave the GPU scene");
  Require(gpu.GetStatistics().active_object_count == 0, "no object may leak in the GPU scene");

  // Unloading a scene removes its objects; Release() removes the rest.
  const auto again = Create(world, side_scene);
  const auto kept = Create(world, main_scene);
  (void)sync.Sync(world, gpu, Resolve, 6);
  Require(sync.Handle(again) && sync.Handle(kept), "new mesh renderers must be mirrored");
  Require(world.RequestUnload(side_scene), "unload failed");
  stats = sync.Sync(world, gpu, Resolve, 7);
  Require(stats.destroyed == 1 && !sync.Handle(again) && sync.Handle(kept),
          "a scene that stops being active must lose its objects");

  // An object destroyed behind the sync's back is mirrored again.
  Require(gpu.Destroy(*sync.Handle(kept), 8), "external destroy failed");
  stats = sync.Sync(world, gpu, Resolve, 8);
  Require(stats.created == 1 && gpu.Read(*sync.Handle(kept)).has_value(),
          "a stale handle must be replaced by a new object");
  sync.Release(gpu, 9);
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
  auto stats = sync.Sync(world, gpu, Resolve, 1);
  Require(stats.rejected == 1 && stats.created == 1 && !sync.Handle(inner) && sync.Handle(fine),
          "a world matrix outside float range must be rejected, not rendered as inf");

  // An object that becomes unrepresentable is removed, and comes back when it fits again.
  Require(Move(world, inner, {}), "shrinking failed");
  stats = sync.Sync(world, gpu, Resolve, 2);
  Require(stats.created == 1 && sync.Handle(inner), "a representable pose must be mirrored");
  Require(Move(world, inner, huge), "growing failed");
  stats = sync.Sync(world, gpu, Resolve, 3);
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
  const auto stats = sync.Sync(world, gpu, Resolve, 1);
  Require(stats.rejected == 2 && stats.created == 1 && sync.Handle(fine),
          "a cyclic parent chain must be rejected without hanging");
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
  const auto stats = sync.Sync(world, gpu, Resolve, 1);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  Require(stats.created == static_cast<std::size_t>(kDepth), "every link must be mirrored");
  for (const auto id : {chain.front(), chain[kDepth / 2], chain.back()})
    Require(SameMatrix(Object(sync, gpu, id).current_transform,
                       runtime::ToRenderMatrix(*world.WorldMatrix(id))),
            "the memoized matrices must equal WorldMatrix bit for bit");
  // A per-entity walk would be ~8 million matrix products; the memoized pass is 4000.
  Require(elapsed < std::chrono::seconds(2), "the sync must stay linear on a deep chain");
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
    TestDeepChainsMatchWorldMatrix();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
