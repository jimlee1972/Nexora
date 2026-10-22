#include "EditorSystemFont.h"
#include "EditorFontProbe.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string_view>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Nexora::EditorImGuiDetail {
namespace {
std::span<const std::uint32_t> DefaultFontCoverage() {
  static const auto coverage = [] {
    // This atlas never becomes a context's atlas or changes the current context. Build exactly
    // the default source's configured ranges; its TTF cmap can contain additional unused glyphs.
    ImFontAtlas atlas;
    ImFontConfig config;
    config.SizePixels = 13.0F;
    auto *font = atlas.AddFontDefault(&config);
    if (!font || !atlas.Build())
      throw std::runtime_error("Default Editor font coverage could not be built");
    std::vector<std::uint32_t> result;
    for (const auto &glyph : font->Glyphs)
      if (font->FindGlyphNoFallback(glyph.Codepoint))
        result.push_back(glyph.Codepoint);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }();
  return coverage;
}
constexpr std::uint16_t traditional[] = {
#include "TraditionalCommonCp950Scalars.inc"
};
std::filesystem::path NativePath(std::string_view utf8) {
  return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}
std::string Utf8(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return std::string(text.begin(), text.end());
}
bool TraditionalFamily(std::string_view name) {
  return name.find("CJK TC") != name.npos || name.find("PingFang TC") != name.npos ||
         name.find("JhengHei") != name.npos || name.find("MingLiU") != name.npos ||
         name.find("Heiti TC") != name.npos || name.find("Traditional") != name.npos;
}
std::vector<std::uint32_t> Ranges(std::span<const std::uint32_t> scalars) {
  std::vector<std::uint32_t> result;
  for (const auto scalar : scalars) {
    if (!result.empty() && scalar == result.back() + 1)
      result.back() = scalar;
    else {
      result.push_back(scalar);
      result.push_back(scalar);
    }
  }
  result.push_back(0);
  return result;
}
} // namespace
std::span<const std::uint16_t> TraditionalCommonScalars() { return traditional; }
std::vector<std::uint32_t> BuildFontProfile(std::span<const std::uint32_t> simplified_ranges) {
  std::set<std::uint32_t> points;
  const auto append = [&](std::uint32_t first, std::uint32_t last) {
    if (first > last || last > 0x10FFFF || last - first >= FontProfileLimit)
      return false;
    for (auto point = first; point <= last; ++point) {
      if (point >= 0xD800 && point <= 0xDFFF)
        return false;
      points.insert(point);
      if (points.size() > FontProfileLimit)
        return false;
    }
    return true;
  };
  if (simplified_ranges.size() % 2)
    return {};
  for (std::size_t i = 0; i < simplified_ranges.size(); i += 2)
    if (!append(simplified_ranges[i], simplified_ranges[i + 1]))
      return {};
  points.insert(std::begin(traditional), std::end(traditional));
  if (!append(0x2000, 0x206F) || !append(0x3000, 0x303F) || !append(0x3100, 0x312F) ||
      !append(0xFF00, 0xFFEF))
    return {};
  points.insert(0x1F600);
  points.insert(0x1F642);
  return points.size() <= FontProfileLimit ? std::vector(points.begin(), points.end())
                                           : std::vector<std::uint32_t>{};
}
std::vector<FontSource> InstalledFontSources() {
  std::vector<FontSource> result;
  const auto add = [&](const std::filesystem::path &path, bool traditional = false) {
    if (result.size() < 16)
      result.push_back({Utf8(path), traditional});
  };
  std::string configured;
#if defined(_WIN32)
  if (const auto *value = _wgetenv(L"NEXORA_EDITOR_FONT_PATHS"))
    configured = Utf8(value);
  constexpr char delimiter = ';';
#else
  if (const auto *value = std::getenv("NEXORA_EDITOR_FONT_PATHS"))
    configured = value;
  constexpr char delimiter = ':';
#endif
  for (std::size_t first = 0; first < configured.size();) {
    const auto last = configured.find(delimiter, first);
    const auto part = configured.substr(first, last == configured.npos ? last : last - first);
    if (!part.empty() && result.size() < 16)
      result.push_back({part, false});
    if (last == configured.npos)
      break;
    first = last + 1;
  }
#if defined(_WIN32)
  std::array<wchar_t, 32768> directory{};
  const auto length = GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
  if (length && length < directory.size()) {
    const auto fonts = std::filesystem::path(directory.data()) / L"Fonts";
    for (const auto *file : {L"msjh.ttc", L"mingliu.ttc", L"msyh.ttc", L"simsun.ttc",
                             L"seguiemj.ttf", L"seguisym.ttf"})
      add(fonts / file);
  }
#elif defined(__APPLE__)
  add("/System/Library/Fonts/PingFang.ttc", true);
  add("/System/Library/Fonts/STHeiti Medium.ttc", true);
  add("/System/Library/Fonts/Apple Symbols.ttf");
  add("/Library/Fonts/Arial Unicode.ttf");
#else
  if (const auto *xdg = std::getenv("XDG_DATA_HOME"))
    add(std::filesystem::path(xdg) / "fonts/NotoSansCJK-Regular.ttc", true);
  else if (const auto *home = std::getenv("HOME"))
    add(std::filesystem::path(home) / ".local/share/fonts/NotoSansCJK-Regular.ttc", true);
  add("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", true);
  add("/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc", true);
  add("/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc");
  add("/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf");
  add("/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf");
  add("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
  // Probe and explicitly classify the common bitmap-only candidate; never claim emoji from cmap.
  add("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
#endif
  return result;
}
std::shared_ptr<const FontPlan> PrepareFontPlan(std::span<const std::uint32_t> profile,
                                                std::span<const FontSource> sources) {
  const auto started = std::chrono::steady_clock::now();
  auto plan = std::make_shared<FontPlan>();
  const auto finish = [&]() -> std::shared_ptr<const FontPlan> {
    plan->probe_microseconds =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now() - started)
                                       .count());
    return plan;
  };
  if (profile.empty() || profile.size() > FontProfileLimit ||
      std::any_of(profile.begin(), profile.end(),
                  [](std::uint32_t scalar) {
                    return scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF);
                  }) ||
      !std::is_sorted(profile.begin(), profile.end()) ||
      std::adjacent_find(profile.begin(), profile.end()) != profile.end()) {
    plan->profile_rejected = true;
    return finish();
  }
  // The first actual default-atlas build is inside this probe's timing interval. Subsequent
  // plans reuse its immutable scalar set; no pointer to that temporary atlas is retained.
  const auto baseline = DefaultFontCoverage();
  if (baseline.size() > FontProfileLimit || !std::is_sorted(baseline.begin(), baseline.end()) ||
      std::adjacent_find(baseline.begin(), baseline.end()) != baseline.end() ||
      std::any_of(baseline.begin(), baseline.end(), [](std::uint32_t scalar) {
        return scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF);
      })) {
    plan->profile_rejected = true;
    return finish();
  }
  std::vector<std::uint32_t> remaining;
  std::set_difference(profile.begin(), profile.end(), baseline.begin(), baseline.end(),
                      std::back_inserter(remaining));
  std::set<std::filesystem::path> visited;
  for (const auto &source : sources.first(std::min<std::size_t>(sources.size(), 16))) {
    if (remaining.empty() || plan->faces.size() == 4)
      break;
    ++plan->candidates;
    if (source.path.empty() || source.path.size() > 4096 ||
        source.path.find('\0') != source.path.npos) {
      ++plan->rejected;
      continue;
    }
    std::error_code error;
    std::filesystem::path path;
    try {
      path = std::filesystem::canonical(NativePath(source.path), error);
    } catch (const std::filesystem::filesystem_error &) {
      ++plan->rejected;
      continue;
    }
    if (error || !visited.insert(path).second || !std::filesystem::is_regular_file(path, error)) {
      ++plan->rejected;
      continue;
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size < 12 || size > 32U * 1024U * 1024U ||
        size > 64U * 1024U * 1024U - plan->bytes || size > std::numeric_limits<int>::max()) {
      ++plan->rejected;
      continue;
    }
    auto bytes = std::make_shared<std::vector<std::byte>>(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char *>(bytes->data()), static_cast<std::streamsize>(size))) {
      ++plan->rejected;
      continue;
    }
    const auto identities = InspectFontFaces(*bytes);
    bool admitted = false, scalable = false, bitmap_only = false;
    const bool require_tc = source.require_traditional_face ||
                            std::ranges::any_of(identities, [](const auto &identity) {
                              return identity.family.find("Noto ") != std::string::npos &&
                                     identity.family.find("CJK ") != std::string::npos;
                            });
    for (const auto &identity : identities) {
      scalable |= identity.scalable;
      bitmap_only |= identity.bitmap_only;
      if (!identity.scalable || (require_tc && !TraditionalFamily(identity.family)))
        continue;
      FontOutlineCoverage coverage;
      if (!ProbeFontOutlines(*bytes, identity.index, remaining, coverage) ||
          coverage.scalars.empty())
        continue;
      // Reserve default-font/packing headroom before invoking ImGui's whole-atlas builder.
      if (coverage.area > (FontAtlasMaximumPixels - 65536) / 2 ||
          plan->conservative_area > FontAtlasMaximumPixels - 65536 - coverage.area * 2) {
        plan->atlas_admission_rejected = true;
        continue;
      }
      plan->conservative_area += coverage.area * 2;
      plan->faces.push_back({bytes, identity.index, source.path, identity.family, coverage.scalars,
                             Ranges(coverage.scalars)});
      plan->bytes += bytes->size();
      std::vector<std::uint32_t> next;
      std::set_difference(remaining.begin(), remaining.end(), coverage.scalars.begin(),
                          coverage.scalars.end(), std::back_inserter(next));
      remaining = std::move(next);
      admitted = true;
      break; // At most one face per file; name and actual coverage select the TTC face.
    }
    if (!admitted) {
      ++plan->rejected;
      if (bitmap_only && !scalable)
        ++plan->bitmap_only;
    }
  }
  return finish();
}
std::shared_ptr<const FontPlan> InstalledFontPlan(std::span<const std::uint32_t> profile) {
  static std::mutex mutex;
  static std::shared_ptr<const FontPlan> cached;
  const std::lock_guard guard(mutex);
  if (!cached)
    cached = PrepareFontPlan(profile, InstalledFontSources());
  return cached;
}
} // namespace Nexora::EditorImGuiDetail
