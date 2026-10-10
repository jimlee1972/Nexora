#include "Nexora/Editor/PrefabPlacementOverrides.h"
#include "Nexora/Editor/SceneComparison.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace nexora::editor {
namespace {
using Fields = std::map<std::string, std::string, std::less<>>;
using Nodes = std::map<runtime::Id, Fields>;
std::optional<Nodes> Snapshot(std::string_view bytes, std::string *error) {
  const auto comparison = CompareSceneRevisions({}, bytes, {}, error);
  if (!comparison)
    return {};
  Nodes result;
  for (const auto &row : comparison->rows) {
    constexpr std::string_view prefix = "entities/";
    if (!row.stable_path.starts_with(prefix) || !row.local)
      continue;
    const auto path = std::string_view(row.stable_path).substr(prefix.size());
    const auto slash = path.find('/');
    if (slash == path.npos)
      return {};
    runtime::Id id{};
    const auto parsed = std::from_chars(path.data(), path.data() + slash, id);
    if (parsed.ec != std::errc{} || parsed.ptr != path.data() + slash || !id)
      return {};
    const auto field = path.substr(slash + 1);
    // Scene-global identity and absolute sibling positions are not instance properties.
    if (field == "present" || field == "authoring/tracked" || field == "sibling/index")
      continue;
    result[id].emplace(field, *row.local);
  }
  return result;
}
std::string Property(std::string_view field) {
  if (field.starts_with("authoring/euler/"))
    return "transform.rotation";
  for (const auto prefix : {"transform/position/", "transform/rotation/", "transform/scale/"})
    if (field.starts_with(prefix)) {
      std::string result(prefix);
      result.pop_back();
      std::ranges::replace(result, '/', '.');
      return result;
    }
  if (field.starts_with("opaque/")) {
    const auto slash = field.find('/', 7);
    return std::string(field.substr(0, slash));
  }
  return std::string(field.substr(0, field.find('/')));
}
std::optional<std::string> Value(const Fields &fields, const std::string &name) {
  const auto found = fields.find(name);
  return found == fields.end() ? std::nullopt : std::optional(found->second);
}
} // namespace

std::optional<PrefabPlacementOverrideReview>
PrefabPlacementOverrides::Prepare(const ProjectWorkspace &workspace, const SceneDocument &target,
                                  SceneDocument::NodeKey selected, std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) -> std::optional<PrefabPlacementOverrideReview> {
    if (error && error->empty())
      *error = message;
    return {};
  };
  auto inspection = PrefabPlacementInspector::Inspect(workspace, target, selected);
  if (!inspection || !inspection->graph_)
    return fail("Prefab instance retained source is unavailable or incompatible.");
  runtime::World world;
  SceneDocument baseline(world, world.LoadScene("Prefab override baseline"));
  const auto empty = baseline.PrepareSave();
  if (!empty)
    return fail("Prefab baseline preparation failed.");
  const auto mapping = PrefabAssets::Instantiate(inspection->Source(), inspection->graph_->assets,
                                                 baseline, *empty, true);
  const auto prepared = baseline.PrepareSave();
  if (!mapping || !prepared || mapping->size() != inspection->placement_.nodes.size())
    return fail("Prefab retained source materialization failed.");
  auto retained = Snapshot(prepared->Bytes(), error);
  const auto local = Snapshot(inspection->expected_.Bytes(), error);
  if (!retained || !local)
    return fail("Prefab property review exceeds semantic snapshot budgets.");
  std::map<runtime::Id, runtime::Id> translated;
  for (const auto &node : *mapping) {
    const auto live = std::ranges::find_if(inspection->placement_.nodes, [&](const auto &value) {
      return value.scope == node.scope && value.source_node == node.node;
    });
    if (live == inspection->placement_.nodes.end() || !target.Key(live->target) ||
        !translated.emplace(node.target.id, live->target).second)
      return fail("Prefab scoped source-to-target mapping changed.");
  }
  std::vector<PrefabPlacementOverrideRow> rows;
  std::size_t bytes{};
  for (const auto &node : *mapping) {
    const auto live_id = translated.at(node.target.id);
    const auto source_fields = retained->find(node.target.id);
    const auto live_fields = local->find(live_id);
    if (source_fields == retained->end() || live_fields == local->end())
      return fail("Prefab mapped node is absent from the semantic snapshot.");
    auto &fields = source_fields->second;
    auto parent = fields.find("parent");
    if (parent == fields.end())
      return fail("Prefab baseline has no hierarchy observation.");
    runtime::Id parent_id{};
    const auto parsed = std::from_chars(parent->second.data(),
                                        parent->second.data() + parent->second.size(), parent_id);
    if (parsed.ec != std::errc{} || parsed.ptr != parent->second.data() + parent->second.size() ||
        (parent_id && !translated.contains(parent_id)))
      return fail("Prefab baseline parent is outside its materialized closure.");
    parent->second = std::to_string(parent_id ? translated.at(parent_id) : 0);
    const auto instance = std::ranges::find(inspection->graph_->instances, node.scope,
                                            &ResolvedPrefabInstance::scope);
    if (instance == inspection->graph_->instances.end())
      return fail("Prefab source scope is unavailable.");
    const auto asset = std::ranges::find_if(inspection->graph_->assets, [&](const auto &value) {
      return value.id == instance->source.asset && value.revision == instance->source.revision;
    });
    if (asset == inspection->graph_->assets.end())
      return fail("Prefab source revision is unavailable.");
    const auto identity = std::ranges::find(asset->nodes, node.node, &PrefabNodeIdentity::id);
    if (identity == asset->nodes.end())
      return fail("Prefab stable source node identity is unavailable.");
    std::set<std::string, std::less<>> names;
    for (const auto &[name, value] : fields) {
      static_cast<void>(value);
      names.insert(name);
    }
    for (const auto &[name, value] : live_fields->second) {
      static_cast<void>(value);
      names.insert(name);
    }
    for (const auto &field : names) {
      const auto old = Value(fields, field), current = Value(live_fields->second, field);
      if (old == current)
        continue;
      const auto property =
          std::ranges::find(identity->properties, Property(field), &PrefabPropertyIdentity::field);
      const auto size = field.size() + node.scope.size() * sizeof(foundation::Uuid) +
                        (old ? old->size() : 0) + (current ? current->size() : 0);
      if (rows.size() >= kMaximumRows || size > kMaximumReportBytes - bytes)
        return fail("Prefab override report exceeds its row or logical byte budget.");
      bytes += size;
      rows.push_back(
          {node.scope, instance->source, node.node,
           property == identity->properties.end() ? std::nullopt : std::optional(property->id),
           field, *target.Key(live_id), old, current, field == "parent"});
    }
  }
  if (!PrefabPlacementInspector::Matches(workspace, target, *inspection))
    return fail("Prefab project, source closure or target changed during review.");
  return PrefabPlacementOverrideReview{std::move(*inspection), std::move(rows)};
}
bool PrefabPlacementOverrides::Matches(const ProjectWorkspace &workspace,
                                       const SceneDocument &target,
                                       const PrefabPlacementOverrideReview &review) {
  return PrefabPlacementInspector::Matches(workspace, target, review.inspection_);
}
} // namespace nexora::editor
