#pragma once
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <span>
#include <string>
#include <vector>

namespace Nexora::EditorImGuiDetail {
struct FontFaceIdentity final {
  int index{};
  std::string family;
  bool scalable{}, bitmap_only{};
};
struct FontOutlineCoverage final {
  std::vector<std::uint32_t> scalars;
  // Conservative packing estimate at 26px, oversampling 1, including glyph padding.
  std::uint64_t area{};
};
// Implemented alongside the one pinned imgui_draw.cpp translation unit. Basic sfnt/table
// preflight rejects missing/truncated/bitmap-only input; this is not a hostile-font sanitizer.
[[nodiscard]] IMGUI_API std::vector<FontFaceIdentity>
InspectFontFaces(std::span<const std::byte> bytes);
[[nodiscard]] IMGUI_API bool ProbeFontOutlines(std::span<const std::byte> bytes, int face,
                                               std::span<const std::uint32_t> profile,
                                               FontOutlineCoverage &coverage);
} // namespace Nexora::EditorImGuiDetail
