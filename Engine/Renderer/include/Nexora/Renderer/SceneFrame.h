#pragma once

#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/Api.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nexora::renderer {
struct SceneVertex final {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> uv{};
};

struct Camera final {
  std::array<float, 3> position{0.0F, 1.5F, 4.0F};
  std::array<float, 3> target{};
  float vertical_fov_radians{1.0471976F};
  float near_plane{0.1F};
  float far_plane{100.0F};
};

struct Material final {
  std::array<float, 4> base_color{1.0F, 1.0F, 1.0F, 1.0F};
  float roughness{0.5F};
  float metallic{};
};

struct Light final {
  std::array<float, 3> direction{-0.4F, -1.0F, -0.2F};
  std::array<float, 3> color{1.0F, 0.95F, 0.85F};
  float intensity{3.0F};
};

struct Mesh final {
  std::vector<SceneVertex> vertices;
  std::vector<std::uint16_t> indices;
};

// UV/normal seams (including mirrored islands) must already have split vertices. Returns owning
// unit tangent XYZ + handedness W; degenerate UVs use a stable orthogonal basis. No mesh mutation
// or serialized layout change. Invalid bounded geometry returns nullopt.
[[nodiscard]] NEXORA_RENDERER_API std::optional<std::vector<std::array<float, 4>>>
GenerateMeshTangents(const Mesh &mesh);

struct SceneFrame final {
  Camera camera;
  Light light;
  Mesh mesh;
  Material material;
};

struct SceneResourceCounts final {
  std::uint32_t vertex_buffers{};
  std::uint32_t index_buffers{};
  std::uint32_t constant_buffers{};
  std::uint32_t depth_textures{};
  std::uint32_t sampled_textures{};
  std::uint32_t samplers{};
};

class NEXORA_RENDERER_API FrameResources final {
public:
  FrameResources(rhi::Device &device, const SceneFrame &frame, std::uint32_t width,
                 std::uint32_t height);
  ~FrameResources();
  FrameResources(const FrameResources &) = delete;
  FrameResources &operator=(const FrameResources &) = delete;
  FrameResources(FrameResources &&) = delete;
  FrameResources &operator=(FrameResources &&) = delete;

  [[nodiscard]] rhi::BufferHandle Vertices() const noexcept { return vertices_; }
  [[nodiscard]] rhi::BufferHandle Indices() const noexcept { return indices_; }
  [[nodiscard]] rhi::BufferHandle Constants() const noexcept { return constants_; }
  [[nodiscard]] rhi::TextureHandle Depth() const noexcept { return depth_; }
  [[nodiscard]] rhi::TextureHandle Albedo() const noexcept { return albedo_; }
  [[nodiscard]] std::uint32_t IndexCount() const noexcept { return index_count_; }
  [[nodiscard]] SceneResourceCounts Counts() const noexcept;

private:
  rhi::Device *device_{};
  rhi::BufferHandle vertices_{};
  rhi::BufferHandle indices_{};
  rhi::BufferHandle constants_{};
  rhi::TextureHandle depth_{};
  rhi::TextureHandle albedo_{};
  std::uint32_t index_count_{};
};

[[nodiscard]] NEXORA_RENDERER_API SceneFrame MakeProceduralRenderingRoom();
[[nodiscard]] NEXORA_RENDERER_API bool ValidateSceneFrame(const SceneFrame &frame) noexcept;
} // namespace nexora::renderer
