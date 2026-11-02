#include "Nexora/Editor/GameBuildProgress.h"
#include "Nexora/Foundation/Types.h"
#include <array>
#include <charconv>

namespace nexora::editor {
namespace {
std::string Bounded(std::string_view text, std::size_t maximum) {
  std::string result(text.substr(0, maximum));
  while (!foundation::IsValidUtf8(result) && !result.empty())
    result.pop_back();
  return result;
}
bool Number(std::string_view text, std::uint32_t &value) {
  if (text.empty())
    return false;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value != 0;
}
void Diagnostic(GameBuildProgress &progress, std::string_view text) {
  constexpr std::array markers{std::string_view{": note: "}, std::string_view{": warning: "},
                               std::string_view{": error: "}};
  GameBuildDiagnostic value;
  bool recognized{};
  std::size_t at = text.npos, marker{};
  for (std::size_t index = 0; index < markers.size(); ++index) {
    const auto candidate = text.find(markers[index]);
    if (candidate < at) {
      at = candidate;
      marker = index;
    }
  }
  if (at != text.npos) {
    recognized = true;
    value.severity = static_cast<GameBuildDiagnosticSeverity>(marker);
    value.message = Bounded(text.substr(at + markers[marker].size()),
                            GameBuildProgress::kMaximumDiagnosticBytes);
    const auto location = text.substr(0, at);
    const auto column = location.rfind(':');
    if (column != location.npos && column != 0) {
      const auto line = location.rfind(':', column - 1);
      if (line != location.npos && Number(location.substr(column + 1), value.column) &&
          Number(location.substr(line + 1, column - line - 1), value.line))
        value.file = Bounded(location.substr(0, line), GameBuildProgress::kMaximumDiagnosticBytes);
      else
        value.line = value.column = 0;
    }
  }
  if (!recognized && (text.starts_with("error: ") || text.find("CMake Error") != text.npos)) {
    recognized = true;
    value.severity = GameBuildDiagnosticSeverity::Error;
    value.message = Bounded(text, GameBuildProgress::kMaximumDiagnosticBytes);
  }
  if (!recognized)
    return;
  if (progress.diagnostics.size() == GameBuildProgress::kMaximumDiagnostics) {
    progress.diagnostics_truncated = true;
    return;
  }
  progress.diagnostics.push_back(std::move(value));
}
} // namespace

GameBuildProgress ObserveGameBuildProgress(GameBuildStage stage, std::string_view raw) {
  GameBuildProgress progress;
  progress.stage = stage;
  if (raw.size() > GameBuildProgress::kMaximumOutputBytes) {
    raw.remove_prefix(raw.size() - GameBuildProgress::kMaximumOutputBytes);
    while (!raw.empty() && (static_cast<unsigned char>(raw.front()) & 0xc0U) == 0x80U)
      raw.remove_prefix(1);
    progress.output_truncated = true;
  }
  if (!foundation::IsValidUtf8(raw)) {
    progress.output = "Tool output is not valid UTF-8 and cannot be displayed.";
    return progress;
  }
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const auto c = static_cast<unsigned char>(raw[i]);
    if (c == 27) {
      if (i + 1 < raw.size() && raw[i + 1] == '[') {
        i += 2;
        while (i < raw.size() && (raw[i] < 0x40 || raw[i] > 0x7e))
          ++i;
      } else if (i + 1 < raw.size() && raw[i + 1] == ']') {
        i += 2;
        while (i < raw.size() && raw[i] != 7 &&
               !(raw[i] == 27 && i + 1 < raw.size() && raw[i + 1] == '\\'))
          ++i;
        if (i < raw.size() && raw[i] == 27)
          ++i;
      }
      continue;
    }
    if (c == '\r')
      continue;
    progress.output += (c < 32 && c != '\n' && c != '\t') || c == 127 ? '?' : static_cast<char>(c);
  }
  std::string_view lines = progress.output;
  while (!lines.empty()) {
    const auto end = lines.find('\n');
    Diagnostic(progress, lines.substr(0, end));
    if (end == lines.npos)
      break;
    lines.remove_prefix(end + 1);
  }
  return progress;
}
} // namespace nexora::editor
