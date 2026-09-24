#include "Nexora/Renderer/GPUScene.h"

#include <algorithm>
#include <utility>

namespace nexora::renderer {
namespace {

[[nodiscard]] bool Equal(const math::Matrix4 &left, const math::Matrix4 &right) {
  return left.values == right.values;
}
[[nodiscard]] bool Equal(const math::Sphere &left, const math::Sphere &right) {
  return left.center.x == right.center.x && left.center.y == right.center.y &&
         left.center.z == right.center.z && left.radius == right.radius;
}

} // namespace

struct GPUScene::Impl final {
  struct Slot final {
    GPUObjectDescriptor descriptor{};
    std::uint32_t generation{1};
    GPUSceneDirtyFlags dirty{GPUSceneDirtyFlags::None};
    bool active{};
  };
  struct RetiredSlot final {
    std::uint32_t slot{};
    std::uint64_t fence{};
  };

  std::vector<Slot> slots;
  std::vector<std::uint32_t> free_slots;
  std::vector<RetiredSlot> retired_slots;
  std::vector<GPUSceneUpdate> retirement_updates;
  std::uint32_t active_count{};

  [[nodiscard]] Slot *Find(GPUObjectHandle object) {
    if (!object.IsValid() || object.slot >= slots.size())
      return nullptr;
    Slot &slot = slots[object.slot];
    return slot.active && slot.generation == object.generation ? &slot : nullptr;
  }
  [[nodiscard]] const Slot *Find(GPUObjectHandle object) const {
    if (!object.IsValid() || object.slot >= slots.size())
      return nullptr;
    const Slot &slot = slots[object.slot];
    return slot.active && slot.generation == object.generation ? &slot : nullptr;
  }
};

GPUScene::GPUScene() : impl_(std::make_unique<Impl>()) {}
GPUScene::~GPUScene() = default;
GPUScene::GPUScene(GPUScene &&) noexcept = default;
GPUScene &GPUScene::operator=(GPUScene &&) noexcept = default;

GPUObjectHandle GPUScene::Create(const GPUObjectDescriptor &descriptor) {
  std::uint32_t slot_index{};
  if (impl_->free_slots.empty()) {
    slot_index = static_cast<std::uint32_t>(impl_->slots.size());
    impl_->slots.emplace_back();
  } else {
    slot_index = impl_->free_slots.front();
    impl_->free_slots.erase(impl_->free_slots.begin());
  }

  Impl::Slot &slot = impl_->slots[slot_index];
  slot.descriptor = descriptor;
  slot.descriptor.previous_transform = descriptor.current_transform;
  slot.dirty = GPUSceneDirtyFlags::FullCreate;
  slot.active = true;
  ++impl_->active_count;
  return {slot_index, slot.generation};
}

bool GPUScene::Update(GPUObjectHandle object, const GPUObjectDescriptor &descriptor) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  if (!Equal(slot->descriptor.current_transform, descriptor.current_transform)) {
    slot->descriptor.current_transform = descriptor.current_transform;
    slot->dirty |= GPUSceneDirtyFlags::Transform;
  }
  if (!Equal(slot->descriptor.world_bounds, descriptor.world_bounds)) {
    slot->descriptor.world_bounds = descriptor.world_bounds;
    slot->dirty |= GPUSceneDirtyFlags::Bounds;
  }
  if (slot->descriptor.mesh_resource_index != descriptor.mesh_resource_index ||
      slot->descriptor.material_resource_index != descriptor.material_resource_index) {
    slot->descriptor.mesh_resource_index = descriptor.mesh_resource_index;
    slot->descriptor.material_resource_index = descriptor.material_resource_index;
    slot->dirty |= GPUSceneDirtyFlags::Resources;
  }
  if (slot->descriptor.visibility_flags != descriptor.visibility_flags) {
    slot->descriptor.visibility_flags = descriptor.visibility_flags;
    slot->dirty |= GPUSceneDirtyFlags::Visibility;
  }
  if (slot->descriptor.lod != descriptor.lod) {
    slot->descriptor.lod = descriptor.lod;
    slot->dirty |= GPUSceneDirtyFlags::LOD;
  }
  return true;
}

bool GPUScene::UpdateTransform(GPUObjectHandle object, const math::Matrix4 &transform) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  if (!Equal(slot->descriptor.current_transform, transform)) {
    slot->descriptor.current_transform = transform;
    slot->dirty |= GPUSceneDirtyFlags::Transform;
  }
  return true;
}

bool GPUScene::UpdateBounds(GPUObjectHandle object, const math::Sphere &bounds) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  if (!Equal(slot->descriptor.world_bounds, bounds)) {
    slot->descriptor.world_bounds = bounds;
    slot->dirty |= GPUSceneDirtyFlags::Bounds;
  }
  return true;
}

bool GPUScene::UpdateResources(GPUObjectHandle object, std::uint32_t mesh_resource_index,
                               std::uint32_t material_resource_index) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  if (slot->descriptor.mesh_resource_index != mesh_resource_index ||
      slot->descriptor.material_resource_index != material_resource_index) {
    slot->descriptor.mesh_resource_index = mesh_resource_index;
    slot->descriptor.material_resource_index = material_resource_index;
    slot->dirty |= GPUSceneDirtyFlags::Resources;
  }
  return true;
}

bool GPUScene::UpdateVisibility(GPUObjectHandle object, std::uint32_t visibility_flags) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  if (slot->descriptor.visibility_flags != visibility_flags) {
    slot->descriptor.visibility_flags = visibility_flags;
    slot->dirty |= GPUSceneDirtyFlags::Visibility;
  }
  return true;
}

bool GPUScene::UpdateLOD(GPUObjectHandle object, const GPUObjectLODMetadata &lod) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  if (slot->descriptor.lod != lod) {
    slot->descriptor.lod = lod;
    slot->dirty |= GPUSceneDirtyFlags::LOD;
  }
  return true;
}

bool GPUScene::Destroy(GPUObjectHandle object, std::uint64_t retire_fence) {
  Impl::Slot *slot = impl_->Find(object);
  if (slot == nullptr)
    return false;
  slot->active = false;
  slot->dirty = GPUSceneDirtyFlags::None;
  --impl_->active_count;
  impl_->retired_slots.push_back({object.slot, retire_fence});
  impl_->retirement_updates.push_back(
      {object, GPUSceneDirtyFlags::Retirement, slot->descriptor, retire_fence});
  ++slot->generation;
  if (slot->generation == 0)
    ++slot->generation;
  return true;
}

void GPUScene::Collect(std::uint64_t completed_fence) {
  auto retired = impl_->retired_slots.begin();
  while (retired != impl_->retired_slots.end()) {
    if (retired->fence <= completed_fence) {
      const auto position =
          std::lower_bound(impl_->free_slots.begin(), impl_->free_slots.end(), retired->slot);
      impl_->free_slots.insert(position, retired->slot);
      retired = impl_->retired_slots.erase(retired);
    } else {
      ++retired;
    }
  }
}

std::optional<GPUObjectDescriptor> GPUScene::Read(GPUObjectHandle object) const {
  const Impl::Slot *slot = impl_->Find(object);
  return slot == nullptr ? std::nullopt : std::optional<GPUObjectDescriptor>{slot->descriptor};
}

GPUSceneUploadBatch GPUScene::ExtractUpdates() {
  GPUSceneUploadBatch batch;
  batch.updates.reserve(impl_->active_count + impl_->retirement_updates.size());
  for (std::uint32_t index = 0; index < impl_->slots.size(); ++index) {
    Impl::Slot &slot = impl_->slots[index];
    if (slot.active && slot.dirty != GPUSceneDirtyFlags::None) {
      batch.updates.push_back({{index, slot.generation}, slot.dirty, slot.descriptor, 0});
      slot.dirty = GPUSceneDirtyFlags::None;
    }
  }
  batch.updates.insert(batch.updates.end(), impl_->retirement_updates.begin(),
                       impl_->retirement_updates.end());
  impl_->retirement_updates.clear();
  std::sort(batch.updates.begin(), batch.updates.end(), [](const auto &left, const auto &right) {
    if (left.object.slot != right.object.slot)
      return left.object.slot < right.object.slot;
    return left.object.generation < right.object.generation;
  });
  return batch;
}

GPUSceneReferenceSnapshot GPUScene::ExtractReferenceSnapshot() const {
  GPUSceneReferenceSnapshot snapshot;
  snapshot.objects.reserve(impl_->active_count);
  for (std::uint32_t index = 0; index < impl_->slots.size(); ++index) {
    const Impl::Slot &slot = impl_->slots[index];
    if (!slot.active)
      continue;
    snapshot.objects.push_back({{index, slot.generation}, slot.descriptor});
    if ((slot.descriptor.visibility_flags & VisibilityFlags(GPUObjectVisibility::Visible)) != 0)
      ++snapshot.draw_instance_count;
  }
  return snapshot;
}

GPUSceneStatistics GPUScene::GetStatistics() const noexcept {
  std::uint32_t dirty_count{};
  for (const Impl::Slot &slot : impl_->slots) {
    if (slot.active && slot.dirty != GPUSceneDirtyFlags::None)
      ++dirty_count;
  }
  return {impl_->active_count, static_cast<std::uint32_t>(impl_->slots.size()),
          static_cast<std::uint32_t>(impl_->retired_slots.size()),
          static_cast<std::uint32_t>(impl_->free_slots.size()), dirty_count};
}

void GPUScene::CommitFrame() {
  for (Impl::Slot &slot : impl_->slots) {
    if (slot.active)
      slot.descriptor.previous_transform = slot.descriptor.current_transform;
  }
}

void GPUScene::Clear() noexcept {
  impl_->slots.clear();
  impl_->free_slots.clear();
  impl_->retired_slots.clear();
  impl_->retirement_updates.clear();
  impl_->active_count = 0;
}

} // namespace nexora::renderer
