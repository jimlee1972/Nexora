#pragma once
#include "Nexora/Editor/PrefabPropertyPlan.h"
#include "Nexora/Editor/SceneComparison.h"
namespace nexora::editor {
enum class PrefabRebaseDecision : unsigned char { KeepLocal, TakeSource };
struct PrefabRebaseChoice final {
  PrefabPropertySelection property;
  PrefabRebaseDecision decision{PrefabRebaseDecision::KeepLocal};
};
struct PrefabRebasePlan final {
  SceneComparison changes{};
  std::vector<PrefabPropertySelection> conflicts{};
  std::size_t unresolved{};
  std::optional<std::string> candidate{};
};
// Owning, same-identity property plan. Complete groups conflict even when separate lanes change.
// Missing conflict choices leave a reviewable plan without a candidate. Unknown/duplicate choices
// and incompatible structure reject. Choices confer no authority. No IO, mutation or history.
[[nodiscard]] NEXORA_EDITOR_API std::optional<PrefabRebasePlan>
BuildPrefabRebasePlan(const PrefabAsset &old_base, const PrefabAsset &local,
                      const PrefabAsset &new_base, std::span<const PrefabRebaseChoice> choices = {},
                      std::string *error = nullptr);
} // namespace nexora::editor
