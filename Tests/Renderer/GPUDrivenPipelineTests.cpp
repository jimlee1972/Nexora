#include "Nexora/Renderer/GPUDrivenPipeline.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <bit>
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
  GPUSceneReferenceSnapshot scene;
  scene.objects = {Object(8, {0, 0, 0.2F}, 0.05F, 8, 2),  Object(2, {0.2F, 0, 0.2F}, 0.05F, 8, 2),
                   Object(7, {0, 0, 0.8F}, 0.05F, 3, 1),  Object(4, {2, 0, 0.2F}, 0.05F, 5, 0),
                   Object(5, {0, 0, 0.95F}, 0.05F, 7, 0), Object(9, {0, 0, 0.4F}, 0.05F, 6, 0)};
  scene.objects.back().descriptor.visibility_flags = 0;
  const float depth[] = {0.25F, 0.25F, 0.25F, 0.25F};
  const auto hi_z = HiZPyramid::Build(2, 2, depth);
  GPUDrivenView view;
  view.maximum_distance = 0.85F;
  view.lod_distances = {0.15F, 0.5F};
  view.hi_z = &hi_z;
  view.hi_z_policy = HiZPolicy::Enabled;
  const auto reference = BuildGPUDrivenCommands(scene, view);

  constexpr std::size_t header_words = 44;
  constexpr std::size_t candidate_words = 12;
  std::vector<std::uint32_t> packed(header_words + scene.objects.size() * candidate_words);
  const auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value); };
  packed[0] = static_cast<std::uint32_t>(scene.objects.size());
  packed[1] = hi_z.width;
  packed[2] = hi_z.height;
  packed[3] = static_cast<std::uint32_t>(hi_z.mips.size());
  packed[4] = bits(view.maximum_distance);
  packed[5] = bits(view.occlusion_bias);
  packed[6] = static_cast<std::uint32_t>(view.hi_z_policy);
  packed[7] = bits(view.camera_position.x);
  packed[8] = bits(view.camera_position.y);
  packed[9] = bits(view.camera_position.z);
  packed[10] = static_cast<std::uint32_t>(view.lod_distances.size());
  for (std::size_t index = 0; index < view.lod_distances.size(); ++index)
    packed[11 + index] = bits(view.lod_distances[index]);
  for (std::size_t index = 0; index < view.view_projection.values.size(); ++index)
    packed[20 + index] = bits(view.view_projection.values[index]);
  for (std::size_t index = 0; index < scene.objects.size(); ++index) {
    const auto &entry = scene.objects[index];
    const auto &object = entry.descriptor;
    const auto base = header_words + index * candidate_words;
    packed[base] = bits(object.world_bounds.center.x);
    packed[base + 1] = bits(object.world_bounds.center.y);
    packed[base + 2] = bits(object.world_bounds.center.z);
    packed[base + 3] = bits(object.world_bounds.radius);
    packed[base + 4] = entry.object.slot;
    packed[base + 5] = entry.object.generation;
    packed[base + 6] = object.mesh_resource_index;
    packed[base + 7] = object.material_resource_index;
    packed[base + 8] = object.visibility_flags;
    packed[base + 9] = object.lod.lod_count;
  }
  for (std::size_t mip = 0; mip < hi_z.mips.size(); ++mip) {
    packed[36 + mip] = static_cast<std::uint32_t>(packed.size());
    for (const float value : hi_z.mips[mip])
      packed.push_back(bits(value));
  }
  const auto input =
      device->CreateBuffer({packed.size() * sizeof(std::uint32_t), "compute candidates"});
  const auto visible =
      device->CreateBuffer({scene.objects.size() * 5 * sizeof(std::uint32_t), "compute visible"});
  const auto indirect =
      device->CreateBuffer({scene.objects.size() * 5 * sizeof(std::uint32_t), "compute indirect"});
  const auto statistics = device->CreateBuffer({8 * sizeof(std::uint32_t), "compute statistics"});
  device->WriteBuffer(input, 0, std::as_bytes(std::span{packed}));
  const auto pipeline = device->CreatePipeline(
      {1, 2, rhi::TextureFormat::Rgba8Unorm, "native compute", rhi::PipelineType::Compute});
  const auto graphics_pipeline =
      device->CreatePipeline({1, 1, rhi::TextureFormat::Rgba8Unorm, "native indirect"});
  const rhi::TextureDescriptor target_descriptor{1, 1, rhi::TextureFormat::Rgba8Unorm,
                                                 rhi::ResourceState::ShaderRead,
                                                 "GPU-driven graph token"};
  const auto target = device->CreateTexture(target_descriptor);
  RenderGraph graph;
  const auto token = graph.ImportTexture(target, target_descriptor);
  const auto compute_pass =
      graph.AddPass({"GPU-driven compute",
                     rhi::QueueType::Compute,
                     {},
                     {{token, rhi::ResourceState::ShaderRead}},
                     [&](rhi::CommandList &commands, std::span<const rhi::TextureHandle>) {
                       commands.BindStorageBuffer(0, input);
                       commands.BindStorageBuffer(1, visible);
                       commands.BindStorageBuffer(2, indirect);
                       commands.BindStorageBuffer(3, statistics);
                       commands.BindPipeline(pipeline);
                       commands.Dispatch(1);
                     }});
  const auto graphics_pass =
      graph.AddPass({"GPU-driven indirect draw",
                     rhi::QueueType::Graphics,
                     {},
                     {{token, rhi::ResourceState::RenderTarget}},
                     [&](rhi::CommandList &commands, std::span<const rhi::TextureHandle> textures) {
                       commands.BeginRendering({textures[token.id], 1, 1});
                       commands.BindPipeline(graphics_pipeline);
                       commands.BindIndirectBuffer(indirect);
                       commands.DrawIndirect(1);
                       commands.EndRendering();
                     }});
  graph.AddDependency(compute_pass, graphics_pass);
  graph.Compile();
  graph.Execute(*device);
  Require(graph.GetStatistics().queue_transfer_count == 2,
          "RenderGraph owns graphics-to-compute and compute-to-graphics queue transfers");
  std::vector<std::uint32_t> output(scene.objects.size() * 5);
  std::vector<std::uint32_t> arguments(scene.objects.size() * 5);
  std::uint32_t counts[8]{};
  device->ReadBufferForTesting(visible, 0, std::as_writable_bytes(std::span{output}));
  device->ReadBufferForTesting(indirect, 0, std::as_writable_bytes(std::span{arguments}));
  device->ReadBufferForTesting(statistics, 0, std::as_writable_bytes(std::span{counts}));
  GPUDrivenResult gpu_output;
  gpu_output.statistics = {counts[0], counts[1], counts[2], counts[3]};
  for (std::size_t index = 0; index < counts[4]; ++index) {
    const auto base = index * 5;
    gpu_output.instances.push_back(
        {{output[base], output[base + 1]}, output[base + 2], output[base + 3], output[base + 4]});
  }
  for (std::size_t index = 0; index < counts[5]; ++index) {
    const auto base = index * 5;
    gpu_output.commands.push_back({arguments[base], arguments[base + 1], arguments[base + 2],
                                   arguments[base + 3], arguments[base + 4]});
  }
  Require(CompareGPUDrivenResults(reference, gpu_output).matches,
          "native Vulkan stages match CPU frustum, distance/LOD, Hi-Z, compaction, "
          "classification, and indirect generation");
  Require(device->Diagnostics().compute_dispatches == 1 && device->Diagnostics().readbacks == 3,
          "Vulkan compute diagnostics distinguish dispatch from test-only readback");
  Require(device->Diagnostics().indirect_draw_calls == 1,
          "RenderGraph graphics pass consumes the compute stage before indirect drawing");
  device->DestroyTexture(target);
  device->DestroyPipeline(graphics_pipeline);
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
