#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <cstddef>
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

enum class ShaderBindingAccess : std::uint8_t { ReadOnly, ReadWrite };
struct ShaderResourceBindingMetadata final {
  std::uint32_t set{};
  std::uint32_t binding{};
  BindingType type{BindingType::ConstantBuffer};
  std::uint8_t stages{};
  std::uint32_t array_count{1};
  ShaderBindingAccess access{ShaderBindingAccess::ReadOnly};
  std::uint32_t argument_buffer_group{};
  friend bool operator==(const ShaderResourceBindingMetadata &,
                         const ShaderResourceBindingMetadata &) = default;
};
struct ShaderDeviceCapabilities final {
  Backend backend{Backend::Null};
  bool supports_argument_buffers{};
  std::uint32_t max_argument_buffer_bindings{4096};
  std::uint32_t max_descriptor_sets{8};
};

// Compiler output is carried across the RHI boundary without exposing Slang compiler types.
// MetalSource contains Slang-generated MSL source for the platform Metal compiler.
enum class ShaderBinaryFormat : std::uint8_t { Dxil, SpirV, MetalSource };
struct ShaderModuleArtifact final {
  std::string shader_id;
  ShaderBinaryFormat format{ShaderBinaryFormat::SpirV};
  std::string entry_point;
  std::vector<std::byte> binary;
  PipelineLayoutMetadata reflection;
};

[[nodiscard]] NEXORA_RHI_API std::uint64_t
ComputeLayoutHash(std::span<const ShaderBinding> bindings);
[[nodiscard]] NEXORA_RHI_API bool IsCanonicalLayout(const PipelineLayoutMetadata &left,
                                                    const PipelineLayoutMetadata &right) noexcept;
[[nodiscard]] NEXORA_RHI_API PipelineLayoutMetadata TrianglePipelineLayout();
[[nodiscard]] NEXORA_RHI_API bool IsArtifactCompatible(
    const ShaderModuleArtifact &artifact, Backend backend,
    std::uint64_t expected_layout_hash) noexcept;
[[nodiscard]] NEXORA_RHI_API bool ValidateShaderResourceBindings(
    std::span<const ShaderResourceBindingMetadata> resources,
    const ShaderDeviceCapabilities &capabilities, std::string &error);
} // namespace nexora::rhi
