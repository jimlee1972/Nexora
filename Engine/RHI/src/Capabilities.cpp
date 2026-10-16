#include "Nexora/RHI/Capabilities.h"

#include <stdexcept>
#include <string>

namespace nexora::rhi {
CapabilityCheck CheckGraphicsRequirements(const DeviceCapabilities &capabilities,
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

GraphicsPathSelection SelectGraphicsPath(const DeviceCapabilities &capabilities,
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
  case CapabilityFailure::None:
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
  case CapabilityFailure::UnknownShaderModel:
    return "DXIL shader model support is unknown";
  case CapabilityFailure::UnsupportedShaderModel:
    return "DXIL shader model is unsupported";
  case CapabilityFailure::InsufficientShaderModel:
    return "DXIL shader model is too low";
  }
  return "invalid capability failure";
}

void RequireGraphicsCapabilities(const DeviceCapabilities &capabilities,
                                 const GraphicsRequirements &requirements) {
  const auto check = CheckGraphicsRequirements(capabilities, requirements);
  if (!check.Succeeded())
    throw std::invalid_argument(std::string(CapabilityFailureName(check.failure)) + " (feature " +
                                std::to_string(static_cast<unsigned>(check.feature)) + ")");
}
} // namespace nexora::rhi
