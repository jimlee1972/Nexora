#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Nexora::EditorImGuiDetail {
inline constexpr std::size_t FontProfileLimit = 8192;
inline constexpr std::uint64_t FontAtlasMaximumPixels = 4096ULL * 4096ULL;
inline constexpr std::uint64_t FontAtlasMaximumBytes = FontAtlasMaximumPixels * 4;
struct FontSource final {
  std::string path; // UTF-8; opened through std::filesystem's native path conversion.
  bool
      require_traditional_face{}; // TTC face selection uses its family name, never a guessed index.
};
struct FontFace final {
  std::shared_ptr<const std::vector<std::byte>> bytes;
  int index{};
  std::string path, family;
  std::vector<std::uint32_t> scalars;
  // Inclusive pairs, terminated by zero, owned until the corresponding atlas has been cleared.
  std::vector<std::uint32_t> ranges;
};
struct FontPlan final {
  std::vector<FontFace> faces;
  std::size_t candidates{}, rejected{}, bitmap_only{}, bytes{};
  std::uint64_t conservative_area{}, probe_microseconds{};
  bool profile_rejected{}, atlas_admission_rejected{};
};
// The immutable fixed profile unions pinned ImGui SimplifiedCommon with generated Traditional
// common scalars, Bopomofo/punctuation and two explicit supplementary samples. Empty means reject.
[[nodiscard]] std::vector<std::uint32_t>
BuildFontProfile(std::span<const std::uint32_t> simplified_ranges);
[[nodiscard]] std::span<const std::uint16_t> TraditionalCommonScalars();
[[nodiscard]] std::vector<FontSource> InstalledFontSources();
// The default baseline comes from a process-once independent built-in atlas, including only
// actual configured glyphs. Its first build is included in the plan's probe timing.
[[nodiscard]] std::shared_ptr<const FontPlan>
PrepareFontPlan(std::span<const std::uint32_t> profile, std::span<const FontSource> sources);
// First-use process cache owns at most 64 MiB of admitted font bytes. Environment configuration
// NEXORA_EDITOR_FONT_PATHS is read once; explicit test/owner plans bypass this cache.
[[nodiscard]] std::shared_ptr<const FontPlan>
InstalledFontPlan(std::span<const std::uint32_t> profile);
} // namespace Nexora::EditorImGuiDetail
