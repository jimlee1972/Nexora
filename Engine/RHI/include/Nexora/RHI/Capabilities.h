#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <string_view>

namespace nexora::rhi {
enum class CapabilityFailure : std::uint8_t {
  None,
  UnknownSupport,
  Unsupported,
  InsufficientTier,
  NotEnabled,
  NotImplemented,
  UnknownShaderModel,
  UnsupportedShaderModel,
  InsufficientShaderModel
};
struct CapabilityCheck final {
  CapabilityFailure failure{CapabilityFailure::None};
  // Count identifies a shader-model failure rather than a feature failure.
  GraphicsFeature feature{GraphicsFeature::Count};
  [[nodiscard]] bool Succeeded() const noexcept { return failure == CapabilityFailure::None; }
};
enum class GraphicsPath : std::uint8_t { Unavailable, Preferred, Fallback };
struct GraphicsPathSelection final {
  GraphicsPath path{GraphicsPath::Unavailable};
  CapabilityCheck preferred;
  CapabilityCheck fallback;
};

[[nodiscard]] NEXORA_RHI_API CapabilityCheck CheckGraphicsRequirements(
    const DeviceCapabilities &capabilities, const GraphicsRequirements &requirements) noexcept;
[[nodiscard]] NEXORA_RHI_API GraphicsPathSelection
SelectGraphicsPath(const DeviceCapabilities &capabilities, const GraphicsRequirements &preferred,
                   const GraphicsRequirements &fallback) noexcept;
[[nodiscard]] NEXORA_RHI_API std::string_view
CapabilityFailureName(CapabilityFailure failure) noexcept;
// Checks only admission; does not allocate, record, submit, or select a fallback implicitly.
NEXORA_RHI_API void RequireGraphicsCapabilities(const DeviceCapabilities &capabilities,
                                                const GraphicsRequirements &requirements);
} // namespace nexora::rhi
