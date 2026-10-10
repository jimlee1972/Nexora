#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <utility>

namespace nexora::editor {
std::optional<ChromeTraceCapture>
ProjectWorkspace::ImportChromeTraceJson(ChromeTraceSelection selection, std::string *error) const {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) -> std::optional<ChromeTraceCapture> {
    if (error)
      *error = message;
    return {};
  };
  if (Root().empty() || Project().id.IsNil() || HasRecoveryJournal() || HasExternalChange())
    return fail("Open a project with resolved recovery and external changes before importing.");
  std::error_code ec;
  const auto root = std::filesystem::canonical(Root(), ec);
  if (ec || root != Root())
    return fail("Chrome trace project root is unavailable or aliased.");
  const auto metadata = Root() / ".nexora";
  const auto directory = std::filesystem::symlink_status(metadata, ec);
  if (ec || !std::filesystem::is_directory(directory))
    return fail("Chrome trace metadata directory is unavailable or unsafe.");
  const auto path = metadata / "chrome-trace.json";
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status))
    return fail("Place a regular Chrome trace JSON file at .nexora/chrome-trace.json.");
  const auto links = std::filesystem::hard_link_count(path, ec);
  if (ec || links != 1)
    return fail("Chrome trace aliases are unsupported.");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return fail("Could not read Chrome trace JSON.");
  std::string bytes;
  std::array<char, 4096> buffer{};
  while (input) {
    const auto requested =
        std::min(buffer.size(), ChromeTraceImporter::kMaximumBytes - bytes.size() + 1);
    input.read(buffer.data(), static_cast<std::streamsize>(requested));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > ChromeTraceImporter::kMaximumBytes - bytes.size())
      return fail("Chrome trace JSON exceeds the supported byte limit.");
    bytes.append(buffer.data(), count);
  }
  if (!input.eof() || HasRecoveryJournal() || HasExternalChange())
    return fail("Chrome trace read failed or project state changed.");
  return ChromeTraceImporter::Import(bytes, std::move(selection), error);
}
} // namespace nexora::editor
