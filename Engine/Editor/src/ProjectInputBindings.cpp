#include "AtomicFile.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <array>
#include <fstream>

namespace nexora::editor {
namespace {
constexpr std::array<std::string_view, PlayInputBindings::kActions> kActions{
    "left", "right", "forward", "backward", "action", "primary", "secondary", "sprint", "modifier"};
constexpr std::array<std::string_view, static_cast<std::size_t>(PlayInputControl::Count)> kControls{
    "None",      "A",         "B",         "C",          "D",           "E",
    "F",         "G",         "H",         "I",          "J",           "K",
    "L",         "M",         "N",         "O",          "P",           "Q",
    "R",         "S",         "T",         "U",          "V",           "W",
    "X",         "Y",         "Z",         "Left",       "Right",       "Up",
    "Down",      "Space",     "LeftShift", "RightShift", "LeftControl", "RightControl",
    "MouseLeft", "MouseRight"};
bool Fail(std::string *error, std::string_view message) {
  if (error)
    *error = message;
  return false;
}
bool Missing(const std::filesystem::file_status &status, const std::error_code &error) {
  return error == std::errc::no_such_file_or_directory ||
         (!error && status.type() == std::filesystem::file_type::not_found);
}
// Only a verified missing path is absence. Leaf symlinks (including dangling links), directories
// and uninspectable entries never become a default setting or a write destination.
bool MetadataDirectory(const std::filesystem::path &root, bool missing_ok, std::string *error) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(root / ".nexora", ec);
  if (missing_ok && Missing(status, ec))
    return true;
  return !ec && std::filesystem::is_directory(status)
             ? true
             : Fail(error, "Play input settings metadata directory is unavailable or unsafe");
}
std::optional<PlayInputBindings> Parse(std::string_view text) {
  if (text.empty() || text.find('\0') != std::string_view::npos)
    return std::nullopt;
  const auto line = [&]() {
    const auto end = text.find('\n');
    auto value = text.substr(0, end);
    if (end == std::string_view::npos)
      text = {};
    else
      text.remove_prefix(end + 1);
    if (value.ends_with('\r'))
      value.remove_suffix(1);
    return value;
  };
  if (line() != "schema=1")
    return std::nullopt;
  PlayInputBindings result;
  std::array<bool, PlayInputBindings::kActions> seen{};
  while (!text.empty()) {
    const auto record = line();
    const auto equal = record.find('=');
    if (equal == std::string_view::npos)
      return std::nullopt;
    const auto action = std::ranges::find(kActions, record.substr(0, equal));
    if (action == kActions.end())
      return std::nullopt;
    const auto index = static_cast<std::size_t>(action - kActions.begin());
    if (seen[index])
      return std::nullopt;
    seen[index] = true;
    const auto values = record.substr(equal + 1);
    const auto comma = values.find(',');
    if (comma == std::string_view::npos)
      return std::nullopt;
    const std::array tokens{values.substr(0, comma), values.substr(comma + 1)};
    for (std::size_t slot = 0; slot < tokens.size(); ++slot) {
      const auto control = std::ranges::find(kControls, tokens[slot]);
      if (control == kControls.end())
        return std::nullopt;
      result.controls[index][slot] = static_cast<PlayInputControl>(control - kControls.begin());
    }
  }
  if (!std::ranges::all_of(seen, [](bool present) { return present; }) || !result.Valid())
    return std::nullopt;
  return result;
}
} // namespace
bool ProjectWorkspace::SavePlayInputBindings(const PlayInputBindings &bindings,
                                             std::string *error) {
  if (error)
    error->clear();
  if (!Writable() || root_.empty() || HasRecoveryJournal())
    return Fail(error, "Play input settings require a writable project with resolved recovery");
  if (!bindings.Valid())
    return Fail(error, "invalid Play input bindings");
  if (!MetadataDirectory(root_, false, error))
    return false;
  const auto path = root_ / ".nexora/play-input.ini";
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (!Missing(status, ec) && (ec || !std::filesystem::is_regular_file(status)))
    return Fail(error, "Play input settings destination is unavailable or unsafe");
  std::string text = "schema=1\n";
  for (std::size_t action = 0; action < bindings.controls.size(); ++action) {
    text += kActions[action];
    text += '=';
    text += kControls[static_cast<std::size_t>(bindings.controls[action][0])];
    text += ',';
    text += kControls[static_cast<std::size_t>(bindings.controls[action][1])];
    text += '\n';
  }
  if (text.size() > kMaximumPlayInputSettingsBytes)
    return Fail(error, "Play input settings exceed byte limit");
  return detail::AtomicWrite(path, text, error);
}
std::optional<PlayInputBindings> ProjectWorkspace::LoadPlayInputBindings(std::string *error) const {
  if (error)
    error->clear();
  const auto fail = [&](std::string_view message) -> std::optional<PlayInputBindings> {
    Fail(error, message);
    return std::nullopt;
  };
  if (root_.empty() || HasRecoveryJournal())
    return fail("Play input settings require an open project with resolved recovery");
  if (!MetadataDirectory(root_, true, error))
    return std::nullopt;
  const auto path = root_ / ".nexora/play-input.ini";
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (Missing(status, ec))
    return std::nullopt;
  if (ec || !std::filesystem::is_regular_file(status))
    return fail("Play input settings file is unavailable or unsafe");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return fail("could not read Play input settings");
  std::array<char, kMaximumPlayInputSettingsBytes + 1> buffer{};
  input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  const auto count = static_cast<std::size_t>(input.gcount());
  if (input.bad() || count > kMaximumPlayInputSettingsBytes)
    return fail("Play input settings are unreadable or exceed byte limit");
  const auto parsed = Parse(std::string_view{buffer.data(), count});
  if (!parsed)
    return fail("invalid or unsupported Play input settings");
  return parsed;
}
} // namespace nexora::editor
