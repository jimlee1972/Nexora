#pragma once

#include <cstdint>
#include <string_view>

namespace nexora::editor {
inline constexpr std::string_view kScalarMaterialToolService = "nexora.editor.scalar-material.v1";
// Edit bytes: four ASCII bytes NXM1, uint32 little-endian lane, IEEE-754 binary32 little-endian
// scalar, then exact schema-1 material source (no terminator). Total at most 64 KiB.
// Inspect/Serialize take source alone. All successful operations return canonical source.
// Preview is unavailable. Host authorizing document changes/IO is separate from this service.
enum class ScalarMaterialLane : std::uint32_t {
  BaseRed = 0,
  BaseGreen,
  BaseBlue,
  Metallic,
  Roughness,
  Occlusion,
  EmissionRed,
  EmissionGreen,
  EmissionBlue
};
inline constexpr std::uint32_t kScalarMaterialEditPrefixBytes = 12;
} // namespace nexora::editor
