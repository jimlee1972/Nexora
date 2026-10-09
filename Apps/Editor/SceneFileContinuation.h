#pragma once

#include "Nexora/EditorImGui/EditorImGui.h"

// Compose the owning retry after a scene save has returned a reviewed disk revision. The save
// destination and the subsequent New/Open destination are independent values.
inline bool PrepareSceneOverwriteRequest(nexora::editor::imgui::SceneFileRequest &request,
                                         const nexora::editor::SceneFileResult &result,
                                         const std::optional<std::filesystem::path> &current_path) {
  if (result.status != nexora::editor::SceneFileStatus::NeedsOverwrite || !result.overwrite_token)
    return false;
  if (request.save_current && !request.save_path) {
    if (!current_path)
      return false;
    request.save_path = current_path;
  }
  request.replace_existing = false;
  request.overwrite_token = result.overwrite_token;
  return true;
}
