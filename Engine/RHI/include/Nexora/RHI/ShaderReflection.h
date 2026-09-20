#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nexora::rhi {
struct ShaderBinding final {
  std::uint64_t resource_id{};
  std::uint32_t binding{};
  BindingType type{BindingType::ConstantBuffer};
  std::uint8_t stages{};
  std::uint32_t byte_size{};
  friend bool operator==(const ShaderBinding &, const ShaderBinding &) = default;
};
struct PipelineLayoutMetadata final {
  std::uint32_t schema_version{1};
  std::vector<ShaderBinding> bindings;
  std::uint64_t layout_hash{};
};

[[nodiscard]] NEXORA_RHI_API std::uint64_t
ComputeLayoutHash(std::span<const ShaderBinding> bindings);
[[nodiscard]] NEXORA_RHI_API bool IsCanonicalLayout(const PipelineLayoutMetadata &left,
                                                    const PipelineLayoutMetadata &right) noexcept;
[[nodiscard]] NEXORA_RHI_API PipelineLayoutMetadata TrianglePipelineLayout();
} // namespace nexora::rhi
