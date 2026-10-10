#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabDocumentSession.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-prefab-rebase-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "Project", "Rebase"), "Project fixture failed");
  runtime::World world;
  editor::SceneDocument original(world, world.LoadScene("Original"));
  const auto node = original.Create("Original");
  Require(original.SetOpaqueComponent(*original.Key(node), {99, "Absent", {0, 255, 27}}),
          "Opaque fixture failed");
  const auto untouched = original.PrepareSave()->Bytes();
  const foundation::Uuid base{1200, 1}, variant{1200, 2};
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{1201, ++serial}; };
  editor::PrefabDocumentSession session(workspace), source(workspace);
  Require(session.Create(base, original) && session.Save(factory) && session.Variant(variant) &&
              session.Save(factory) && source.Open(base),
          "Saved variant fixture failed");
  const auto saved = *session.SourceBaseline();
  auto *local = session.EditableDocument();
  auto *remote = source.EditableDocument();
  const auto key = *local->Key(node);
  const auto generation = local->Generation();
  Require(local->Rename(key, "Local name") &&
              local->SetTransform(node, runtime::Transform{0, 0, 0, 0, 0, 0, 1, 2, 2, 2}) &&
              remote->Rename(*remote->Key(node), "Source name") &&
              remote->SetTransform(node, {4, 0, 0}) && source.Save(factory),
          "Independent local/source edit failed");
  const std::array selection{node};
  Require(local->Select(selection) && local->CopySelection(), "Selection fixture failed");
  const auto before = *local->PrepareSave();
  auto review = session.ReviewRebase();
  Require(review && !review->CanApply() && review->Conflicts().size() == 1 &&
              review->Unresolved() == 1 &&
              review->PreviousReference() == editor::PrefabRevisionReference{base, 1} &&
              review->NextReference() == editor::PrefabRevisionReference{base, 2} &&
              !session.Rebase(*review, true) && local->MatchesPreparedSave(before),
          "Unresolved review changed data");
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(!session.ReviewRebase() && !session.Rebase(*review, true) &&
              local->MatchesPreparedSave(before),
          "Recovery allowed rebase or changed local data");
  std::filesystem::remove(recovery);
  const auto property = review->Conflicts().front();
  const std::array keep{
      editor::PrefabRebaseChoice{property, editor::PrefabRebaseDecision::KeepLocal}};
  const std::array duplicate{keep.front(), keep.front()};
  const std::array unknown{
      editor::PrefabRebaseChoice{{{1, 1}, {1, 2}}, editor::PrefabRebaseDecision::TakeSource}};
  Require(!session.ResolveRebase(*review, duplicate) && !session.ResolveRebase(*review, unknown) &&
              local->MatchesPreparedSave(before),
          "Invalid conflict choices changed data");
  auto resolved = session.ResolveRebase(*review, keep);
  Require(resolved && resolved->CanApply() && !resolved->Unresolved() &&
              !session.Rebase(*resolved, false) && local->MatchesPreparedSave(before) &&
              session.Rebase(*resolved, true) && local->Name(node) == "Local name" &&
              local->Transform(node)->x == 4 && local->Transform(node)->sx == 2 &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
              session.SourceBaseline() == saved && local->Generation() == generation &&
              local->Key(node) == key && local->Selection().size() == 1 &&
              !session.Rebase(*resolved, true),
          "Resolved rebase lost overrides/scope");
  Require(local->Undo() && local->MatchesPreparedSave(before) &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 1} &&
              local->Redo() && local->Transform(node)->x == 4 && session.Save(factory) &&
              !session.Dirty() && session.SourceBaseline()->revision == 2 &&
              session.SourceBaseline()->base == editor::PrefabRevisionReference{base, 2} &&
              editor::PrefabAssets::LoadRevision(workspace, {variant, 1}) == saved &&
              local->Undo() && session.Dirty() && local->Redo() && !session.Dirty() &&
              local->Paste() && local->Undo() && !session.Dirty(),
          "Rebase history/save/clipboard failed");
  Require(session.Close() && session.Open(variant) && !session.Dirty() &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
              original.PrepareSave()->Bytes() == untouched,
          "Rebase reopen changed original");
  local = session.EditableDocument();
  Require(local->SetTransform(node, {8, 0, 0}) && remote->SetTransform(node, {4, 3, 0}) &&
              source.Save(factory),
          "Different-lane fixture failed");
  review = session.ReviewRebase();
  Require(review && review->Conflicts().size() == 1 && review->Changes().conflicts == 0 &&
              !review->CanApply(),
          "Different lanes of one vector were silently merged");
  const std::array take{editor::PrefabRebaseChoice{review->Conflicts().front(),
                                                   editor::PrefabRebaseDecision::TakeSource}};
  resolved = session.ResolveRebase(*review, take);
  const auto second_before = *local->PrepareSave();
  Require(resolved && resolved->CanApply() && session.Rebase(*resolved, true) &&
              local->Transform(node)->x == 4 && local->Transform(node)->y == 3 &&
              local->Name(node) == "Local name" &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 3} &&
              local->Undo() && local->MatchesPreparedSave(second_before),
          "Whole-group source choice failed");
  // A new actual current source invalidates the frozen plan even if its old revision is retained.
  Require(remote->Rename(*remote->Key(node), "Newer") && source.Save(factory) &&
              !session.ResolveRebase(*review, take) && !session.Rebase(*resolved, true) &&
              local->MatchesPreparedSave(second_before),
          "Advanced actual source accepted stale review");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly), "Observer failed");
    editor::PrefabDocumentSession inspection(observer);
    Require(inspection.Open(variant), "Read-only variant reopen failed");
    auto observed = inspection.ReviewRebase();
    Require(observed.has_value(), "Read-only comparison failed");
    std::vector<editor::PrefabRebaseChoice> choices;
    for (const auto conflict : observed->Conflicts())
      choices.push_back({conflict, editor::PrefabRebaseDecision::KeepLocal});
    auto ready = inspection.ResolveRebase(*observed, choices);
    Require(ready && ready->CanApply() && !inspection.Rebase(*ready, true) && !inspection.Dirty(),
            "Read-only rebase wrote");
  }
  editor::PrefabDocumentSession clean(workspace);
  const foundation::Uuid clean_variant{1200, 3};
  Require(clean.Open(base) && clean.Variant(clean_variant) && clean.Save(factory) &&
              remote->Rename(*remote->Key(node), "One-sided source") && source.Save(factory),
          "Conflict-free fixture failed");
  const auto clean_saved = *clean.SourceBaseline();
  auto automatic = clean.ReviewRebase();
  Require(automatic && automatic->Conflicts().empty() && automatic->CanApply() &&
              clean.Rebase(*automatic, true) &&
              clean.Document()->Name(node) == "One-sided source" &&
              clean.BaseReference() == editor::PrefabRevisionReference{base, 5} &&
              clean.SourceBaseline() == clean_saved && clean.EditableDocument()->Undo() &&
              !clean.Dirty() && clean.BaseReference() == editor::PrefabRevisionReference{base, 4} &&
              clean.EditableDocument()->Redo() && clean.Save(factory) && !clean.Dirty() &&
              clean.SourceBaseline()->base == editor::PrefabRevisionReference{base, 5},
          "Source-only update failed automatic candidate/history/wrapped publication");
  // Source structure cannot be silently adopted by the same-identity planner.
  Require(remote->Create("New source node") && source.Save(factory) && !session.ReviewRebase() &&
              local->MatchesPreparedSave(second_before),
          "Structural source update changed local data");
}
void RunHierarchy() {
  runtime::World original_world, local_world, source_world;
  editor::SceneDocument original(original_world, original_world.LoadScene("Hierarchy"));
  const auto first = original.Create("First"), second = original.Create("Second");
  std::uint64_t serial = 1;
  const auto factory = [&] { return foundation::Uuid{1401, ++serial}; };
  const auto base = editor::PrefabAssets::Capture({1400, 1}, original, factory);
  Require(base.has_value(), "Hierarchy base fixture failed");
  editor::SceneDocument local(local_world, local_world.LoadScene("Local")),
      remote(source_world, source_world.LoadScene("Remote"));
  Require(local.ReloadBytes(base->scene_bytes) && remote.ReloadBytes(base->scene_bytes) &&
              local.Reparent(first, second) && remote.Reparent(second, first),
          "Independent valid hierarchy edits failed");
  const auto own = editor::PrefabAssets::Capture({1400, 2}, local, factory, &*base);
  const auto next = editor::PrefabAssets::Capture(base->id, remote, factory, &*base);
  const auto expected = *local.PrepareSave();
  Require(own && next, "Hierarchy revision capture failed");
  const auto blocked = editor::BuildPrefabRebasePlan(*base, *own, *next);
  Require(blocked && !blocked->candidate && !blocked->conflicts.empty(),
          "Parent/sibling changes bypassed complete-group conflict review");
  std::vector<editor::PrefabRebaseChoice> choices;
  for (const auto conflict : blocked->conflicts)
    choices.push_back({conflict, editor::PrefabRebaseDecision::TakeSource});
  Require(!editor::BuildPrefabRebasePlan(*base, *own, *next, choices) &&
              local.MatchesPreparedSave(expected),
          "Resolved parent choices produced an accepted merged cycle");
  auto changed_identity = *next;
  changed_identity.nodes.front().properties.front().id = {1499, 1};
  Require(editor::PrefabAssets::Validate(changed_identity) &&
              !editor::BuildPrefabRebasePlan(*base, *own, changed_identity),
          "Replaced stable field identity was adopted silently");
}
void RunClosure() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-rebase-closure-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "Project", "Closure"), "Closure project failed");
  runtime::World world;
  editor::SceneDocument original(world, world.LoadScene("Closure"));
  const auto node = original.Create("Original");
  const foundation::Uuid ancestor{1500, 1}, source_id{1500, 2}, variant{1500, 3};
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{1501, ++serial}; };
  editor::PrefabDocumentSession source(workspace), local(workspace);
  Require(source.Create(ancestor, original) && source.Save(factory), "Ancestor publication failed");
  const auto saved_ancestor = *source.SourceBaseline();
  Require(source.Variant(source_id) && source.Save(factory) && local.Open(source_id) &&
              local.Variant(variant) && local.Save(factory) &&
              source.EditableDocument()->Rename(*source.Document()->Key(node), "Updated source") &&
              source.Save(factory),
          "Two-level reference fixture failed");
  const auto expected = *local.Document()->PrepareSave();
  const auto review = local.ReviewRebase();
  Require(review && review->CanApply(), "Closure review failed");
  runtime::World tampered_world;
  editor::SceneDocument tampered(tampered_world, tampered_world.LoadScene("Tampered"));
  Require(tampered.ReloadBytes(saved_ancestor.scene_bytes) &&
              tampered.Rename(*tampered.Key(node), "Externally replaced ancestor"),
          "Tampered ancestor fixture failed");
  auto replaced = saved_ancestor;
  replaced.scene_bytes = tampered.PrepareSave()->Bytes();
  const auto path = workspace.Root() / ".nexora/prefabs" / (ancestor.ToString() + ".nxprefab");
  const auto overwrite = [&](const editor::PrefabAsset &asset) {
    const auto bytes = editor::PrefabAssets::Encode(asset);
    Require(bytes.has_value(), "Closure fixture encoding failed");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(bytes->data()),
                 static_cast<std::streamsize>(bytes->size()));
    output.close();
    Require(output.good(), "Closure fixture write failed");
  };
  overwrite(replaced);
  Require(!local.ResolveRebase(*review, {}) && !local.Rebase(*review, true) &&
              local.Document()->MatchesPreparedSave(expected) && !local.Dirty(),
          "Valid replacement of a transitive same-revision source accepted frozen review");
  overwrite(saved_ancestor);
  Require(local.Rebase(*review, true) && local.Document()->Name(node) == "Updated source" &&
              local.BaseReference() == editor::PrefabRevisionReference{source_id, 2} &&
              local.EditableDocument()->Undo() && local.Document()->MatchesPreparedSave(expected) &&
              !local.Dirty(),
          "Restored exact source closure lost owning review or atomic reference history");
}
} // namespace
int main() {
  try {
    Run();
    RunHierarchy();
    RunClosure();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
