#pragma once
#include "Nexora/Editor/PrefabPlacementInspection.h"

namespace nexora::editor {
struct PrefabPlacementOverrideRow final {
  std::vector<foundation::Uuid> scope;
  PrefabRevisionReference source;
  foundation::Uuid node;
  // New local opaque component fields have no retained source field identity.
  std::optional<foundation::Uuid> property;
  std::string field;
  SceneDocument::NodeKey target;
  std::optional<std::string> retained, local;
  bool structural{};
  friend bool operator==(const PrefabPlacementOverrideRow &,
                         const PrefabPlacementOverrideRow &) = default;
};
// Immutable owning read-only review. Rows' borrows expire with this review; no write capability.
class NEXORA_EDITOR_API PrefabPlacementOverrideReview final {
public:
  [[nodiscard]] foundation::Uuid Instance() const noexcept { return inspection_.Instance(); }
  [[nodiscard]] PrefabRevisionReference Source() const noexcept { return inspection_.Source(); }
  [[nodiscard]] std::optional<std::uint64_t> PublishedRevision() const noexcept {
    return inspection_.PublishedRevision();
  }
  [[nodiscard]] std::span<const PrefabPlacementOverrideRow> Rows() const noexcept { return rows_; }

private:
  friend class PrefabPlacementOverrides;
  PrefabPlacementOverrideReview(PrefabPlacementInspection inspection,
                                std::vector<PrefabPlacementOverrideRow> rows)
      : inspection_(std::move(inspection)), rows_(std::move(rows)) {}
  PrefabPlacementInspection inspection_;
  std::vector<PrefabPlacementOverrideRow> rows_;
};
class NEXORA_EDITOR_API PrefabPlacementOverrides final {
public:
  static constexpr std::size_t kMaximumRows = 32768;
  static constexpr std::size_t kMaximumReportBytes = 16 * 1024 * 1024;
  // Explicit serialized authoring-thread review. Exact retained sources materialize only in a
  // temporary document. Whole-scene semantic comparison budgets also apply; rejection is atomic.
  [[nodiscard]] static std::optional<PrefabPlacementOverrideReview>
  Prepare(const ProjectWorkspace &, const SceneDocument &, SceneDocument::NodeKey,
          std::string *error = nullptr);
  [[nodiscard]] static bool Matches(const ProjectWorkspace &, const SceneDocument &,
                                    const PrefabPlacementOverrideReview &);
  // Restores the whole instance's supported source properties as one Undo/Redo transaction.
  // Current writer and explicit host authoring authority are required. Structural rows reject.
  static bool Revert(const ProjectWorkspace &, SceneDocument &,
                     const PrefabPlacementOverrideReview &, bool authorized,
                     std::string *error = nullptr);
  // Row indices belong to this immutable review. Each selected semantic lane expands to its
  // complete scoped property group, including rotation hints or dormant component values.
  static bool RevertSelected(const ProjectWorkspace &, SceneDocument &,
                             const PrefabPlacementOverrideReview &,
                             std::span<const std::size_t> rows, bool authorized,
                             std::string *error = nullptr);

private:
  static bool RevertProperties(const ProjectWorkspace &, SceneDocument &,
                               const PrefabPlacementOverrideReview &,
                               std::optional<std::span<const std::size_t>>, bool authorized,
                               std::string *error);
};
} // namespace nexora::editor
