#include "EditorFontProbe.h"
#include "EditorImGuiTestAccess.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
using Host = nexora::editor::imgui::EditorImGuiHost;
using Access = nexora::editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path &path) {
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}
std::vector<std::byte> Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  const std::vector<char> raw{std::istreambuf_iterator<char>{input},
                              std::istreambuf_iterator<char>{}};
  std::vector<std::byte> bytes(raw.size());
  std::transform(raw.begin(), raw.end(), bytes.begin(), [](char value) {
    return static_cast<std::byte>(static_cast<unsigned char>(value));
  });
  return bytes;
}
void Bound(const nexora::editor::imgui::EditorFontTestState &state) {
  Require(!state.profile.empty() && state.profile.size() <= 8192 && state.width > 0 &&
              state.height > 0 && state.width <= 4096 && state.height <= 4096 &&
              std::uint64_t(state.width) * state.height * 4 <= 64ULL * 1024 * 1024 &&
              state.owned_bytes <= 64ULL * 1024 * 1024 && state.faces.size() <= 4,
          "Font profile, owned memory or built atlas exceeded its admission limit");
}
void WindowsParity() {
  const auto generated = Access::TraditionalFontScalars();
  Require(generated.size() == 5401 && std::is_sorted(generated.begin(), generated.end()),
          "Generated Traditional common profile changed");
#if defined(_WIN32)
  std::set<std::uint16_t> actual;
  for (unsigned lead = 0xA4; lead <= 0xC6; ++lead)
    for (unsigned trail = 0x40; trail <= 0xFE; ++trail) {
      if ((trail > 0x7E && trail < 0xA1) || (lead == 0xC6 && trail > 0x7E))
        continue;
      const char bytes[]{static_cast<char>(lead), static_cast<char>(trail)};
      wchar_t decoded[2]{};
      if (MultiByteToWideChar(950, MB_ERR_INVALID_CHARS, bytes, 2, decoded, 2) == 1 &&
          decoded[0] >= 0x4E00 && decoded[0] <= 0x9FFF)
        actual.insert(static_cast<std::uint16_t>(decoded[0]));
    }
  Require(std::vector<std::uint16_t>(actual.begin(), actual.end()) == generated,
          "Python cp950 profile differs from Windows MultiByteToWideChar CP950");
#else
  std::cout << "Windows CP950 exact-set parity: NOT EXECUTED on this host\n";
#endif
}
void RealAtlas(const std::filesystem::path &font, bool remove_after_load = false) {
  Host host;
  Host baseline;
  Access::FontSources(baseline, std::span<const std::string>{});
  const std::array paths{Utf8(font)};
  Access::FontSources(host, paths);
  std::uint32_t previous_generation = 0;
  std::uint64_t first_cross_hash = 0;
  for (const float dpi : {1.0F, 1.25F, 1.5F, 2.0F, 1.0F}) {
    baseline.SetDisplay(1280, 720, dpi);
    const auto default_latin = Access::FontGlyph(baseline, 0x41);
    const auto missing_latin = Access::FontGlyph(baseline, 0x80);
    host.SetDisplay(1280, 720, dpi);
    const auto state = Access::FontState(host);
    Bound(state);
    Require(!state.fallback && state.faces == std::vector{0} &&
                state.generation > previous_generation &&
                std::ranges::find(state.missing, 0x1F600U) != state.missing.end() &&
                std::ranges::find(state.missing, 0x1F642U) == state.missing.end(),
            "Atlas generation or actual missing supplementary coverage was incorrect");
    const auto latin = Access::FontGlyph(host, 0x41);
    const auto cross = Access::FontGlyph(host, 0x4E2D);
    const auto diamond = Access::FontGlyph(host, 0x1F642);
    const auto filled_latin = Access::FontGlyph(host, 0x80);
    Require(default_latin.present && default_latin.ink_pixels &&
                latin.raster_hash == default_latin.raster_hash &&
                latin.ink_pixels == default_latin.ink_pixels,
            "Merging a different synthetic Latin outline overwrote the actual default raster");
    Require(!missing_latin.present && filled_latin.present && filled_latin.ink_pixels &&
                std::ranges::find(state.missing, 0x80U) == state.missing.end(),
            "Actual missing default-range glyph was excluded instead of merged from the source");
    Require(latin.present && cross.present && diamond.present && latin.ink_pixels &&
                cross.ink_pixels && diamond.ink_pixels &&
                cross.raster_hash != diamond.raster_hash && cross.uv[2] > cross.uv[0] &&
                cross.uv[3] > cross.uv[1] && diamond.uv[2] > diamond.uv[0] &&
                diamond.uv[3] > diamond.uv[1] && !Access::FontGlyph(host, 0x4E2E).present &&
                !Access::FontGlyph(host, 0x1F600).present,
            "Actual FindGlyphNoFallback, raster ink, UV or unsupported glyph contract failed");
    if (!previous_generation)
      first_cross_hash = cross.raster_hash;
    else if (dpi == 1.0F)
      Require(cross.raster_hash == first_cross_hash, "DPI round trip accumulated raster scaling");
    if (!previous_generation && remove_after_load)
      Require(std::filesystem::remove(font), "Owned-memory source lifetime setup failed");
    previous_generation = state.generation;
    nexora::editor::ProductShell shell;
    host.BeginFrame();
    host.DrawProductShell(shell);
    static_cast<void>(host.EndFrame());
    std::cout << "font bucket=" << dpi << " profile=" << state.profile.size()
              << " missing=" << state.missing.size() << " owned=" << state.owned_bytes
              << " atlas=" << state.width << 'x' << state.height
              << " probe_us=" << state.probe_microseconds
              << " build_us=" << state.build_microseconds << '\n';
  }
}
void RejectedSources(const std::filesystem::path &fixtures, const std::filesystem::path &invalid) {
  for (const auto &path :
       {fixtures / "missing.ttf", fixtures / "NexoraBitmapOnlyContainer.ttf",
        fixtures / "NexoraInvalidContours.ttf", fixtures / "NexoraInvalidCmap.ttf",
        fixtures / "NexoraInvalidCff.otf", fixtures / "NexoraInvalidCompoundDepth.ttf", invalid}) {
    Host host;
    Access::FontSources(host, std::array{Utf8(path)});
    host.SetDisplay(800, 600, 1);
    const auto state = Access::FontState(host);
    Bound(state);
    Require(state.fallback && state.faces.empty() && state.rejected == 1 &&
                !Access::FontGlyph(host, 0x4E2D).present &&
                Access::FontGlyph(host, 0x41).ink_pixels > 0,
            "Missing, invalid or bitmap-only source escaped explicit Latin fallback");
    if (path.filename() == "NexoraBitmapOnlyContainer.ttf")
      Require(state.bitmap_only == 1, "Bitmap-only rejection was not observable");
  }
  const auto source = Read(fixtures / "NexoraSyntheticGlyphTest.ttf");
  for (const auto length : {std::size_t{0}, std::size_t{12}, source.size() / 2}) {
    const auto truncated = std::span(source).first(length);
    const auto identities = Nexora::EditorImGuiDetail::InspectFontFaces(truncated);
    Require(std::ranges::none_of(identities, [](const auto &face) { return face.scalable; }),
            "Truncated sfnt was admitted as scalable before atlas loading");
    Nexora::EditorImGuiDetail::FontOutlineCoverage coverage;
    Require(!Nexora::EditorImGuiDetail::ProbeFontOutlines(
                truncated, 0, std::array<std::uint32_t, 1>{0x4E2D}, coverage),
            "Truncated font reached a successful outline probe");
  }
  Nexora::EditorImGuiDetail::FontOutlineCoverage coverage;
  Require(!Nexora::EditorImGuiDetail::ProbeFontOutlines(
              source, 0, std::vector<std::uint32_t>(8193, 0x4E2D), coverage),
          "Oversized requested profile bypassed the hard 8192 bound");
  Host oversized;
  Access::FontSources(oversized, std::array{Utf8(fixtures / "NexoraAtlasAdmission.ttf")});
  oversized.SetDisplay(800, 600, 2);
  const auto state = Access::FontState(oversized);
  Bound(state);
  Require(state.fallback && state.admission_rejected && state.faces.empty(),
          "Oversized outline packing estimate reached the whole-atlas builder");
}
void Collection(const std::filesystem::path &fixtures) {
  Host host;
  Access::FontSources(host, std::array{Utf8(fixtures / "NexoraSyntheticFaces.ttc")}, true);
  host.SetDisplay(800, 600, 1);
  const auto state = Access::FontState(host);
  Require(
      state.faces == std::vector{3} &&
          state.families == std::vector<std::string>{"Synthetic CJK TC"} &&
          Access::FontGlyph(host, 0x4E2D).ink_pixels > 0,
      "TTC source was selected by an arbitrary face index instead of TC name and outline coverage");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--system") {
      Host host;
      host.SetDisplay(1280, 720, 1);
      const auto state = Access::FontState(host);
      Bound(state);
      std::cout << "system profile=" << state.profile.size() << " missing=" << state.missing.size()
                << " owned_source_bytes=" << state.owned_bytes << " atlas=" << state.width << 'x'
                << state.height << " probe_us=" << state.probe_microseconds
                << " build_us=" << state.build_microseconds << '\n';
      for (const auto scalar : state.missing)
        std::cout << "system missing_scalar=" << scalar << '\n';
      for (std::size_t face = 0; face < state.faces.size(); ++face)
        std::cout << "system font=" << state.paths[face] << " face=" << state.faces[face]
                  << " family=" << state.families[face] << '\n';
      for (const auto scalar : {0x41U, 0x4E2DU, 0x1F600U, 0x1F642U}) {
        const auto glyph = Access::FontGlyph(host, scalar);
        std::cout << "system scalar=" << scalar << " present=" << glyph.present
                  << " ink=" << glyph.ink_pixels << '\n';
      }
      return 0; // Diagnostic mode: absent system coverage is reported, never asserted as accepted.
    }
    Require(argc == 2, "Expected the synthetic fixture directory");
    const auto fixtures = std::filesystem::path(
        std::u8string(argv[1], argv[1] + std::char_traits<char>::length(argv[1])));
    struct Temporary final {
      std::filesystem::path root =
          std::filesystem::temp_directory_path() /
          ("nexora-font-glyphs-" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
      ~Temporary() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
      }
    } temporary;
    std::filesystem::create_directories(temporary.root);
    const auto invalid = temporary.root / "invalid.ttf";
    {
      std::ofstream out(invalid, std::ios::binary);
      out << "invalid sfnt font input";
    }
    const auto unicode = temporary.root / std::filesystem::path(u8"字型🙂.ttf");
    std::filesystem::copy_file(fixtures / "NexoraSyntheticGlyphTest.ttf", unicode);
    WindowsParity();
    RealAtlas(fixtures / "NexoraSyntheticGlyphTest.ttf");
    RealAtlas(fixtures / "NexoraSyntheticCff.otf");
    RealAtlas(unicode, true);
    RejectedSources(fixtures, invalid);
    Collection(fixtures);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
