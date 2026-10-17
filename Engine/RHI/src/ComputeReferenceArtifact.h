#pragma once
#include "Nexora/RHI/Types.h"
#include <stdexcept>
namespace nexora::rhi::detail {
inline const char *ComputeReferenceEnvironment(ComputeShaderVariant variant, Backend backend) {
  const unsigned program =
      variant == ComputeShaderVariant::RayReference               ? 0
      : variant == ComputeShaderVariant::SamplerFeedbackReference ? 1
      : IsModernComputeReference(variant)
          ? 2 + static_cast<unsigned>(variant) -
                static_cast<unsigned>(ComputeShaderVariant::LongVectorReference)
          : throw std::invalid_argument("invalid compute reference recipe");
  const unsigned api = backend == Backend::Direct3D12 ? 0
                       : backend == Backend::Vulkan   ? 1
                       : backend == Backend::Metal
                           ? 2
                           : throw std::invalid_argument("invalid compute reference backend");
  static constexpr const char *names[][3]{
      {"NEXORA_SLANG_RAY_REFERENCE_DXIL_PATH", "NEXORA_SLANG_RAY_REFERENCE_SPIRV_PATH",
       "NEXORA_SLANG_RAY_REFERENCE_METAL_PATH"},
      {"NEXORA_SLANG_FEEDBACK_REFERENCE_DXIL_PATH", "NEXORA_SLANG_FEEDBACK_REFERENCE_SPIRV_PATH",
       "NEXORA_SLANG_FEEDBACK_REFERENCE_METAL_PATH"},
      {"NEXORA_SLANG_LONG_VECTOR_REFERENCE_DXIL_PATH",
       "NEXORA_SLANG_LONG_VECTOR_REFERENCE_SPIRV_PATH",
       "NEXORA_SLANG_LONG_VECTOR_REFERENCE_METAL_PATH"},
      {"NEXORA_SLANG_LINEAR_ALGEBRA_REFERENCE_DXIL_PATH",
       "NEXORA_SLANG_LINEAR_ALGEBRA_REFERENCE_SPIRV_PATH",
       "NEXORA_SLANG_LINEAR_ALGEBRA_REFERENCE_METAL_PATH"},
      {"NEXORA_SLANG_GROUP_SHARED_REFERENCE_DXIL_PATH",
       "NEXORA_SLANG_GROUP_SHARED_REFERENCE_SPIRV_PATH",
       "NEXORA_SLANG_GROUP_SHARED_REFERENCE_METAL_PATH"}};
  return names[program][api];
}
} // namespace nexora::rhi::detail
