#include "Nexora/Renderer/GPUScene.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace nexora;
using namespace nexora::renderer;

namespace {

void Require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

math::Matrix4 Translation(float x, float y, float z) {
  math::Matrix4 result;
  result(0, 3) = x;
  result(1, 3) = y;
  result(2, 3) = z;
  return result;
}

GPUObjectDescriptor Descriptor(std::uint32_t identity) {
  GPUObjectDescriptor descriptor;
  descriptor.current_transform = Translation(static_cast<float>(identity), 2.0F, 3.0F);
  descriptor.world_bounds = {{static_cast<float>(identity), 0.0F, 0.0F}, 4.0F};
  descriptor.mesh_resource_index = identity * 2U;
  descriptor.material_resource_index = identity * 2U + 1U;
  descriptor.lod = {identity % 3U, 3U, 0.25F, 0.5F};
  return descriptor;
}

void TestCreateDirtyAndMotionHistory() {
  GPUScene scene;
  auto descriptor = Descriptor(7);
  descriptor.previous_transform = Translation(99.0F, 0.0F, 0.0F);
  const GPUObjectHandle object = scene.Create(descriptor);
  Require(object.IsValid(), "create returns a valid handle");
  Require(scene.Read(object)->previous_transform.values == descriptor.current_transform.values,
          "new object initializes previous transform from current");

  auto uploads = scene.ExtractUpdates();
  Require(uploads.updates.size() == 1 &&
              HasDirtyFlag(uploads.updates[0].dirty_flags, GPUSceneDirtyFlags::FullCreate),
          "create emits exactly one full upload");
  Require(scene.ExtractUpdates().updates.empty(), "clean frame emits no uploads");

  scene.CommitFrame();
  const auto first = Translation(10.0F, 0.0F, 0.0F);
  const auto second = Translation(11.0F, 0.0F, 0.0F);
  Require(scene.UpdateTransform(object, first) && scene.UpdateTransform(object, second),
          "multiple transform writes succeed");
  const auto updated = scene.Read(object);
  Require(updated->current_transform.values == second.values &&
              updated->previous_transform.values == descriptor.current_transform.values,
          "writes preserve prior-frame motion history");
  uploads = scene.ExtractUpdates();
  Require(uploads.updates.size() == 1 &&
              uploads.updates[0].dirty_flags == GPUSceneDirtyFlags::Transform,
          "transform-only update has a narrow dirty mask");
  scene.CommitFrame();
  Require(scene.Read(object)->previous_transform.values == second.values,
          "commit advances previous transform");
}

void TestStableSlotsRetirementAndStaleHandles() {
  GPUScene scene;
  const GPUObjectHandle first = scene.Create(Descriptor(1));
  const GPUObjectHandle stable = scene.Create(Descriptor(2));
  static_cast<void>(scene.ExtractUpdates());
  Require(scene.UpdateBounds(stable, {{1.0F, 2.0F, 3.0F}, 9.0F}), "active update succeeds");
  Require(scene.Read(stable).has_value() && stable.slot == 1, "ordinary update preserves slot");

  Require(scene.Destroy(first, 10), "destroy succeeds");
  Require(!scene.Destroy(first, 10), "repeated destroy is rejected");
  Require(!scene.Read(first).has_value() && !scene.UpdateVisibility(first, 0),
          "stale read and update are rejected");
  const auto retirement = scene.ExtractUpdates();
  Require(retirement.updates.size() == 2 && retirement.updates[0].object.slot == 0 &&
              HasDirtyFlag(retirement.updates[0].dirty_flags, GPUSceneDirtyFlags::Retirement) &&
              retirement.updates[1].object.slot == 1,
          "retirement and dirty records are deterministically slot sorted");

  scene.Collect(9);
  const GPUObjectHandle before_fence = scene.Create(Descriptor(3));
  Require(before_fence.slot == 2, "retired slot is unavailable before its fence");
  scene.Collect(10);
  const GPUObjectHandle reused = scene.Create(Descriptor(4));
  Require(reused.slot == first.slot && reused.generation != first.generation,
          "completed fence permits reuse with a new generation");
  Require(!scene.UpdateResources(first, 1, 2), "old generation remains stale after reuse");
  Require(scene.Read(reused)->previous_transform.values ==
              scene.Read(reused)->current_transform.values,
          "reused slot does not inherit old transform history");
}

void TestDirtyCategoriesAndReferenceSnapshot() {
  GPUScene scene;
  const GPUObjectHandle first = scene.Create(Descriptor(1));
  const GPUObjectHandle second = scene.Create(Descriptor(2));
  static_cast<void>(scene.ExtractUpdates());

  Require(scene.UpdateResources(first, 44, 55), "resource update succeeds");
  Require(scene.UpdateVisibility(first, 0), "visibility update succeeds");
  Require(scene.UpdateLOD(first, {2, 4, 0.75F, 0.2F}), "LOD update succeeds");
  Require(scene.UpdateBounds(second, {{9.0F, 8.0F, 7.0F}, 6.0F}), "bounds update succeeds");
  const auto uploads = scene.ExtractUpdates();
  Require(uploads.updates.size() == 2 &&
              uploads.updates[0].object.slot < uploads.updates[1].object.slot,
          "dirty uploads are slot ordered");
  Require(HasDirtyFlag(uploads.updates[0].dirty_flags, GPUSceneDirtyFlags::Resources) &&
              HasDirtyFlag(uploads.updates[0].dirty_flags, GPUSceneDirtyFlags::Visibility) &&
              HasDirtyFlag(uploads.updates[0].dirty_flags, GPUSceneDirtyFlags::LOD) &&
              uploads.updates[1].dirty_flags == GPUSceneDirtyFlags::Bounds,
          "resource, visibility, LOD, and bounds categories are retained");

  const auto reference = scene.ExtractReferenceSnapshot();
  Require(reference.objects.size() == 2 && reference.draw_instance_count == 1,
          "reference extractor reports active and visible draw counts");
  Require(reference.objects[0].object == first && reference.objects[1].object == second &&
              reference.objects[0].descriptor.mesh_resource_index == 44 &&
              reference.objects[0].descriptor.material_resource_index == 55 &&
              reference.objects[0].descriptor.lod.selected_lod == 2 &&
              reference.objects[1].descriptor.world_bounds.radius == 6.0F,
          "reference extractor preserves slot, resources, LOD, and bounds");
}

void TestLargeDeterministicSetAndClear() {
  GPUScene scene;
  std::vector<GPUObjectHandle> handles;
  constexpr std::uint32_t object_count = 2048;
  handles.reserve(object_count);
  for (std::uint32_t index = 0; index < object_count; ++index)
    handles.push_back(scene.Create(Descriptor(index)));
  const auto uploads = scene.ExtractUpdates();
  Require(uploads.updates.size() == object_count, "large set emits every create");
  for (std::uint32_t index = 0; index < object_count; ++index)
    Require(uploads.updates[index].object.slot == index, "large upload set is deterministic");
  const auto reference = scene.ExtractReferenceSnapshot();
  Require(reference.objects.size() == object_count && reference.draw_instance_count == object_count,
          "large reference snapshot is equivalent");

  Require(scene.Destroy(handles[0], 100), "pending retirement can be created");
  scene.Clear();
  const auto statistics = scene.GetStatistics();
  Require(statistics.active_object_count == 0 && statistics.allocated_slot_count == 0 &&
              statistics.retired_slot_count == 0 && scene.ExtractUpdates().updates.empty() &&
              scene.ExtractReferenceSnapshot().objects.empty(),
          "clear releases active and pending retirement state");
}

} // namespace

int main() {
  TestCreateDirtyAndMotionHistory();
  TestStableSlotsRetirementAndStaleHandles();
  TestDirtyCategoriesAndReferenceSnapshot();
  TestLargeDeterministicSetAndClear();
  std::cout << "GPUScene tests passed\n";
  return 0;
}
