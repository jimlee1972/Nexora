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
std::optional<std::map<runtime::Id, std::string>>
RuntimePropertyRecords(std::string_view snapshot) {
  // Consume only bounded canonical version-3 output produced by World::SaveScene here.
  // ID/parent remain target-owned; the suffix owns every stored property and presence flag.
  if (!snapshot.starts_with("NEXORA_SCENE 3 "))
    return {};
  auto cursor = snapshot.find('\n');
  if (cursor == snapshot.npos)
    return {};
  ++cursor;
  std::map<runtime::Id, std::string> records;
  while (cursor < snapshot.size()) {
    const auto end = snapshot.find('\n', cursor);
    if (end == snapshot.npos)
      return {};
    const auto line = snapshot.substr(cursor, end - cursor);
    const auto first = line.find(' ');
    const auto second = first == line.npos ? line.npos : line.find(' ', first + 1);
    if (second == line.npos || second + 1 == line.size())
      return {};
    runtime::Id id{};
    const auto parsed = std::from_chars(line.data(), line.data() + first, id);
    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + first || !id ||
        !records.emplace(id, line.substr(second + 1)).second)
      return {};
    cursor = end + 1;
  }
  return records;
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
bool PrefabPlacementOverrides::Revert(const ProjectWorkspace &workspace, SceneDocument &target,
                                      const PrefabPlacementOverrideReview &review, bool authorized,
                                      std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (!authorized || !workspace.Writable() || target.world_.Kind() != runtime::WorldKind::Editor ||
      !Matches(workspace, target, review))
    return fail("Prefab property revert requires a current review and writer authorization.");
  if (std::ranges::any_of(review.rows_, &PrefabPlacementOverrideRow::structural))
    return fail("Prefab property revert does not reconcile structural hierarchy changes.");
  if (review.rows_.empty())
    return true; // Preserve pending Redo and the saved baseline for a semantic no-op.
  const auto &inspection = review.inspection_;
  if (!inspection.graph_)
    return fail("Prefab retained source closure is unavailable.");
  runtime::World source_world, staged_world;
  SceneDocument source(source_world, source_world.LoadScene("Prefab revert source"));
  SceneDocument staged(staged_world, staged_world.LoadScene("Prefab revert target"));
  const auto empty = source.PrepareSave();
  if (!empty || !staged.ReloadBytes(inspection.expected_.Bytes(), 4096))
    return fail("Prefab property revert staging failed.");
  const auto mapping = PrefabAssets::Instantiate(inspection.Source(), inspection.graph_->assets,
                                                 source, *empty, true);
  if (!mapping || mapping->size() != inspection.placement_.nodes.size())
    return fail("Prefab retained source materialization failed.");
  std::map<runtime::Id, SceneDocument::Node *> staged_nodes;
  std::map<runtime::Id, const SceneDocument::Node *> source_nodes;
  for (auto &node : staged.nodes_)
    staged_nodes.emplace(node.id, &node);
  for (const auto &node : source.nodes_)
    source_nodes.emplace(node.id, &node);
  const auto source_runtime =
      source_world.SaveScene(source.scene_, SceneComparison::kMaximumSourceBytes);
  const auto staged_runtime =
      staged_world.SaveScene(staged.scene_, SceneComparison::kMaximumSourceBytes);
  if (!source_runtime || !staged_runtime)
    return fail("Prefab property runtime snapshots exceed their byte budget.");
  const auto source_properties = RuntimePropertyRecords(*source_runtime);
  auto staged_properties = RuntimePropertyRecords(*staged_runtime);
  if (!source_properties || !staged_properties)
    return fail("Prefab property runtime snapshot schema is unavailable.");
  for (const auto &node : *mapping) {
    const auto live = std::ranges::find_if(inspection.placement_.nodes, [&](const auto &value) {
      return value.scope == node.scope && value.source_node == node.node;
    });
    const auto authoring = source_nodes.find(node.target.id);
    if (live == inspection.placement_.nodes.end() || authoring == source_nodes.end() ||
        !staged_nodes.contains(live->target) || !source_properties->contains(node.target.id) ||
        !staged_properties->contains(live->target))
      return fail("Prefab scoped source-to-live identity is unavailable.");
    auto &destination = *staged_nodes.at(live->target);
    const auto generation = destination.generation;
    destination = *authoring->second;
    destination.id = live->target;
    destination.generation = generation;
    staged_properties->at(live->target) = source_properties->at(node.target.id);
  }
  std::string next_runtime = staged_runtime->substr(0, staged_runtime->find('\n') + 1);
  const auto *staged_scene = staged_world.FindScene(staged.scene_);
  if (!staged_scene)
    return fail("Prefab staging scene is unavailable.");
  for (const auto &entity : staged_scene->entities) {
    const auto line = std::to_string(entity.id) + ' ' + std::to_string(entity.parent) + ' ' +
                      staged_properties->at(entity.id) + '\n';
    if (line.size() > SceneComparison::kMaximumSourceBytes - next_runtime.size())
      return fail("Prefab property runtime snapshot exceeds its byte budget.");
    next_runtime += line;
  }
  if (!staged_world.ReplaceSceneSnapshot(staged.scene_, next_runtime))
    return fail("Prefab property staging rejected source component values.");
  staged.opaque_dirty_.reset();
  const auto prepared = staged.PrepareSave();
  if (!prepared || prepared->Bytes().size() > SceneComparison::kMaximumSourceBytes || !authorized ||
      !workspace.Writable() || !Matches(workspace, target, review))
    return fail("Prefab project, source closure or target changed before revert.");
  if (!target.ApplyPropertySnapshot(inspection.expected_, prepared->Bytes(), authorized))
    return fail("Prefab property revert could not publish its atomic transaction.");
  return true;
}
} // namespace nexora::editor
