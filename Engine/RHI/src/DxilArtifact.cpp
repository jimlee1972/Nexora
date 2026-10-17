#include "Nexora/RHI/DxilArtifact.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nexora::rhi {
namespace {
// Layout/encoding: microsoft/DirectXShaderCompiler include/dxc/DxilContainer/DxilContainer.h.
// Read little-endian words explicitly: input need not have native alignment or host endianness.
std::uint32_t Word(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return std::to_integer<std::uint32_t>(bytes[offset]) |
         (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}
constexpr std::uint32_t Dxbc = 0x43425844;
constexpr std::uint32_t Dxil = 0x4c495844;
constexpr std::uint32_t Bitcode = 0xdec04342;
} // namespace
std::optional<DxilProgramInfo> InspectDxilProgram(std::span<const std::byte> bytes) noexcept {
  try {
    constexpr std::size_t header_size = 32;
    if (bytes.size() < header_size || Word(bytes, 0) != Dxbc || Word(bytes, 20) != 1 ||
        Word(bytes, 24) != bytes.size())
      return {};
    const auto count = Word(bytes, 28);
    if (count == 0 || count > (bytes.size() - header_size) / 4)
      return {};
    const auto table_end = header_size + static_cast<std::size_t>(count) * 4;
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(count);
    std::optional<DxilProgramInfo> result;
    for (std::size_t part = 0; part < count; ++part) {
      const auto offset = static_cast<std::size_t>(Word(bytes, header_size + part * 4));
      if (offset < table_end || offset % 4 != 0 || offset > bytes.size() ||
          bytes.size() - offset < 8)
        return {};
      const auto size = static_cast<std::size_t>(Word(bytes, offset + 4));
      if (size > bytes.size() - offset - 8)
        return {};
      ranges.emplace_back(offset, offset + 8 + size);
      if (Word(bytes, offset) != Dxil)
        continue;
      if (result || size < 24)
        return {}; // Exactly one executable entry program, never ambiguous duplicate DXIL parts.
      const auto program = bytes.subspan(offset + 8, size);
      const auto words = Word(program, 4);
      if (words < 6 || words > size / 4)
        return {};
      const auto program_size = static_cast<std::size_t>(words) * 4;
      if (Word(program, 8) != Dxil)
        return {};
      const auto bitcode_offset = static_cast<std::size_t>(Word(program, 16));
      const auto bitcode_size = static_cast<std::size_t>(Word(program, 20));
      // Bitcode offsets are relative to the embedded bitcode header, eight bytes into program.
      if (bitcode_offset < 16 || bitcode_offset > program_size - 8 || bitcode_size < 4 ||
          bitcode_size > program_size - 8 - bitcode_offset ||
          Word(program, 8 + bitcode_offset) != Bitcode)
        return {};
      const auto version = Word(program, 0);
      const FeatureVersion model{static_cast<std::uint16_t>((version >> 4U) & 0xfU),
                                 static_cast<std::uint16_t>(version & 0xfU)};
      if (model.major < 6 || (version & 0xff00U) != 0)
        return {};
      ShaderStage stage;
      switch (version >> 16U) {
      case 0: // DXIL::ShaderKind::Pixel
        stage = ShaderStage::Fragment;
        break;
      case 1: // DXIL::ShaderKind::Vertex
        stage = ShaderStage::Vertex;
        break;
      case 3: // DXIL::ShaderKind::Hull
        stage = ShaderStage::TessellationControl;
        break;
      case 4: // DXIL::ShaderKind::Domain
        stage = ShaderStage::TessellationEvaluation;
        break;
      case 5: // DXIL::ShaderKind::Compute
        stage = ShaderStage::Compute;
        break;
      default:
        return {}; // Libraries and advanced stages need separate implementation admission.
      }
      result = DxilProgramInfo{stage, model};
    }
    std::ranges::sort(ranges);
    for (std::size_t index = 1; index < ranges.size(); ++index)
      if (ranges[index].first < ranges[index - 1].second)
        return {};
    return result;
  } catch (...) {
    return {}; // Scratch-allocation failure is closed admission, with no native side effects.
  }
}
DxilProgramInfo RequireDxilArtifact(std::span<const std::byte> artifact, ShaderStage stage,
                                    FeatureVersion minimum_shader_model) {
  const auto info = InspectDxilProgram(artifact);
  if (!info)
    throw std::invalid_argument("DXIL artifact has invalid or unsupported program metadata");
  if (info->stage != stage)
    throw std::invalid_argument("DXIL artifact stage does not match the pipeline stage");
  if (info->shader_model < minimum_shader_model)
    throw std::invalid_argument("DXIL artifact shader model is too low");
  return *info;
}
} // namespace nexora::rhi
