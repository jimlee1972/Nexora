#include "Nexora/Editor/PrefabPlacementRebase.h"
#include "PrefabPlacementPropertyInternal.h"
#include <array>
#include <charconv>
#include <map>
#include <set>

namespace nexora::editor {
namespace {
using Fields = detail::PlacementFields;
using Nodes = detail::PlacementNodes;
using Path = std::vector<std::uint64_t>;
struct Group final {
  runtime::Id target{};
  std::string property;
  bool take_source{};
};
Path Scoped(std::span<const foundation::Uuid> scope, foundation::Uuid node) {
  Path result;
  for (const auto id : scope) {
    result.push_back(id.high);
    result.push_back(id.low);
  }
  result.push_back(node.high);
  result.push_back(node.low);
  return result;
}
std::optional<Nodes> Values(std::string_view bytes, std::string *error) {
  auto result = detail::PlacementSnapshot(bytes, error);
  if (!result)
    return {};
  for (auto &[id, fields] : *result) {
    static_cast<void>(id);
    fields["authoring/euler/present"] = "false";
  }
  // Canonical source records distinguish absent hints from explicitly authored zero angles.
  while (!bytes.empty()) {
    const auto end = bytes.find('\n');
    if (end == bytes.npos)
      return {};
    const auto line = bytes.substr(0, end);
    bytes.remove_prefix(end + 1);
    if (line == "world")
      return result;
    if (!line.starts_with("euler "))
      continue;
    const auto last = line.find(' ', 6);
    runtime::Id id{};
    if (last == line.npos)
      return {};
    const auto parsed = std::from_chars(line.data() + 6, line.data() + last, id);
    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + last || !result->contains(id))
      return {};
    result->at(id)["authoring/euler/present"] = "true";
  }
  return {};
}
bool Schema(const PrefabAsset &before, const PrefabAsset &after) {
  if (before.id != after.id || before.base != after.base || before.nested != after.nested)
    return false;
  const auto identity = [](const PrefabAsset &asset) {
    std::map<std::array<std::uint64_t, 2>, std::map<std::string, std::array<std::uint64_t, 2>>>
        nodes;
    for (const auto &node : asset.nodes)
      for (const auto &property : node.properties)
        nodes[{node.id.high, node.id.low}].emplace(property.field,
                                                   std::array{property.id.high, property.id.low});
    return nodes;
  };
  return identity(before) == identity(after);
}
std::optional<std::map<Path, runtime::Id>>
Translate(std::span<const InstantiatedPrefabNode> mapping,
          const SceneDocument::PrefabPlacement &placement, Nodes &values) {
  if (mapping.size() != placement.nodes.size())
    return {};
  std::map<Path, runtime::Id> paths;
  std::map<runtime::Id, runtime::Id> ids;
  for (const auto &node : mapping) {
    const auto live = std::ranges::find_if(placement.nodes, [&](const auto &value) {
      return value.scope == node.scope && value.source_node == node.node;
    });
    if (live == placement.nodes.end() || !values.contains(node.target.id) ||
        !paths.emplace(Scoped(node.scope, node.node), node.target.id).second ||
        !ids.emplace(node.target.id, live->target).second)
      return {};
  }
  for (const auto &[id, live] : ids) {
    static_cast<void>(live);
    auto &fields = values.at(id);
    const auto found = fields.find("parent");
    runtime::Id parent{};
    if (found == fields.end())
      return {};
    const auto &text = found->second;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), parent);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        (parent && !ids.contains(parent)))
      return {};
    found->second = std::to_string(parent ? ids.at(parent) : 0);
  }
  return paths;
}
} // namespace
struct PrefabPlacementRebaseReview::State final {
  PrefabPlacementInspection inspection;
  ResolvedPrefabGraph published;
  PrefabRevisionReference next_source;
  std::vector<PrefabPlacementRebaseRow> rows;
  std::vector<Group> groups;
};
std::span<const PrefabPlacementRebaseRow> PrefabPlacementRebaseReview::Rows() const noexcept {
  return state_ ? std::span<const PrefabPlacementRebaseRow>(state_->rows)
                : std::span<const PrefabPlacementRebaseRow>{};
}
PrefabRevisionReference PrefabPlacementRebaseReview::Source() const noexcept {
  return state_ ? state_->inspection.Source() : PrefabRevisionReference{};
}
PrefabRevisionReference PrefabPlacementRebaseReview::PublishedSource() const noexcept {
  return state_ ? state_->next_source : PrefabRevisionReference{};
}
foundation::Uuid PrefabPlacementRebaseReview::Instance() const noexcept {
  return state_ ? state_->inspection.Instance() : foundation::Uuid{};
}
std::optional<PrefabPlacementRebaseReview>
PrefabPlacementRebase::Prepare(const ProjectWorkspace &workspace, const SceneDocument &target,
                               SceneDocument::NodeKey selected, std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) -> std::optional<PrefabPlacementRebaseReview> {
    if (error)
      *error = message;
    return {};
  };
  auto inspection = PrefabPlacementInspector::Inspect(workspace, target, selected);
  if (!inspection || !inspection->graph_ || !inspection->current_ ||
      inspection->current_->revision <= inspection->Source().revision)
    return fail("Prefab rebase requires an available newer same-source publication.");
  const auto next_source =
      PrefabRevisionReference{inspection->current_->id, inspection->current_->revision};
  auto published = PrefabAssets::ResolveProject(workspace, next_source);
  const auto original = std::ranges::find_if(inspection->graph_->assets, [&](const auto &asset) {
    return asset.id == inspection->Source().asset &&
           asset.revision == inspection->Source().revision;
  });
  if (!published || original == inspection->graph_->assets.end() ||
      !Schema(*original, *inspection->current_) ||
      published->assets.size() != inspection->graph_->assets.size())
    return fail("Prefab rebase cannot reconcile changed stable schema or dependency references.");
  for (const auto &asset : inspection->graph_->assets)
    if (&asset != &*original &&
        std::ranges::find(published->assets, asset) == published->assets.end())
      return fail("Prefab rebase cannot change the retained nested/base closure.");
  runtime::World old_world, new_world;
  SceneDocument old_scene(old_world, old_world.LoadScene("Retained rebase source"));
  SceneDocument new_scene(new_world, new_world.LoadScene("Published rebase source"));
  const auto old_empty = old_scene.PrepareSave(), new_empty = new_scene.PrepareSave();
  if (!old_empty || !new_empty)
    return fail("Prefab rebase source staging failed.");
  const auto old_mapping = PrefabAssets::Instantiate(
      inspection->Source(), inspection->graph_->assets, old_scene, *old_empty, true);
  const auto new_mapping =
      PrefabAssets::Instantiate(next_source, published->assets, new_scene, *new_empty, true);
  const auto old_bytes = old_scene.PrepareSave(), new_bytes = new_scene.PrepareSave();
  if (!old_mapping || !new_mapping || !old_bytes || !new_bytes)
    return fail("Prefab rebase source materialization failed.");
  auto retained = Values(old_bytes->Bytes(), error), current = Values(new_bytes->Bytes(), error);
  const auto local = Values(inspection->expected_.Bytes(), error);
  if (!retained || !current || !local)
    return fail("Prefab rebase exceeds semantic snapshot budgets.");
  const auto old_paths = Translate(*old_mapping, inspection->placement_, *retained);
  const auto new_paths = Translate(*new_mapping, inspection->placement_, *current);
  if (!old_paths || !new_paths || old_paths->size() != new_paths->size())
    return fail("Prefab rebase scoped node mapping changed.");
  std::vector<PrefabPlacementRebaseRow> rows;
  std::vector<Group> groups;
  std::size_t bytes{};
  for (const auto &node : inspection->placement_.nodes) {
    const auto path = Scoped(node.scope, node.source_node);
    if (!old_paths->contains(path) || !new_paths->contains(path) || !local->contains(node.target))
      return fail("Prefab rebase mapped node is absent.");
    const auto &before = retained->at(old_paths->at(path)),
               &after = current->at(new_paths->at(path));
    const auto &live = local->at(node.target);
    if (detail::PlacementValue(before, "parent") != detail::PlacementValue(after, "parent") ||
        detail::PlacementValue(before, "parent") != detail::PlacementValue(live, "parent"))
      return fail("Prefab rebase cannot reconcile source or live hierarchy changes.");
    const auto old_instance = std::ranges::find(inspection->graph_->instances, node.scope,
                                                &ResolvedPrefabInstance::scope);
    const auto new_instance =
        std::ranges::find(published->instances, node.scope, &ResolvedPrefabInstance::scope);
    if (old_instance == inspection->graph_->instances.end() ||
        new_instance == published->instances.end())
      return fail("Prefab rebase source scope is absent.");
    const auto asset = std::ranges::find_if(inspection->graph_->assets, [&](const auto &value) {
      return value.id == old_instance->source.asset &&
             value.revision == old_instance->source.revision;
    });
    if (asset == inspection->graph_->assets.end())
      return fail("Prefab rebase retained identity is absent.");
    const auto identity =
        std::ranges::find(asset->nodes, node.source_node, &PrefabNodeIdentity::id);
    if (identity == asset->nodes.end())
      return fail("Prefab rebase stable node identity is absent.");
    std::map<std::string, std::set<std::string>> fields;
    for (const auto *values : {&before, &live, &after})
      for (const auto &[field, value] : *values) {
        static_cast<void>(value);
        if (field != "parent")
          fields[detail::PlacementPropertyGroup(field)].insert(field);
      }
    for (const auto &[group, names] : fields) {
      bool local_changed = false, source_changed = false, equal = true;
      std::vector<PrefabPlacementRebaseValue> values;
      std::size_t size = group.size() + node.scope.size() * sizeof(foundation::Uuid) + 128;
      for (const auto &field : names) {
        const auto old = detail::PlacementValue(before, field),
                   own = detail::PlacementValue(live, field),
                   remote = detail::PlacementValue(after, field);
        local_changed |= own != old;
        source_changed |= remote != old;
        equal &= own == remote;
        size += field.size() + (old ? old->size() : 0) + (own ? own->size() : 0) +
                (remote ? remote->size() : 0);
        values.push_back({field, old, own, remote});
      }
      if (!local_changed && !source_changed)
        continue;
      if (rows.size() >= kMaximumRows || size > kMaximumReportBytes - bytes)
        return fail("Prefab rebase report exceeds its logical row/byte budget.");
      bytes += size;
      const auto property =
          std::ranges::find(identity->properties, group, &PrefabPropertyIdentity::field);
      if (source_changed && property == identity->properties.end())
        return fail("Prefab changed source property lacks a retained stable field identity.");
      const auto key = target.Key(node.target);
      if (!key)
        return fail("Prefab rebase target generation changed.");
      rows.push_back(
          {node.scope, old_instance->source, new_instance->source, node.source_node,
           property == identity->properties.end() ? std::nullopt : std::optional(property->id),
           *key, group, std::move(values), local_changed && source_changed && !equal});
      groups.push_back({node.target, group, source_changed && !local_changed});
    }
  }
  if (!PrefabPlacementInspector::Matches(workspace, target, *inspection))
    return fail("Prefab source or live document changed during rebase review.");
  auto state = std::make_shared<const PrefabPlacementRebaseReview::State>(
      PrefabPlacementRebaseReview::State{std::move(*inspection), std::move(*published), next_source,
                                         std::move(rows), std::move(groups)});
  PrefabPlacementRebaseReview review{std::move(state), std::nullopt, 0};
  return Resolve(review, {}, error);
}
std::optional<PrefabPlacementRebaseReview>
PrefabPlacementRebase::Resolve(const PrefabPlacementRebaseReview &review,
                               std::span<const PrefabPlacementRebaseChoice> choices,
                               std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) -> std::optional<PrefabPlacementRebaseReview> {
    if (error)
      *error = message;
    return {};
  };
  if (!review.state_ || choices.size() > kMaximumRows)
    return fail("Prefab rebase review or conflict choices are invalid.");
  const auto &state = *review.state_;
  std::map<std::size_t, PrefabPlacementRebaseDecision> decisions;
  for (const auto &choice : choices)
    if (choice.row >= state.rows.size() || !state.rows[choice.row].conflict ||
        (choice.decision != PrefabPlacementRebaseDecision::KeepLocal &&
         choice.decision != PrefabPlacementRebaseDecision::TakeSource) ||
        !decisions.emplace(choice.row, choice.decision).second)
      return fail("Choose each actual conflicting property group at most once.");
  std::size_t unresolved{};
  std::map<runtime::Id, std::set<std::string>> selected;
  for (std::size_t i = 0; i < state.rows.size(); ++i) {
    bool take = state.groups[i].take_source;
    if (state.rows[i].conflict) {
      const auto choice = decisions.find(i);
      if (choice == decisions.end())
        ++unresolved;
      else
        take = choice->second == PrefabPlacementRebaseDecision::TakeSource;
    }
    if (take)
      selected[state.groups[i].target].insert(state.groups[i].property);
  }
  if (unresolved)
    return PrefabPlacementRebaseReview{review.state_, std::nullopt, unresolved};
  const auto &inspection = state.inspection;
  runtime::World source_world, staged_world;
  SceneDocument source(source_world, source_world.LoadScene("Resolved rebase source"));
  SceneDocument staged(staged_world, staged_world.LoadScene("Staged placement rebase"));
  const auto empty = source.PrepareSave();
  if (!empty || !staged.ReloadBytes(inspection.expected_.Bytes(), 4096))
    return fail("Prefab rebase candidate staging failed.");
  const auto mapping =
      PrefabAssets::Instantiate(state.next_source, state.published.assets, source, *empty, true);
  if (!mapping || mapping->size() != inspection.placement_.nodes.size())
    return fail("Prefab rebase published materialization changed scoped structure.");
  std::map<runtime::Id, SceneDocument::Node *> target_nodes;
  std::map<runtime::Id, const SceneDocument::Node *> source_nodes;
  for (auto &node : staged.nodes_)
    target_nodes.emplace(node.id, &node);
  for (const auto &node : source.nodes_)
    source_nodes.emplace(node.id, &node);
  const auto source_runtime =
      source_world.SaveScene(source.scene_, SceneComparison::kMaximumSourceBytes);
  const auto staged_runtime =
      staged_world.SaveScene(staged.scene_, SceneComparison::kMaximumSourceBytes);
  if (!source_runtime || !staged_runtime)
    return fail("Prefab rebase runtime staging exceeds its budget.");
  const auto source_properties = detail::PlacementRuntimeProperties(*source_runtime);
  auto target_properties = detail::PlacementRuntimeProperties(*staged_runtime);
  if (!source_properties || !target_properties)
    return fail("Prefab rebase canonical runtime property schema is unavailable.");
  for (const auto &node : *mapping) {
    const auto live = std::ranges::find_if(inspection.placement_.nodes, [&](const auto &value) {
      return value.scope == node.scope && value.source_node == node.node;
    });
    if (live == inspection.placement_.nodes.end() || !source_nodes.contains(node.target.id) ||
        !target_nodes.contains(live->target) || !source_properties->contains(node.target.id) ||
        !target_properties->contains(live->target))
      return fail("Prefab rebase candidate scoped identity is absent.");
    const auto chosen = selected.find(live->target);
    if (chosen == selected.end())
      continue;
    auto &destination = *target_nodes.at(live->target);
    const auto &original = *source_nodes.at(node.target.id);
    const auto &groups = chosen->second;
    if (groups.contains("name"))
      destination.name = original.name;
    if (groups.contains("transform.rotation"))
      destination.euler_hint = original.euler_hint;
    for (const auto &group : groups)
      if (group.starts_with("opaque/")) {
        const auto text = std::string_view(group).substr(7);
        runtime::TypeId type{};
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), type);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !type)
          return fail("Prefab rebase opaque property identity is invalid.");
        const auto old = std::ranges::find(original.opaque, type, &OpaqueComponent::type);
        const auto own = std::ranges::find(destination.opaque, type, &OpaqueComponent::type);
        if (old == original.opaque.end())
          std::erase_if(destination.opaque, [&](const auto &value) { return value.type == type; });
        else if (own == destination.opaque.end())
          destination.opaque.push_back(*old);
        else
          *own = *old;
      }
    const auto properties = detail::SelectPlacementRuntimeProperties(
        source_properties->at(node.target.id), target_properties->at(live->target), groups);
    if (!properties)
      return fail("Prefab rebase complete property schema is unavailable.");
    target_properties->at(live->target) = *properties;
    selected.erase(chosen);
  }
  if (!selected.empty())
    return fail("Prefab rebase selected group has no scoped target.");
  const auto *scene = staged_world.FindScene(staged.scene_);
  if (!scene)
    return fail("Prefab rebase staging scene is absent.");
  std::string runtime = staged_runtime->substr(0, staged_runtime->find('\n') + 1);
  for (const auto &entity : scene->entities) {
    const auto line = std::to_string(entity.id) + ' ' + std::to_string(entity.parent) + ' ' +
                      target_properties->at(entity.id) + '\n';
    if (line.size() > SceneComparison::kMaximumSourceBytes - runtime.size())
      return fail("Prefab rebase runtime candidate exceeds its byte budget.");
    runtime += line;
  }
  if (!staged_world.ReplaceSceneSnapshot(staged.scene_, runtime))
    return fail("Prefab rebase published property values are invalid.");
  auto placements = std::make_shared<std::vector<SceneDocument::PrefabPlacement>>(
      staged.PrefabPlacements().begin(), staged.PrefabPlacements().end());
  const auto placement = std::ranges::find(*placements, inspection.Instance(),
                                           &SceneDocument::PrefabPlacement::instance);
  if (placement == placements->end())
    return fail("Prefab rebase binding is absent from its candidate.");
  placement->revision = state.next_source.revision;
  staged.prefab_placements_ = std::move(placements);
  staged.opaque_dirty_.reset();
  const auto candidate = staged.PrepareSave();
  if (!candidate || candidate->Bytes().size() > SceneComparison::kMaximumSourceBytes)
    return fail("Prefab rebase complete candidate exceeds its budget.");
  return PrefabPlacementRebaseReview{review.state_, candidate->Bytes(), 0};
}
bool PrefabPlacementRebase::Matches(const ProjectWorkspace &workspace, const SceneDocument &target,
                                    const PrefabPlacementRebaseReview &review) {
  if (!review.state_ ||
      !PrefabPlacementInspector::Matches(workspace, target, review.state_->inspection))
    return false;
  const auto graph = PrefabAssets::ResolveProject(workspace, review.state_->next_source);
  return graph && graph->assets == review.state_->published.assets &&
         PrefabPlacementInspector::Matches(workspace, target, review.state_->inspection);
}
bool PrefabPlacementRebase::Apply(const ProjectWorkspace &workspace, SceneDocument &target,
                                  const PrefabPlacementRebaseReview &review, bool authorized,
                                  std::string *error) {
  if (error)
    error->clear();
  if (!authorized || !workspace.Writable() || !review.Ready() || review.unresolved_ ||
      !Matches(workspace, target, review) || !workspace.Writable() ||
      !target.ApplyPrefabPlacementSnapshot(review.state_->inspection.expected_, *review.candidate_,
                                           review.Instance(), true)) {
    if (error)
      *error = "Prefab rebase requires resolved current sources, target and authoring authority.";
    return false;
  }
  return true;
}
} // namespace nexora::editor
