#include "Nexora/Renderer/GPUDrivenPipeline.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
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
  const auto compute_pipeline = device->CreatePipeline(
      {1, 2, rhi::TextureFormat::Rgba8Unorm, "gpu-driven compute", rhi::PipelineType::Compute});
  const auto storage = device->CreateBuffer({sizeof(std::uint32_t), "gpu-driven storage"});
  compute->BindPipeline(compute_pipeline);
  compute->BindStorageBuffer(0, storage);
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
  device->DestroyPipeline(compute_pipeline);
  device->DestroyBuffer(storage);
  device->DestroyTexture(target);
}
void TestNormalPathRecordsNoReadback() {
  TestNormalPathRecordsNoReadbackOn(rhi::CreateValidationDevice(), "validation backend");
}
void TestDispatchPreconditionsOnVulkan() {
  // Only Vulkan implements Dispatch (vkCmdDispatch) today -- D3D12/Metal
  // override neither Dispatch nor DrawIndirect yet (V2-M3 Phase 3/4).
  //
  // Keep the explicit argument-validation regression alongside the real native dispatch below.
  if (!rhi::IsBackendAvailable(rhi::Backend::Vulkan)) {
    const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
    Require(!(required && *required == '1'), "Vulkan is required but unavailable");
    return;
  }
  auto device = rhi::CreateDevice(rhi::Backend::Vulkan);
  auto compute = device->CreateCommandList(rhi::QueueType::Compute);
  const auto pipeline = device->CreatePipeline(
      {1, 2, rhi::TextureFormat::Rgba8Unorm, "compute", rhi::PipelineType::Compute});
  const auto storage = device->CreateBuffer({sizeof(std::uint32_t), "compute storage"});
  compute->BindPipeline(pipeline);
  compute->BindStorageBuffer(0, storage);
  bool threw = false;
  try {
    compute->Dispatch(0);
  } catch (const std::logic_error &) {
    threw = true;
  }
  Require(threw, "Vulkan Dispatch must reject a zero group count before touching the driver");
  device->DestroyBuffer(storage);
  device->DestroyPipeline(pipeline);
}
void TestNativeComputeOnVulkan() {
  if (!rhi::IsBackendAvailable(rhi::Backend::Vulkan)) {
    const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
    Require(!(required && *required == '1'), "Vulkan is required but unavailable");
    return;
  }
  auto device = rhi::CreateDevice(rhi::Backend::Vulkan);
  const std::uint32_t candidates[] = {4, 7, 12, 15};
  const std::uint32_t initial_statistics[4]{0, 0, 4, 0};
  const auto input = device->CreateBuffer({sizeof(candidates), "compute candidates"});
  const auto visible = device->CreateBuffer({sizeof(candidates), "compute visible"});
  const auto indirect = device->CreateBuffer({sizeof(candidates), "compute indirect"});
  const auto statistics = device->CreateBuffer({sizeof(initial_statistics), "compute statistics"});
  device->WriteBuffer(input, 0, std::as_bytes(std::span{candidates}));
  device->WriteBuffer(statistics, 0, std::as_bytes(std::span{initial_statistics}));
  const auto pipeline = device->CreatePipeline(
      {1, 2, rhi::TextureFormat::Rgba8Unorm, "native compute", rhi::PipelineType::Compute});
  auto commands = device->CreateCommandList(rhi::QueueType::Compute);
  commands->BindStorageBuffer(0, input);
  commands->BindStorageBuffer(1, visible);
  commands->BindStorageBuffer(2, indirect);
  commands->BindStorageBuffer(3, statistics);
  commands->BindPipeline(pipeline);
  commands->Dispatch(1);
  const auto completion = device->Submit(*commands);
  device->WaitForSubmission(completion);
  std::uint32_t output[4]{};
  std::uint32_t counts[4]{};
  device->ReadBufferForTesting(visible, 0, std::as_writable_bytes(std::span{output}));
  device->ReadBufferForTesting(statistics, 0, std::as_writable_bytes(std::span{counts}));
  Require(counts[0] == 2 && counts[1] == 2, "Vulkan compute culls and compacts candidates");
  Require((output[0] == 4 || output[0] == 12) && (output[1] == 4 || output[1] == 12) &&
              output[0] != output[1],
          "Vulkan storage-buffer output contains the visible candidates");
  Require(device->Diagnostics().compute_dispatches == 1 && device->Diagnostics().readbacks == 2,
          "Vulkan compute diagnostics distinguish dispatch from test-only readback");
  device->DestroyPipeline(pipeline);
  device->DestroyBuffer(statistics);
  device->DestroyBuffer(indirect);
  device->DestroyBuffer(visible);
  device->DestroyBuffer(input);
}
} // namespace
int main() {
  TestCullingLODCompactionAndClassification();
  TestHiZAndInvalidation();
  TestInvalidDepthInput();
  TestReferenceComparison();
  TestNormalPathRecordsNoReadback();
  TestDispatchPreconditionsOnVulkan();
  TestNativeComputeOnVulkan();
  std::cout << "GPU-driven pipeline tests passed\n";
  return 0;
}
