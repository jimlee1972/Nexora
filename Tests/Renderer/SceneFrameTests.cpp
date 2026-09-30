#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/SceneFrame.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora;
  const auto first = renderer::MakeProceduralRenderingRoom();
  const auto second = renderer::MakeProceduralRenderingRoom();
  Require(renderer::ValidateSceneFrame(first), "procedural room must be valid");
  Require(first.mesh.vertices.size() == 8 && first.mesh.indices.size() == 36,
          "procedural cube topology changed");
  Require(first.mesh.indices == second.mesh.indices &&
              first.material.base_color == second.material.base_color,
          "procedural room must be deterministic");

  auto invalid = first;
  invalid.mesh.indices.push_back(999);
  Require(!renderer::ValidateSceneFrame(invalid), "out-of-range index must be rejected");
  invalid = first;
  invalid.camera.near_plane = invalid.camera.far_plane;
  Require(!renderer::ValidateSceneFrame(invalid), "invalid camera clip range must be rejected");
  invalid = first;
  invalid.material.roughness = std::nanf("");
  Require(!renderer::ValidateSceneFrame(invalid), "non-finite material must be rejected");

  auto device = rhi::CreateValidationDevice();
  {
    renderer::FrameResources resources{*device, first, 640, 360};
    const auto counts = resources.Counts();
    Require(counts.vertex_buffers == 1 && counts.index_buffers == 1 &&
                counts.constant_buffers == 1 && counts.depth_textures == 1 &&
                counts.sampled_textures == 1 && counts.samplers == 1,
            "frame resource diagnostics are incomplete");
    const rhi::TextureDescriptor target_descriptor{640, 360, rhi::TextureFormat::Rgba8Unorm,
                                                   rhi::ResourceState::Present, "target"};
    const auto target = device->CreateTexture(target_descriptor);
    const auto layout = rhi::TrianglePipelineLayout();
    const auto pipeline =
        device->CreatePipeline({layout.layout_hash, 1, rhi::TextureFormat::Rgba8Unorm, "scene"});
    const auto result =
        renderer::ExecuteSceneFrame(*device, target, target_descriptor, pipeline, resources);
    Require(result.passes == 1 && result.barriers == 2, "scene frame diagnostics are unexpected");
    Require(device->Diagnostics().draw_calls == 1, "scene indexed draw was not submitted");
    device->DestroyPipeline(pipeline);
    device->DestroyTexture(target);
  }

  bool rejected_zero_extent = false;
  try {
    renderer::FrameResources invalid_resources{*device, first, 0, 360};
  } catch (const std::invalid_argument &) {
    rejected_zero_extent = true;
  }
  Require(rejected_zero_extent, "zero extent must fail before allocating resources");
  device->WaitIdle();
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
