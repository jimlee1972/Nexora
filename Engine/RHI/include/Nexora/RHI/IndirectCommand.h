#pragma once

#include "Nexora/RHI/IndirectCommandABI.inc"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace nexora::rhi {

// Canonical cross-backend indirect-buffer ABI. The leading four words are intentionally identical
// to VkDrawIndirectCommand, D3D12_DRAW_ARGUMENTS, and MTLDrawPrimitivesIndirectArguments. Backend
// code must consume this prefix and advance by GPUDrivenIndirectCommandStride; it must not define
// a private command layout.
struct DrawIndirectArguments final {
  std::uint32_t vertex_count{};
  std::uint32_t instance_count{};
  std::uint32_t first_vertex{};
  std::uint32_t first_instance{};
};

// Classification metadata follows the native draw prefix so correctness gates can compare the
// generated bins without translating or rewriting the buffer before submission.
struct GPUDrivenIndirectCommand final {
  DrawIndirectArguments draw;
  std::uint32_t mesh_resource_index{};
  std::uint32_t material_resource_index{};
  std::uint32_t lod_index{};
  std::uint32_t compacted_first_instance{};
  std::uint32_t compacted_instance_count{};
};

inline constexpr std::uint32_t DrawIndirectArgumentSize = sizeof(DrawIndirectArguments);
inline constexpr std::uint32_t GPUDrivenIndirectCommandStride = sizeof(GPUDrivenIndirectCommand);
inline constexpr std::uint32_t GPUDrivenIndirectCommandWords =
    NEXORA_GPU_DRIVEN_INDIRECT_COMMAND_WORDS;

// Whether `command_count` indirect draws starting at `offset`, `stride` bytes apart, stay inside a
// buffer of `buffer_size` bytes. Each draw reads the DrawIndirectArguments prefix, so the last one
// needs (command_count - 1) * stride + offset + DrawIndirectArgumentSize <= buffer_size: Vulkan's
// vkCmdDrawIndirect rule, and the bytes D3D12's draw signature and Metal's indirect draws read.
// Every backend checks this at DrawIndirect, when the count is known, so an undersized buffer is
// rejected identically everywhere instead of being read past its end. A zero count never fits
// (DrawIndirect rejects it). Overflow-safe for every input.
[[nodiscard]] constexpr bool IndirectDrawRangeFits(std::uint64_t buffer_size, std::uint64_t offset,
                                                   std::uint32_t stride,
                                                   std::uint32_t command_count) noexcept {
  if (command_count == 0 || offset > buffer_size || buffer_size - offset < DrawIndirectArgumentSize)
    return false;
  // (2^32 - 1)^2 fits in 64 bits, so the span cannot overflow.
  const std::uint64_t span = static_cast<std::uint64_t>(command_count - 1) * stride;
  return span <= buffer_size - offset - DrawIndirectArgumentSize;
}

static_assert(std::is_standard_layout_v<DrawIndirectArguments>);
static_assert(std::is_standard_layout_v<GPUDrivenIndirectCommand>);
static_assert(sizeof(DrawIndirectArguments) == 4 * sizeof(std::uint32_t));
static_assert(offsetof(GPUDrivenIndirectCommand, draw) == 0);
static_assert(offsetof(GPUDrivenIndirectCommand, mesh_resource_index) ==
              sizeof(DrawIndirectArguments));
static_assert(offsetof(GPUDrivenIndirectCommand, mesh_resource_index) ==
              NEXORA_INDIRECT_MESH_WORD * sizeof(std::uint32_t));
static_assert(offsetof(GPUDrivenIndirectCommand, compacted_instance_count) ==
              NEXORA_INDIRECT_COMPACTED_INSTANCE_COUNT_WORD * sizeof(std::uint32_t));
static_assert(sizeof(GPUDrivenIndirectCommand) ==
              NEXORA_GPU_DRIVEN_INDIRECT_COMMAND_WORDS * sizeof(std::uint32_t));

} // namespace nexora::rhi
