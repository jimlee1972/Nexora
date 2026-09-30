#include "Nexora/Renderer/SceneFrame.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace nexora::renderer {
namespace {
template <typename T> std::span<const std::byte> Bytes(std::span<const T> values) {
  return std::as_bytes(values);
}
bool Finite(float value) { return std::isfinite(value); }
} // namespace

bool ValidateSceneFrame(const SceneFrame &frame) noexcept {
  if (frame.mesh.vertices.empty() || frame.mesh.indices.empty() ||
      frame.camera.near_plane <= 0.0F || frame.camera.far_plane <= frame.camera.near_plane ||
      frame.camera.vertical_fov_radians <= 0.0F || !Finite(frame.camera.vertical_fov_radians) ||
      !Finite(frame.material.roughness) || frame.material.roughness < 0.0F ||
      frame.material.roughness > 1.0F || !Finite(frame.light.intensity) ||
      frame.light.intensity < 0.0F)
    return false;
  for (const auto index : frame.mesh.indices)
    if (index >= frame.mesh.vertices.size())
      return false;
  return true;
}

SceneFrame MakeProceduralRenderingRoom() {
  SceneFrame frame;
  frame.mesh.vertices = {
      SceneVertex{{-1.0F, -1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F}},
      SceneVertex{{1.0F, -1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F}},
      SceneVertex{{1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}},
      SceneVertex{{-1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}},
      SceneVertex{{-1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {1.0F, 1.0F}},
      SceneVertex{{1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F}},
      SceneVertex{{1.0F, 1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F}},
      SceneVertex{{-1.0F, 1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F}},
  };
  frame.mesh.indices = {0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5,
                        4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4};
  frame.material.base_color = {0.18F, 0.55F, 0.95F, 1.0F};
  frame.material.roughness = 0.35F;
  return frame;
}

FrameResources::FrameResources(rhi::Device &device, const SceneFrame &frame, std::uint32_t width,
                               std::uint32_t height)
    : device_(&device) {
  if (!ValidateSceneFrame(frame) || width == 0 || height == 0)
    throw std::invalid_argument("invalid scene frame resources");
  try {
    vertices_ = device.CreateBuffer(
        {frame.mesh.vertices.size() * sizeof(SceneVertex), "Rendering Room vertices"});
    indices_ = device.CreateBuffer(
        {frame.mesh.indices.size() * sizeof(std::uint16_t), "Rendering Room indices"});
    struct Constants final {
      Camera camera;
      Light light;
      Material material;
    } constants{frame.camera, frame.light, frame.material};
    constants_ = device.CreateBuffer({sizeof(constants), "Rendering Room constants"});
    device.WriteBuffer(vertices_, 0, Bytes<SceneVertex>(frame.mesh.vertices));
    device.WriteBuffer(indices_, 0, Bytes<std::uint16_t>(frame.mesh.indices));
    device.WriteBuffer(constants_, 0, Bytes<Constants>(std::span{&constants, 1U}));
    depth_ = device.CreateTexture({width, height, rhi::TextureFormat::Depth32Float,
                                   rhi::ResourceState::Undefined, "Rendering Room depth"});
    albedo_ = device.CreateTexture({1, 1, rhi::TextureFormat::Rgba8Unorm,
                                    rhi::ResourceState::ShaderRead, "Rendering Room albedo"});
    const std::array texel{static_cast<std::byte>(46), static_cast<std::byte>(140),
                           static_cast<std::byte>(242), static_cast<std::byte>(255)};
    device.WriteTextureRgba8(albedo_, texel, 4);
    index_count_ = static_cast<std::uint32_t>(frame.mesh.indices.size());
  } catch (...) {
    if (albedo_.IsValid())
      device.DestroyTexture(albedo_);
    if (depth_.IsValid())
      device.DestroyTexture(depth_);
    if (constants_.IsValid())
      device.DestroyBuffer(constants_);
    if (indices_.IsValid())
      device.DestroyBuffer(indices_);
    if (vertices_.IsValid())
      device.DestroyBuffer(vertices_);
    throw;
  }
}

FrameResources::~FrameResources() {
  if (!device_)
    return;
  device_->WaitIdle();
  device_->DestroyTexture(albedo_);
  device_->DestroyTexture(depth_);
  device_->DestroyBuffer(constants_);
  device_->DestroyBuffer(indices_);
  device_->DestroyBuffer(vertices_);
}

SceneResourceCounts FrameResources::Counts() const noexcept { return {1, 1, 1, 1, 1, 1}; }
} // namespace nexora::renderer
