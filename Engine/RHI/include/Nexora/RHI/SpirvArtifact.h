#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace nexora::rhi {
// Bounded little-endian SPIR-V 1.0-1.3 entry metadata for the Vulkan 1.1 baseline.
// This is not semantic validation, interface reflection, provenance or device admission.
[[nodiscard]] NEXORA_RHI_API bool HasSpirvEntryPoint(std::span<const std::byte> artifact,
                                                     ShaderStage stage,
                                                     std::string_view entry_point) noexcept;
} // namespace nexora::rhi
