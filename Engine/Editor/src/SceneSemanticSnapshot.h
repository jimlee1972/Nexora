#pragma once
#include "Nexora/Editor/SceneComparison.h"
#include <map>

namespace nexora::editor::detail {
using SceneSemanticFields = std::map<std::string, std::string, std::less<>>;
std::optional<SceneSemanticFields> ReadSceneSemanticFields(std::optional<std::string_view>,
                                                           std::string *error);
std::optional<SceneComparison> CompareSceneSemanticFields(const SceneSemanticFields &,
                                                          const SceneSemanticFields &,
                                                          const SceneSemanticFields &,
                                                          std::string *error);
std::optional<std::string> BuildMergedScene(const SceneSemanticFields &, std::string *error);
} // namespace nexora::editor::detail
