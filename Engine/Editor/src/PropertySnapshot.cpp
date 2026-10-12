#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <tuple>

namespace nexora::editor {
namespace {
// Inspect only bounded canonical PrepareSave output, never unchecked caller text.
std::optional<std::map<runtime::Id, std::string>>
CanonicalMetadata(std::string_view bytes, const std::set<runtime::Id> &excluded,
                  bool euler_only = false) {
  std::map<runtime::Id, std::string> records;
  while (!bytes.empty()) {
    const auto end = bytes.find('\n');
    if (end == bytes.npos)
      return {};
    const auto line = bytes.substr(0, end);
    bytes.remove_prefix(end + 1);
    if (line == "world")
      return records;
    if (!line.starts_with("node ") && !line.starts_with("euler ") && !line.starts_with("opaque "))
      continue;
    if (euler_only && !line.starts_with("euler "))
      continue;
    const auto first = line.find(' ') + 1;
    const auto last = line.find(' ', first);
    if (last == line.npos)
      return {};
    runtime::Id id{};
    const auto parsed = std::from_chars(line.data() + first, line.data() + last, id);
    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + last || !id)
      return {};
    if (!excluded.contains(id))
      records[id].append(line).push_back('\n');
  }
  return {};
}
} // namespace
bool SceneDocument::ApplyPropertySnapshot(const PreparedSave &expected, std::string_view source,
                                          bool authorized) {
  return ApplyOwnedPropertySnapshot(expected, source, std::nullopt, authorized);
}
bool SceneDocument::ApplyPrefabPlacementSnapshot(const PreparedSave &expected,
                                                 std::string_view source, foundation::Uuid instance,
                                                 bool authorized) {
  if (instance.IsNil())
    return false;
  return ApplyOwnedPropertySnapshot(expected, source, instance, authorized);
}
bool SceneDocument::ApplyOwnedPropertySnapshot(const PreparedSave &expected,
                                               std::string_view source,
                                               std::optional<foundation::Uuid> instance,
                                               bool authorized) {
  constexpr std::size_t maximum_nodes = 4096, maximum_bytes = 8 * 1024 * 1024;
  if (!authorized || world_.Kind() != runtime::WorldKind::Editor || source.empty() ||
      source.size() > maximum_bytes || expected.Bytes().size() > maximum_bytes ||
      nodes_.size() > maximum_nodes || !MatchesPreparedSave(expected))
    return false;
  const auto *current = world_.FindScene(scene_);
  if (!current || current->entities.size() != nodes_.size())
    return false;
  runtime::World staged_world;
  const auto staged_scene = staged_world.LoadScene("Property snapshot");
  SceneDocument staged(staged_world, staged_scene);
  if (!staged.ReloadBytes(source, maximum_nodes))
    return false;
  const auto *parsed = staged_world.FindScene(staged_scene);
  if (!parsed || parsed->name != current->name || parsed->persistent != current->persistent ||
      parsed->entities.size() != current->entities.size() || staged.nodes_.size() != nodes_.size())
    return false;
  std::set<runtime::Id> owned;
  bool revision_changed = false;
  if (instance) {
    const auto old = PrefabPlacements(), next = staged.PrefabPlacements();
    if (old.size() != next.size())
      return false;
    bool found = false;
    for (std::size_t i = 0; i < old.size(); ++i) {
      if (old[i].instance != *instance) {
        if (old[i] != next[i])
          return false;
        continue;
      }
      if (old[i].instance != next[i].instance || old[i].source != next[i].source ||
          old[i].nodes != next[i].nodes || next[i].revision < old[i].revision)
        return false;
      found = true;
      revision_changed = next[i].revision != old[i].revision;
      for (const auto &node : old[i].nodes)
        owned.insert(node.target);
    }
    if (!found)
      return false;
    const auto stored = [](const runtime::Entity &entity) {
      return std::tie(entity.transform, entity.camera, entity.light, entity.mesh_renderer,
                      entity.camera_data.vertical_field_of_view, entity.camera_data.near_plane,
                      entity.camera_data.far_plane, entity.light_data.intensity,
                      entity.mesh_data.mesh, entity.mesh_data.material.shader);
    };
    for (std::size_t i = 0; i < current->entities.size(); ++i) {
      const auto &before = current->entities[i], &after = parsed->entities[i];
      if (before.id != after.id || before.parent != after.parent ||
          (!owned.contains(before.id) && stored(before) != stored(after)))
        return false;
    }
  } else if (!std::ranges::equal(staged.PrefabPlacements(), PrefabPlacements()))
    return false;
  const auto prepared = staged.PrepareSave();
  const auto previous_runtime = world_.SaveScene(scene_, maximum_bytes);
  const auto next_runtime = staged_world.SaveScene(staged_scene, maximum_bytes);
  if (!prepared || prepared->Bytes().size() > maximum_bytes || !previous_runtime || !next_runtime)
    return false;
  if (instance) {
    const auto before_metadata = CanonicalMetadata(expected.Bytes(), owned);
    const auto after_metadata = CanonicalMetadata(prepared->Bytes(), owned);
    if (!before_metadata || !after_metadata || *before_metadata != *after_metadata)
      return false;
  }
  const auto before_eulers = CanonicalMetadata(expected.Bytes(), {}, true);
  const auto after_eulers = CanonicalMetadata(prepared->Bytes(), {}, true);
  if (!before_eulers || !after_eulers)
    return false;
  std::map<runtime::Id, const runtime::Entity *> previous_entities, next_entities;
  for (const auto &entity : current->entities)
    previous_entities.emplace(entity.id, &entity);
  for (const auto &entity : parsed->entities)
    next_entities.emplace(entity.id, &entity);
  auto next_nodes = nodes_;
  std::map<runtime::Id, Node *> lookup;
  for (auto &node : next_nodes)
    if (!lookup.emplace(node.id, &node).second || !Key(node.id))
      return false;
  for (const auto &node : staged.nodes_) {
    const auto found = lookup.find(node.id);
    if (found == lookup.end() || node.name.size() > 1024 ||
        node.name.find('\0') != std::string::npos || !foundation::IsValidUtf8(node.name))
      return false;
    if (instance && !owned.contains(node.id)) {
      // Preserve even latent hints whose old quaternion no longer matches current Runtime.
      // Canonical serialized metadata was already validated without normalizing this owner.
      lookup.erase(found);
      continue;
    }
    if (!previous_entities.contains(node.id) || !next_entities.contains(node.id))
      return false;
    const auto &before_pose = previous_entities.at(node.id)->transform;
    const auto &after_pose = next_entities.at(node.id)->transform;
    const auto old_euler = before_eulers->find(node.id), new_euler = after_eulers->find(node.id);
    const bool same_metadata =
        (old_euler == before_eulers->end() && new_euler == after_eulers->end()) ||
        (old_euler != before_eulers->end() && new_euler != after_eulers->end() &&
         old_euler->second == new_euler->second);
    const bool same_rotation =
        std::tie(before_pose.qx, before_pose.qy, before_pose.qz, before_pose.qw) ==
        std::tie(after_pose.qx, after_pose.qy, after_pose.qz, after_pose.qw);
    const auto previous_hint = found->second->euler_hint;
    const auto generation = found->second->generation;
    *found->second = node;
    found->second->generation = generation;
    // Canonical save omits hints for another quaternion. Preserve that dormant authored
    // state when neither the rotation nor its serialized hint was explicitly changed.
    if (same_metadata && same_rotation)
      found->second->euler_hint = previous_hint;
    lookup.erase(found);
  }
  if (!lookup.empty())
    return false;
  // Canonical equivalent input is a no-op and must preserve pending Redo.
  if (prepared->signature_ == expected.signature_ &&
      prepared->opaque_records_ == expected.opaque_records_)
    return true;
  // A retained revision must advance for a nontrivial rebase; equal revision is only a no-op.
  if (instance && !revision_changed)
    return false;
  UndoEntry entry;
  entry.previous_placements = prefab_placements_;
  entry.redo_placements = instance ? staged.prefab_placements_ : prefab_placements_;

  entry.kind = UndoEntry::Kind::PropertySnapshot;
  entry.property_snapshot = std::make_shared<UndoEntry::PropertySnapshot>(
      UndoEntry::PropertySnapshot{expected,
                                  PreparedSave(document_generation_, prepared->bytes_,
                                               prepared->signature_, prepared->opaque_records_),
                                  *previous_runtime, *next_runtime, nodes_, next_nodes});
  // All document/history storage exists before Runtime's atomic snapshot replacement.
  undo_.reserve(undo_.size() + 1);
  if (!MatchesPreparedSave(expected) || !world_.ReplaceSceneSnapshot(scene_, *next_runtime))
    return false;
  nodes_.swap(next_nodes);
  const auto next_placements = entry.redo_placements;
  // PushUndo captures the current binding as the before-state; advance only afterward.
  PushUndo(std::move(entry));
  prefab_placements_ = next_placements;
  return true;
}
bool SceneDocument::ReplayPropertySnapshot(bool forward) {
  auto &source = forward ? redo_ : undo_;
  auto &destination = forward ? undo_ : redo_;
  if (source.empty() || !source.back().property_snapshot)
    return false;
  const auto &snapshot = *source.back().property_snapshot;
  if (!MatchesPreparedSave(forward ? snapshot.before : snapshot.after))
    return false;
  auto staged_nodes = forward ? snapshot.next_nodes : snapshot.previous_nodes;
  destination.reserve(destination.size() + 1);
  if (!world_.ReplaceSceneSnapshot(scene_,
                                   forward ? snapshot.next_runtime : snapshot.previous_runtime))
    return false;
  nodes_.swap(staged_nodes);
  prefab_placements_ = forward ? source.back().redo_placements : source.back().previous_placements;
  destination.push_back(std::move(source.back()));
  source.pop_back();
  opaque_dirty_.reset();
  return true;
}
} // namespace nexora::editor
