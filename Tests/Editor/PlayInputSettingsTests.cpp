#include "Nexora/Editor/EditorWorkspace.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora::editor;
namespace fs = std::filesystem;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const fs::path &path) {
  std::ifstream f(path, std::ios::binary);
  Require(static_cast<bool>(f), "fixture read failed");
  return {std::istreambuf_iterator<char>{f}, {}};
}
void Write(const fs::path &path, std::string_view bytes) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << bytes;
  f.close();
  Require(!f.fail(), "fixture write failed");
}
struct Cleanup {
  fs::path path;
  ~Cleanup() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};
void Run() {
  const auto base = fs::temp_directory_path() /
                    ("nexora-play-settings-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Cleanup cleanup{base};
  const auto root = base / fs::path(u8"專案🙂");
  ProjectWorkspace workspace, observer;
  std::string error;
  Require(workspace.Create(root, "Input persistence", &error) &&
              observer.Open(root, ProjectAccess::ReadOnly, &error),
          "project setup failed");
  Require(!workspace.LoadPlayInputBindings(&error) && error.empty(),
          "missing settings were not legacy defaults");
  const auto path = root / ".nexora/play-input.ini";
  const auto stage = root / ".nexora/play-input.ini.tmp";
  const auto backup = base / "last-good.ini";
  PlayInputBindings profile;
  profile.controls[1][0] = PlayInputControl::B;
  profile.controls[4] = {PlayInputControl::None, PlayInputControl::None};
  Require(workspace.SavePlayInputBindings(profile, &error) && error.empty() &&
              workspace.LoadPlayInputBindings(&error) == profile &&
              observer.LoadPlayInputBindings(&error) == profile,
          "owning round trip failed");
  const auto committed = Read(path);
  const auto owning = workspace.LoadPlayInputBindings(&error);
  Require(!observer.SavePlayInputBindings({}, &error) && !error.empty() && Read(path) == committed,
          "read-only save changed settings");
  auto invalid = profile;
  invalid.controls[0][0] = PlayInputControl::B;
  Require(!workspace.SavePlayInputBindings(invalid, &error) && !error.empty() &&
              Read(path) == committed && !fs::exists(stage),
          "invalid profile staged or replaced settings");
  // Reordered records, CRLF, and no final LF retain the same schema/profile.
  std::vector<std::string> records;
  for (std::size_t start = 0; start < committed.size();) {
    const auto end = committed.find('\n', start);
    records.push_back(committed.substr(start, end - start));
    start = end + 1;
  }
  std::string compatible = records[0] + "\r\n";
  for (std::size_t i = records.size() - 1; i > 0; --i)
    compatible += records[i] + (i == 1 ? "" : "\r\n");
  Write(path, compatible);
  Require(workspace.LoadPlayInputBindings(&error) == profile && error.empty(),
          "compatible record ordering/line endings rejected");
  std::vector<std::string> corrupt{
      "",
      "schema=2\n",
      "schema=1\n",
      committed + "extra=None,None\n",
      committed + records[1] + "\n",
      committed + "\n",
      committed.substr(0, committed.find("modifier=")),
      std::string(ProjectWorkspace::kMaximumPlayInputSettingsBytes + 1, 'x')};
  auto replace = [&](std::string_view from, std::string_view to) {
    auto result = committed;
    const auto at = result.find(from);
    Require(at != std::string::npos, "corruption fixture key absent");
    result.replace(at, from.size(), to);
    corrupt.push_back(result);
  };
  replace("right=B,Right", "right=A,Right");
  replace("left=A,Left", "left=MouseLeft,Left");
  replace("right=B,Right", "right=Escape,Right");
  replace("right=B,Right", "right=B,B");
  replace("right=B,Right", "right=B,Right,Space");
  replace("right=B,Right", "right=b,Right");
  replace("right=B,Right", std::string("right=B\0,Right", 14));
  for (const auto &bytes : corrupt) {
    Write(path, bytes);
    Require(!workspace.LoadPlayInputBindings(&error) && !error.empty() && Read(path) == bytes,
            "corrupt input was admitted or modified");
  }
  // Explicit save replaces corrupt settings; ordinary reads never do.
  Require(workspace.SavePlayInputBindings(profile, &error) && Read(path) == committed,
          "explicit corrupt replacement failed");
  Write(stage, "unrelated stage");
  Require(!workspace.SavePlayInputBindings({}, &error) && Read(path) == committed &&
              Read(stage) == "unrelated stage",
          "occupied stage file was truncated or settings changed");
  fs::remove(stage);
  fs::create_directory(stage);
  Require(!workspace.SavePlayInputBindings({}, &error) && fs::is_directory(stage) &&
              Read(path) == committed,
          "occupied stage directory changed");
  fs::remove(stage);
  Write(backup, committed);
  for (bool dangling : {false, true}) {
    std::error_code ec;
    fs::create_symlink(dangling ? base / "missing" : backup, stage, ec);
    if (!ec) {
      Require(!workspace.SavePlayInputBindings({}, &error) &&
                  fs::is_symlink(fs::symlink_status(stage)) && Read(path) == committed &&
                  Read(backup) == committed,
              "stage alias changed data");
      fs::remove(stage);
    }
  }
  fs::remove(path);
  fs::create_directory(path);
  Require(!workspace.LoadPlayInputBindings(&error) &&
              !workspace.SavePlayInputBindings({}, &error) && fs::is_directory(path),
          "directory settings destination was admitted or removed");
  fs::remove(path);
  for (bool dangling : {false, true}) {
    std::error_code ec;
    fs::create_symlink(dangling ? base / "missing" : backup, path, ec);
    if (!ec) {
      Require(!workspace.LoadPlayInputBindings(&error) && !error.empty() &&
                  !workspace.SavePlayInputBindings({}, &error) &&
                  fs::is_symlink(fs::symlink_status(path)) && Read(backup) == committed,
              "settings alias was followed or replaced");
      fs::remove(path);
    }
  }
  Require(workspace.SavePlayInputBindings(profile, &error), "valid retry failed");
  Write(root / ".nexora/workspace.recovery", "pending");
  Require(!workspace.LoadPlayInputBindings(&error) &&
              !workspace.SavePlayInputBindings({}, &error) && Read(path) == committed &&
              workspace.DiscardRecovery(&error),
          "recovery gate changed settings");
  Require(workspace.LoadPlayInputBindings(&error) == profile,
          "resolved recovery did not allow reload");
  // Metadata-directory aliases are rejected without touching their target.
  const auto metadata = base / "Metadata";
  fs::rename(root / ".nexora", metadata);
  std::error_code ec;
  fs::create_directory_symlink(metadata, root / ".nexora", ec);
  if (!ec) {
    Require(!workspace.LoadPlayInputBindings(&error) &&
                !workspace.SavePlayInputBindings({}, &error) &&
                Read(metadata / "play-input.ini") == committed,
            "metadata alias was followed");
    fs::remove(root / ".nexora");
  }
  fs::rename(metadata, root / ".nexora");
  workspace = ProjectWorkspace{};
  Require(workspace.Open(root, &error) && workspace.LoadPlayInputBindings(&error) == profile &&
              owning == profile,
          "process-scope reopen or owning profile failed");
  Require(workspace.SavePlayInputBindings({}, &error) && owning == profile,
          "new save changed owning snapshot");
  const auto legacy = base / "Legacy";
  fs::create_directories(legacy);
  Write(legacy / "project.nexora", "schema=1\nname=Legacy\n");
  ProjectWorkspace legacy_reader;
  Require(legacy_reader.Open(legacy, ProjectAccess::ReadOnly, &error) &&
              !legacy_reader.LoadPlayInputBindings(&error) && error.empty() &&
              !fs::exists(legacy / ".nexora"),
          "legacy read created metadata or failed defaulting");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Project Play input settings contracts passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
