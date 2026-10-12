#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <map>

namespace nexora::editor {
bool SceneDocument::ApplyPropertySnapshot(const PreparedSave &expected, std::string_view source,
                                          bool authorized) {
  return ApplyPropertySnapshotWithReference(expected, source, authorized, prefab_base_);
}
bool SceneDocument::ApplyPrefabPropertySnapshot(const PreparedSave &expected,
                                                std::string_view source,
                                                PrefabBaseReference reference, bool authorized) {
  if (reference.asset.IsNil() || !reference.revision)
    return false;
  return ApplyPropertySnapshotWithReference(expected, source, authorized, reference);
}
bool SceneDocument::ApplyPropertySnapshotWithReference(
    const PreparedSave &expected, std::string_view source, bool authorized,
    std::optional<PrefabBaseReference> reference) {
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
      parsed->entities.size() != current->entities.size() ||
      staged.nodes_.size() != nodes_.size() ||
      !std::ranges::equal(staged.PrefabPlacements(), PrefabPlacements()))
    return false;
  staged.prefab_base_ = reference;
  const auto prepared = staged.PrepareSave();
  const auto previous_runtime = world_.SaveScene(scene_, maximum_bytes);
  const auto next_runtime = staged_world.SaveScene(staged_scene, maximum_bytes);
  if (!prepared || prepared->Bytes().size() > maximum_bytes || !previous_runtime || !next_runtime)
    return false;
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
    const auto generation = found->second->generation;
    *found->second = node;
    found->second->generation = generation;
    lookup.erase(found);
  }
  if (!lookup.empty())
    return false;
  // Canonical equivalent input is a no-op and must preserve pending Redo.
  if (prepared->signature_ == expected.signature_ &&
      prepared->opaque_records_ == expected.opaque_records_)
    return true;
  UndoEntry entry;
  entry.kind = UndoEntry::Kind::PropertySnapshot;
  entry.property_snapshot =
      std::make_shared<UndoEntry::PropertySnapshot>(UndoEntry::PropertySnapshot{
          expected,
          PreparedSave(document_generation_, prepared->bytes_, prepared->signature_,
                       prepared->opaque_records_),
          *previous_runtime, *next_runtime, nodes_, next_nodes, prefab_base_, reference});
  // All document/history storage exists before Runtime's atomic snapshot replacement.
  undo_.reserve(undo_.size() + 1);
  if (!MatchesPreparedSave(expected) || !world_.ReplaceSceneSnapshot(scene_, *next_runtime))
    return false;
  nodes_.swap(next_nodes);
  prefab_base_ = reference;
  PushUndo(std::move(entry));
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
  prefab_base_ = forward ? snapshot.next_base : snapshot.previous_base;
  destination.push_back(std::move(source.back()));
  source.pop_back();
  opaque_dirty_.reset();
  return true;
}
} // namespace nexora::editor
