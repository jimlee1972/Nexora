#include "EditorSystemFont.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <mutex>
#include <optional>

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

#if defined(_WIN32)
// Big5 level 1 (lead bytes 0xA4..0xC6, ending at 0xC67E) is the ~5,400 most common traditional
// characters. Decoding it through the Windows code page avoids shipping a large static table and
// keeps the atlas small enough for the bounded whole-atlas upload.
void AppendBig5LevelOne(std::vector<std::uint32_t> &points) {
  for (unsigned lead = 0xA4; lead <= 0xC6; ++lead) {
    for (unsigned trail = 0x40; trail <= 0xFE; ++trail) {
      if (trail > 0x7E && trail < 0xA1)
        continue;
      if (lead == 0xC6 && trail > 0x7E)
        continue;
      const char bytes[2] = {static_cast<char>(lead), static_cast<char>(trail)};
      wchar_t decoded[2] = {};
      if (MultiByteToWideChar(950, MB_ERR_INVALID_CHARS, bytes, 2, decoded, 2) == 1 &&
          decoded[0] >= 0x4E00 && decoded[0] <= 0x9FFF)
        points.push_back(decoded[0]);
    }
  }
}

std::optional<SystemCjkFont> Probe() {
  wchar_t windows_directory[MAX_PATH] = {};
  const UINT length = GetWindowsDirectoryW(windows_directory, MAX_PATH);
  if (length == 0 || length >= MAX_PATH)
    return std::nullopt;
  const std::filesystem::path fonts = std::filesystem::path(windows_directory) / L"Fonts";
  // Microsoft JhengHei (Traditional) first, then YaHei, MingLiU and SimSun as fallbacks.
  constexpr std::array<const wchar_t *, 4> candidates = {L"msjh.ttc", L"msyh.ttc", L"mingliu.ttc",
                                                         L"simsun.ttc"};
  std::filesystem::path chosen;
  for (const auto *name : candidates) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(fonts / name, ec) && !ec) {
      chosen = fonts / name;
      break;
    }
  }
  if (chosen.empty())
    return std::nullopt;

  std::vector<std::uint32_t> points;
  AppendBig5LevelOne(points);
  for (std::uint32_t c = 0x2000; c <= 0x206F; ++c) // General punctuation.
    points.push_back(c);
  for (std::uint32_t c = 0x3000; c <= 0x303F; ++c) // CJK symbols and punctuation.
    points.push_back(c);
  for (std::uint32_t c = 0x3100; c <= 0x312F; ++c) // Bopomofo.
    points.push_back(c);
  for (std::uint32_t c = 0xFF00; c <= 0xFFEF; ++c) // Half- and full-width forms.
    points.push_back(c);
  std::ranges::sort(points);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() < 1000)
    return std::nullopt; // The code page lookup failed; do not claim CJK coverage.

  SystemCjkFont font;
  const auto utf8 = chosen.u8string();
  font.path.assign(reinterpret_cast<const char *>(utf8.data()), utf8.size());
  std::uint32_t first = points.front(), last = first;
  for (std::size_t i = 1; i < points.size(); ++i) {
    if (points[i] == last + 1) {
      last = points[i];
      continue;
    }
    font.glyph_pairs.push_back(first);
    font.glyph_pairs.push_back(last);
    first = last = points[i];
  }
  font.glyph_pairs.push_back(first);
  font.glyph_pairs.push_back(last);
  return font;
}
#endif

} // namespace

bool FindSystemCjkFont(SystemCjkFont &out) {
#if defined(_WIN32)
  static std::once_flag once;
  static std::optional<SystemCjkFont> cached;
  std::call_once(once, [] { cached = Probe(); });
  if (!cached)
    return false;
  out = *cached;
  return true;
#else
  static_cast<void>(out);
  return false;
#endif
}

} // namespace Nexora::EditorImGuiDetail
