#pragma once

#include "Nexora/Foundation/Types.h"
#include "Nexora/Math/Math.h"
#include "Nexora/Renderer/Api.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace nexora::renderer {

inline constexpr std::uint32_t kInvalidGPUObjectSlot = UINT32_MAX;

struct GPUObjectHandle final {
  std::uint32_t slot{kInvalidGPUObjectSlot};
  std::uint32_t generation{};

  [[nodiscard]] constexpr bool IsValid() const noexcept {
    return slot != kInvalidGPUObjectSlot && generation != 0;
  }
  friend constexpr bool operator==(GPUObjectHandle, GPUObjectHandle) = default;
};

enum class GPUObjectVisibility : std::uint32_t {
  None = 0,
  Visible = 1U << 0U,
  CastsShadow = 1U << 1U,
  ReceivesShadow = 1U << 2U,
};

[[nodiscard]] constexpr std::uint32_t VisibilityFlags(GPUObjectVisibility value) noexcept {
  return static_cast<std::uint32_t>(value);
}

struct GPUObjectLODMetadata final {
  std::uint32_t selected_lod{};
  std::uint32_t lod_count{1};
  float transition_fraction{};
  float screen_size{};
  friend constexpr bool operator==(const GPUObjectLODMetadata &,
                                   const GPUObjectLODMetadata &) = default;
};

struct GPUObjectDescriptor final {
  math::Matrix4 current_transform{};
  math::Matrix4 previous_transform{};
  math::Sphere world_bounds{};
  std::uint32_t mesh_resource_index{};
  std::uint32_t material_resource_index{};
  std::uint32_t visibility_flags{VisibilityFlags(GPUObjectVisibility::Visible)};
  GPUObjectLODMetadata lod{};
};

enum class GPUSceneDirtyFlags : std::uint32_t {
  None = 0,
  Transform = 1U << 0U,
  Bounds = 1U << 1U,
  Resources = 1U << 2U,
  Visibility = 1U << 3U,
  LOD = 1U << 4U,
  FullCreate = 1U << 5U,
  Retirement = 1U << 6U,
};

[[nodiscard]] constexpr GPUSceneDirtyFlags operator|(GPUSceneDirtyFlags left,
                                                     GPUSceneDirtyFlags right) noexcept {
  return static_cast<GPUSceneDirtyFlags>(static_cast<std::uint32_t>(left) |
                                         static_cast<std::uint32_t>(right));
}
constexpr GPUSceneDirtyFlags &operator|=(GPUSceneDirtyFlags &left,
                                         GPUSceneDirtyFlags right) noexcept {
  left = left | right;
  return left;
}
[[nodiscard]] constexpr bool HasDirtyFlag(GPUSceneDirtyFlags value,
                                          GPUSceneDirtyFlags flag) noexcept {
  return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

struct GPUSceneUpdate final {
  GPUObjectHandle object{};
  GPUSceneDirtyFlags dirty_flags{GPUSceneDirtyFlags::None};
  GPUObjectDescriptor descriptor{};
  std::uint64_t retire_fence{};
};

struct GPUSceneUploadBatch final {
  std::vector<GPUSceneUpdate> updates;
};

struct GPUSceneStatistics final {
  std::uint32_t active_object_count{};
  std::uint32_t allocated_slot_count{};
  std::uint32_t retired_slot_count{};
  std::uint32_t free_slot_count{};
  std::uint32_t dirty_object_count{};
};

struct GPUSceneReferenceObject final {
  GPUObjectHandle object{};
  GPUObjectDescriptor descriptor{};
};

struct GPUSceneReferenceSnapshot final {
  std::vector<GPUSceneReferenceObject> objects;
  std::uint32_t draw_instance_count{};
};

class NEXORA_RENDERER_API GPUScene final {
public:
  GPUScene();
  ~GPUScene();
  GPUScene(GPUScene &&) noexcept;
  GPUScene &operator=(GPUScene &&) noexcept;
  GPUScene(const GPUScene &) = delete;
  GPUScene &operator=(const GPUScene &) = delete;

  [[nodiscard]] GPUObjectHandle Create(const GPUObjectDescriptor &descriptor);
  [[nodiscard]] bool Update(GPUObjectHandle object, const GPUObjectDescriptor &descriptor);
  [[nodiscard]] bool UpdateTransform(GPUObjectHandle object, const math::Matrix4 &transform);
  [[nodiscard]] bool UpdateBounds(GPUObjectHandle object, const math::Sphere &bounds);
  [[nodiscard]] bool UpdateResources(GPUObjectHandle object, std::uint32_t mesh_resource_index,
                                     std::uint32_t material_resource_index);
  [[nodiscard]] bool UpdateVisibility(GPUObjectHandle object, std::uint32_t visibility_flags);
  [[nodiscard]] bool UpdateLOD(GPUObjectHandle object, const GPUObjectLODMetadata &lod);
  [[nodiscard]] bool Destroy(GPUObjectHandle object, std::uint64_t retire_fence);
  void Collect(std::uint64_t completed_fence);

  [[nodiscard]] std::optional<GPUObjectDescriptor> Read(GPUObjectHandle object) const;
  [[nodiscard]] GPUSceneUploadBatch ExtractUpdates();
  [[nodiscard]] GPUSceneReferenceSnapshot ExtractReferenceSnapshot() const;
  [[nodiscard]] GPUSceneStatistics GetStatistics() const noexcept;

  // Advances motion history after extraction. Multiple transform writes before this call preserve
  // the previous frame's transform.
  void CommitFrame();
  void Clear() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace nexora::renderer
