#include "Nexora/Cryptography/Signature.h"
#include "Nexora/Editor/SceneMergePublication.h"
#include "SceneMergePublicationTestAccess.h"
#include "SceneMergeTestAccess.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Publisher = editor::SceneMergePublisher;
using Status = editor::SceneMergePublishStatus;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(static_cast<bool>(f), "Fixture write failed");
}
std::string Bytes(const editor::SceneDocument &d) {
  auto p = d.PrepareSave();
  Require(p.has_value(), "Prepare failed");
  return p->Bytes();
}
std::string Hash(std::string_view bytes) {
  auto hash = cryptography::Sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
  Require(hash.has_value(), "Hash backend unavailable");
  std::string result;
  constexpr char hex[] = "0123456789abcdef";
  for (const auto b : *hash) {
    auto n = std::to_integer<unsigned char>(b);
    result.push_back(hex[n >> 4]);
    result.push_back(hex[n & 15]);
  }
  return result;
}
struct Scratch {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-scene-merge-publish-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~Scratch() {
    std::error_code e;
    std::filesystem::remove_all(root, e);
  }
};
struct Fixture {
  Scratch scratch;
  editor::ProjectWorkspace workspace;
  runtime::World world;
  editor::SceneDocument document{world, world.LoadScene("Publish")};
  runtime::Id entity{};
  std::string base, local, disk;
  std::optional<editor::SceneMergeReview> review;
  editor::SceneMergeRequest request;
  const std::filesystem::path relative{"Content/Authoring.scene"};
  Fixture() {
    Require(workspace.Create(scratch.root, "Merge publication"), "Project create failed");
    std::filesystem::create_directories(scratch.root / "Content");
    entity = document.Create("Base");
    Require(entity != 0, "Base node failed");
    Require(
        document.SetOpaqueComponent(*document.Key(entity), {812, "Missing.Provider", {0, 255, 42}}),
        "Opaque fixture failed");
  }
};
// The file session must be created after ProjectWorkspace::Create; use a separate local owner.
void Run() {
  Fixture f;
  editor::SceneFileSession files(f.workspace, f.document);
  Require(files.SaveAs(files.Token(), f.relative).Applied(), "Base save failed");
  f.base = Read(f.scratch.root / f.relative);
  Require(f.document.Rename(*f.document.Key(f.entity), "Local"), "Local rename failed");
  f.local = Bytes(f.document);
  runtime::World remote_world;
  editor::SceneDocument remote(remote_world, remote_world.LoadScene("Disk"));
  Require(remote.ReloadBytes(f.base) && remote.Rename(*remote.Key(f.entity), "Disk"),
          "Remote rename failed");
  auto pose = *remote.Transform(f.entity);
  pose.z = 7;
  Require(remote.SetTransform(f.entity, pose), "Remote pose failed");
  f.disk = Bytes(remote);
  Write(f.scratch.root / f.relative, f.disk);
  f.review = editor::SceneMerge::Prepare(f.base, f.local, f.disk);
  Require(f.review.has_value(), "Review failed");
  f.request = {1, f.review->Identity(), {}};
  for (std::size_t i = 0; i < f.review->Comparison().rows.size(); ++i)
    if (f.review->Comparison().rows[i].choice == editor::SceneComparisonChoice::Unresolved)
      f.request.choices.push_back({i, editor::SceneMergeChoice::Local});
  auto prepare = [&] { return Publisher::Prepare(files, *f.review, f.request, true); };
  const auto generation = f.document.Generation();
  const auto prepared = *f.document.PrepareSave();
  const std::array selected{f.entity};
  Require(f.document.Select(selected), "Selection failed");
  Require(!Publisher::Prepare(files, *f.review, f.request, false),
          "Unauthorized preparation accepted");
  auto unresolved = f.request;
  unresolved.choices.clear();
  Require(!Publisher::Prepare(files, *f.review, unresolved, true), "Unresolved candidate accepted");
  if (cryptography::ActiveProvider() == cryptography::Provider::Unavailable) {
    std::string error;
    Require(!Publisher::Prepare(files, *f.review, f.request, true, &error) && !error.empty() &&
                Read(f.scratch.root / f.relative) == f.disk &&
                !std::filesystem::exists(f.scratch.root / ".nexora/scene-merge-history"),
            "Missing cryptography backend accepted publication or touched sources");
    std::cout << "Cryptography unavailable: explicit rejection branch verified; positive "
                 "publication unavailable\n";
    return;
  }
  editor::detail::SceneMergeTestAccess::FailNextAfterLoad();
  bool allocation_failed{};
  try {
    static_cast<void>(prepare());
  } catch (const std::bad_alloc &) {
    allocation_failed = true;
  }
  Require(allocation_failed && Read(f.scratch.root / f.relative) == f.disk &&
              !std::filesystem::exists(f.scratch.root / ".nexora/scene-merge-history") &&
              f.document.MatchesPreparedSave(prepared),
          "Allocation failure changed source, authoring data or archive");
  auto p = prepare();
  Require(p && !p->Consumed() && p->Source() == f.relative &&
              Read(f.scratch.root / f.relative) == f.disk,
          "Preparation mutated source");
  auto copied = *p;
  core::CancellationSource cancelled;
  cancelled.Cancel();
  Require(Publisher::Publish(files, *p, true, cancelled.Token()).status == Status::Cancelled &&
              copied.Consumed() &&
              Publisher::Publish(files, copied, true).status == Status::Rejected &&
              !std::filesystem::exists(f.scratch.root / ".nexora/scene-merge-history"),
          "Cancel consumed authority incorrectly or wrote files");
  p = prepare();
  Require(p.has_value(), "Reprepare failed");
  const auto old_time = std::filesystem::last_write_time(f.scratch.root / f.relative);
  Write(f.scratch.root / f.relative, f.base);
  std::filesystem::last_write_time(f.scratch.root / f.relative, old_time);
  Require(Publisher::Publish(files, *p, true).status == Status::Rejected &&
              Read(f.scratch.root / f.relative) == f.base,
          "Same-timestamp stale disk accepted");
  Write(f.scratch.root / f.relative, f.disk);
  p = prepare();
  Require(p.has_value(), "Prepare after disk restoration failed");
  Require(f.document.Rename(*f.document.Key(f.entity), "New local") &&
              Publisher::Publish(files, *p, true).status == Status::Rejected && f.document.Undo(),
          "Stale same-generation local accepted");
  p = prepare();
  Require(p.has_value(), "Prepare after Undo failed");
  Require(Publisher::Publish(files, *p, false).status == Status::Rejected &&
              Read(f.scratch.root / f.relative) == f.disk,
          "Revoked owner accepted");
  const auto recovery_path = f.scratch.root / ".nexora/workspace.recovery";
  Write(recovery_path, "unresolved recovery");
  Require(!prepare() && Read(recovery_path) == "unresolved recovery" &&
              Read(f.scratch.root / f.relative) == f.disk,
          "Recovery policy ignored or recovery source modified");
  std::filesystem::remove(recovery_path);
  std::filesystem::remove(f.scratch.root / f.relative);
  Require(!prepare(), "Missing remote source accepted as file creation");
  Write(f.scratch.root / f.relative, "oversized");
  std::filesystem::resize_file(f.scratch.root / f.relative,
                               editor::SceneComparison::kMaximumSourceBytes + 1);
  Require(!prepare() && std::filesystem::file_size(f.scratch.root / f.relative) ==
                            editor::SceneComparison::kMaximumSourceBytes + 1,
          "Oversized disk accepted or truncated");
  Write(f.scratch.root / f.relative, f.disk);
  p = prepare();
  Require(p.has_value(), "Prepare for occupied archive failed");
  const auto archive_path = f.scratch.root / p->Archive();
  std::filesystem::create_directories(archive_path);
  Write(archive_path / "manifest.bin", "foreign occupied manifest");
  Require(Publisher::Publish(files, *p, true).status == Status::Rejected &&
              Read(archive_path / "manifest.bin") == "foreign occupied manifest" &&
              Read(f.scratch.root / f.relative) == f.disk,
          "Foreign archive overwritten");
  std::filesystem::remove_all(archive_path);
  p = prepare();
  Require(p.has_value(), "Prepare for occupied temporary failed");
  const auto temporary = (f.scratch.root / f.relative).parent_path() /
                         (".nexora-merge-" + p->Archive().filename().string() + ".tmp");
  Write(temporary, "occupied temporary");
  Require(Publisher::Publish(files, *p, true).status == Status::Rejected &&
              Read(temporary) == "occupied temporary" &&
              Read(f.scratch.root / f.relative) == f.disk,
          "Existing temporary truncated");
  Require(Publisher::Inspect(f.workspace, p->Archive()).has_value(),
          "Pre-commit exact archive missing");
  std::filesystem::remove(temporary);
  // Pause after actual archive and temporary verification, then change the source or cancel.
  for (const bool change_disk : {true, false}) {
    p = prepare();
    Require(p.has_value(), "Pre-commit gate preparation failed");
    auto gate = std::make_shared<editor::SceneMergePublicationGate>();
    editor::SceneMergePublicationTestAccess::Gate(*p, gate);
    core::CancellationSource late_cancel;
    std::exception_ptr writer_error;
    std::thread observer([&] {
      gate->entered.wait(false, std::memory_order_acquire);
      try {
        if (change_disk)
          Write(f.scratch.root / f.relative, f.base);
        else
          late_cancel.Cancel();
      } catch (...) {
        writer_error = std::current_exception();
      }
      gate->release.store(true, std::memory_order_release);
      gate->release.notify_all();
    });
    const auto result = Publisher::Publish(files, *p, true, late_cancel.Token());
    observer.join();
    if (writer_error)
      std::rethrow_exception(writer_error);
    Require(result.status == (change_disk ? Status::Rejected : Status::Cancelled) &&
                Read(f.scratch.root / f.relative) == (change_disk ? f.base : f.disk),
            "Last-moment disk change or cancellation crossed native commit");
    std::filesystem::remove(temporary);
    Write(f.scratch.root / f.relative, f.disk);
  }
  p = prepare();
  Require(p.has_value(), "Session replacement preparation failed");
  editor::SceneFileSession replacement(f.workspace, f.document);
  Require(replacement.BindCurrent(f.relative) &&
              Publisher::Publish(replacement, *p, true).status == Status::Rejected,
          "Same-path replacement session accepted old authority");
  p = prepare();
  Require(p.has_value(), "Final preparation failed");
  const std::string candidate(p->CandidateBytes());
  auto published = Publisher::Publish(files, *p, true);
  Require(published.status == Status::Published && published.Published() && p->Consumed() &&
              Read(f.scratch.root / f.relative) == candidate,
          "Validated candidate not published");
  Require(f.document.MatchesPreparedSave(prepared) && f.document.Generation() == generation &&
              Bytes(f.document) == f.local &&
              std::ranges::equal(f.document.Selection(), selected) && f.document.Redo() &&
              f.document.Name(f.entity) == "New local" && f.document.Undo() &&
              Bytes(f.document) == f.local,
          "File publication changed authoring state, generation, selection or Redo");
  Require(files.Save(files.Token()).status == editor::SceneFileStatus::NeedsOverwrite,
          "File-only publication silently advanced the live baseline");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopen"));
  Require(reopened.ReloadBytes(candidate) && reopened.Name(f.entity) == "Local" &&
              reopened.Transform(f.entity)->z == 7 &&
              reopened.OpaqueComponents(*reopened.Key(f.entity))->front().data ==
                  std::vector<std::uint8_t>({0, 255, 42}),
          "Published file lost chosen fields, remote pose or unknown bytes");
  const auto inspected = Publisher::Inspect(f.workspace, published.archive);
  Require(inspected && inspected->base == f.base && inspected->local == f.local &&
              inspected->disk == f.disk && inspected->candidate == candidate &&
              inspected->project == f.workspace.Project().id && inspected->source == f.relative,
          "Archive lost raw provenance");
  editor::ProjectWorkspace reader;
  Require(reader.Open(f.scratch.root, editor::ProjectAccess::ReadOnly),
          "Read-only project open failed");
  Require(Publisher::Inspect(reader, published.archive) == inspected,
          "Read-only inspection failed");
  editor::SceneFileSession read_files(reader, reopened);
  Require(read_files.BindCurrent(f.relative), "Read-only bind failed");
  auto read_review = editor::SceneMerge::Prepare(candidate, candidate, candidate);
  Require(read_review.has_value(), "Reader review failed");
  Require(!Publisher::Prepare(read_files, *read_review, {1, read_review->Identity(), {}}, true),
          "Read-only writer accepted");
  Require(!Publisher::Inspect(reader, "../escape") &&
              !Publisher::Inspect(reader, published.archive / ".."),
          "Unsafe inspection path accepted");
  Write(archive_path / "disk.scene", "corrupt disk payload");
  Require(!Publisher::Inspect(reader, published.archive) &&
              Read(f.scratch.root / f.relative) == candidate,
          "Corrupt archive accepted or changed source");
  Write(archive_path / "disk.scene", f.disk);
  const auto original_manifest = Read(archive_path / "manifest.bin");
  auto future = original_manifest.substr(0, original_manifest.size() - 64);
  future[8] = 2;
  future += Hash(future);
  const auto future_path = archive_path.parent_path() / Hash(future);
  std::filesystem::create_directory(future_path);
  Write(future_path / "manifest.bin", future);
  Require(!Publisher::Inspect(reader, future_path.lexically_relative(f.scratch.root)) &&
              Read(future_path / "manifest.bin") == future,
          "Unsupported version changed or accepted");
  auto truncated = original_manifest.substr(0, 25);
  Write(archive_path / "manifest.bin", truncated);
  Require(!Publisher::Inspect(reader, published.archive) &&
              Read(archive_path / "manifest.bin") == truncated,
          "Truncated manifest accepted or repaired silently");
  Write(archive_path / "manifest.bin", original_manifest);
  // Hard links and symlinks must never redirect a write or archive read.
  Write(f.scratch.root / f.relative, f.disk);
  Require(prepare().has_value(), "Plain source was not eligible before alias rejection");
  const auto alias = f.scratch.root / "Content/Alias.scene";
  std::error_code error;
  std::filesystem::create_hard_link(f.scratch.root / f.relative, alias, error);
  if (!error) {
    Require(!Publisher::Prepare(files, *f.review, f.request, true), "Hard-linked source accepted");
    std::filesystem::remove(alias);
  }
  p = prepare();
  Require(p.has_value(), "Parent alias preparation failed");
  const auto moved_parent = f.scratch.root / "MovedContent";
  std::filesystem::rename(f.scratch.root / "Content", moved_parent);
  error.clear();
  std::filesystem::create_directory_symlink(moved_parent, f.scratch.root / "Content", error);
  if (!error) {
    Require(Publisher::Publish(files, *p, true).status == Status::Rejected &&
                Read(moved_parent / "Authoring.scene") == f.disk,
            "Aliased parent accepted or redirected publication");
    std::filesystem::remove(f.scratch.root / "Content");
  }
  std::filesystem::rename(moved_parent, f.scratch.root / "Content");
  Write(f.scratch.root / f.relative, candidate);
  std::filesystem::create_symlink(archive_path / "disk.scene", archive_path / "link.scene", error);
  if (!error) {
    std::filesystem::remove(archive_path / "local.scene");
    std::filesystem::rename(archive_path / "link.scene", archive_path / "local.scene");
    Require(!Publisher::Inspect(reader, published.archive), "Symlink archive payload accepted");
  }
  Require(Read(f.scratch.root / f.relative) == candidate &&
              f.document.MatchesPreparedSave(prepared),
          "Archive rejection changed last-good or local document");
  Write(f.scratch.root / f.relative, f.disk);
  p = prepare();
  Require(p.has_value(), "Root replacement preparation failed");
  Scratch other_project;
  {
    editor::ProjectWorkspace creator;
    Require(creator.Create(other_project.root, "Other project"), "Other project create failed");
  }
  Require(f.workspace.Open(other_project.root) &&
              Publisher::Publish(files, *p, true).status == Status::Rejected &&
              Read(f.scratch.root / f.relative) == f.disk &&
              f.document.MatchesPreparedSave(prepared),
          "Replaced root/project accepted old publication authority");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Scene merge file publication passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
