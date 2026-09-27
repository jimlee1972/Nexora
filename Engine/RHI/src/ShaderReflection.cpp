#include "Nexora/RHI/ShaderReflection.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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

bool IsArtifactCompatible(const ShaderModuleArtifact &artifact, Backend backend,
                         std::uint64_t expected_layout_hash) noexcept {
  const bool target_matches =
      (backend == Backend::Direct3D12 && artifact.format == ShaderBinaryFormat::Dxil) ||
      (backend == Backend::Vulkan && artifact.format == ShaderBinaryFormat::SpirV) ||
      (backend == Backend::Metal && artifact.format == ShaderBinaryFormat::MetalSource);
  if (!target_matches || artifact.shader_id.empty() || artifact.entry_point.empty() ||
      artifact.binary.empty() || artifact.reflection.schema_version != 1 ||
      artifact.reflection.layout_hash != expected_layout_hash)
    return false;
  return ComputeLayoutHash(artifact.reflection.bindings) == artifact.reflection.layout_hash;
}

bool ValidateShaderResourceBindings(std::span<const ShaderResourceBindingMetadata> resources,
                                    const ShaderDeviceCapabilities &capabilities,
                                    std::string &error) {
  if (capabilities.max_argument_buffer_bindings == 0 || capabilities.max_descriptor_sets == 0) {
    error = "shader device reports zero binding capacity";
    return false;
  }
  std::unordered_set<std::uint64_t> occupied;
  std::unordered_map<std::uint32_t, std::uint32_t> group_counts;
  for (const auto &resource : resources) {
    if (resource.set >= capabilities.max_descriptor_sets || resource.binding >= 4096 ||
        resource.array_count == 0 || resource.stages == 0 ||
        (resource.stages & ~static_cast<std::uint8_t>(7)) != 0) {
      error = "shader resource binding exceeds device limits";
      return false;
    }
    const auto key = (static_cast<std::uint64_t>(resource.set) << 32U) | resource.binding;
    if (!occupied.insert(key).second) {
      error = "shader resource binding collides within a descriptor set";
      return false;
    }
    if (resource.access == ShaderBindingAccess::ReadWrite &&
        resource.type != BindingType::StorageBuffer) {
      error = "read/write shader resources must use a storage-buffer binding";
      return false;
    }
    if (resource.argument_buffer_group != 0) {
      if (!capabilities.supports_argument_buffers) {
        error = "shader requires argument buffers unsupported by this device";
        return false;
      }
      auto &count = group_counts[resource.argument_buffer_group];
      if (resource.array_count > capabilities.max_argument_buffer_bindings ||
          count > capabilities.max_argument_buffer_bindings - resource.array_count) {
        error = "shader argument buffer exceeds the device binding limit";
        return false;
      }
      count += resource.array_count;
    }
  }
  error.clear();
  return true;
}
} // namespace nexora::rhi
