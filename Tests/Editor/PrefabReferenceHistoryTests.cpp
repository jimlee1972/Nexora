#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabDocumentSession.h"
#include "Nexora/Editor/SceneSaveBatch.h"
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
std::string Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-prefab-reference-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "Project", "Reference history"), "Project fixture failed");
  runtime::World world;
  editor::SceneDocument original(world, world.LoadScene("Original"));
  const auto node = original.Create("Original");
  Require(original.SetOpaqueComponent(*original.Key(node), {99, "Absent.Provider", {0, 255, 27}}),
          "Opaque fixture failed");
  const auto original_bytes = original.PrepareSave()->Bytes();
  const foundation::Uuid base{1001, 1}, variant{1001, 2};
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{1002, ++serial}; };
  editor::PrefabDocumentSession session(workspace);
  Require(session.Create(base, original) && session.Save(factory) && session.Variant(variant) &&
              session.Save(factory),
          "Variant fixture failed");
  const auto first = *session.SourceBaseline();
  auto *document = session.EditableDocument();
  const auto key = *document->Key(node);
  const auto generation = document->Generation();
  const std::array selected{node};
  Require(document->Select(selected) && document->CopySelection() &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 1} &&
              !session.Dirty(),
          "Initial saved reference/clipboard failed");
  editor::PrefabDocumentSession base_editor(workspace);
  Require(
      base_editor.Open(base) &&
          base_editor.EditableDocument()->Rename(*base_editor.Document()->Key(node), "Advanced") &&
          base_editor.Save(factory),
      "Base advancement failed");
  const auto expected = *document->PrepareSave();
  const editor::SceneDocument::PrefabBaseReference next{base, 2};
  Require(!document->ApplyPrefabPropertySnapshot(expected, expected.Bytes(), next, false) &&
              !document->ApplyPrefabPropertySnapshot(expected, expected.Bytes(), {{}, 2}, true) &&
              !document->ApplyPrefabPropertySnapshot(expected, expected.Bytes(), {base, 0}, true) &&
              document->MatchesPreparedSave(expected),
          "Invalid/unauthorized reference changed content/history");
  Require(
      document->ApplyPrefabPropertySnapshot(expected, expected.Bytes(), next, true) &&
          document->PrepareSave()->Bytes() == expected.Bytes() &&
          !document->MatchesPreparedSave(expected) && session.Dirty() &&
          document->Generation() == generation && document->Key(node) == key &&
          document->Selection().size() == 1 &&
          session.BaseReference() == editor::PrefabRevisionReference{base, 2} && document->Undo() &&
          session.BaseReference() == editor::PrefabRevisionReference{base, 1} && !session.Dirty() &&
          document->Redo() && session.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
          session.Dirty(),
      "Metadata-only reference was not one dirty/history transaction or changed keys/bytes");
  runtime::World changed_world;
  editor::SceneDocument changed(changed_world, changed_world.LoadScene("Changed"));
  Require(changed.ReloadBytes(expected.Bytes()) && changed.Rename(*changed.Key(node), "Mixed") &&
              changed.SetTransform(node, {9, 0, 0}),
          "Mixed fixture failed");
  const auto mixed = changed.PrepareSave()->Bytes();
  auto current = document->PrepareSave();
  Require(document->ApplyPropertySnapshot(*current, mixed, true) &&
              document->PrefabBase() == next && document->Undo() &&
              document->PrefabBase() == next &&
              document->PrepareSave()->Bytes() == expected.Bytes(),
          "Ordinary property snapshot/replay erased prefab reference");
  Require(document->Rename(key, "Pending redo") && document->Undo(), "Redo fixture failed");
  current = document->PrepareSave();
  Require(document->ApplyPropertySnapshot(*current, current->Bytes(), true) &&
              session.Save(factory) && session.SourceBaseline()->revision == 2 &&
              session.SourceBaseline()->base == editor::PrefabRevisionReference{base, 2} &&
              session.SourceBaseline()->scene_bytes == first.scene_bytes && !session.Dirty() &&
              editor::PrefabAssets::LoadRevision(workspace, {variant, 1}) == first &&
              document->Redo() && document->Name(node) == "Pending redo" && document->Undo() &&
              !session.Dirty(),
          "Wrapped metadata-only save failed revision/baseline/history/retention contract");
  const auto second = *session.SourceBaseline();
  Require(session.Save(factory) && session.SourceBaseline() == second && document->Redo() &&
              document->Undo(),
          "No-op reference save erased Redo or advanced revision");
  Require(document->Undo() && session.BaseReference() == editor::PrefabRevisionReference{base, 1} &&
              session.Dirty(),
          "Saved rebase Undo failed to restore old reference");
  auto old_review = session.Review();
  Require(old_review && !old_review->CanRevert() && session.SourceBaseline() == second &&
              session.Save(factory) && session.SourceBaseline()->revision == 3 &&
              session.SourceBaseline()->base == editor::PrefabRevisionReference{base, 1} &&
              !session.Dirty(),
          "Review/save ignored current undoable reference or mutated publication before Save");
  Require(document->Redo() && session.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
              session.Save(factory) && session.SourceBaseline()->revision == 4 &&
              !session.Dirty() && document->Paste() && document->Undo() && !session.Dirty(),
          "Reference Redo/save lost clipboard or baseline");
  const auto saved = *session.SourceBaseline();
  Require(document->ApplyPrefabPropertySnapshot(*document->PrepareSave(), mixed, {base, 1}, true) &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 1} &&
              document->Name(node) == "Mixed" && document->Undo() &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
              document->PrepareSave()->Bytes() == saved.scene_bytes && !session.Dirty() &&
              document->Redo() &&
              document->PrefabBase() == editor::SceneDocument::PrefabBaseReference{base, 1} &&
              document->Transform(node)->x == 9 && document->Undo(),
          "Mixed properties/reference did not share one Undo/Redo");
  Require(session.Close() && session.Open(variant) &&
              session.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
              !session.Dirty() && session.Document()->PrepareSave()->Bytes() == saved.scene_bytes &&
              original.PrepareSave()->Bytes() == original_bytes,
          "Reference failed clean wrapped reopen or changed original");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly), "Observer failed");
    editor::PrefabDocumentSession inspection(observer);
    Require(inspection.Open(variant) &&
                inspection.BaseReference() == editor::PrefabRevisionReference{base, 2} &&
                !inspection.Dirty() && !inspection.Save(factory),
            "Read-only metadata was dirty or permitted publication");
  }
  // Plain scene routes must not acknowledge metadata omitted from their file format.
  editor::SceneDocument plain(world, world.LoadScene("Plain"));
  plain.Create("Plain node");
  editor::SceneFileSession files(workspace, plain);
  const auto path = std::filesystem::path("Content/Plain.scene");
  Require(files.SaveAs(files.Token(), path).Applied(), "Named scene fixture failed");
  const auto disk = Read(workspace.Root() / path);
  const auto untagged = *plain.PrepareSave();
  Require(plain.ApplyPrefabPropertySnapshot(untagged, untagged.Bytes(), next, true) &&
              !plain.Save(workspace.Root() / path) && !files.Save(files.Token()).Applied() &&
              Read(workspace.Root() / path) == disk && plain.Dirty(),
          "Ordinary scene save acknowledged unpersisted prefab metadata");
  editor::SceneSaveBatch batch(workspace);
  const std::array batch_files{&files};
  Require(
      !batch.Prepare(batch_files) && Read(workspace.Root() / path) == disk && plain.Dirty() &&
          plain.Undo() && !plain.PrefabBase() && !plain.Dirty() && batch.Prepare(batch_files) &&
          plain.ApplyPrefabPropertySnapshot(*plain.PrepareSave(), untagged.Bytes(), next, true) &&
          !batch.Publish().Published() && Read(workspace.Root() / path) == disk && plain.Dirty(),
      "Save All omitted or acknowledged changed reference metadata");
  auto *editable = session.EditableDocument();
  Require(editable->ApplyPrefabPropertySnapshot(*editable->PrepareSave(), saved.scene_bytes,
                                                {{1009, 99}, 1}, true) &&
              !session.Save(factory) && editor::PrefabAssets::Load(workspace, variant) == saved &&
              editable->Undo() && !session.Dirty(),
          "Missing source closure was published or erased local reference history");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
