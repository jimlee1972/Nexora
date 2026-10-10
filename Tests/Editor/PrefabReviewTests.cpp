#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabDocumentSession.h"
#include <algorithm>
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
  const auto name_field =
      std::ranges::find(first.nodes[0].properties, "name", &editor::PrefabPropertyIdentity::field);
  Require(name_field != first.nodes[0].properties.end(), "Stable name field missing");
  const std::array selected_name{
      editor::PrefabPropertySelection{first.nodes[0].id, name_field->id}};
  const auto selected_review = session.SelectReview(*review, selected_name, &error);
  Require(selected_review && selected_review->Targeted() && selected_review->CanRevert() &&
              selected_review->Selections().size() == 1 && !session.SelectReview(*review, {}) &&
              session.Revert(*selected_review, true) && document->Name(parent) == "Parent" &&
              document->Transform(parent)->x == 3.5 &&
              document->OpaqueComponents(*document->Key(child))->front().data ==
                  std::vector<std::uint8_t>{27, 255, 0} &&
              document->Undo() && document->PrepareSave()->Bytes() == edited && document->Redo() &&
              document->Name(parent) == "Parent" && document->Undo(),
          "Selected review changed unselected properties or failed one-step Undo/Redo");
  const std::array invalid{editor::PrefabPropertySelection{first.nodes[0].id, {999, 1}}};
  Require(!session.SelectReview(*review, invalid) && document->PrepareSave()->Bytes() == edited,
          "Unknown selected field changed the reviewed document");
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
              !session.SelectReview(*review, selected_name) &&
              document->Name(parent) == "After review" && document->Undo() &&
              document->PrepareSave()->Bytes() == edited,
          "Stale review overwrote a later document edit");
  review = session.Review();
  Require(review && session.Save(factory) && !session.Revert(*review, true) &&
              !session.SelectReview(*review, selected_name),
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
    auto selected_inspection =
        inspected ? inspection.SelectReview(*inspected, selected_name) : std::nullopt;
    Require(inspected && inspected->CanRevert() && selected_inspection &&
                selected_inspection->Targeted() && selected_inspection->CanRevert() &&
                !inspection.Revert(*selected_inspection, true) &&
                !inspection.Revert(*inspected, true) &&
                inspection.Document()->PrepareSave()->Bytes() == published_variant.scene_bytes,
            "Read-only review failed or permitted a revert");
  }
  review = session.Review();
  const auto before = document->PrepareSave()->Bytes();
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(review && !session.Review() && !session.Revert(*review, true) &&
              !session.SelectReview(*review, selected_name) &&
              document->PrepareSave()->Bytes() == before,
          "Recovery allowed review/revert or changed live properties");
  std::filesystem::remove(recovery);
  const auto metadata = workspace.Root() / ".nexora/workspace";
  const auto timestamp = std::filesystem::last_write_time(metadata);
  std::filesystem::last_write_time(metadata, timestamp + std::chrono::seconds(5));
  Require(!session.Review() && !session.Revert(*review, true) &&
              !session.SelectReview(*review, selected_name) &&
              document->PrepareSave()->Bytes() == before,
          "External workspace change admitted an old review");
  std::filesystem::last_write_time(metadata, timestamp);
  const auto archived =
      workspace.Root() / ".nexora/prefabs/revisions" / base.ToString() / "2.nxprefab";
  std::ofstream(archived, std::ios::binary | std::ios::trunc) << "corrupt retained source";
  Require(!session.Revert(*review, true) && !session.Review() &&
              !session.SelectReview(*review, selected_name) &&
              document->PrepareSave()->Bytes() == before,
          "Invalid retained source fell back to latest or authorized stale revert");
  Require(session.Close(true) && session.Open(variant) && !session.Revert(*review, true) &&
              !session.SelectReview(*review, selected_name),
          "Replacement revived a prior session review");
  Require(source.PrepareSave()->Bytes() == original, "Review workflow modified original scene");
}

void ApplySource() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-prefab-source-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "Project", "Apply source"), "Apply project fixture failed");
  runtime::World world;
  editor::SceneDocument original(world, world.LoadScene("Primary"));
  const auto node = original.Create("Original");
  Require(original.SetOpaqueComponent(*original.Key(node), {99, "Absent.Provider", {0, 255, 27}}),
          "Apply opaque fixture failed");
  const auto original_bytes = original.PrepareSave()->Bytes();
  editor::PrefabDocumentSession session(workspace);
  const foundation::Uuid base{901, 1}, variant{901, 2};
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{902, ++serial}; };
  Require(session.Create(base, original) && session.Save(factory), "Apply base fixture failed");
  const auto base_first = *session.SourceBaseline();
  Require(session.Variant(variant), "Apply variant fixture failed");
  auto *document = session.EditableDocument();
  const auto key = *document->Key(node);
  Require(document->Rename(key, "Published variant"), "Apply variant rename failed");
  auto draft_review = session.Review();
  Require(draft_review && !draft_review->CanApplyToSource() &&
              !session.ApplyToSource(*draft_review, true) && session.Save(factory),
          "Unsaved variant applied source or lost its initial-save precondition");
  const auto published_variant = *session.SourceBaseline();
  const auto generation = document->Generation();
  const std::array selected{node};
  Require(document->Select(selected) && document->CopySelection() &&
              document->Rename(key, "Selected local") && document->SetTransform(node, {7, 2, 1}) &&
              document->SetOpaqueComponent(key, {99, "Absent.Provider", {77, 255, 0}}),
          "Apply mixed fixture failed");
  const auto edited = document->PrepareSave()->Bytes();
  auto review = session.Review();
  const auto name = std::ranges::find(base_first.nodes[0].properties, "name",
                                      &editor::PrefabPropertyIdentity::field);
  Require(review && review->CanApplyToSource() && name != base_first.nodes[0].properties.end(),
          "Saved variant source application unavailable");
  const std::array fields{editor::PrefabPropertySelection{base_first.nodes[0].id, name->id}};
  auto targeted = session.SelectReview(*review, fields);
  Require(targeted && targeted->CanApplyToSource() && !session.ApplyToSource(*targeted, false) &&
              editor::PrefabAssets::Load(workspace, base) == base_first &&
              document->PrepareSave()->Bytes() == edited,
          "Inspection granted source publication authority");
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(!session.ApplyToSource(*targeted, true) && document->PrepareSave()->Bytes() == edited,
          "Recovery admitted source application");
  std::filesystem::remove(recovery);
  Require(document->Rename(key, "Later edit") && !session.ApplyToSource(*targeted, true) &&
              document->Undo() && document->PrepareSave()->Bytes() == edited,
          "Stale source review overwrote a later edit");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly),
            "Apply observer failed");
    editor::PrefabDocumentSession inspection(observer);
    Require(inspection.Open(variant), "Apply read-only open failed");
    auto readonly_review = inspection.Review();
    Require(readonly_review && readonly_review->CanApplyToSource() &&
                !inspection.ApplyToSource(*readonly_review, true) &&
                editor::PrefabAssets::Load(observer, base) == base_first,
            "Read-only source inspection published an asset");
  }
  const auto source_path = workspace.Root() / ".nexora/prefabs" / (base.ToString() + ".nxprefab");
  auto stage = source_path;
  stage += ".tmp";
  std::ofstream(stage) << "unrelated stage";
  Require(!session.ApplyToSource(*targeted, true) &&
              editor::PrefabAssets::Load(workspace, base) == base_first &&
              document->PrepareSave()->Bytes() == edited,
          "Occupied staging changed current source or variant");
  std::filesystem::remove(stage);
  auto applied = session.ApplyToSource(*targeted, true);
  Require(applied && applied->id == base && applied->revision == 2 &&
              applied->nodes == base_first.nodes && applied->base == base_first.base &&
              applied->nested == base_first.nested &&
              editor::PrefabAssets::LoadRevision(workspace, {base, 1}) == base_first &&
              editor::PrefabAssets::Load(workspace, base) == applied &&
              editor::PrefabAssets::Load(workspace, variant) == published_variant &&
              session.SourceBaseline() == published_variant &&
              document->PrepareSave()->Bytes() == edited && session.Dirty() &&
              document->Generation() == generation && document->Key(node) == key &&
              document->Selection().size() == 1 &&
              original.PrepareSave()->Bytes() == original_bytes,
          "Selected source publication changed local history/baseline/identity/original scene");
  runtime::World source_world;
  editor::SceneDocument source_document(source_world, source_world.LoadScene("Source"));
  Require(source_document.ReloadBytes(applied->scene_bytes) &&
              source_document.Name(node) == "Selected local" &&
              source_document.Transform(node)->x == 0 &&
              source_document.OpaqueComponents(*source_document.Key(node))->front().data ==
                  std::vector<std::uint8_t>{0, 255, 27},
          "Selected source publication changed unselected transform or opaque bytes");
  Require(!session.ApplyToSource(*targeted, true) &&
              editor::PrefabAssets::Load(workspace, base) == applied && document->Undo() &&
              document->Redo() && document->PrepareSave()->Bytes() == edited && document->Paste() &&
              document->Nodes().size() == 2 && document->Undo() &&
              document->PrepareSave()->Bytes() == edited,
          "Changed current source was overwritten or local history/clipboard erased");

  editor::PrefabDocumentSession full(workspace);
  Require(full.Open(base) && full.Variant({901, 3}) && full.Save(factory),
          "Full apply fixture failed");
  const auto full_published = *full.SourceBaseline();
  auto *full_document = full.EditableDocument();
  Require(full_document->Rename(*full_document->Key(node), "Full local") &&
              full_document->SetTransform(node, {12, 3, 4}) &&
              full_document->SetOpaqueComponent(*full_document->Key(node),
                                                {99, "Absent.Provider", {42, 0, 255}}),
          "Full source mixed edits failed");
  const auto full_edited = full_document->PrepareSave()->Bytes();
  auto full_review = full.Review();
  Require(full_review && full_review->CanApplyToSource(), "Full source review failed");
  const auto full_result = full.ApplyToSource(*full_review, true);
  Require(full_result && full_result->revision == 3 && full_result->scene_bytes == full_edited &&
              full_result->nodes == applied->nodes &&
              editor::PrefabAssets::LoadRevision(workspace, {base, 2}) == applied &&
              full_document->PrepareSave()->Bytes() == full_edited &&
              full.SourceBaseline() == full_published && full.Dirty() && full_document->Undo() &&
              full_document->Redo() && full_document->PrepareSave()->Bytes() == full_edited &&
              original.PrepareSave()->Bytes() == original_bytes,
          "Full source application was partial or changed the isolated history/baseline/original");
}
} // namespace
int main() {
  try {
    Run();
    ApplySource();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
