#include "Nexora/RHI/Capabilities.h"

#include <stdexcept>
#include <string>

namespace nexora::rhi {
GraphicsRequirements EffectiveRayProgramRequirements(const RayProgramDescriptor &descriptor) {
  if (descriptor.mode > RayTracingMode::InlineQuery)
    throw std::invalid_argument("invalid ray tracing mode");
  auto requirements = descriptor.graphics_requirements;
  const FeatureVersion tier =
      descriptor.mode == RayTracingMode::InlineQuery ? FeatureVersion{1, 1} : FeatureVersion{1, 0};
  const FeatureVersion model =
      descriptor.mode == RayTracingMode::InlineQuery ? FeatureVersion{6, 5} : FeatureVersion{6, 3};
  auto &minimum = requirements.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::RayTracing)];
  if (minimum < tier)
    minimum = tier;
  if (requirements.minimum_dxil_shader_model < model)
    requirements.minimum_dxil_shader_model = model;
  return requirements;
}

CapabilityCheck CheckGraphicsRequirements(const GraphicsDeviceCapabilities &capabilities,
                                          const GraphicsRequirements &requirements) noexcept {
  for (std::size_t index = 0; index < GraphicsFeatureCount; ++index) {
    const auto minimum = requirements.minimum_tiers[index];
    if (minimum == FeatureVersion{})
      continue;
    const auto feature = static_cast<GraphicsFeature>(index);
    const auto &available = capabilities.features[index];
    if (available.support == CapabilitySupport::Unknown)
      return {CapabilityFailure::UnknownSupport, feature};
    if (available.support != CapabilitySupport::Supported)
      return {CapabilityFailure::Unsupported, feature};
    if (available.tier < minimum)
      return {CapabilityFailure::InsufficientTier, feature};
    if (!available.implemented)
      return {CapabilityFailure::NotImplemented, feature};
    if (available.implemented_tier != FeatureVersion{} && available.implemented_tier < minimum)
      return {CapabilityFailure::InsufficientImplementedTier, feature};
    if (!available.enabled)
      return {CapabilityFailure::NotEnabled, feature};
  }
  if (requirements.minimum_dxil_shader_model != FeatureVersion{}) {
    if (capabilities.dxil_shader_model_support == CapabilitySupport::Unknown)
      return {CapabilityFailure::UnknownShaderModel};
    if (capabilities.dxil_shader_model_support != CapabilitySupport::Supported)
      return {CapabilityFailure::UnsupportedShaderModel};
    if (capabilities.highest_dxil_shader_model < requirements.minimum_dxil_shader_model)
      return {CapabilityFailure::InsufficientShaderModel};
  }
  return {};
}

GraphicsPathSelection SelectGraphicsPath(const GraphicsDeviceCapabilities &capabilities,
                                         const GraphicsRequirements &preferred,
                                         const GraphicsRequirements &fallback) noexcept {
  const auto preferred_check = CheckGraphicsRequirements(capabilities, preferred);
  const auto fallback_check = CheckGraphicsRequirements(capabilities, fallback);
  return {preferred_check.Succeeded()  ? GraphicsPath::Preferred
          : fallback_check.Succeeded() ? GraphicsPath::Fallback
                                       : GraphicsPath::Unavailable,
          preferred_check, fallback_check};
}

std::string_view CapabilityFailureName(CapabilityFailure failure) noexcept {
  switch (failure) {
  case CapabilityFailure::NoFailure:
    return "supported";
  case CapabilityFailure::UnknownSupport:
    return "device feature support is unknown";
  case CapabilityFailure::Unsupported:
    return "device feature is unsupported";
  case CapabilityFailure::InsufficientTier:
    return "device feature tier is too low";
  case CapabilityFailure::NotEnabled:
    return "device feature is not enabled";
  case CapabilityFailure::NotImplemented:
    return "backend feature path is not implemented";
  case CapabilityFailure::InsufficientImplementedTier:
    return "backend implementation tier is too low";
  case CapabilityFailure::UnknownShaderModel:
    return "DXIL shader model support is unknown";
  case CapabilityFailure::UnsupportedShaderModel:
    return "DXIL shader model is unsupported";
  case CapabilityFailure::InsufficientShaderModel:
    return "DXIL shader model is too low";
  }
  return "invalid capability failure";
}

void RequireGraphicsCapabilities(const GraphicsDeviceCapabilities &capabilities,
                                 const GraphicsRequirements &requirements) {
  const auto check = CheckGraphicsRequirements(capabilities, requirements);
  if (!check.Succeeded())
    throw std::invalid_argument(std::string(CapabilityFailureName(check.failure)) + " (feature " +
                                std::to_string(static_cast<unsigned>(check.feature)) + ")");
}
GraphicsRequirements EffectivePipelineRequirements(const PipelineDescriptor &descriptor) {
  if (descriptor.type > PipelineType::Compute ||
      descriptor.topology > PrimitiveTopology::PatchList ||
      descriptor.graphics_shader > GraphicsShaderVariant::MeshTriangle ||
      descriptor.shading_rate > ShadingRate::Coarse2x2)
    throw std::invalid_argument("invalid pipeline type, topology or shader variant");
  if (descriptor.type == PipelineType::Compute &&
      (descriptor.topology != PrimitiveTopology::TriangleList || descriptor.patch_control_points ||
       descriptor.graphics_shader != GraphicsShaderVariant::Triangle ||
       descriptor.shading_rate != ShadingRate::Full))
    throw std::invalid_argument("compute pipelines cannot select graphics topology or variants");
  if (descriptor.compute_shader > ComputeShaderVariant::GroupShared610 ||
      (descriptor.type != PipelineType::Compute &&
       descriptor.compute_shader != ComputeShaderVariant::GPUDriven))
    throw std::invalid_argument("invalid compute shader variant for pipeline type");
  auto requirements = descriptor.graphics_requirements;
  if (descriptor.compute_shader >= ComputeShaderVariant::LongVector69) {
    const FeatureVersion minimum = descriptor.compute_shader == ComputeShaderVariant::LongVector69
                                       ? FeatureVersion{6, 9}
                                       : FeatureVersion{6, 10};
    if (requirements.minimum_dxil_shader_model < minimum)
      requirements.minimum_dxil_shader_model = minimum;
  }

  if (descriptor.topology == PrimitiveTopology::PatchList) {
    if (descriptor.patch_control_points != 3 ||
        descriptor.graphics_shader != GraphicsShaderVariant::Triangle)
      throw std::invalid_argument("built-in tessellation requires a three-control-point patch");
    auto &minimum =
        requirements.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::Tessellation)];
    if (minimum < FeatureVersion{1, 0})
      minimum = {1, 0};
  } else if (descriptor.patch_control_points) {
    throw std::invalid_argument("triangle lists cannot specify patch control points");
  }
  if (descriptor.graphics_shader == GraphicsShaderVariant::MeshTriangle) {
    auto &minimum =
        requirements.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::MeshShader)];
    if (minimum < FeatureVersion{1, 0})
      minimum = {1, 0};
    if (requirements.minimum_dxil_shader_model < FeatureVersion{6, 5})
      requirements.minimum_dxil_shader_model = {6, 5};
  }
  if (descriptor.shading_rate != ShadingRate::Full) {
    auto &minimum =
        requirements.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::VariableRateShading)];
    if (minimum < FeatureVersion{1, 0})
      minimum = {1, 0};
  }
  if (requirements.minimum_dxil_shader_model >= FeatureVersion{6, 10} &&
      !descriptor.allow_preview_shader_models)
    throw std::invalid_argument("Shader Model 6.10 requires explicit preview opt-in");
  return requirements;
}
void RequirePipelineCapabilities(const GraphicsDeviceCapabilities &capabilities,
                                 const PipelineDescriptor &descriptor) {
  const auto requirements = EffectivePipelineRequirements(descriptor);
  RequireGraphicsCapabilities(capabilities, requirements);
  if (requirements.minimum_dxil_shader_model >= FeatureVersion{6, 10} &&
      !capabilities.d3d12_preview_runtime_enabled)
    throw std::invalid_argument("Shader Model 6.10 requires the isolated preview runtime");
}
GraphicsRequirements EffectiveWorkGraphRequirements(const WorkGraphDescriptor &descriptor) {
  auto requirements = descriptor.graphics_requirements;
  auto &minimum = requirements.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::WorkGraphs)];
  if (minimum < FeatureVersion{1, 0})
    minimum = {1, 0};
  if (requirements.minimum_dxil_shader_model < FeatureVersion{6, 8})
    requirements.minimum_dxil_shader_model = {6, 8};
  return requirements;
}
} // namespace nexora::rhi
