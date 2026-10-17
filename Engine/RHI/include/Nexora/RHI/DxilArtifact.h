#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <optional>
#include <span>

namespace nexora::rhi {
struct DxilProgramInfo final {
  ShaderStage stage;
  FeatureVersion shader_model;
};
// Reads bounded container/program metadata for supported entry stages. This does not validate
// LLVM bitcode, signatures, compiler provenance, preview opt-in or device execution support.
[[nodiscard]] NEXORA_RHI_API std::optional<DxilProgramInfo>
InspectDxilProgram(std::span<const std::byte> artifact) noexcept;
// Rejects absent/malformed/wrong-stage or too-old bytecode before native PSO allocation.
NEXORA_RHI_API DxilProgramInfo RequireDxilArtifact(std::span<const std::byte> artifact,
                                                   ShaderStage stage,
                                                   FeatureVersion minimum_shader_model = {});
} // namespace nexora::rhi
