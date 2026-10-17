#include "Nexora/RHI/SpirvArtifact.h"

#include <optional>

namespace nexora::rhi {
namespace {
std::uint32_t Word(std::span<const std::byte> bytes, std::size_t index) noexcept {
  std::uint32_t value{};
  for (unsigned byte = 0; byte < 4; ++byte)
    value |= std::to_integer<std::uint32_t>(bytes[index * 4 + byte]) << (byte * 8U);
  return value;
}
std::optional<std::uint32_t> ExecutionModel(ShaderStage stage) noexcept {
  switch (stage) {
  case ShaderStage::Vertex:
    return 0;
  case ShaderStage::TessellationControl:
    return 1;
  case ShaderStage::TessellationEvaluation:
    return 2;
  case ShaderStage::Fragment:
    return 4;
  case ShaderStage::Compute:
    return 5;
  case ShaderStage::Mesh:
    return {}; // Mesh execution is not admitted by the bounded Vulkan1.1/SPIR-V1.3 path.
  }
  return std::nullopt;
}
} // namespace

bool HasSpirvEntryPoint(std::span<const std::byte> artifact, ShaderStage stage,
                        std::string_view entry_point) noexcept {
  const auto model = ExecutionModel(stage);
  if (!model || entry_point.empty() || entry_point.find('\0') != std::string_view::npos ||
      artifact.size() < 20 || artifact.size() % 4 || Word(artifact, 0) != 0x07230203U)
    return false;
  const auto version = Word(artifact, 1);
  const auto bound = Word(artifact, 3);
  if (version < 0x00010000U || version > 0x00010300U || (version & 0xff0000ffU) || !bound ||
      bound > 0x003fffffU || Word(artifact, 4))
    return false;
  const auto words = artifact.size() / 4;
  bool found = false;
  for (std::size_t index = 5; index < words;) {
    const auto instruction = Word(artifact, index);
    const auto count = instruction >> 16U;
    if (!count || count > words - index)
      return false;
    if ((instruction & 0xffffU) == 15) { // OpEntryPoint
      if (count < 4 || !Word(artifact, index + 2) || Word(artifact, index + 2) >= bound)
        return false;
      const auto name_begin = (index + 3) * 4;
      const auto instruction_end = (index + count) * 4;
      auto name_end = name_begin;
      while (name_end < instruction_end && artifact[name_end] != std::byte{0})
        ++name_end;
      if (name_end == instruction_end)
        return false;
      const auto interfaces = (name_end + 4) / 4;
      for (auto padding = name_end + 1; padding < interfaces * 4; ++padding)
        if (artifact[padding] != std::byte{0})
          return false;
      for (auto interface = interfaces; interface < index + count; ++interface)
        if (!Word(artifact, interface) || Word(artifact, interface) >= bound)
          return false;
      bool matches = name_end - name_begin == entry_point.size();
      for (std::size_t byte = 0; matches && byte < entry_point.size(); ++byte)
        matches = std::to_integer<unsigned char>(artifact[name_begin + byte]) ==
                  static_cast<unsigned char>(entry_point[byte]);
      if (matches && Word(artifact, index + 1) == *model) {
        if (found)
          return false;
        found = true;
      }
    }
    index += count;
  }
  return found;
}
} // namespace nexora::rhi
