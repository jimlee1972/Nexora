#if defined(_WIN32)
// Exercise public headers after WinGDI's DeviceCapabilities macro has already been defined.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "Nexora/Renderer/PipelineCache.h"

#if defined(NEXORA_TEST_X11_HEADERS)
#include <X11/Xlib.h>
#endif

#include "Nexora/RHI/Capabilities.h"

#include "../../Engine/RHI/src/D3D12CapabilityMapping.h"

#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace nexora::rhi;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

GraphicsRequirements Needs(GraphicsFeature feature, FeatureVersion tier = {1, 0}) {
  GraphicsRequirements result;
  result.minimum_tiers[static_cast<std::size_t>(feature)] = tier;
  return result;
}

void TestAdmissionAndFallback() {
  auto device = CreateValidationDevice();
  auto capabilities = device->Capabilities();
  Require(!GraphicsDeviceCapabilities{}.independent_compute_recording &&
              capabilities.independent_compute_recording,
          "limited compute admission must default closed and distinguish validation support");
  Require(!GraphicsDeviceCapabilities{}.independent_graphics_recording &&
              capabilities.independent_graphics_recording,
          "limited graphics admission must default closed and identify validation support");
  Require(capabilities.backend == Backend::Null && capabilities.validation_only &&
              capabilities.software_device,
          "validation capability snapshot must identify the software validation model");
  const auto compute = Needs(GraphicsFeature::Compute);
  const auto mesh = Needs(GraphicsFeature::MeshShader);
  auto selection = SelectGraphicsPath(capabilities, mesh, compute);
  Require(selection.path == GraphicsPath::Fallback &&
              selection.preferred.failure == CapabilityFailure::Unsupported &&
              selection.preferred.feature == GraphicsFeature::MeshShader &&
              selection.fallback.Succeeded(),
          "missing mesh path must select the explicitly supported compute fallback");

  // Model hardware that advertises Mesh Shader while this backend still lacks an execution path.
  auto &available = capabilities.features[static_cast<std::size_t>(GraphicsFeature::MeshShader)];
  available = {CapabilitySupport::Supported, {1, 0}, true, false};
  selection = SelectGraphicsPath(capabilities, mesh, compute);
  Require(selection.path == GraphicsPath::Fallback &&
              selection.preferred.failure == CapabilityFailure::NotImplemented,
          "hardware support must not authorize a missing native backend implementation");
  available.implemented = true;
  available.enabled = false;
  Require(CheckGraphicsRequirements(capabilities, mesh).failure == CapabilityFailure::NotEnabled,
          "implemented but disabled features must remain unavailable");
  available.enabled = true;
  Require(SelectGraphicsPath(capabilities, mesh, compute).path == GraphicsPath::Preferred,
          "fully available preferred path was not selected");
  available.support = CapabilitySupport::Unknown;
  selection = SelectGraphicsPath(capabilities, mesh, Needs(GraphicsFeature::RayTracing));
  Require(selection.path == GraphicsPath::Unavailable &&
              selection.preferred.failure == CapabilityFailure::UnknownSupport &&
              selection.fallback.failure == CapabilityFailure::Unsupported,
          "unknown support must fail closed without fabricating a fallback");
  Require(CheckGraphicsRequirements(capabilities, {}).Succeeded(),
          "legacy pipelines with no additional requirements must stay admissible");
}

void TestTiersAndShaderModels() {
  GraphicsDeviceCapabilities capabilities;
  auto &rt = capabilities.features[static_cast<std::size_t>(GraphicsFeature::RayTracing)];
  rt = {CapabilitySupport::Supported, {1, 1}, true, true};
  Require(
      CheckGraphicsRequirements(capabilities, Needs(GraphicsFeature::RayTracing, {1, 2})).failure ==
          CapabilityFailure::InsufficientTier,
      "DXR 1.1 must not satisfy a DXR 1.2 requirement");
  Require(CheckGraphicsRequirements(capabilities, Needs(GraphicsFeature::RayTracing, {1, 0}))
              .Succeeded(),
          "higher tier must satisfy a lower-tier requirement");
  capabilities.features[static_cast<std::size_t>(GraphicsFeature::SamplerFeedback)] = {
      CapabilitySupport::Supported, {0, 9}, true, true};
  Require(CheckGraphicsRequirements(capabilities, Needs(GraphicsFeature::SamplerFeedback, {0, 9}))
              .Succeeded(),
          "Sampler Feedback 0.9 must not be confused with an absent requirement");
  Require(CheckGraphicsRequirements(capabilities, Needs(GraphicsFeature::SamplerFeedback, {1, 0}))
                  .failure == CapabilityFailure::InsufficientTier,
          "feedback tier 0.9 must not satisfy tier 1.0");
  GraphicsRequirements shader;
  shader.minimum_dxil_shader_model = {6, 9};
  Require(CheckGraphicsRequirements(capabilities, shader).failure ==
              CapabilityFailure::UnknownShaderModel,
          "unknown shader model must fail closed");
  capabilities.dxil_shader_model_support = CapabilitySupport::Unsupported;
  Require(CheckGraphicsRequirements(capabilities, shader).failure ==
              CapabilityFailure::UnsupportedShaderModel,
          "non-DXIL backends must not inherit DirectX shader model guarantees");
  capabilities.dxil_shader_model_support = CapabilitySupport::Supported;
  capabilities.highest_dxil_shader_model = {6, 6};
  Require(CheckGraphicsRequirements(capabilities, shader).failure ==
              CapabilityFailure::InsufficientShaderModel,
          "SM 6.6 must not admit SM 6.9 artifacts");
  capabilities.highest_dxil_shader_model = {6, 10};
  Require(CheckGraphicsRequirements(capabilities, shader).Succeeded(),
          "SM 6.10 must compare newer than SM 6.9 numerically");
  shader.minimum_dxil_shader_model = {6, 10};
  capabilities.highest_dxil_shader_model = {6, 9};
  Require(!CheckGraphicsRequirements(capabilities, shader).Succeeded(),
          "SM 6.9 must not admit SM 6.10 preview requirements");
}

void TestNativeTierDecoding() {
  // Values from the SDK contract: sampler feedback uses 90/100, unlike DXR's 10/11/12.
  Require(detail::DecodeD3D12Tier(GraphicsFeature::SamplerFeedback, 90) == FeatureVersion{0, 9} &&
              detail::DecodeD3D12Tier(GraphicsFeature::SamplerFeedback, 100) ==
                  FeatureVersion{1, 0},
          "sampler-feedback SDK encoding must not be interpreted as tiers 9.0 or 10.0");
  Require(detail::DecodeD3D12Tier(GraphicsFeature::RayTracing, 12) == FeatureVersion{1, 2} &&
              detail::DecodeD3D12Tier(GraphicsFeature::VariableRateShading, 2) ==
                  FeatureVersion{2, 0} &&
              detail::DecodeD3D12Tier(GraphicsFeature::MeshShader, 10) == FeatureVersion{1, 0} &&
              detail::DecodeD3D12Tier(GraphicsFeature::WorkGraphs, 10) == FeatureVersion{1, 0},
          "native feature tier encodings must map to independent portable versions");
  Require(!detail::DecodeD3D12Tier(GraphicsFeature::RayTracing, 99) &&
              !detail::DecodeD3D12Tier(GraphicsFeature::SamplerFeedback, 9),
          "unknown native enum values must remain unknown");
  Require(detail::DecodeD3D12Tier(GraphicsFeature::SamplerFeedback, 0) == FeatureVersion{},
          "native NOT_SUPPORTED must remain distinct from unknown");
}

void TestPipelineAndCacheAdmission() {
  auto device = CreateValidationDevice();
  core::JobSystem jobs{2};
  jobs.Start();
  {
    renderer::PipelineCache cache{*device, jobs};
    PipelineDescriptor baseline{1, 2, TextureFormat::Rgba8Unorm, "baseline"};
    const auto accepted = cache.Request(baseline).Get();
    Require(accepted.IsValid(), "legacy pipeline must remain usable");
    auto advanced = baseline;
    advanced.graphics_requirements = Needs(GraphicsFeature::MeshShader);
    Require(renderer::MakePipelineCacheKey(baseline) != renderer::MakePipelineCacheKey(advanced),
            "feature requirements must participate in complete PSO cache identity");
    bool rejected = false;
    try {
      (void)cache.Request(advanced).Get();
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    Require(rejected && cache.Size() == 2,
            "cached legacy PSO must not bypass advanced feature admission");
    Require(cache.Request(baseline).Get() == accepted,
            "rejected advanced request must not invalidate the original cached pipeline");
    auto shader = baseline;
    shader.graphics_requirements.minimum_dxil_shader_model = {6, 9};
    Require(renderer::MakePipelineCacheKey(shader) != renderer::MakePipelineCacheKey(baseline),
            "shader-model requirement must participate in PSO cache identity");
    rejected = false;
    try {
      (void)device->CreatePipeline(shader);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    Require(rejected, "device pipeline creation must enforce admission independently of the cache");
  }
  jobs.Stop();
  const auto diagnostics = device->Diagnostics();
  Require(diagnostics.submitted_command_lists == 0 && diagnostics.draw_calls == 0 &&
              diagnostics.compute_dispatches == 0,
          "capability queries and rejected pipelines must not submit GPU work");
}
} // namespace

int main() {
  try {
    TestAdmissionAndFallback();
    TestTiersAndShaderModels();
    TestNativeTierDecoding();
    TestPipelineAndCacheAdmission();
    std::cout << "Graphics capability admission and fallback tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
