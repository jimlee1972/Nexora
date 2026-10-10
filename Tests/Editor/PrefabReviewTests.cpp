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
                    ("nexora-prefab-review-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root / "Project", "Review", &error), "Project fixture failed");
  runtime::World world;
  editor::SceneDocument source(world, world.LoadScene("Original"));
  const auto parent = source.Create("Parent"), child = source.Create("Child", parent);
  Require(source.SetOpaqueComponent(*source.Key(child), {99, "Absent.Provider", {0, 255, 27}}),
          "Opaque fixture failed");
  const auto original = source.PrepareSave()->Bytes();
  editor::PrefabDocumentSession session(workspace);
  const foundation::Uuid base{801, 1}, variant{801, 2};
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{802, ++serial}; };
  Require(session.Create(base, source) && !session.Review() && session.Save(factory),
          "Draft review fabricated stable source identities");
  const auto first = *session.SourceBaseline();
  auto *document = session.EditableDocument();
  const auto key = *document->Key(parent);
  const auto generation = document->Generation();
  const std::array selected{parent};
  Require(document->Select(selected) && document->CopySelection() &&
              document->Rename(key, "Edited") && document->SetTransform(parent, {3.5, 2, 1}) &&
              document->SetOpaqueComponent(*document->Key(child),
                                           {99, "Absent.Provider", {27, 255, 0}}),
          "Mixed property fixture failed");
  const auto edited = document->PrepareSave()->Bytes();
  auto review = session.Review(&error);
  Require(review && review->CanRevert() && review->Changes().rows.size() >= 3 &&
              document->PrepareSave()->Bytes() == edited && !session.Revert(*review, false) &&
              document->PrepareSave()->Bytes() == edited,
          "Read-only inspection changed content or granted write authority");
  Require(session.Revert(*review, true, &error) && document->PrepareSave()->Bytes() == original &&
              !session.Dirty() && document->Generation() == generation &&
              document->Key(parent) == key && document->Selection().size() == 1 &&
              document->Selection()[0] == parent && document->Undo() &&
              document->PrepareSave()->Bytes() == edited && document->Redo() &&
              document->PrepareSave()->Bytes() == original && document->Undo(),
          "Full revert was partial, changed identities or failed one-step Undo/Redo");
  Require(editor::PrefabAssets::Load(workspace, base) == first &&
              source.PrepareSave()->Bytes() == original && document->Paste() &&
              document->Nodes().size() == 4 && document->Undo() &&
              document->PrepareSave()->Bytes() == edited,
          "Revert changed published/original source or erased the clipboard");
  review = session.Review();
  Require(review && document->Rename(key, "After review") && !session.Revert(*review, true) &&
              document->Name(parent) == "After review" && document->Undo() &&
              document->PrepareSave()->Bytes() == edited,
          "Stale review overwrote a later document edit");
  review = session.Review();
  Require(review && session.Save(factory) && !session.Revert(*review, true),
          "Save retained authority from an earlier published baseline");
  const auto second = *session.SourceBaseline();
  Require(second.revision == 2 && session.Variant(variant) && session.Save(factory),
          "Variant fixture failed");
  const auto variant_source = *session.SourceBaseline();
  Require(variant_source.base == editor::PrefabRevisionReference{base, 2},
          "Variant lost its exact base");
  document = session.EditableDocument();
  Require(document->Rename(*document->Key(parent), "Variant override") && session.Save(factory),
          "Variant override save failed");
  const auto published_variant = *session.SourceBaseline();
  // Advance the base independently. Review must retain revision two, not use the newest source.
  editor::PrefabDocumentSession base_editor(workspace);
  Require(base_editor.Open(base) &&
              base_editor.EditableDocument()->Rename(*base_editor.Document()->Key(parent),
                                                     "New base") &&
              base_editor.Save(factory),
          "Base advancement fixture failed");
  const auto third = *base_editor.SourceBaseline();
  review = session.Review(&error);
  Require(review && review->CanRevert() && session.Revert(*review, true) &&
              document->Name(parent) == "Edited" && session.Dirty() &&
              session.SourceBaseline() == published_variant &&
              editor::PrefabAssets::Load(workspace, base) == third &&
              editor::PrefabAssets::Load(workspace, variant) == published_variant &&
              document->Undo() && document->Name(parent) == "Variant override",
          "Variant revert used newest base or published source implicitly");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly), "Observer failed");
    editor::PrefabDocumentSession inspection(observer);
    Require(inspection.Open(variant), "Read-only open failed");
    auto inspected = inspection.Review();
    Require(inspected && inspected->CanRevert() && !inspection.Revert(*inspected, true) &&
                inspection.Document()->PrepareSave()->Bytes() == published_variant.scene_bytes,
            "Read-only review failed or permitted a revert");
  }
  review = session.Review();
  const auto before = document->PrepareSave()->Bytes();
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(review && !session.Review() && !session.Revert(*review, true) &&
              document->PrepareSave()->Bytes() == before,
          "Recovery allowed review/revert or changed live properties");
  std::filesystem::remove(recovery);
  const auto metadata = workspace.Root() / ".nexora/workspace";
  const auto timestamp = std::filesystem::last_write_time(metadata);
  std::filesystem::last_write_time(metadata, timestamp + std::chrono::seconds(5));
  Require(!session.Review() && !session.Revert(*review, true) &&
              document->PrepareSave()->Bytes() == before,
          "External workspace change admitted an old review");
  std::filesystem::last_write_time(metadata, timestamp);
  const auto archived =
      workspace.Root() / ".nexora/prefabs/revisions" / base.ToString() / "2.nxprefab";
  std::ofstream(archived, std::ios::binary | std::ios::trunc) << "corrupt retained source";
  Require(!session.Revert(*review, true) && !session.Review() &&
              document->PrepareSave()->Bytes() == before,
          "Invalid retained source fell back to latest or authorized stale revert");
  Require(session.Close(true) && session.Open(variant) && !session.Revert(*review, true),
          "Replacement revived a prior session review");
  Require(source.PrepareSave()->Bytes() == original, "Review workflow modified original scene");
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
