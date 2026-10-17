#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/GPUDrivenPipeline.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <array>
#include <barrier>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace nexora;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class Function> void Reject(Function function, const char *message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception &) {
    rejected = true;
  }
  Require(rejected, message);
}
void TestAdmission() {
  using namespace rhi;
  PipelineDescriptor full{TrianglePipelineLayout().layout_hash, 0x4d455348,
                          TextureFormat::Rgba8Unorm, "mesh fallback"};
  auto mesh = full;
  mesh.graphics_shader = GraphicsShaderVariant::MeshTriangle;
  const auto required = EffectivePipelineRequirements(mesh);
  Require(required.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::MeshShader)] ==
                  FeatureVersion{1, 0} &&
              required.minimum_dxil_shader_model == FeatureVersion{6, 5},
          "mesh selection must imply feature and profile admission");
  Require(renderer::MakePipelineCacheKey(mesh) != renderer::MakePipelineCacheKey(full),
          "mesh and raster pipelines alias");
  auto invalid = mesh;
  invalid.type = PipelineType::Compute;
  Reject([&] { (void)EffectivePipelineRequirements(invalid); }, "compute mesh pipeline admitted");
  invalid = mesh;
  invalid.topology = PrimitiveTopology::PatchList;
  invalid.patch_control_points = 3;
  Reject([&] { (void)EffectivePipelineRequirements(invalid); }, "mesh patch pipeline admitted");
  auto model = CreateValidationDevice();
  Reject([&] { (void)model->CreatePipeline(mesh); }, "Validation fabricated native mesh support");
  auto commands = model->CreateCommandList(QueueType::Graphics);
  Reject([&] { commands->DispatchMesh(1); }, "unsupported backend fabricated mesh dispatch");
  Require(!IsBuiltinGeometryDrawValid(PrimitiveTopology::TriangleList,
                                      GraphicsShaderVariant::MeshTriangle, 3) &&
              !SupportsBuiltinIndirectDraw(PrimitiveTopology::TriangleList,
                                           GraphicsShaderVariant::MeshTriangle),
          "mesh pipeline admitted IA draws");
  ShaderResourceBindingMetadata binding{0, 0, BindingType::StorageBuffer,
                                        static_cast<std::uint8_t>(ShaderStage::Mesh)};
  std::string error;
  Require(ValidateShaderResourceBindings(std::span{&binding, 1}, {}, error),
          "mesh reflection stage rejected");
}
struct Workload final {
  std::array<rhi::BufferHandle, 4> buffers;
  renderer::GPUDrivenResult reference;
};
Workload MakeWorkload(rhi::Device &device, bool visible) {
  renderer::GPUSceneReferenceObject object;
  object.object = {7, 1};
  object.descriptor.world_bounds = {{visible ? 0.0F : 5.0F, 0, 0.25F}, 0.05F};
  object.descriptor.mesh_resource_index = 11;
  object.descriptor.material_resource_index = 13;
  object.descriptor.visibility_flags = 1;
  object.descriptor.lod.lod_count = 1;
  renderer::GPUSceneReferenceSnapshot scene;
  scene.objects.push_back(object);
  renderer::GPUDrivenView view;
  view.maximum_distance = 10;
  std::array<std::uint32_t, 56> packed{};
  packed[0] = 1;
  packed[4] = std::bit_cast<std::uint32_t>(view.maximum_distance);
  packed[6] = static_cast<std::uint32_t>(renderer::HiZPolicy::Invalidated);
  for (std::size_t index = 0; index < view.view_projection.values.size(); ++index)
    packed[20 + index] = std::bit_cast<std::uint32_t>(view.view_projection.values[index]);
  packed[44] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.center.x);
  packed[46] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.center.z);
  packed[47] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.radius);
  packed[48] = object.object.slot;
  packed[49] = object.object.generation;
  packed[50] = object.descriptor.mesh_resource_index;
  packed[51] = object.descriptor.material_resource_index;
  packed[52] = object.descriptor.visibility_flags;
  packed[53] = 1;
  Workload result{{device.CreateBuffer({sizeof(packed), "mesh candidates"}),
                   device.CreateBuffer({5 * sizeof(std::uint32_t), "mesh visible"}),
                   device.CreateBuffer({rhi::GPUDrivenIndirectCommandStride, "mesh indirect"}),
                   device.CreateBuffer({8 * sizeof(std::uint32_t), "mesh statistics"})},
                  renderer::BuildGPUDrivenCommands(scene, view)};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{packed}));
  Require(result.reference.instances.size() == (visible ? 1U : 0U), "mesh CPU culling fixture");
  return result;
}
void Produce(rhi::Device &device, rhi::PipelineHandle compute, const Workload &workload) {
  auto commands = device.CreateCommandList(rhi::QueueType::Compute);
  commands->BindPipeline(compute);
  for (std::uint32_t slot = 0; slot < workload.buffers.size(); ++slot)
    commands->BindStorageBuffer(slot, workload.buffers[slot]);
  commands->Dispatch(1);
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  std::array<std::uint32_t, 5> visible{};
  std::array<std::uint32_t, rhi::GPUDrivenIndirectCommandWords> indirect{};
  std::array<std::uint32_t, 8> counts{};
  device.ReadBufferForTesting(workload.buffers[1], 0, std::as_writable_bytes(std::span{visible}));
  device.ReadBufferForTesting(workload.buffers[2], 0, std::as_writable_bytes(std::span{indirect}));
  device.ReadBufferForTesting(workload.buffers[3], 0, std::as_writable_bytes(std::span{counts}));
  Require(counts[4] == workload.reference.instances.size() &&
              counts[5] == workload.reference.commands.size(),
          "GPU culling count differs from CPU");
  renderer::GPUDrivenResult actual;
  actual.statistics = {counts[0], counts[1], counts[2], counts[3]};
  if (counts[4]) {
    actual.instances.push_back({{visible[0], visible[1]}, visible[2], visible[3], visible[4]});
    actual.commands.push_back({indirect[4], indirect[5], indirect[6], indirect[7], indirect[8]});
    Require(indirect[0] == 3 && indirect[1] == 1 && indirect[2] == 0 && indirect[3] == 0,
            "visible native indirect prefix differs");
  } else {
    Require(indirect[0] == 0 && indirect[1] == 0, "culled indirect command is nonempty");
  }
  Require(renderer::CompareGPUDrivenResults(workload.reference, actual).matches,
          "mesh producer outputs differ from CPU oracle");
}
using Pixels = std::vector<std::byte>;
Pixels Read(rhi::Device &device, rhi::TextureHandle target) {
  Pixels pixels(64 * 64 * 4);
  device.ReadTextureForTesting(target, pixels);
  return pixels;
}
void Geometry(rhi::CommandList &commands, rhi::PipelineHandle pipeline, const Workload &workload,
              bool mesh) {
  commands.BindPipeline(pipeline);
  if (mesh) {
    commands.BindStorageBuffer(0, workload.buffers[3]);
    commands.DispatchMesh(1);
  } else {
    commands.BindIndirectBuffer(workload.buffers[2], 0, rhi::GPUDrivenIndirectCommandStride);
    commands.DrawIndirect(1);
  }
}
Pixels Draw(rhi::Device &device, rhi::TextureHandle target, rhi::PipelineHandle pipeline,
            const Workload &workload, bool mesh, bool clear = false) {
  auto commands = device.CreateCommandList(rhi::QueueType::Graphics);
  commands->Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
  commands->BeginRendering({target, 64, 64});
  if (!clear)
    Geometry(*commands, pipeline, workload, mesh);
  commands->EndRendering();
  commands->Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  return Read(device, target);
}
void Same(const Pixels &reference, const Pixels &actual) {
  Require(reference.size() == actual.size(), "mesh image extent differs");
  for (std::size_t index = 0; index < reference.size(); ++index)
    Require(
        std::abs(std::to_integer<int>(reference[index]) - std::to_integer<int>(actual[index])) <= 1,
        "mesh output differs from native indirect fallback");
}
void TestGraph(rhi::Device &device, rhi::PipelineHandle selected, rhi::PipelineHandle fallback,
               const Workload &workload, bool mesh, const Pixels &reference,
               rhi::TextureFormat format) {
  const rhi::TextureDescriptor descriptor{64, 64, format, rhi::ResourceState::ShaderRead,
                                          "mesh graph"};
  const std::array targets{device.CreateTexture(descriptor), device.CreateTexture(descriptor)};
  const auto sentinel = Draw(device, targets[0], fallback, workload, false, true);
  (void)Draw(device, targets[1], fallback, workload, false, true);
  renderer::RenderGraph graph;
  const auto access = mesh ? rhi::BufferAccess::ShaderRead : rhi::BufferAccess::IndirectRead;
  const auto input =
      graph.ImportBuffer(workload.buffers[mesh ? 3 : 2], access, rhi::QueueType::Graphics);
  std::array<renderer::GraphTexture, 2> textures;
  core::CancellationSource cancellation;
  std::barrier overlap{2};
  bool fail = false, cancel = false;
  for (std::size_t index = 0; index < targets.size(); ++index) {
    textures[index] = graph.ImportTexture(targets[index], descriptor);
    renderer::RenderGraph::PassDescriptor pass{
        "GPU selected geometry",
        rhi::QueueType::Graphics,
        {},
        {{textures[index], rhi::ResourceState::RenderTarget}},
        [&, index](auto &commands, auto) {
          commands.BeginRendering({targets[index], 64, 64});
          Geometry(commands, selected, workload, mesh);
          commands.EndRendering();
          overlap.arrive_and_wait();
          if (index == 0 && fail)
            throw std::runtime_error("intentional mesh recording failure");
          if (index == 1 && cancel)
            cancellation.Cancel();
        }};
    pass.buffer_reads.push_back({input, access});
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  for (auto texture : textures)
    (void)graph.AddPass({"restore mesh target",
                         rhi::QueueType::Graphics,
                         {{texture, rhi::ResourceState::ShaderRead}},
                         {},
                         [](auto &, auto) {}});
  graph.Compile();
  core::JobSystem jobs{2};
  jobs.Start();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 2};
  for (unsigned mode = 0; mode < 2; ++mode) {
    fail = mode == 0;
    cancel = mode == 1;
    options.cancellation = cancel ? cancellation.Token() : core::CancellationToken{};
    const auto before = device.Diagnostics();
    Reject([&] { graph.Execute(device, options); }, "aborted mesh graph succeeded");
    const auto after = device.Diagnostics();
    Require(before.submitted_command_lists == after.submitted_command_lists &&
                before.mesh_dispatch_calls == after.mesh_dispatch_calls,
            "aborted mesh graph committed work or native counts");
    for (auto target : targets)
      Require(Read(device, target) == sentinel, "aborted mesh graph changed pixels");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2,
          "mesh/fallback workers did not overlap");
  for (auto target : targets) {
    Same(reference, Read(device, target));
    device.DestroyTexture(target);
  }
  jobs.Stop();
}
void TestInvalidNative(rhi::Device &device, rhi::PipelineHandle mesh, const Workload &workload,
                       rhi::TextureHandle target, const Pixels &sentinel) {
  auto small = device.CreateBuffer({4, "too-small mesh statistics"});
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Graphics);
    abandoned->Transition(
        {target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    abandoned->BeginRendering({target, 64, 64});
    abandoned->BindPipeline(mesh);
    Reject([&] { abandoned->DispatchMesh(1); }, "mesh dispatch accepted missing input");
    Reject([&] { abandoned->Draw(3); }, "mesh pipeline emitted IA draw");
    Reject([&] { abandoned->DrawIndirect(1); }, "mesh pipeline emitted IA indirect draw");
    Reject([&] { abandoned->BindStorageBuffer(1, workload.buffers[3]); },
           "mesh accepted wrong slot");
    Reject([&] { abandoned->BindStorageBuffer(0, small); }, "mesh accepted undersized input");
    abandoned->BindStorageBuffer(0, workload.buffers[3]);
    Reject([&] { abandoned->DispatchMesh(0); }, "zero mesh groups admitted");
    Reject([&] { abandoned->DispatchMesh(2); }, "unbounded mesh groups admitted");
    abandoned->DispatchMesh(1);
    abandoned->EndRendering();
  }
  device.DestroyBuffer(small);
  Same(sentinel, Read(device, target));
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Graphics);
    stale->Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    stale->BeginRendering({target, 64, 64});
    Geometry(*stale, mesh, workload, true);
    stale->EndRendering();
    stale->Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
    std::uint32_t visible{};
    device.ReadBufferForTesting(workload.buffers[3], 4 * sizeof(visible),
                                std::as_writable_bytes(std::span{&visible, 1}));
    const auto before = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); }, "stale mesh input reached execution");
    Require(device.Diagnostics().submitted_command_lists == before, "stale mesh list submitted");
  }
  Same(sentinel, Read(device, target));
}
#if defined(_WIN32)
class Environment final {
public:
  Environment(const char *name, const char *replacement) : name_(name) {
    if (const auto *value = std::getenv(name)) {
      existed_ = true;
      previous_ = value;
    }
    _putenv_s(name, replacement ? replacement : "");
  }
  ~Environment() { _putenv_s(name_, existed_ ? previous_.c_str() : ""); }

private:
  const char *name_;
  std::string previous_;
  bool existed_{};
};
#endif
bool Required() {
  const auto *value = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
  return value && *value == '1';
}
void TestNative(bool enhanced) {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
  constexpr auto compute_artifact = "NEXORA_SLANG_COMPUTE_DXIL_PATH";
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
  constexpr auto compute_artifact = "NEXORA_SLANG_COMPUTE_METAL_PATH";
#else
  constexpr auto backend = rhi::Backend::Vulkan;
  constexpr auto compute_artifact = "NEXORA_SLANG_COMPUTE_SPIRV_PATH";
#endif
  const auto *artifact = std::getenv(compute_artifact);
  if (!artifact || !*artifact || !rhi::IsBackendAvailable(backend)) {
    Require(!Required(), "required native mesh/indirect artifacts or device unavailable");
    std::cout << "Native mesh/fallback unavailable\n";
    return;
  }
  auto device = rhi::CreateDeviceWithOptions(backend, {true, false, enhanced});
  auto compute = device->CreatePipeline(
      {1, 0x4d4353, rhi::TextureFormat::Rgba8Unorm, "mesh producer", rhi::PipelineType::Compute});
  const auto caps = device->Capabilities();
  for (const auto format : {rhi::TextureFormat::Rgba8Unorm, rhi::TextureFormat::Bgra8Unorm}) {
    rhi::PipelineDescriptor raster{rhi::TrianglePipelineLayout().layout_hash, 0x4d455348, format,
                                   "mesh reference"};
    auto mesh = raster;
    mesh.graphics_shader = rhi::GraphicsShaderVariant::MeshTriangle;
    const bool enabled =
        rhi::CheckGraphicsRequirements(caps, rhi::EffectivePipelineRequirements(mesh)).Succeeded();
    Require(!enabled || backend == rhi::Backend::Direct3D12, "unimplemented backend admitted mesh");
    if (!enabled)
      Reject([&] { (void)device->CreatePipeline(mesh); }, "unavailable mesh pipeline admitted");
#if defined(_WIN32)
    if (enabled) {
      {
        Environment missing{"NEXORA_SLANG_MESH_DXIL_PATH", nullptr};
        Reject([&] { (void)device->CreatePipeline(mesh); },
               "missing requested mesh artifact admitted");
      }
      {
        Environment wrong{"NEXORA_SLANG_MESH_DXIL_PATH",
                          std::getenv("NEXORA_SLANG_DXIL_VERTEX_PATH")};
        Reject([&] { (void)device->CreatePipeline(mesh); }, "wrong requested mesh stage admitted");
      }
      if (caps.highest_dxil_shader_model >= rhi::FeatureVersion{6, 6}) {
        auto newer = mesh;
        newer.graphics_requirements.minimum_dxil_shader_model = {6, 6};
        Reject([&] { (void)device->CreatePipeline(newer); },
               "selected mesh profile was not checked");
      }
    }
#endif
    const auto fallback = device->CreatePipeline(raster);
    const auto selected = enabled ? device->CreatePipeline(mesh) : fallback;
    const auto target =
        device->CreateTexture({64, 64, format, rhi::ResourceState::ShaderRead, "mesh"});
    for (bool visible : {false, true}) {
      auto workload = MakeWorkload(*device, visible);
      Produce(*device, compute, workload);
      const auto clear = Draw(*device, target, fallback, workload, false, true);
      const auto reference = Draw(*device, target, fallback, workload, false);
      Require((reference != clear) == visible, "indirect fallback culling pixels are incorrect");
      Same(reference, Draw(*device, target, selected, workload, enabled));
      if (enabled) {
        TestInvalidNative(*device, selected, workload, target, reference);
        Same(reference, Draw(*device, target, selected, workload, true));
      }
      TestGraph(*device, selected, fallback, workload, enabled, reference, format);
      for (auto buffer : workload.buffers)
        device->DestroyBuffer(buffer);
    }
    device->DestroyTexture(target);
    if (enabled)
      device->DestroyPipeline(selected);
    device->DestroyPipeline(fallback);
    Require(enabled ? device->Diagnostics().mesh_dispatch_calls > 0
                    : device->Diagnostics().mesh_dispatch_calls == 0,
            "mesh native execution counts do not match selected path");
    std::cout << "Native mesh backend=" << static_cast<unsigned>(backend) << " enabled=" << enabled
              << " dispatches=" << device->Diagnostics().mesh_dispatch_calls
              << "; GPU culling0/1, RGBA/BGRA indirect parity, shared input workers, "
                 "cancellation/replay passed\n";
  }
  device->DestroyPipeline(compute);
  const auto feature =
      device->Capabilities()
          .features[static_cast<std::size_t>(rhi::GraphicsFeature::EnhancedBarriers)];
  const auto diagnostics = device->Diagnostics();
  Require(!feature.enabled || (enhanced && feature.implemented &&
                               feature.support == rhi::CapabilitySupport::Supported),
          "invalid enhanced admission");
  if (feature.enabled)
    Require(diagnostics.enhanced_texture_barriers > 0 && diagnostics.enhanced_buffer_barriers > 0,
            "selected native geometry did not use enhanced barriers");
  else
    Require(diagnostics.enhanced_texture_barriers == 0 && diagnostics.enhanced_buffer_barriers == 0,
            "legacy geometry fabricated enhanced barriers");
  if (enhanced)
    std::cout << "Geometry enhanced barriers enabled=" << feature.enabled
              << " textures=" << diagnostics.enhanced_texture_barriers
              << " buffers=" << diagnostics.enhanced_buffer_barriers << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    const bool enhanced = argc == 2 && std::string_view{argv[1]} == "--enhanced-barriers";
    Require(argc == 1 || enhanced, "usage: [--enhanced-barriers]");
#if !defined(_WIN32)
    Require(!enhanced, "enhanced geometry requires D3D12");
#endif
    TestAdmission();
    TestNative(enhanced);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
