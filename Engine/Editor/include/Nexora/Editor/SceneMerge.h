#pragma once
#include "Nexora/Editor/SceneComparison.h"
#include <cstdint>
#include <memory>
#include <utility>

namespace nexora::editor {
enum class SceneMergeChoice : std::uint8_t { Local, Remote };
struct SceneMergeSelection final {
  std::size_t row{};
  SceneMergeChoice choice{};
};
struct SceneMergeRequest final {
  std::uint32_t schema{1};
  std::uint64_t review{};
  std::vector<SceneMergeSelection> choices;
};
class NEXORA_EDITOR_API SceneMergeReview final {
public:
  [[nodiscard]] std::uint64_t Identity() const noexcept;
  [[nodiscard]] const SceneComparison &Comparison() const noexcept;
  // Owning immutable review retains these exact raw sources; borrows expire with the review.
  [[nodiscard]] std::optional<std::string_view> BaseBytes() const noexcept;
  [[nodiscard]] std::optional<std::string_view> LocalBytes() const noexcept;
  [[nodiscard]] std::optional<std::string_view> RemoteBytes() const noexcept;

private:
  struct State;
  friend class SceneMerge;
  explicit SceneMergeReview(std::shared_ptr<const State> state) : state_(std::move(state)) {}
  std::shared_ptr<const State> state_;
};
struct SceneMergeCandidate final {
  // nullopt is an explicit absent whole scene, never an empty or invalid present file.
  std::optional<std::string> bytes;
};
class NEXORA_EDITOR_API SceneMerge final {
public:
  static constexpr std::uint32_t kSchemaVersion = 1;
  // Synchronous serialized snapshot work in isolated Worlds. No live authoring, IO or callbacks.
  // Source/snapshot/result budgets are inherited from SceneComparison; allocation may throw.
  [[nodiscard]] static std::optional<SceneMergeReview>
  Prepare(std::optional<std::string_view> base, std::optional<std::string_view> local,
          std::optional<std::string_view> remote, std::string *error = nullptr);
  // Indices bind to this immutable review identity. Every unresolved row needs an explicit side;
  // nonconflicting hints apply unless overridden. Reparse/reproject must match every chosen field.
  // Rotation/Euler and each complete prefab binding must match one captured source group.
  // Output is owning data, not project/write authority; publication requires separate current
  // project/document/file/reference validation. All inputs/last-good sources remain unchanged.
  [[nodiscard]] static std::optional<SceneMergeCandidate>
  Resolve(const SceneMergeReview &, const SceneMergeRequest &, std::string *error = nullptr);
};
} // namespace nexora::editor
