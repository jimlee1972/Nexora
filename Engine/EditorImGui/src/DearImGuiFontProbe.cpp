// Compile the pinned implementation exactly once and reuse its private stb routines.
// Do not add imgui_draw.cpp as a second target source.
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "EditorFontProbe.h"
#include <algorithm>
#include <cmath>
#include <imgui_draw.cpp>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <string_view>

namespace Nexora::EditorImGuiDetail {
namespace {
struct Slice {
  std::size_t offset{}, size{};
};
bool Bounds(std::span<const std::byte> bytes, std::size_t offset, std::size_t size) {
  return offset <= bytes.size() && size <= bytes.size() - offset;
}
std::uint16_t U16(std::span<const std::byte> bytes, std::size_t offset) {
  return (std::to_integer<std::uint16_t>(bytes[offset]) << 8) |
         std::to_integer<std::uint16_t>(bytes[offset + 1]);
}
std::uint32_t U32(std::span<const std::byte> bytes, std::size_t offset) {
  return (std::uint32_t{U16(bytes, offset)} << 16) | U16(bytes, offset + 2);
}
std::optional<Slice> Table(std::span<const std::byte> bytes, std::size_t face,
                           std::string_view tag) {
  if (!Bounds(bytes, face, 12))
    return std::nullopt;
  const auto count = U16(bytes, face + 4);
  if (count > 128 || !Bounds(bytes, face + 12, std::size_t{count} * 16))
    return std::nullopt;
  for (std::size_t i = 0; i < count; ++i) {
    const auto record = face + 12 + i * 16;
    const Slice slice{U32(bytes, record + 8), U32(bytes, record + 12)};
    if (!Bounds(bytes, slice.offset, slice.size))
      return std::nullopt;
    if (std::equal(tag.begin(), tag.end(), reinterpret_cast<const char *>(bytes.data() + record)))
      return slice;
  }
  return std::nullopt;
}
std::optional<std::size_t> FaceOffset(std::span<const std::byte> bytes, int index) {
  if (index < 0 || !Bounds(bytes, 0, 12))
    return std::nullopt;
  if (U32(bytes, 0) == 0x74746366U) {
    const auto count = U32(bytes, 8);
    if (count == 0 || count > 32 || static_cast<unsigned>(index) >= count ||
        !Bounds(bytes, 12, std::size_t{count} * 4))
      return std::nullopt;
    const auto offset = U32(bytes, 12 + std::size_t(index) * 4);
    if (!Bounds(bytes, offset, 12))
      return std::nullopt;
    return offset;
  }
  return index == 0 ? std::optional<std::size_t>{0} : std::nullopt;
}
bool ValidCmap(std::span<const std::byte> bytes, const Slice &cmap) {
  if (cmap.size < 4)
    return false;
  const auto count = U16(bytes, cmap.offset + 2);
  if (count > 64 || 4 + std::size_t(count) * 8 > cmap.size)
    return false;
  bool supported = false;
  unsigned selected_format = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const auto record = cmap.offset + 4 + i * 8;
    const auto relative = U32(bytes, record + 4);
    if (relative > cmap.size || cmap.size - relative < 2)
      return false;
    const auto offset = cmap.offset + relative;
    const auto format = U16(bytes, offset);
    const auto platform = U16(bytes, record), encoding = U16(bytes, record + 2);
    if (platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10)))
      selected_format = format;
    if (format == 4) {
      if (cmap.size - relative < 16)
        return false;
      const auto length = U16(bytes, offset + 2), segments = U16(bytes, offset + 6);
      if (segments == 0 || segments % 2 || length < 16 + std::size_t(segments) * 4 ||
          length > cmap.size - relative)
        return false;
      // idRangeOffset addresses must remain inside this subtable for every possible code point.
      const std::size_t n = segments / 2;
      unsigned power = 1, log = 0;
      while (power * 2 <= static_cast<unsigned>(n)) {
        power *= 2;
        ++log;
      }
      if (U16(bytes, offset + 8) != power * 2 || U16(bytes, offset + 10) != log ||
          U16(bytes, offset + 12) != segments - power * 2)
        return false;
      std::uint16_t previous_end = 0;
      for (std::size_t segment = 0; segment < n; ++segment) {
        const auto first = U16(bytes, offset + 16 + n * 2 + segment * 2);
        const auto last = U16(bytes, offset + 14 + segment * 2);
        const auto location = offset + 16 + n * 6 + segment * 2;
        const auto displacement = U16(bytes, location);
        if ((segment && first <= previous_end) ||
            (segment + 1 == static_cast<unsigned>(n) && (first != 0xFFFF || last != 0xFFFF)) ||
            first > last ||
            (displacement &&
             (location + displacement + std::size_t(last - first) * 2 + 2 > offset + length)))
          return false;
        previous_end = last;
      }
      supported = true;
    } else if (format == 12 || format == 13) {
      if (cmap.size - relative < 16)
        return false;
      const auto length = U32(bytes, offset + 4), groups = U32(bytes, offset + 12);
      if (length > cmap.size - relative || groups > 65536 ||
          16 + std::uint64_t(groups) * 12 > length)
        return false;
      std::uint32_t previous = 0;
      for (std::size_t group = 0; group < groups; ++group) {
        const auto first = U32(bytes, offset + 16 + group * 12);
        const auto last = U32(bytes, offset + 20 + group * 12);
        if (first > last || last > 0x10FFFF || (group && first <= previous))
          return false;
        previous = last;
      }
      supported = true;
    }
  }
  return supported && (selected_format == 4 || selected_format == 12 || selected_format == 13);
}
#include "EditorFontValidation.inl"
bool Preflight(std::span<const std::byte> bytes, std::size_t face) {
  if (!Bounds(bytes, face, 12))
    return false;
  const auto version = U32(bytes, face);
  if (version != 0x00010000U && version != 0x4F54544FU && version != 0x74727565U)
    return false;
  const auto head = Table(bytes, face, "head"), hhea = Table(bytes, face, "hhea");
  const auto hmtx = Table(bytes, face, "hmtx"), maxp = Table(bytes, face, "maxp");
  const auto cmap = Table(bytes, face, "cmap");
  if (!head || head->size < 54 || !hhea || hhea->size < 36 || !maxp || maxp->size < 6 || !hmtx ||
      !cmap || !ValidCmap(bytes, *cmap) || U16(bytes, head->offset + 18) == 0)
    return false;
  const auto glyphs = U16(bytes, maxp->offset + 4), metrics = U16(bytes, hhea->offset + 34);
  if (!glyphs || !metrics || metrics > glyphs ||
      hmtx->size < std::size_t(metrics) * 4 + std::size_t(glyphs - metrics) * 2)
    return false;
  const auto glyf = Table(bytes, face, "glyf"), loca = Table(bytes, face, "loca");
  if (glyf && loca) {
    const auto format = U16(bytes, head->offset + 50);
    if (format > 1 || loca->size < (std::size_t(glyphs) + 1) * (format ? 4 : 2))
      return false;
    return ValidGlyf(bytes, *glyf, *loca, format, glyphs);
  }
  const auto cff = Table(bytes, face, "CFF ");
  // CBDT/CBLC, sbix, SVG and COLR alone are not scalable outlines supported by this atlas path.
  return cff && CffPreflight(bytes.subspan(cff->offset, cff->size), glyphs).has_value();
}
std::string FamilyName(std::span<const std::byte> bytes, std::size_t face) {
  const auto table = Table(bytes, face, "name");
  if (!table || table->size < 6)
    return {};
  const auto count = U16(bytes, table->offset + 2), strings = U16(bytes, table->offset + 4);
  if (count > 2048 || 6 + std::size_t(count) * 12 > table->size)
    return {};
  for (const unsigned wanted : {16U, 1U}) {
    for (const unsigned language : {0x0409U, 0U}) {
      for (std::size_t i = 0; i < count; ++i) {
        const auto record = table->offset + 6 + i * 12;
        const auto platform = U16(bytes, record), identifier = U16(bytes, record + 6);
        const auto length = U16(bytes, record + 8), relative = U16(bytes, record + 10);
        if (identifier != wanted || U16(bytes, record + 4) != language ||
            (platform != 0 && platform != 3) || length % 2 || length > 512 ||
            std::size_t(strings) + relative + length > table->size)
          continue;
        std::string result;
        // Source selection uses the ASCII English family identifiers, never a guessed TTC index.
        for (std::size_t position = 0; position < length; position += 2) {
          const auto scalar = U16(bytes, table->offset + strings + relative + position);
          result += scalar >= 32 && scalar < 127 ? static_cast<char>(scalar) : '?';
        }
        if (!result.empty())
          return result;
      }
    }
  }
  return {};
}
} // namespace
std::vector<FontFaceIdentity> InspectFontFaces(std::span<const std::byte> bytes) {
  std::vector<FontFaceIdentity> result;
  if (bytes.size() < 12 || bytes.size() > 32U * 1024U * 1024U)
    return result;
  const auto count = U32(bytes, 0) == 0x74746366U ? U32(bytes, 8) : 1;
  if (count > 32)
    return result;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto offset = FaceOffset(bytes, static_cast<int>(i));
    if (!offset)
      return {};
    const bool scalable = Preflight(bytes, *offset);
    const bool bitmap_only =
        !scalable && (Table(bytes, *offset, "CBDT") || Table(bytes, *offset, "sbix") ||
                      Table(bytes, *offset, "EBDT"));
    result.push_back({static_cast<int>(i), FamilyName(bytes, *offset), scalable, bitmap_only});
  }
  return result;
}
bool ProbeFontOutlines(std::span<const std::byte> bytes, int face,
                       std::span<const std::uint32_t> profile, FontOutlineCoverage &coverage) {
  coverage = {};
  const auto offset = FaceOffset(bytes, face);
  if (!offset || !Preflight(bytes, *offset) || profile.size() > 8192 ||
      std::any_of(profile.begin(), profile.end(), [](std::uint32_t scalar) {
        return scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF);
      }))
    return false;
  std::optional<CffData> cff_data;
  if (!Table(bytes, *offset, "glyf")) {
    const auto cff = Table(bytes, *offset, "CFF ");
    const auto maxp = Table(bytes, *offset, "maxp");
    if (!cff || !maxp)
      return false;
    cff_data = CffPreflight(bytes.subspan(cff->offset, cff->size), U16(bytes, maxp->offset + 4));
    if (!cff_data)
      return false;
  }
  stbtt_fontinfo info{};
  if (!stbtt_InitFont(&info, reinterpret_cast<const unsigned char *>(bytes.data()),
                      static_cast<int>(*offset)))
    return false;
  const float scale = stbtt_ScaleForPixelHeight(&info, 26.0F);
  if (!std::isfinite(scale) || scale <= 0)
    return false;
  for (const auto scalar : profile) {
    const int glyph = stbtt_FindGlyphIndex(&info, static_cast<int>(scalar));
    if (glyph <= 0 || glyph >= info.numGlyphs)
      continue;
    if (cff_data && !ValidType2(*cff_data, static_cast<unsigned>(glyph)))
      return false;
    int x0{}, y0{}, x1{}, y1{};
    // CFF's counting context executes the outline and returns real vertex count/bounds without
    // allocating and re-running it twice. TrueType's bbox alone is not proof of an outline.
    const int cff_vertices = cff_data ? stbtt__GetGlyphInfoT2(&info, glyph, &x0, &y0, &x1, &y1) : 0;
    const bool has_box = cff_data ? cff_vertices > 0 && cff_vertices <= 16384
                                  : stbtt_GetGlyphBox(&info, glyph, &x0, &y0, &x1, &y1) != 0;
    const auto width = std::ceil((static_cast<double>(x1) - x0) * scale) + 4.0;
    const auto height = std::ceil((static_cast<double>(y1) - y0) * scale) + 4.0;
    const bool whitespace = scalar == 0x20 || scalar == 0xA0 || scalar == 0x1680 ||
                            (scalar >= 0x2000 && scalar <= 0x200A) || scalar == 0x2028 ||
                            scalar == 0x2029 || scalar == 0x202F || scalar == 0x205F ||
                            scalar == 0x3000;
    if (whitespace && (cff_data ? cff_vertices == 0 : stbtt_IsGlyphEmpty(&info, glyph))) {
      int advance{}, bearing{};
      stbtt_GetGlyphHMetrics(&info, glyph, &advance, &bearing);
      if (advance > 0)
        coverage.scalars.push_back(scalar);
      continue;
    }
    if (!has_box || width <= 4 || height <= 4 || width > 512 || height > 512)
      continue;
    bool outlined = cff_data.has_value();
    if (!cff_data) {
      stbtt_vertex *vertices = nullptr;
      const auto count = stbtt_GetGlyphShape(&info, glyph, &vertices);
      outlined = count > 0 && count <= 16384 && vertices != nullptr;
      stbtt_FreeShape(&info, vertices);
    }
    if (!outlined)
      continue;
    coverage.scalars.push_back(scalar);
    coverage.area += static_cast<std::uint64_t>(width * height);
  }
  return true;
}
} // namespace Nexora::EditorImGuiDetail
