#pragma once

#include "Nexora/Renderer/GPUScene.h"

#include <cstdint>
#include <span>
#include <vector>

namespace nexora::renderer {

struct NEXORA_RENDERER_API HiZPyramid final {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::vector<float>> mips;

  [[nodiscard]] static HiZPyramid Build(std::uint32_t width, std::uint32_t height,
                                        std::span<const float> depth);
};

enum class HiZPolicy : std::uint8_t { Enabled, Relaxed, Invalidated };

struct GPUDrivenView final {
  math::Matrix4 view_projection{};
  math::Vector3 camera_position{};
  float maximum_distance{10000.0F};
  std::vector<float> lod_distances;
  const HiZPyramid *hi_z{};
  HiZPolicy hi_z_policy{HiZPolicy::Invalidated};
  float occlusion_bias{0.001F};
};

struct VisibleInstance final {
  GPUObjectHandle object{};
  std::uint32_t mesh_resource_index{};
  std::uint32_t material_resource_index{};
  std::uint32_t lod_index{};
};

struct IndirectDrawCommand final {
  std::uint32_t mesh_resource_index{};
  std::uint32_t material_resource_index{};
  std::uint32_t lod_index{};
  std::uint32_t first_instance{};
  std::uint32_t instance_count{};
};

struct GPUDrivenStatistics final {
  std::uint32_t candidates{};
  std::uint32_t frustum_rejected{};
  std::uint32_t distance_rejected{};
  std::uint32_t occlusion_rejected{};
};

struct GPUDrivenResult final {
  std::vector<VisibleInstance> instances;
  std::vector<IndirectDrawCommand> commands;
  GPUDrivenStatistics statistics{};
};

// Deterministic reference implementation of the GPU compute stages. Backends upload the same
// inputs and compare their compacted instances and indirect arguments against this result.
[[nodiscard]] NEXORA_RENDERER_API GPUDrivenResult
BuildGPUDrivenCommands(const GPUSceneReferenceSnapshot &scene, const GPUDrivenView &view);

} // namespace nexora::renderer
