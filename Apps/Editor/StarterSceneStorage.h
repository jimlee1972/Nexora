#pragma once

#include "Nexora/Editor/EditorWorkspace.h"

#include <filesystem>

namespace nexora::editor::app {
// The bootstrap scene is named before its first save. Save All intentionally requires
// existing ordinary parents, so the writer prepares this one private directory on Save All.
inline bool PrepareStarterSceneStorage(const ProjectWorkspace &workspace) {
  if (!workspace.Writable() || workspace.HasRecoveryJournal() || workspace.HasExternalChange())
    return false;
  const auto ordinary_directory = [](const std::filesystem::path &path) {
    std::error_code error;
    return std::filesystem::is_directory(std::filesystem::symlink_status(path, error)) && !error;
  };
  const auto metadata = workspace.Root() / ".nexora";
  if (!ordinary_directory(workspace.Root()) || !ordinary_directory(metadata))
    return false;
  const auto scenes = metadata / "scenes";
  std::error_code error;
  const auto status = std::filesystem::symlink_status(scenes, error);
  if (error == std::errc::no_such_file_or_directory ||
      (!error && status.type() == std::filesystem::file_type::not_found)) {
    error.clear();
    std::filesystem::create_directory(scenes, error);
    if (error)
      return false;
  }
  return ordinary_directory(scenes);
}
} // namespace nexora::editor::app
