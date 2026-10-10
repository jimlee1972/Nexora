#pragma once
#include "Nexora/Editor/PrefabPlacementInspection.h"
#include <memory>

namespace nexora::editor {
enum class PrefabPlacementRebaseDecision : std::uint8_t { KeepLocal, TakeSource };
struct PrefabPlacementRebaseChoice final {
  std::size_t row{};
  PrefabPlacementRebaseDecision decision{PrefabPlacementRebaseDecision::KeepLocal};
};
struct PrefabPlacementRebaseValue final {
  std::string field;
  std::optional<std::string> retained, local, published;
};
struct PrefabPlacementRebaseRow final {
  std::vector<foundation::Uuid> scope;
  PrefabRevisionReference retained_source, published_source;
  foundation::Uuid node;
  std::optional<foundation::Uuid> property;
  SceneDocument::NodeKey target;
  std::string group;
  std::vector<PrefabPlacementRebaseValue> values;
  bool conflict{};
};
// Shared immutable owning observations. Row borrows expire with the last owning review.
// Choices and prepared candidates convey no current authoring authority.
class NEXORA_EDITOR_API PrefabPlacementRebaseReview final {
public:
  [[nodiscard]] std::span<const PrefabPlacementRebaseRow> Rows() const noexcept;
  [[nodiscard]] PrefabRevisionReference Source() const noexcept;
  [[nodiscard]] PrefabRevisionReference PublishedSource() const noexcept;
  [[nodiscard]] foundation::Uuid Instance() const noexcept;
  [[nodiscard]] bool Ready() const noexcept { return state_ && candidate_.has_value(); }
  [[nodiscard]] std::size_t Unresolved() const noexcept { return unresolved_; }

private:
  friend class PrefabPlacementRebase;
  struct State;
  PrefabPlacementRebaseReview(std::shared_ptr<const State> state,
                              std::optional<std::string> candidate, std::size_t unresolved)
      : state_(std::move(state)), candidate_(std::move(candidate)), unresolved_(unresolved) {}
  std::shared_ptr<const State> state_;
  std::optional<std::string> candidate_;
  std::size_t unresolved_{};
};
class NEXORA_EDITOR_API PrefabPlacementRebase final {
public:
  static constexpr std::size_t kMaximumRows = 32768;
  static constexpr std::size_t kMaximumReportBytes = 16 * 1024 * 1024;
  // Explicit authoring-thread read. Requires a newer current root and identical stable schema,
  // dependency references and hierarchy; unsupported structural reconciliation rejects.
  [[nodiscard]] static std::optional<PrefabPlacementRebaseReview>
  Prepare(const ProjectWorkspace &, const SceneDocument &, SceneDocument::NodeKey,
          std::string *error = nullptr);
  // Pure owning computation against this exact immutable review. No IO or document mutation.
  [[nodiscard]] static std::optional<PrefabPlacementRebaseReview>
  Resolve(const PrefabPlacementRebaseReview &, std::span<const PrefabPlacementRebaseChoice>,
          std::string *error = nullptr);
  [[nodiscard]] static bool Matches(const ProjectWorkspace &, const SceneDocument &,
                                    const PrefabPlacementRebaseReview &);
  static bool Apply(const ProjectWorkspace &, SceneDocument &, const PrefabPlacementRebaseReview &,
                    bool authorized, std::string *error = nullptr);
};
} // namespace nexora::editor
