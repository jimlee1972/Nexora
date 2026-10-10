#pragma once
#include "Nexora/Editor/EditorWorkspace.h"
namespace nexora::editor::detail {
[[nodiscard]] std::optional<std::string>
    EncodePrefabPlacements(std::span<const SceneDocument::PrefabPlacement>);
bool ReadPrefabPlacementLine(std::string_view, std::vector<SceneDocument::PrefabPlacement> &);
} // namespace nexora::editor::detail
