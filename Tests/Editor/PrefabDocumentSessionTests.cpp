#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabDocumentSession.h"
#include <chrono>
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
                    ("nexora-prefab-isolation-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root / "Project", "Isolation", &error), "Project creation failed");
  runtime::World source_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Original"));
  const auto parent = source.Create("Parent"), child = source.Create("Child", parent);
  Require(source.SetOpaqueComponent(*source.Key(child), {99, "Absent.Provider", {0, 255, 27}}),
          "Opaque source fixture failed");
  const std::string source_bytes = source.PrepareSave()->Bytes();
  const auto source_generation = source.Generation();
  editor::PrefabDocumentSession session(workspace);
  const foundation::Uuid base_id{501, 1}, variant_id{501, 2};
  std::uint64_t identity = 100;
  const auto factory = [&] { return foundation::Uuid{502, ++identity}; };
  Require(!session.Document() && !session.Dirty() && !session.Save(factory, &error) &&
              !session.Open(base_id, false, &error),
          "Empty owner fabricated a source");
  Require(session.Create(base_id, source, false, &error) && session.Dirty() &&
              session.Document()->PrepareSave()->Bytes() == source_bytes &&
              session.AssetId() == base_id && !session.SourceBaseline(),
          "Draft lost exact owning source");
  auto *document = session.EditableDocument();
  Require(document && document != &source, "Isolation borrows the caller document");
  const auto key = *document->Key(parent);
  const auto document_generation = document->Generation(),
             session_generation = session.Generation();
  const std::array selected{parent};
  Require(document->Select(selected) && document->CopySelection() &&
              document->Rename(key, "Isolated"),
          "Isolated edit fixture failed");
  Require(source.PrepareSave()->Bytes() == source_bytes &&
              source.Generation() == source_generation && !session.Close(false, &error) &&
              !session.Open(base_id, false, &error) &&
              !session.Create({501, 3}, source, false, &error) && session.Document() == document &&
              session.Generation() == session_generation,
          "Dirty replacement changed either owner");
  bool callback_ran = false;
  bool callback_threw = false;
  try {
    static_cast<void>(session.Save(
        []() -> foundation::Uuid { throw std::runtime_error("Identity factory fixture"); }));
  } catch (const std::runtime_error &) {
    callback_threw = true;
  }
  Require(callback_threw && session.Dirty() && session.EditableDocument() == document &&
              !session.SourceBaseline() && !editor::PrefabAssets::Load(workspace, base_id),
          "Throwing identity callback published source or retained a busy owner");
  const auto guarded_factory = [&] {
    callback_ran = true;
    Require(!session.Close(true) && !session.Create({501, 3}, source, true) &&
                !session.Open(base_id, true) && !session.Save(factory) &&
                !session.EditableDocument(),
            "Identity callback replaced an active save owner");
    return factory();
  };
  Require(session.Save(guarded_factory, &error) && callback_ran && !session.Dirty() &&
              session.Document() == document && document->Generation() == document_generation &&
              document->Key(parent) == key && document->Selection().size() == 1 &&
              document->Selection()[0] == parent,
          "Save lost current document identity or selection");
  const auto first = session.SourceBaseline();
  Require(first && first->revision == 1 &&
              editor::PrefabAssets::Load(workspace, base_id) == first && document->Undo() &&
              session.Dirty() && document->Name(parent) == "Parent" && document->Redo() &&
              !session.Dirty() && document->Name(parent) == "Isolated",
          "Save erased history or failed actual reopen");
  Require(document->Paste() && document->Nodes().size() == 4 && document->Undo() &&
              !session.Dirty(),
          "Save erased the isolated clipboard");
  Require(document->Rename(key, "Dirty variant") && !session.Variant(variant_id, &error) &&
              session.Document() == document && document->Undo() && !session.Dirty(),
          "Variant silently replaced unsaved history");
  Require(session.Variant(variant_id, &error) && session.AssetId() == variant_id &&
              session.Dirty() && session.SourceBaseline() == first &&
              session.Document()->PrepareSave()->Bytes() == first->scene_bytes,
          "Variant lost exact source baseline");
  document = session.EditableDocument();
  const auto variant_key = *document->Key(parent);
  Require(document->Rename(variant_key, "Variant") && session.Save(factory, &error),
          "Variant edit/save failed");
  const auto variant = session.SourceBaseline();
  Require(variant && variant->id == variant_id && variant->revision == 1 &&
              variant->base == editor::PrefabRevisionReference{base_id, 1} &&
              variant->nodes == first->nodes &&
              editor::PrefabAssets::Load(workspace, base_id) == first &&
              editor::PrefabAssets::Load(workspace, variant_id) == variant && document->Undo() &&
              session.Dirty() && document->Redo() && !session.Dirty(),
          "Variant modified its base, lost identities or erased edit history");
  Require(session.Close(false, &error) && !session.Document() &&
              session.Open(variant_id, false, &error) &&
              session.Document()->PrepareSave()->Bytes() == variant->scene_bytes &&
              !session.Dirty(),
          "Actual isolated source close/reopen failed");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly, &error),
            "Observer failed");
    editor::PrefabDocumentSession read_only(observer);
    Require(read_only.Open(variant_id, false, &error) && read_only.Document() &&
                !read_only.EditableDocument() && !read_only.Save(factory, &error) &&
                !read_only.Variant({501, 4}, &error) &&
                !read_only.Create({501, 4}, source, true, &error) &&
                read_only.SourceBaseline() == variant && read_only.Close(),
            "Read-only isolation published or discarded source");
  }
  Require(session.Open(base_id, false, &error), "Base reopen failed");
  document = session.EditableDocument();
  Require(document->Rename(*document->Key(parent), "Foreign revision"), "Foreign edit failed");
  const auto foreign = editor::PrefabAssets::Capture(base_id, *document, factory, &*first);
  Require(foreign && editor::PrefabAssets::Publish(workspace, *foreign, &*first, &error) &&
              !session.Save(factory, &error) && session.Dirty() &&
              session.SourceBaseline() == first && document->Undo() && !session.Dirty() &&
              !session.Variant({501, 4}, &error) && session.Document() == document,
          "Foreign current revision was overwritten or silently became a variant base");
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(!session.EditableDocument() && !session.Open(variant_id, true, &error) &&
              !session.Save(factory, &error) && session.Document() == document && session.Close(),
          "Recovery did not freeze admission or prevent safe owner release");
  std::filesystem::remove(recovery);
  Require(session.Open(variant_id, false, &error), "Recovery release failed");
  Require(workspace.Create(root / "Replacement", "Replacement", &error) && !session.Document() &&
              session.AssetId().IsNil() && !session.SourceBaseline() &&
              !session.EditableDocument() && !session.Open(variant_id, true, &error) &&
              !session.Save(factory, &error) && session.Close(),
          "Rebound workspace revived an old prefab owner");
  Require(source.PrepareSave()->Bytes() == source_bytes && source.Generation() == source_generation,
          "Isolation changed caller source/history");
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
