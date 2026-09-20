#include "Nexora/RHI/ShaderReflection.h"

#include <algorithm>

namespace nexora::rhi {
std::uint64_t ComputeLayoutHash(std::span<const ShaderBinding> bindings) {
  constexpr std::uint64_t kOffset = 1469598103934665603ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  auto hash = kOffset;
  for (const auto &binding : bindings) {
    for (const auto value :
         {binding.resource_id, static_cast<std::uint64_t>(binding.binding),
          static_cast<std::uint64_t>(binding.type), static_cast<std::uint64_t>(binding.stages),
          static_cast<std::uint64_t>(binding.byte_size)}) {
      hash ^= value;
      hash *= kPrime;
    }
  }
  return hash;
}

bool IsCanonicalLayout(const PipelineLayoutMetadata &left,
                       const PipelineLayoutMetadata &right) noexcept {
  return left.schema_version == right.schema_version && left.layout_hash == right.layout_hash &&
         left.bindings == right.bindings;
}

PipelineLayoutMetadata TrianglePipelineLayout() {
  std::vector<ShaderBinding> bindings{
      {0x01, 0, BindingType::ConstantBuffer, static_cast<std::uint8_t>(ShaderStage::Vertex), 64}};
  return {1, bindings, ComputeLayoutHash(bindings)};
}
} // namespace nexora::rhi
