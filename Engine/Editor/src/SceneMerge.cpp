#include "Nexora/Editor/SceneMerge.h"
#include "SceneSemanticSnapshot.h"
#include <atomic>
#include <limits>
#include <map>
#include <set>

namespace nexora::editor {
namespace {
std::atomic<std::uint64_t> next_review{1};
std::uint64_t NextReview() noexcept {
  auto value = next_review.load(std::memory_order_relaxed);
  while (value != std::numeric_limits<std::uint64_t>::max())
    if (next_review.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
      return value;
  return 0;
}
std::optional<std::string_view> View(const std::optional<std::string> &source) noexcept {
  return source ? std::optional<std::string_view>(*source) : std::nullopt;
}
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
bool SamePrefix(const detail::SceneSemanticFields &a, const detail::SceneSemanticFields &b,
                const std::string &prefix) {
  auto left = a.lower_bound(prefix), right = b.lower_bound(prefix);
  while (left != a.end() && left->first.starts_with(prefix)) {
    if (right == b.end() || left->first != right->first || left->second != right->second)
      return false;
    ++left;
    ++right;
  }
  return right == b.end() || !right->first.starts_with(prefix);
}
bool SameRotation(const detail::SceneSemanticFields &a, const detail::SceneSemanticFields &b,
                  const std::string &prefix) {
  return SamePrefix(a, b, prefix + "transform/rotation/") &&
         SamePrefix(a, b, prefix + "authoring/euler/");
}
} // namespace
struct SceneMergeReview::State final {
  std::uint64_t identity{};
  std::optional<std::string> base, local, remote;
  detail::SceneSemanticFields base_fields, local_fields, remote_fields;
  SceneComparison comparison;
};
std::uint64_t SceneMergeReview::Identity() const noexcept { return state_ ? state_->identity : 0; }
const SceneComparison &SceneMergeReview::Comparison() const noexcept {
  static const SceneComparison empty;
  return state_ ? state_->comparison : empty;
}
std::optional<std::string_view> SceneMergeReview::BaseBytes() const noexcept {
  return state_ ? View(state_->base) : std::nullopt;
}
std::optional<std::string_view> SceneMergeReview::LocalBytes() const noexcept {
  return state_ ? View(state_->local) : std::nullopt;
}
std::optional<std::string_view> SceneMergeReview::RemoteBytes() const noexcept {
  return state_ ? View(state_->remote) : std::nullopt;
}
std::optional<SceneMergeReview> SceneMerge::Prepare(std::optional<std::string_view> base,
                                                    std::optional<std::string_view> local,
                                                    std::optional<std::string_view> remote,
                                                    std::string *error) {
  if (error)
    error->clear();
  for (const auto source : {base, local, remote})
    if (source && (source->empty() || source->size() > SceneComparison::kMaximumSourceBytes)) {
      Fail(error, "Merge source is empty or exceeds the supported source budget.");
      return {};
    }
  auto state = std::make_shared<SceneMergeReview::State>();
  if (base)
    state->base = *base;
  if (local)
    state->local = *local;
  if (remote)
    state->remote = *remote;
  auto b = detail::ReadSceneSemanticFields(View(state->base), error);
  if (!b)
    return {};
  auto l = detail::ReadSceneSemanticFields(View(state->local), error);
  if (!l)
    return {};
  auto r = detail::ReadSceneSemanticFields(View(state->remote), error);
  if (!r)
    return {};
  auto comparison = detail::CompareSceneSemanticFields(*b, *l, *r, error);
  if (!comparison)
    return {};
  state->identity = NextReview();
  if (!state->identity) {
    Fail(error, "Merge review identity capacity is exhausted.");
    return {};
  }
  state->base_fields = std::move(*b);
  state->local_fields = std::move(*l);
  state->remote_fields = std::move(*r);
  state->comparison = std::move(*comparison);
  return SceneMergeReview(std::move(state));
}
std::optional<SceneMergeCandidate> SceneMerge::Resolve(const SceneMergeReview &review,
                                                       const SceneMergeRequest &request,
                                                       std::string *error) {
  if (error)
    error->clear();
  if (!review.state_ || request.schema != kSchemaVersion || !request.review ||
      request.review != review.Identity() ||
      request.choices.size() > review.Comparison().rows.size()) {
    Fail(error, "Merge choice schema, immutable review identity or selection capacity is invalid.");
    return {};
  }
  std::map<std::size_t, SceneMergeChoice> choices;
  for (const auto choice : request.choices)
    if (choice.row >= review.Comparison().rows.size() ||
        (choice.choice != SceneMergeChoice::Local && choice.choice != SceneMergeChoice::Remote) ||
        !choices.emplace(choice.row, choice.choice).second) {
      Fail(error, "Merge choice is absent, duplicated or unsupported.");
      return {};
    }
  auto fields = review.state_->base_fields;
  for (std::size_t i = 0; i < review.Comparison().rows.size(); ++i) {
    const auto &row = review.Comparison().rows[i];
    const auto explicit_choice = choices.find(i);
    const std::optional<std::string> *value{};
    if (explicit_choice != choices.end())
      value = explicit_choice->second == SceneMergeChoice::Local ? &row.local : &row.remote;
    else if (row.choice == SceneComparisonChoice::Shared ||
             row.choice == SceneComparisonChoice::Local)
      value = &row.local;
    else if (row.choice == SceneComparisonChoice::Remote)
      value = &row.remote;
    else {
      Fail(error, "Every unresolved merge field requires an explicit source choice.");
      return {};
    }
    if (*value)
      fields[row.stable_path] = **value;
    else
      fields.erase(row.stable_path);
  }
  std::size_t bytes{};
  for (const auto &[path, value] : fields) {
    if (value.size() > SceneComparison::kMaximumValueBytes ||
        path.size() > SceneComparison::kMaximumSnapshotBytes - bytes ||
        value.size() > SceneComparison::kMaximumSnapshotBytes - bytes - path.size()) {
      Fail(error, "Chosen merge fields exceed the aggregate snapshot/value budget.");
      return {};
    }
    bytes += path.size() + value.size();
  }
  if (fields.size() > SceneComparison::kMaximumRows) {
    Fail(error, "Chosen merge fields exceed the snapshot field capacity.");
    return {};
  }
  if (fields.empty())
    return SceneMergeCandidate{};
  std::set<std::string> rotations, bindings;
  for (const auto &[path, value] : fields) {
    static_cast<void>(value);
    if (path.starts_with("entities/")) {
      const auto slash = path.find('/', 9);
      if (slash != path.npos)
        rotations.insert(path.substr(0, slash + 1));
    } else if (path.starts_with("prefabs/")) {
      const auto slash = path.find('/', 8);
      if (slash != path.npos)
        bindings.insert(path.substr(0, slash + 1));
    }
  }
  for (const auto *groups : {&rotations, &bindings})
    for (const auto &prefix : *groups) {
      bool coherent{};
      for (const auto *source : {&review.state_->base_fields, &review.state_->local_fields,
                                 &review.state_->remote_fields})
        coherent = coherent || (groups == &rotations ? SameRotation(fields, *source, prefix)
                                                     : SamePrefix(fields, *source, prefix));
      if (!coherent) {
        Fail(error,
             "Rotation/Euler and each prefab binding must retain one complete source group.");
        return {};
      }
    }
  auto candidate = detail::BuildMergedScene(fields, error);
  if (!candidate)
    return {};
  const auto verified = detail::ReadSceneSemanticFields(*candidate, error);
  if (!verified || *verified != fields) {
    Fail(error, "Complete candidate validation changed or discarded selected semantic fields.");
    return {};
  }
  return SceneMergeCandidate{std::move(candidate)};
}
} // namespace nexora::editor
