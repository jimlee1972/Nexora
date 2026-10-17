#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <string_view>

namespace nexora::rhi {
enum class CapabilityFailure : std::uint8_t {
  NoFailure,
  UnknownSupport,
  Unsupported,
  InsufficientTier,
  NotEnabled,
  NotImplemented,
  UnknownShaderModel,
  UnsupportedShaderModel,
  InsufficientShaderModel,
  InsufficientImplementedTier
};
struct CapabilityCheck final {
  CapabilityFailure failure{CapabilityFailure::NoFailure};
  // Count identifies a shader-model failure rather than a feature failure.
  GraphicsFeature feature{GraphicsFeature::Count};
  [[nodiscard]] bool Succeeded() const noexcept { return failure == CapabilityFailure::NoFailure; }
};
enum class GraphicsPath : std::uint8_t { Unavailable, Preferred, Fallback };
struct GraphicsPathSelection final {
  GraphicsPath path{GraphicsPath::Unavailable};
  CapabilityCheck preferred;
  CapabilityCheck fallback;
};

[[nodiscard]] NEXORA_RHI_API CapabilityCheck
CheckGraphicsRequirements(const GraphicsDeviceCapabilities &capabilities,
                          const GraphicsRequirements &requirements) noexcept;
[[nodiscard]] NEXORA_RHI_API GraphicsPathSelection SelectGraphicsPath(
    const GraphicsDeviceCapabilities &capabilities, const GraphicsRequirements &preferred,
    const GraphicsRequirements &fallback) noexcept;
[[nodiscard]] NEXORA_RHI_API std::string_view
CapabilityFailureName(CapabilityFailure failure) noexcept;
// Device admission only; selected shader artifacts need independent validation.
// Does not allocate, record, submit, or select a fallback implicitly.
NEXORA_RHI_API void RequireGraphicsCapabilities(const GraphicsDeviceCapabilities &capabilities,
                                                const GraphicsRequirements &requirements);
// Validate topology/program combinations and include requirements implied by the selected path.
[[nodiscard]] NEXORA_RHI_API GraphicsRequirements
EffectivePipelineRequirements(const PipelineDescriptor &descriptor);
NEXORA_RHI_API void RequirePipelineCapabilities(const GraphicsDeviceCapabilities &capabilities,
                                                const PipelineDescriptor &descriptor);
} // namespace nexora::rhi
