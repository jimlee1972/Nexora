#pragma once

#include "Nexora/Editor/Api.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor {
enum class SceneComparisonChoice : unsigned char { Shared, Local, Remote, Unresolved };
struct SceneComparisonRow final {
  std::string stable_path;
  std::optional<std::string> base, local, remote;
  SceneComparisonChoice choice{SceneComparisonChoice::Unresolved};
  bool operator==(const SceneComparisonRow &) const = default;
};
struct SceneComparison final {
  static constexpr std::size_t kMaximumSourceBytes = 8 * 1024 * 1024;
  static constexpr std::size_t kMaximumEntities = 4096;
  static constexpr std::size_t kMaximumValueBytes = 64 * 1024;
  static constexpr std::size_t kMaximumSnapshotBytes = 4 * 1024 * 1024;
  static constexpr std::size_t kMaximumRows = 131072;
  static constexpr std::size_t kMaximumResultBytes = 16 * 1024 * 1024;
  std::vector<SceneComparisonRow> rows;
  std::size_t conflicts{};
  bool operator==(const SceneComparison &) const = default;
};
// Synchronous, owning read-only comparison. nullopt denotes an absent whole source, distinct from
// an empty/corrupt present source. Production parsing/migration runs in isolated temporary Worlds.
// Includes only changed stable fields; choices are inspection hints, not a validated merged tree.
// No IO, callback, plugin load, source publication or live-document/history/baseline mutation.
// Limits describe logical source/snapshot/result bytes, not total process memory. Callers provide
// project/revision provenance and serialize access to borrowed input views for this call.
[[nodiscard]] NEXORA_EDITOR_API std::optional<SceneComparison>
CompareSceneRevisions(std::optional<std::string_view> base, std::optional<std::string_view> local,
                      std::optional<std::string_view> remote, std::string *error = nullptr);
} // namespace nexora::editor
