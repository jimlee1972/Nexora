#pragma once
#include "Nexora/Editor/PrefabAssets.h"

namespace nexora::editor {
// Owning complete property candidate aligned by stable node UUID. No IO or live mutation.
// Exact node identity sets must match; target scene name/persistence and entity IDs survive.
[[nodiscard]] NEXORA_EDITOR_API std::optional<std::string>
BuildPrefabPropertySnapshot(const PrefabAsset &current, const PrefabAsset &source);
struct PrefabPropertySelection final {
  foundation::Uuid node, field;
};
// Explicit stable fields; empty selection is a canonical no-op. Duplicate/unknown selections,
// unsupported metadata fields and incompatible resulting hierarchy reject the whole candidate.
[[nodiscard]] NEXORA_EDITOR_API std::optional<std::string>
BuildPrefabPropertySnapshot(const PrefabAsset &current, const PrefabAsset &source,
                            std::span<const PrefabPropertySelection> selected);
} // namespace nexora::editor
