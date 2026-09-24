#include "Nexora/Renderer/GPUDrivenPipeline.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

using namespace nexora;
using namespace nexora::renderer;

namespace {
void Require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
GPUSceneReferenceObject Object(std::uint32_t slot, math::Vector3 center, float radius,
                               std::uint32_t mesh, std::uint32_t material,
                               std::uint32_t lod_count = 3) {
  GPUSceneReferenceObject result;
  result.object = {slot, 1};
  result.descriptor.world_bounds = {center, radius};
  result.descriptor.mesh_resource_index = mesh;
  result.descriptor.material_resource_index = material;
  result.descriptor.lod.lod_count = lod_count;
  return result;
}
void TestCullingLODCompactionAndClassification() {
  GPUSceneReferenceSnapshot scene;
  scene.objects = {Object(0, {0, 0, 0.2F}, 0.05F, 8, 2), Object(1, {0.2F, 0, 0.2F}, 0.05F, 8, 2),
                   Object(2, {0, 0, 0.8F}, 0.05F, 3, 1), Object(3, {2, 0, 0.2F}, 0.05F, 5, 0),
                   Object(4, {0, 0, 0.9F}, 0.05F, 6, 0)};
  scene.objects.back().descriptor.visibility_flags = 0;
  GPUDrivenView view;
  view.maximum_distance = 0.85F;
  view.lod_distances = {0.15F, 0.5F};
  const auto result = BuildGPUDrivenCommands(scene, view);
  Require(result.statistics.candidates == 5 && result.statistics.frustum_rejected == 1,
          "frustum and visibility stages reject expected candidates");
  Require(result.instances.size() == 3 && result.commands.size() == 2,
          "visible instances are compacted into bins");
  Require(result.commands[0].material_resource_index == 1 && result.commands[0].lod_index == 2 &&
              result.commands[0].instance_count == 1 &&
              result.commands[1].material_resource_index == 2 &&
              result.commands[1].lod_index == 1 && result.commands[1].instance_count == 2,
          "LOD selection and indirect arguments are deterministic");
  Require(result.commands[0].first_instance == 0 && result.commands[1].first_instance == 1,
          "indirect offsets address compacted storage");
}
void TestHiZAndInvalidation() {
  const float depth[] = {0.25F, 0.25F, 0.25F, 0.25F};
  const auto pyramid = HiZPyramid::Build(2, 2, depth);
  Require(pyramid.mips.size() == 2 && pyramid.mips[1][0] == 0.25F,
          "Hi-Z builds a max-depth mip chain");
  GPUSceneReferenceSnapshot scene;
  scene.objects = {Object(0, {0, 0, 0.8F}, 0.05F, 1, 1)};
  GPUDrivenView view;
  view.hi_z = &pyramid;
  view.hi_z_policy = HiZPolicy::Enabled;
  auto result = BuildGPUDrivenCommands(scene, view);
  Require(result.instances.empty() && result.statistics.occlusion_rejected == 1,
          "Hi-Z rejects a sphere behind stored depth");
  view.hi_z_policy = HiZPolicy::Invalidated;
  result = BuildGPUDrivenCommands(scene, view);
  Require(result.instances.size() == 1, "invalidated history accepts candidates");
}
void TestInvalidDepthInput() {
  const float depth[] = {1.0F};
  const auto invalid = HiZPyramid::Build(2, 2, depth);
  Require(invalid.mips.empty() && invalid.width == 0, "invalid depth dimensions fail closed");
}
void TestReferenceComparison() {
  GPUSceneReferenceSnapshot scene;
  scene.objects = {Object(3, {0, 0, 0.2F}, 0.05F, 4, 2)};
  const auto reference = BuildGPUDrivenCommands(scene, {});
  auto gpu_output = reference;
  Require(CompareGPUDrivenResults(reference, gpu_output).matches,
          "identical GPU output matches the CPU reference");
  ++gpu_output.commands[0].instance_count;
  const auto mismatch = CompareGPUDrivenResults(reference, gpu_output);
  Require(!mismatch.matches && mismatch.first_command_mismatch == 0,
          "indirect argument mismatch identifies its first command");
}
void TestNormalPathRecordsNoReadbackOn(const std::unique_ptr<rhi::Device> &device,
                                       std::string_view label) {
  auto compute = device->CreateCommandList(rhi::QueueType::Compute);
  auto graphics = device->CreateCommandList(rhi::QueueType::Graphics);
  const auto target = device->CreateTexture(
      {1, 1, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::RenderTarget, "target"});
  const auto pipeline =
      device->CreatePipeline({1, 1, rhi::TextureFormat::Rgba8Unorm, "gpu-driven"});
  graphics->BeginRendering({target, 1, 1});
  graphics->BindPipeline(pipeline);
  RecordGPUDrivenExecution(*compute, *graphics, 4096, 7);
  graphics->EndRendering();
  device->Submit(*compute);
  device->Submit(*graphics);
  const auto diagnostics = device->Diagnostics();
  Require(diagnostics.compute_dispatches == 1 && diagnostics.indirect_draw_calls == 1 &&
              diagnostics.draw_calls == 1,
          std::string(label) + ": normal path dispatches once and submits GPU-generated bins "
                               "indirectly");
  Require(diagnostics.readbacks == 0,
          std::string(label) + ": normal GPU-driven execution records no readback");
  device->DestroyPipeline(pipeline);
  device->DestroyTexture(target);
}
void TestNormalPathRecordsNoReadback() {
  TestNormalPathRecordsNoReadbackOn(rhi::CreateValidationDevice(), "validation backend");
}
void TestNormalPathOnVulkan() {
  // Only Vulkan implements both Dispatch (vkCmdDispatch) and DrawIndirect
  // (vkCmdDrawIndirect) today -- D3D12/Metal override neither yet (V2-M3
  // Phase 3/4), so this is deliberately Vulkan-specific rather than a
  // generic "whichever native backend is available" check, which would
  // throw on those two.
  if (!rhi::IsBackendAvailable(rhi::Backend::Vulkan))
    return;
  TestNormalPathRecordsNoReadbackOn(rhi::CreateDevice(rhi::Backend::Vulkan), "Vulkan backend");
}
} // namespace
int main() {
  TestCullingLODCompactionAndClassification();
  TestHiZAndInvalidation();
  TestInvalidDepthInput();
  TestReferenceComparison();
  TestNormalPathRecordsNoReadback();
  TestNormalPathOnVulkan();
  std::cout << "GPU-driven pipeline tests passed\n";
  return 0;
}
