#pragma once

#include "Nexora/EditorImGui/EditorImGui.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace nexora::editor::app {
struct ProjectFiles final {
  std::vector<imgui::ProjectFileItem> items;
  std::string error;
  std::size_t inspected_entries{};
};

// Owning filesystem-only snapshot; safe on a reader without document or UI borrows.
inline ProjectFiles CaptureProjectFiles(const std::filesystem::path &root,
                                        std::size_t maximum_entries = 100000) {
  ProjectFiles result;
  const auto maximum = std::min<std::size_t>(maximum_entries, 100000);
  std::size_t bytes{};
  const auto visit = [&](auto &&self, const std::filesystem::path &folder,
                         std::size_t depth) -> bool {
    if (depth >= 64) {
      result.error = "Project tree reached its 64-level limit. Deeper entries are not listed.";
      return false;
    }
    std::error_code error;
    std::vector<std::filesystem::path> entries;
    std::filesystem::directory_iterator iterator(root / folder, error), end;
    for (; !error && iterator != end; iterator.increment(error)) {
      if (result.inspected_entries >= maximum) {
        result.error = "Project tree reached its entry limit. Narrow the project folder.";
        return false;
      }
      ++result.inspected_entries;
      auto relative = folder / iterator->path().filename();
      bytes += relative.generic_u8string().size();
      if (bytes > 32 * 1024 * 1024) {
        result.error = "Project tree exceeded its path budget. Narrow the project folder.";
        return false;
      }
      entries.push_back(std::move(relative));
    }
    if (error) {
      result.error =
          "Some project entries could not be read. Check folder permissions and Refresh.";
      return true;
    }
    std::ranges::sort(entries);
    for (const auto &path : entries) {
      const auto status = std::filesystem::symlink_status(root / path, error);
      if (error) {
        result.error = "Some project entries changed during Refresh. Refresh to retry.";
        continue;
      }
      const bool directory = std::filesystem::is_directory(status);
      result.items.push_back({path, directory, std::filesystem::is_symlink(status)});
      if (directory && !self(self, path, depth + 1))
        return false;
    }
    return true;
  };
  visit(visit, {}, 0);
  return result;
}
} // namespace nexora::editor::app
