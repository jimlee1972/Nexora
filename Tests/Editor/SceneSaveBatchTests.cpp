#include "../../Engine/Editor/src/SceneSaveBatchTestAccess.h"
#include "Nexora/Editor/SceneSaveBatch.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Batch = editor::SceneSaveBatch;
using Status = editor::SceneSaveBatchStatus;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(static_cast<bool>(file), "Fixture file write failed");
}
struct ProjectFixture {
  editor::ProjectWorkspace workspace;
  runtime::World world;
  std::unique_ptr<editor::SceneDocument> first, second;
  std::unique_ptr<editor::SceneFileSession> first_file, second_file;
  runtime::Id a{}, b{}, first_id{}, second_id{};
  std::filesystem::path root;
  const std::filesystem::path first_path = std::filesystem::path(u8"Content/一.scene");
  const std::filesystem::path second_path = "Content/Second.scene";
  std::array<editor::SceneFileSession *, 2> files{};
  std::array<std::string, 2> originals;
  explicit ProjectFixture(const std::filesystem::path &path) : root(path) {
    Require(workspace.Create(root, "Batch project"), "Workspace create failed");
    first_id = world.LoadScene("First");
    second_id = world.LoadScene("Second");
    first = std::make_unique<editor::SceneDocument>(world, first_id);
    second = std::make_unique<editor::SceneDocument>(world, second_id);
    a = first->CreateCamera("First camera");
    b = second->CreateLight("Second light");
    Require(a && b && first->Select(std::array{*first->Key(a)}) &&
                first->SetEulerField(std::array{*first->Key(a)}, 1, 720) &&
                first->SetOpaqueComponent(*first->Key(a), {71, "Unknown.Batch", {0, 1, 255}}),
            "Document metadata fixture failed");
    first_file = std::make_unique<editor::SceneFileSession>(workspace, *first);
    second_file = std::make_unique<editor::SceneFileSession>(workspace, *second);
    files = {first_file.get(), second_file.get()};
    Require(first_file->SaveAs(first_file->Token(), first_path).Applied() &&
                second_file->SaveAs(second_file->Token(), second_path).Applied(),
            "Original saves failed");
    originals = {Read(root / first_path), Read(root / second_path)};
    Require(first->Rename(*first->Key(a), "First edited") &&
                second->Rename(*second->Key(b), "Second edited"),
            "Dirty fixture failed");
  }
  std::filesystem::path Journal() const { return root / ".nexora/scene-save-all.recovery"; }
  void OriginalsUnchanged() const {
    Require(Read(root / first_path) == originals[0] && Read(root / second_path) == originals[1] &&
                first->Dirty() && second->Dirty(),
            "Rejected batch changed originals or acknowledged document baselines");
  }
};
void Success(const std::filesystem::path &root) {
  ProjectFixture f(root);
  const auto token_a = f.first_file->Token(), token_b = f.second_file->Token();
  const auto capture_a = f.first->CaptureRuntimeScene(),
             capture_b = f.second->CaptureRuntimeScene();
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Batch preparation failed");
  const auto result = batch.Publish();
  Require(result.status == Status::Published && result.Published() &&
              !f.workspace.HasRecoveryJournal() && !f.first->Dirty() && !f.second->Dirty() &&
              f.first_file->Token() == token_a && f.second_file->Token() == token_b &&
              f.first->CaptureRuntimeScene() == capture_a &&
              f.second->CaptureRuntimeScene() == capture_b && f.first->Selection().size() == 1 &&
              (*f.first->EulerAngles(f.a))[1] == 720,
          "Complete Save All lost document identity, metadata or clean baselines");
  const auto first_saved = Read(root / f.first_path);
  Require(first_saved != f.originals[0] && Read(root / f.second_path) != f.originals[1] &&
              f.first->Undo() && f.first->Dirty() && f.first->Redo() && !f.first->Dirty() &&
              f.second->Undo() && f.second->Dirty() && f.second->Redo() && !f.second->Dirty(),
          "Save All changed Undo/Redo or failed to publish every file");
  runtime::World reopened;
  const auto reopened_id = reopened.LoadScene("Reopened");
  editor::SceneDocument document(reopened, reopened_id);
  Require(document.Reload(root / f.first_path) && document.Name(f.a) == "First edited" &&
              document.OpaqueComponents(*document.Key(f.a)) ==
                  std::vector<editor::OpaqueComponent>{{71, "Unknown.Batch", {0, 1, 255}}},
          "Published file did not independently reopen complete opaque metadata");
  Require(f.first->Rename(*f.first->Key(f.a), "After batch"), "Post-batch edit failed");
  Write(root / f.first_path, first_saved + "\n");
  Require(f.first_file->Save(f.first_file->Token()).status ==
              editor::SceneFileStatus::NeedsOverwrite,
          "Successful batch did not install the exact per-file external-change baseline");
}
void Rejections(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  std::array<editor::SceneFileSession *, 2> duplicate{f.first_file.get(), f.first_file.get()};
  std::array<editor::SceneFileSession *, 17> too_many{};
  too_many.fill(f.first_file.get());
  std::array<editor::SceneFileSession *, 1> null{nullptr};
  Require(!batch.Prepare({}) && !batch.Prepare(duplicate) && !batch.Prepare(too_many) &&
              !batch.Prepare(null) && !Batch::HasRecovery(f.workspace),
          "Invalid batch inputs created recovery or bypassed bounds");
  Require(batch.Prepare(f.files) && f.first->Rename(*f.first->Key(f.a), "Changed after prepare") &&
              batch.Publish().status == Status::Rejected,
          "Changed prepared document was published");
  f.OriginalsUnchanged();
  Require(batch.Prepare(f.files), "Disk conflict preparation failed");
  Write(root / f.second_path, f.originals[1] + "\n");
  Require(batch.Publish().status == Status::Rejected &&
              Read(root / f.first_path) == f.originals[0] && !Batch::HasRecovery(f.workspace),
          "External change was detected only after partial publication");
  Write(root / f.second_path, f.originals[1]);
  const auto sibling = std::filesystem::path((root / f.first_path).native() +
                                             std::filesystem::path(".save-all-tmp").native());
  Write(sibling, "foreign staging");
  Require(!batch.Prepare(f.files) && Read(sibling) == "foreign staging",
          "Preparation replaced an occupied staging path");
  std::filesystem::remove(sibling);
  const auto hard_alias = root / "Content/HardAlias.scene";
  std::filesystem::create_hard_link(root / f.first_path, hard_alias);
  Require(!batch.Prepare(f.files) && Read(hard_alias) == f.originals[0],
          "A hard-linked destination bypassed ordinary-file ownership checks");
  std::filesystem::remove(hard_alias);
#if !defined(_WIN32)
  // These hosts permit fixture links without Windows developer-mode/administrator privileges.
  std::filesystem::rename(root / "Content", root / "OriginalContent");
  std::filesystem::create_directory_symlink(root / "OriginalContent", root / "Content");
  Require(!batch.Prepare(f.files) && Read(root / f.first_path) == f.originals[0],
          "An aliased destination parent was accepted");
  std::filesystem::remove(root / "Content");
  std::filesystem::rename(root / "OriginalContent", root / "Content");
#endif
  editor::ProjectWorkspace reader;
  Require(reader.Open(root, editor::ProjectAccess::ReadOnly), "Read-only open failed");
  Batch read_only(reader);
  Require(!read_only.Prepare(f.files), "Read-only or foreign-project sessions were accepted");
  f.OriginalsUnchanged();
}
void PortableCaseCollision(const std::filesystem::path &root) {
  ProjectFixture f(root);
  // Both leaves are absent, independently of the host filesystem's case sensitivity.
  Require(f.first_file->BindCurrent("Content/Case.scene") &&
              f.second_file->BindCurrent("Content/case.scene"),
          "Missing case-variant destination fixture failed");
  Batch batch(f.workspace);
  Require(!batch.Prepare(f.files) && !f.workspace.HasRecoveryJournal() &&
              !std::filesystem::exists(root / "Content/Case.scene") &&
              !std::filesystem::exists(root / "Content/case.scene"),
          "Portable ASCII case collision reached staging or changed source baselines");
  f.OriginalsUnchanged();
}
void Rollback(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Rollback preparation failed");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1) {
      Require(Read(root / f.first_path) != f.originals[0], "First replacement was not real");
      Write(f.Journal() / "manifest.tmp", Read(f.Journal() / "manifest"));
      throw std::runtime_error("Injected second-file interruption");
    }
  });
  Require(batch.Publish().status == Status::Rejected && !f.workspace.HasRecoveryJournal(),
          "Second-file interruption failed to roll back and clean the batch");
  f.OriginalsUnchanged();
  Require(f.first->Undo() && f.first->Redo() && f.second->Undo() && f.second->Redo() &&
              batch.Prepare(f.files),
          "Rollback destroyed history or prevented retry");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, {});
  Require(batch.Publish().Published(), "Restored batch could not be retried");
}
void StaleAndCancelled(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Cancelled preparation failed");
  batch.Cancel();
  Require(batch.Publish().status == Status::Rejected && !Batch::HasRecovery(f.workspace),
          "Cancelled batch created a journal or published files");
  f.OriginalsUnchanged();
  Require(batch.Prepare(f.files) && f.first_file->New(f.first_file->Token(), true).Applied(),
          "Generation replacement fixture failed");
  const auto replaced = f.first->CaptureRuntimeScene();
  Require(batch.Publish().status == Status::Rejected &&
              f.first->CaptureRuntimeScene() == replaced &&
              Read(root / f.first_path) == f.originals[0] &&
              Read(root / f.second_path) == f.originals[1] && !Batch::HasRecovery(f.workspace),
          "Replaced document submitted an old prepared batch");
}
void MissingStage(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Missing stage preparation failed");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1) {
      auto staged = root / f.second_path;
      staged += ".save-all-tmp";
      Require(std::filesystem::remove(staged), "Real staged output was unavailable");
    }
  });
  Require(batch.Publish().status == Status::Rejected && !f.workspace.HasRecoveryJournal(),
          "Missing second stage did not roll back the first real file replacement");
  f.OriginalsUnchanged();
}
void ChangedDuringPublication(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Late document edit preparation failed");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1)
      Require(f.first->Rename(*f.first->Key(f.a), "Edited during publication"),
              "Late authoring edit failed");
  });
  Require(batch.Publish().status == Status::Rejected &&
              f.first->Name(f.a) == "Edited during publication" &&
              !f.workspace.HasRecoveryJournal(),
          "Late authoring edit was acknowledged, lost or left partially published files");
  f.OriginalsUnchanged();
  Require(f.first->Undo() && f.first->Name(f.a) == "First edited" && f.first->Redo() &&
              f.first->Name(f.a) == "Edited during publication",
          "Late-edit rollback consumed document history");
}
void NewNamedDestination(const std::filesystem::path &root) {
  ProjectFixture f(root);
  const auto new_path = std::filesystem::path("Content/NewNamed.scene");
  Require(f.second_file->BindCurrent(new_path), "New named destination binding failed");
  Batch batch(f.workspace);
  const std::array reversed{f.second_file.get(), f.first_file.get()};
  Require(batch.Prepare(reversed), "New named destination preparation failed");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1)
      throw std::runtime_error("Interrupt after publishing the new destination");
  });
  Require(batch.Publish().status == Status::Rejected && !std::filesystem::exists(root / new_path) &&
              Read(root / f.first_path) == f.originals[0] &&
              Read(root / f.second_path) == f.originals[1] && !f.workspace.HasRecoveryJournal(),
          "Absent-original rollback created a source or changed another file");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, {});
  Require(batch.Prepare(reversed) && batch.Publish().status == Status::Published &&
              std::filesystem::exists(root / new_path) &&
              Read(root / f.second_path) == f.originals[1],
          "New named batch destination failed or overwrote an unrelated previous file");
}
void MaximumDocuments(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Maximum documents"), "Maximum workspace create failed");
  runtime::World world;
  std::vector<std::unique_ptr<editor::SceneDocument>> documents;
  std::vector<std::unique_ptr<editor::SceneFileSession>> owners;
  std::vector<editor::SceneFileSession *> files;
  for (std::size_t i = 0; i < Batch::kMaximumDocuments; ++i) {
    auto document = std::make_unique<editor::SceneDocument>(world, world.LoadScene("Batch scene"));
    const auto entity = document->Create("Original");
    auto session = std::make_unique<editor::SceneFileSession>(workspace, *document);
    const auto path = std::filesystem::path("Content") / (std::to_string(i) + ".scene");
    Require(entity && session->SaveAs(session->Token(), path).Applied() &&
                document->Rename(*document->Key(entity), "Edited maximum"),
            "Maximum document fixture failed");
    files.push_back(session.get());
    documents.push_back(std::move(document));
    owners.push_back(std::move(session));
  }
  Batch batch(workspace);
  Require(batch.Prepare(files) && batch.Publish().status == Status::Published,
          "Exactly sixteen actual documents did not publish");
  for (std::size_t i = 0; i < documents.size(); ++i)
    Require(!documents[i]->Dirty() &&
                Read(root / *owners[i]->CurrentPath()).find("Edited maximum") != std::string::npos,
            "Maximum batch missed a real document or source output");
}
void AggregateBudget(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Aggregate budget"), "Budget workspace create failed");
  runtime::World world;
  std::vector<std::unique_ptr<editor::SceneDocument>> documents;
  std::vector<std::unique_ptr<editor::SceneFileSession>> owners;
  std::vector<editor::SceneFileSession *> files;
  std::vector<std::string> originals;
  std::size_t combined = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    auto document = std::make_unique<editor::SceneDocument>(world, world.LoadScene("Budget scene"));
    const auto entity = document->Create("Budget node");
    const auto key = document->Key(entity);
    Require(key.has_value(), "Budget node failed");
    // Eight individually legal 1 MiB components serialize to over 16 MiB per scene.
    // Four originals plus four outputs cross 128 MiB while each scene remains legal.
    for (std::uint64_t type = 1; type <= 8; ++type)
      Require(document->SetOpaqueComponent(
                  *key,
                  {type, "Budget.Unknown",
                   std::vector<std::uint8_t>(editor::UnknownComponentStore::kMaximumComponentBytes,
                                             static_cast<std::uint8_t>(type))}),
              "Legal budget payload was rejected");
    auto session = std::make_unique<editor::SceneFileSession>(workspace, *document);
    const auto path = std::filesystem::path("Content") / (std::to_string(i) + ".scene");
    Require(session->SaveAs(session->Token(), path).Applied() &&
                document->Rename(*key, "Budget edit"),
            "Budget source save failed");
    originals.push_back(Read(root / path));
    const auto prepared = document->PrepareSave();
    Require(prepared.has_value(), "Individual legal budget snapshot failed");
    combined += originals.back().size() + prepared->Bytes().size();
    files.push_back(session.get());
    documents.push_back(std::move(document));
    owners.push_back(std::move(session));
  }
  Batch batch(workspace);
  Require(combined > Batch::kMaximumPayloadBytes && !batch.Prepare(files) &&
              !workspace.HasRecoveryJournal() && batch.Publish().status == Status::Rejected &&
              batch.Prepare(std::span<editor::SceneFileSession *const>(files.data(), 3)),
          "Actual aggregate overflow was admitted or a legal subset was rejected");
  batch.Cancel();
  for (std::size_t i = 0; i < documents.size(); ++i)
    Require(documents[i]->Dirty() && Read(root / *owners[i]->CurrentPath()) == originals[i],
            "Budget rejection/cancellation changed originals or dirty baselines");
}
void ProjectSwitch(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Project switch preparation failed");
  const auto other_root = root.parent_path() / "other-project";
  {
    editor::ProjectWorkspace other;
    Require(other.Create(other_root, "Other project"), "Other project create failed");
  }
  Require(f.workspace.Open(other_root) && batch.Publish().status == Status::Rejected &&
              Read(root / f.first_path) == f.originals[0] &&
              Read(root / f.second_path) == f.originals[1] &&
              !std::filesystem::exists(f.Journal()) && !Batch::HasRecovery(f.workspace),
          "A changed project lease published stale scene bytes");
}
void RetainedRecovery(const std::filesystem::path &root, bool discard) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Retained recovery preparation failed");
  const auto foreign = f.originals[1] + "\nforeign";
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1)
      Write(root / f.second_path, foreign);
  });
  Require(batch.Publish().status == Status::RecoveryRequired && f.workspace.HasRecoveryJournal() &&
              Read(root / f.second_path) == foreign &&
              Read(root / f.first_path) != f.originals[0] &&
              Read(f.Journal() / "before-0.scene") == f.originals[0] &&
              Read(f.Journal() / "before-1.scene") == f.originals[1] && f.first->Dirty() &&
              f.second->Dirty(),
          "Conflict recovery touched foreign bytes or lost original copies");
  const auto partially_published = Read(root / f.first_path);
  Require(!f.workspace.RecoverWorkspace() && Read(root / f.first_path) == partially_published &&
              Read(root / f.second_path) == foreign &&
              !f.first_file->Save(f.first_file->Token()).Applied() &&
              !f.workspace.SaveWorkspace(std::array<std::string, 1>{"Content/Other.scene"}),
          "Recovery failed to preflight all files or gate ordinary authoring");
  const auto manifest = Read(f.Journal() / "manifest");
  auto corrupted = manifest;
  corrupted.back() ^= 1;
  Write(f.Journal() / "manifest", corrupted);
  Require(!f.workspace.RecoverWorkspace() && Read(root / f.first_path) == partially_published &&
              Read(f.Journal() / "before-0.scene") == f.originals[0],
          "Corrupted manifest was accepted or recovery copies were discarded");
  Write(f.Journal() / "manifest", manifest);
  Write(root / f.second_path, f.originals[1]);
  Write(f.Journal() / "foreign", "inspection data");
  Require(!f.workspace.RecoverWorkspace() && Read(root / f.first_path) == partially_published &&
              Read(f.Journal() / "foreign") == "inspection data",
          "Unknown retained entries were erased or ignored before source restoration");
  std::filesystem::remove(f.Journal() / "foreign");
  const auto original_copy = Read(f.Journal() / "before-1.scene");
  Write(f.Journal() / "before-1.scene", original_copy + "\n");
  Require(!f.workspace.RecoverWorkspace() && Read(root / f.first_path) == partially_published,
          "Corrupted original copy was accepted or another file was restored first");
  Write(f.Journal() / "before-1.scene", original_copy);
  Require(discard ? f.workspace.DiscardRecovery() : f.workspace.RecoverWorkspace(),
          "Workspace recovery controls did not restore the complete batch");
  Require(!f.workspace.HasRecoveryJournal(), "Recovered batch kept authoring blocked");
  f.OriginalsUnchanged();
}
void CommittedCleanup(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Committed cleanup preparation failed");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1)
      Write(f.Journal() / "foreign", "preserve this");
  });
  const auto result = batch.Publish();
  const auto published = std::array{Read(root / f.first_path), Read(root / f.second_path)};
  Require(result.status == Status::PublishedRecoveryRequired && result.Published() &&
              !f.first->Dirty() && !f.second->Dirty() && f.workspace.HasRecoveryJournal() &&
              !f.workspace.DiscardRecovery() && Read(f.Journal() / "foreign") == "preserve this",
          "Committed batch cleanup erased an unknown file or reported uncommitted documents");
  std::filesystem::remove(f.Journal() / "foreign");
  Require(f.workspace.DiscardRecovery() && !f.workspace.HasRecoveryJournal() &&
              Read(root / f.first_path) == published[0] &&
              Read(root / f.second_path) == published[1],
          "Committed recovery rolled back an acknowledged batch");
}
void RestartRecovery(const std::filesystem::path &root) {
  std::array<std::string, 2> originals;
  const std::filesystem::path first_path(u8"Content/一.scene"), second_path("Content/Second.scene");
  {
    ProjectFixture f(root);
    originals = f.originals;
    Batch batch(f.workspace);
    Require(batch.Prepare(f.files), "Restart recovery preparation failed");
    editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
      if (index == 1)
        Write(root / second_path, originals[1] + "\nexternal");
    });
    Require(batch.Publish().status == Status::RecoveryRequired,
            "Restart fixture did not retain a real interrupted batch");
  }
  editor::ProjectWorkspace writer, reader;
  Require(writer.Open(root) && writer.HasRecoveryJournal() &&
              reader.Open(root, editor::ProjectAccess::ReadOnly) && reader.HasRecoveryJournal() &&
              !reader.RecoverWorkspace() && !writer.RecoverWorkspace(),
          "Reopened leases lost recovery or overwrote an unresolved external change");
  Write(root / second_path, originals[1]);
  Require(writer.RecoverWorkspace() && !writer.HasRecoveryJournal() &&
              Read(root / first_path) == originals[0] && Read(root / second_path) == originals[1],
          "Reopened writer failed to restore exact originals");
}
void CleanupRetry(const std::filesystem::path &root) {
  ProjectFixture f(root);
  Batch batch(f.workspace);
  Require(batch.Prepare(f.files), "Cleanup retry preparation failed");
  editor::SceneSaveBatchTestAccess::BeforePublish(batch, [&](std::size_t index) {
    if (index == 1)
      Write(f.Journal() / "before-1.scene", "corrupted retained original");
  });
  Require(batch.Publish().status == Status::PublishedRecoveryRequired &&
              !std::filesystem::exists(f.Journal() / "before-0.scene") &&
              std::filesystem::exists(f.Journal() / "before-1.scene") && !f.first->Dirty() &&
              !f.second->Dirty(),
          "Cleanup fixture did not retain a committed batch after partial cleanup");
  const auto published = std::array{Read(root / f.first_path), Read(root / f.second_path)};
  Require(!f.workspace.RecoverWorkspace(), "Corrupted cleanup copy was silently deleted");
  Write(f.Journal() / "before-1.scene", f.originals[1]);
  Require(f.workspace.RecoverWorkspace() && !f.workspace.HasRecoveryJournal() &&
              Read(root / f.first_path) == published[0] &&
              Read(root / f.second_path) == published[1],
          "Completed cleanup required already removed copies or changed committed source bytes");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-scene-save-batch-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Success(root / "success");
    Rejections(root / "reject");
    PortableCaseCollision(root / "case-collision");
    Rollback(root / "rollback");
    StaleAndCancelled(root / "stale");
    MissingStage(root / "missing-stage");
    ChangedDuringPublication(root / "late-edit");
    NewNamedDestination(root / "new-named");
    MaximumDocuments(root / "maximum");
    AggregateBudget(root / "aggregate");
    ProjectSwitch(root / "switch");
    RetainedRecovery(root / "recover", false);
    RetainedRecovery(root / "discard", true);
    CommittedCleanup(root / "committed");
    RestartRecovery(root / "restart");
    CleanupRetry(root / "cleanup-retry");
    std::filesystem::remove_all(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\nFixture retained at " << root << '\n';
    return 1;
  }
}
