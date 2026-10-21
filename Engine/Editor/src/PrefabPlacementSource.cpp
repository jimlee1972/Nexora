#include "Nexora/Editor/PrefabPlacementOverrides.h"
#include "PrefabPlacementPropertyInternal.h"
#include "PrefabPlacementSourceTestAccess.h"
#include <limits>
#include <new>
#include <utility>

namespace nexora::editor {
namespace {
thread_local bool fail_before_publish{};
}
void PrefabPlacementSourceTestAccess::FailNextBeforePublish() noexcept {
  fail_before_publish = true;
}
std::optional<PrefabAsset> PrefabPlacementOverrides::ApplySelectedToSource(
    const ProjectWorkspace &workspace, const SceneDocument &target,
    const PrefabPlacementOverrideReview &review, std::span<const std::size_t> rows, bool authorized,
    std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) -> std::optional<PrefabAsset> {
    if (error)
      *error = message;
    return {};
  };
  const auto &inspection = review.inspection_;
  if (!authorized || !workspace.Writable() || target.world_.Kind() != runtime::WorldKind::Editor ||
      !Matches(workspace, target, review) || rows.empty() || rows.size() > kMaximumRows ||
      !inspection.graph_ || inspection.graph_->instances.size() != 1 || !inspection.current_ ||
      inspection.current_->revision != inspection.Source().revision ||
      std::ranges::any_of(review.rows_, &PrefabPlacementOverrideRow::structural))
    return fail("Source application requires an authorized current non-nested property review.");
  const auto retained = std::ranges::find_if(inspection.graph_->assets, [&](const auto &asset) {
    return asset.id == inspection.Source().asset && asset.revision == inspection.Source().revision;
  });
  if (retained == inspection.graph_->assets.end() || *retained != *inspection.current_ ||
      !retained->nested.empty() || retained->revision == std::numeric_limits<std::uint64_t>::max())
    return fail("The retained source is stale, nested, unavailable or revision-exhausted.");
  using Identity = std::pair<std::uint64_t, std::uint64_t>;
  std::map<Identity, const PrefabNodeIdentity *> identities;
  for (const auto &node : retained->nodes)
    identities.emplace(Identity{node.id.high, node.id.low}, &node);
  std::map<runtime::Id, std::pair<runtime::Id, std::set<std::string>>> selected;
  std::set<std::size_t> unique;
  for (const auto index : rows) {
    if (index >= review.rows_.size() || !unique.insert(index).second)
      return fail("Selected source property row is absent or duplicated.");
    const auto &row = review.rows_[index];
    const auto found = identities.find({row.node.high, row.node.low});
    const auto group = detail::PlacementPropertyGroup(row.field);
    if (!row.scope.empty() || row.source != inspection.Source() || !row.property ||
        found == identities.end() || target.Key(row.target.id) != row.target ||
        (group != "name" && group != "transform.position" && group != "transform.rotation" &&
         group != "transform.scale" && group != "camera" && group != "light" && group != "mesh" &&
         !group.starts_with("opaque/")))
      return fail("Selected source property identity, scope or group is unsupported.");
    const auto &node = *found->second;
    const auto field =
        std::ranges::find(node.properties, *row.property, &PrefabPropertyIdentity::id);
    if (field == node.properties.end() || field->field != group)
      return fail("Selected source field identity no longer matches its canonical group.");
    auto [entry, inserted] =
        selected.try_emplace(node.serialized_node, row.target.id, std::set<std::string>{});
    if (!inserted && entry->second.first != row.target.id)
      return fail(
          "One source node cannot receive different live instances in a single publication.");
    entry->second.second.insert(group);
  }
  runtime::World source_world, local_world;
  SceneDocument source(source_world, source_world.LoadScene("Source publication candidate"));
  SceneDocument local(local_world, local_world.LoadScene("Live property observation"));
  if (!source.ReloadBytes(retained->scene_bytes, PrefabAssets::kMaximumNodes) ||
      !local.ReloadBytes(inspection.expected_.Bytes(), PrefabAssets::kMaximumNodes))
    return fail("Source property staging exceeds supported document budgets.");
  const auto source_runtime =
      source_world.SaveScene(source.scene_, PrefabAssets::kMaximumSceneBytes);
  const auto local_runtime = local_world.SaveScene(local.scene_, PrefabAssets::kMaximumSceneBytes);
  if (!source_runtime || !local_runtime)
    return fail("Source property runtime observations exceed their byte budget.");
  auto source_properties = detail::PlacementRuntimeProperties(*source_runtime);
  const auto local_properties = detail::PlacementRuntimeProperties(*local_runtime);
  if (!source_properties || !local_properties)
    return fail("Source property runtime schema is unavailable.");
  std::map<runtime::Id, SceneDocument::Node *> source_nodes;
  std::map<runtime::Id, const SceneDocument::Node *> local_nodes;
  for (auto &node : source.nodes_)
    source_nodes.emplace(node.id, &node);
  for (const auto &node : local.nodes_)
    local_nodes.emplace(node.id, &node);
  for (const auto &[source_id, selection] : selected) {
    const auto &[live_id, groups] = selection;
    if (!source_nodes.contains(source_id) || !local_nodes.contains(live_id) ||
        !source_properties->contains(source_id) || !local_properties->contains(live_id))
      return fail("Source-to-live property mapping is unavailable.");
    auto &destination = *source_nodes.at(source_id);
    const auto &observation = *local_nodes.at(live_id);
    if (groups.contains("name"))
      destination.name = observation.name;
    if (groups.contains("transform.rotation"))
      destination.euler_hint = observation.euler_hint;
    for (const auto &group : groups)
      if (group.starts_with("opaque/")) {
        runtime::TypeId type{};
        const auto text = std::string_view(group).substr(7);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), type);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !type)
          return fail("Source opaque property type is invalid.");
        const auto value = std::ranges::find(observation.opaque, type, &OpaqueComponent::type);
        const auto old = std::ranges::find(destination.opaque, type, &OpaqueComponent::type);
        if (value == observation.opaque.end())
          std::erase_if(destination.opaque, [&](const auto &item) { return item.type == type; });
        else if (old == destination.opaque.end())
          destination.opaque.push_back(*value);
        else
          *old = *value;
      }
    const auto properties = detail::SelectPlacementRuntimeProperties(
        local_properties->at(live_id), source_properties->at(source_id), groups);
    if (!properties)
      return fail("Selected source runtime property schema is unavailable.");
    source_properties->at(source_id) = *properties;
  }
  std::string runtime = source_runtime->substr(0, source_runtime->find('\n') + 1);
  for (const auto &entity : source_world.FindScene(source.scene_)->entities) {
    const auto line = std::to_string(entity.id) + ' ' + std::to_string(entity.parent) + ' ' +
                      source_properties->at(entity.id) + '\n';
    if (line.size() > PrefabAssets::kMaximumSceneBytes - runtime.size())
      return fail("Source runtime candidate exceeds its byte budget.");
    runtime += line;
  }
  if (!source_world.ReplaceSceneSnapshot(source.scene_, runtime))
    return fail("Source runtime candidate failed complete validation.");
  source.opaque_dirty_.reset();
  auto candidate =
      PrefabAssets::Capture(retained->id, source, [] { return foundation::Uuid{}; }, &*retained);
  if (!candidate || candidate->scene_bytes == retained->scene_bytes)
    return fail("Source application has no compatible changed existing fields.");
  if (!authorized || !workspace.Writable() || !Matches(workspace, target, review))
    return fail("Source or live observation changed before publication.");
  if (std::exchange(fail_before_publish, false))
    throw std::bad_alloc{};
  // Candidate/result storage is complete before IO; the live document is never acknowledged.
  if (!PrefabAssets::Publish(workspace, *candidate, &*retained, error))
    return {};
  return candidate;
}
} // namespace nexora::editor
