#pragma once
#include "Nexora/Editor/PrefabAssets.h"
#include "Nexora/Editor/SceneComparison.h"

namespace nexora::editor {
// Owning read-only comparison aligned by stable node/field UUIDs. nullptr means absent source.
// Official scene validation and SceneComparison budgets apply. No IO or document mutation.
[[nodiscard]] NEXORA_EDITOR_API std::optional<SceneComparison>
ComparePrefabRevisions(const PrefabAsset *base, const PrefabAsset *local, const PrefabAsset *remote,
                       std::string *error = nullptr);
} // namespace nexora::editor
