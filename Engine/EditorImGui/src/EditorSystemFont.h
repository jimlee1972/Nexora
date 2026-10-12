#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Nexora::EditorImGuiDetail {

// A CJK-capable system font plus the glyph ranges worth baking into the font atlas.
struct SystemCjkFont final {
  std::string path; // UTF-8 path of the font file.
  std::vector<std::uint32_t>
      glyph_pairs; // Inclusive [first, last] code point pairs, no terminator.
};

// Locates a Traditional/Simplified Chinese capable system font and builds the common-character
// ranges (Big5 level 1, bopomofo, CJK and full-width punctuation). Returns false when the
// platform has no such font or does not support the lookup; the Editor then keeps its default
// Latin-only font. The result is computed once per process.
[[nodiscard]] bool FindSystemCjkFont(SystemCjkFont &out);

} // namespace Nexora::EditorImGuiDetail
