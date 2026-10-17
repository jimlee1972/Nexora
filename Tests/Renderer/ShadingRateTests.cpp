#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <algorithm>
#include <array>
#include <barrier>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
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
  PipelineDescriptor full{TrianglePipelineLayout().layout_hash, 0x565253, TextureFormat::Rgba8Unorm,
                          "full rate"};
  auto coarse = full;
  coarse.shading_rate = ShadingRate::Coarse2x2;
  const auto index = static_cast<std::size_t>(GraphicsFeature::VariableRateShading);
  Require(EffectivePipelineRequirements(full).minimum_tiers[index] == FeatureVersion{} &&
              EffectivePipelineRequirements(coarse).minimum_tiers[index] == FeatureVersion{1, 0},
          "shading rate must imply feature admission");
  Require(renderer::MakePipelineCacheKey(full) != renderer::MakePipelineCacheKey(coarse),
          "coarse and full-rate pipelines alias");
  auto invalid = coarse;
  invalid.type = PipelineType::Compute;
  Reject([&] { (void)EffectivePipelineRequirements(invalid); }, "compute VRS admitted");
  invalid = full;
  invalid.shading_rate = static_cast<ShadingRate>(255);
  Reject([&] { (void)EffectivePipelineRequirements(invalid); }, "unknown shading rate admitted");
  GraphicsDeviceCapabilities capabilities;
  auto &feature = capabilities.features[index];
  feature = {CapabilitySupport::Supported, {2, 0}, true, true, {1, 0}};
  RequirePipelineCapabilities(capabilities, coarse);
  coarse.graphics_requirements.minimum_tiers[index] = {2, 0};
  Require(CheckGraphicsRequirements(capabilities, EffectivePipelineRequirements(coarse)).failure ==
              CapabilityFailure::InsufficientImplementedTier,
          "hardware tier2 must not admit an unimplemented rate-image path");
  Require(!CapabilityFailureName(CapabilityFailure::InsufficientImplementedTier).empty(),
          "implementation ceiling has no diagnostic");
  // Existing four-field capability reports retain their original admission meaning.
  feature.implemented_tier = {};
  RequirePipelineCapabilities(capabilities, coarse);
  feature.implemented = false;
  Reject([&] { RequirePipelineCapabilities(capabilities, coarse); },
         "ceiling bypassed implementation");
  auto model = CreateValidationDevice();
  coarse.graphics_requirements = {};
  Reject([&] { (void)model->CreatePipeline(coarse); }, "Validation fabricated native VRS");
  auto pipeline = model->CreatePipeline(full);
  model->DestroyPipeline(pipeline);
  Require(model->Diagnostics().coarse_shading_draw_calls == 0, "model fabricated coarse draws");
}
using Pixels = std::vector<std::byte>;
Pixels Read(rhi::Device &device, rhi::TextureHandle target) {
  Pixels result(64 * 64 * 4);
  device.ReadTextureForTesting(target, result);
  return result;
}
void Record(rhi::CommandList &list, rhi::TextureHandle target, rhi::PipelineHandle pipeline,
            bool draw = true) {
  list.Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
  list.BeginRendering({target, 64, 64});
  if (draw) {
    list.BindPipeline(pipeline);
    list.Draw(3);
  }
  list.EndRendering();
  list.Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
}
Pixels Draw(rhi::Device &device, rhi::TextureHandle target, rhi::PipelineHandle pipeline,
            bool draw = true) {
  auto list = device.CreateCommandList(rhi::QueueType::Graphics);
  Record(*list, target, pipeline, draw);
  device.WaitForSubmission(device.Submit(*list));
  list.reset();
  return Read(device, target);
}
void Compare(const Pixels &full, const Pixels &coarse) {
  Require(full.size() == coarse.size(), "VRS image extent differs");
  unsigned covered = 0, changed = 0, maximum = 0;
  for (std::size_t offset = 0; offset < full.size(); offset += 4) {
    Require(full[offset + 3] == coarse[offset + 3], "VRS changed raster coverage");
    const auto visible = [](const Pixels &image, std::size_t pixel) {
      return image[pixel] != std::byte{0} || image[pixel + 1] != std::byte{0} ||
             image[pixel + 2] != std::byte{0};
    };
    Require(visible(full, offset) == visible(coarse, offset), "VRS changed triangle coverage");
    covered += visible(full, offset);
    for (unsigned channel = 0; channel < 3; ++channel) {
      const auto difference =
          static_cast<unsigned>(std::abs(std::to_integer<int>(full[offset + channel]) -
                                         std::to_integer<int>(coarse[offset + channel])));
      maximum = std::max(maximum, difference);
      changed += difference != 0;
      Require(difference <= 32, "2x2 VRS exceeded the bounded gradient error budget");
    }
  }
  Require(covered > 100, "VRS fixture has no nontrivial geometry");
  std::cout << "VRS gradient covered=" << covered << " changed_channels=" << changed
            << " maximum_delta=" << maximum << " budget=32/255\n";
}
void TestRecording(rhi::Device &device, rhi::PipelineHandle full, rhi::PipelineHandle selected,
                   const Pixels &reference, rhi::TextureFormat format, bool enabled) {
  const rhi::TextureDescriptor descriptor{64, 64, format, rhi::ResourceState::ShaderRead, "VRS"};
  const std::array targets{device.CreateTexture(descriptor), device.CreateTexture(descriptor)};
  const auto sentinel = Draw(device, targets[0], full, false);
  (void)Draw(device, targets[1], full, false);
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Graphics);
    Record(*abandoned, targets[0], selected);
  }
  Require(Read(device, targets[0]) == sentinel, "abandoned VRS changed pixels");
  // Coarse state is dynamic: a following full-rate bind, even in a later scope of the
  // same list, must restore 1x1 instead of inheriting the previous draw's setting.
  {
    auto mixed = device.CreateCommandList(rhi::QueueType::Graphics);
    Record(*mixed, targets[0], selected);
    Record(*mixed, targets[1], full);
    device.WaitForSubmission(device.Submit(*mixed));
  }
  Require(Read(device, targets[1]) == reference, "full-rate bind inherited coarse shading");
  Compare(reference, Read(device, targets[0]));
  for (auto target : targets)
    (void)Draw(device, target, full, false);
  renderer::RenderGraph graph;
  core::CancellationSource cancellation;
  std::barrier overlap{2};
  bool fail = false, cancel = false;
  std::array<renderer::GraphTexture, 2> imports;
  for (std::size_t index = 0; index < targets.size(); ++index) {
    const auto imported = graph.ImportTexture(targets[index], descriptor);
    imports[index] = imported;
    renderer::RenderGraph::PassDescriptor pass{"VRS gradient",
                                               rhi::QueueType::Graphics,
                                               {},
                                               {{imported, rhi::ResourceState::RenderTarget}},
                                               [&, index](auto &commands, auto) {
                                                 commands.BeginRendering({targets[index], 64, 64});
                                                 commands.BindPipeline(selected);
                                                 commands.Draw(3);
                                                 commands.EndRendering();
                                                 overlap.arrive_and_wait();
                                                 if (index == 0 && fail)
                                                   throw std::runtime_error(
                                                       "intentional VRS recording failure");
                                                 if (index == 1 && cancel)
                                                   cancellation.Cancel();
                                               }};
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  for (auto imported : imports) {
    (void)graph.AddPass({"restore VRS target",
                         rhi::QueueType::Graphics,
                         {{imported, rhi::ResourceState::ShaderRead}},
                         {},
                         [](auto &, auto) {}});
  }
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
    Reject([&] { graph.Execute(device, options); }, "aborted VRS graph succeeded");
    const auto after = device.Diagnostics();
    Require(after.submitted_command_lists == before.submitted_command_lists &&
                after.coarse_shading_draw_calls == before.coarse_shading_draw_calls,
            "aborted VRS graph committed work or diagnostics");
    for (auto target : targets)
      Require(Read(device, target) == sentinel, "aborted VRS graph changed pixels");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2, "VRS workers did not overlap");
  Require(Read(device, targets[0]) == Read(device, targets[1]), "VRS worker outputs differ");
  Compare(reference, Read(device, targets[0]));
  jobs.Stop();
  for (auto target : targets)
    device.DestroyTexture(target);
  Require(enabled || device.Diagnostics().coarse_shading_draw_calls == 0,
          "full-rate fallback fabricated coarse draws");
}
bool Required() {
  const auto *value = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
  return value && *value == '1';
}
void TestNative(bool enhanced) {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
#else
  constexpr auto backend = rhi::Backend::Vulkan;
#endif
  if (!rhi::IsBackendAvailable(backend)) {
    Require(!Required(), "required native VRS/fallback backend unavailable");
    std::cout << "Native VRS/fallback unavailable\n";
    return;
  }
  auto device = rhi::CreateDeviceWithOptions(backend, {enhanced, false, enhanced});
  const auto capabilities = device->Capabilities();
  for (const auto format : {rhi::TextureFormat::Rgba8Unorm, rhi::TextureFormat::Bgra8Unorm}) {
    rhi::PipelineDescriptor full{rhi::TrianglePipelineLayout().layout_hash, 0x565253, format,
                                 "VRS"};
    auto coarse = full;
    coarse.shading_rate = rhi::ShadingRate::Coarse2x2;
    const bool enabled =
        rhi::CheckGraphicsRequirements(capabilities, rhi::EffectivePipelineRequirements(coarse))
            .Succeeded();
    Require(!enabled || backend == rhi::Backend::Direct3D12, "unimplemented backend admitted VRS");
    const auto baseline = device->CreatePipeline(full);
    const auto selected = enabled ? device->CreatePipeline(coarse) : baseline;
    if (!enabled)
      Reject([&] { (void)device->CreatePipeline(coarse); }, "unavailable coarse path admitted");
    else {
      auto tier2 = coarse;
      tier2.graphics_requirements
          .minimum_tiers[static_cast<std::size_t>(rhi::GraphicsFeature::VariableRateShading)] = {2,
                                                                                                 0};
      Reject([&] { (void)device->CreatePipeline(tier2); }, "unimplemented VRS tier2 admitted");
    }
    const auto target =
        device->CreateTexture({64, 64, format, rhi::ResourceState::ShaderRead, "VRS"});
    const auto clear = Draw(*device, target, baseline, false);
    const auto reference = Draw(*device, target, baseline);
    Require(clear != reference, "native VRS reference is empty");
    Compare(reference, Draw(*device, target, selected));
    TestRecording(*device, baseline, selected, reference, format, enabled);
    device->DestroyTexture(target);
    if (enabled)
      device->DestroyPipeline(selected);
    device->DestroyPipeline(baseline);
    Require(!enabled || device->Diagnostics().coarse_shading_draw_calls > 0,
            "native coarse shading was not executed");
    std::cout
        << "VRS backend=" << static_cast<unsigned>(backend) << " enabled=" << enabled
        << " coarse_draws=" << device->Diagnostics().coarse_shading_draw_calls
        << "; RGBA/BGRA coverage/error, full-rate reset, abandonment, cancellation/replay passed\n";
  }
  const auto feature =
      device->Capabilities()
          .features[static_cast<std::size_t>(rhi::GraphicsFeature::EnhancedBarriers)];
  const auto diagnostics = device->Diagnostics();
  Require(!feature.enabled || (enhanced && feature.implemented &&
                               feature.support == rhi::CapabilitySupport::Supported),
          "invalid enhanced admission");
  if (feature.enabled)
    Require(diagnostics.enhanced_texture_barriers > 0,
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
