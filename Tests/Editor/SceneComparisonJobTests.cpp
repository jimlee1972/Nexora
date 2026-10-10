#include "Nexora/Editor/AdditiveSceneSession.h"
#include "Nexora/Editor/SceneComparisonJob.h"

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Phase = editor::SceneComparisonPhase;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Bytes(const editor::SceneDocument &document) { return document.PrepareSave()->Bytes(); }
std::string Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
void Write(const std::filesystem::path &path, const std::string &bytes) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << bytes;
  Require(static_cast<bool>(stream), "Fixture source write failed");
}
void Drain(editor::SceneComparisonJob &job, const editor::SceneFileSession &files) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!job.Poll(files)) {
    Require(std::chrono::steady_clock::now() < deadline, "Comparison did not finish");
    std::this_thread::yield();
  }
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-comparison-job-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Compare"), "Project creation failed");
  runtime::World world;
  const auto scene = world.LoadScene("Compare");
  editor::SceneDocument document(world, scene);
  editor::SceneFileSession files(workspace, document);
  const auto entity = document.Create("Base");
  Require(entity && files.SaveAs(files.Token(), "Content/Primary.scene").Applied(),
          "Base save failed");
  const auto path = root / "Content/Primary.scene";
  const auto base = Read(path);
  const auto key = *document.Key(entity);
  Require(document.Rename(key, "Local"), "Local rename failed");
  const auto local = Bytes(document);
  runtime::World remote_world;
  const auto remote_scene = remote_world.LoadScene("Remote");
  editor::SceneDocument remote(remote_world, remote_scene);
  Require(remote.ReloadBytes(base) && remote.Rename(*remote.Key(entity), "Disk"),
          "Remote fixture failed");
  const auto disk = Bytes(remote);
  Write(path, disk);
  core::JobSystem jobs(1);
  jobs.Start();
  editor::SceneComparisonJob job(jobs);
  Require(job.Start(files), "Owning comparison did not start");
  Require(!job.Start(files), "Busy comparison accepted replacement");
  Drain(job, files);
  const auto result = job.Snapshot();
  Require(result.phase == Phase::Ready && result.result && result.result->conflicts == 1 &&
              result.path == "Content/Primary.scene" && Bytes(document) == local &&
              Read(path) == disk && document.Undo() && Bytes(document) == base && document.Redo() &&
              Bytes(document) == local,
          "Read-only worker changed source/history or lost conflicting revisions");
  // Hold the sole real worker to test cancellation/stale publication without timing races.
  const auto blocked = [&](const auto &action) {
    std::promise<void> entered, release;
    auto released = release.get_future().share();
    const auto handle = jobs.Submit({[&](const core::CancellationToken &) {
                                       entered.set_value();
                                       released.wait();
                                     },
                                     core::JobPriority::Normal,
                                     {},
                                     "comparison blocker"});
    entered.get_future().wait();
    try {
      action();
    } catch (...) {
      release.set_value();
      jobs.Wait(handle);
      throw;
    }
    release.set_value();
    jobs.Wait(handle);
  };
  blocked([&] {
    Require(job.Start(files) && job.Cancel() && !job.Start(files),
            "Queued cancellation contract failed");
  });
  Drain(job, files);
  Require(job.Snapshot().phase == Phase::Cancelled && !job.Snapshot().result &&
              result.result->conflicts == 1,
          "Cancellation exposed output or invalidated owned prior result");
  blocked([&] {
    Require(job.Start(files) && document.Rename(key, "Changed after capture"),
            "Stale content fixture failed");
  });
  Drain(job, files);
  Require(job.Snapshot().phase == Phase::Stale && !job.Snapshot().result && Read(path) == disk,
          "Changed local content accepted stale publication");
  blocked([&] {
    Require(job.Start(files) && files.New(files.Token(), true).Applied(),
            "Stale generation fixture failed");
  });
  Drain(job, files);
  Require(job.Snapshot().phase == Phase::Stale && !job.Snapshot().result && Read(path) == disk,
          "Replaced document accepted stale publication");
  Require(files.Open(files.Token(), "Content/Primary.scene", true).Applied(),
          "Reader baseline refresh failed");
  Require(document.Rename(*document.Key(entity), "Baseline advanced"), "Baseline fixture failed");
  const auto saved_local = Bytes(document);
  blocked([&] {
    Require(job.Start(files) && files.Save(files.Token()).Applied(),
            "Baseline advancement fixture failed");
  });
  Drain(job, files);
  Require(job.Snapshot().phase == Phase::Stale && !job.Snapshot().result &&
              Bytes(document) == saved_local && Read(path) == saved_local,
          "Unchanged content with an advanced saved baseline accepted an old comparison");
  editor::SceneFileSession replacement_files(workspace, document);
  Require(replacement_files.BindCurrent("Content/Primary.scene"),
          "Replacement session fixture failed");
  blocked([&] { Require(job.Start(files), "Replacement session comparison did not start"); });
  Drain(job, replacement_files);
  Require(job.Snapshot().phase == Phase::Stale && !job.Snapshot().result &&
              Bytes(document) == saved_local && Read(path) == saved_local,
          "A different same-token/path file session accepted old output");
  Write(path, "corrupt");
  Require(job.Start(files), "Corrupt disk intake failed");
  Drain(job, files);
  Require(job.Snapshot().phase == Phase::Failed && !job.Snapshot().result &&
              Read(path) == "corrupt",
          "Corrupt disk produced a partial comparison or changed source");
  Write(path, std::string(editor::SceneComparison::kMaximumSourceBytes + 1, 'x'));
  Require(job.Start(files), "Oversized disk intake failed");
  Drain(job, files);
  Require(job.Snapshot().phase == Phase::Failed && !job.Snapshot().result,
          "Oversized disk produced a comparison");
  Write(path, disk);
  editor::ProjectWorkspace reader;
  Require(reader.Open(root, editor::ProjectAccess::ReadOnly), "Read-only project open failed");
  runtime::World reader_world;
  editor::SceneDocument reader_document(reader_world, reader_world.LoadScene("Reader"));
  editor::SceneFileSession reader_files(reader, reader_document);
  Require(reader_files.Open(reader_files.Token(), "Content/Primary.scene").Applied() &&
              job.Start(reader_files),
          "Read-only inspection was blocked");
  Drain(job, reader_files);
  Require(job.Snapshot().phase == Phase::Ready && job.Snapshot().result->rows.empty() &&
              Read(path) == disk,
          "Read-only inspection changed source");
  {
    runtime::World reference_world;
    const auto reserved = reference_world.LoadScene("Reserved identities");
    for (int i = 0; i < 32; ++i)
      static_cast<void>(reference_world.CreateEntity(reserved));
    editor::SceneDocument reference_document(reference_world,
                                             reference_world.LoadScene("Reference"));
    Require(reference_document.CreateCamera("Reference camera") &&
                reference_document.Save(root / "Content/Reference.scene"),
            "Actual reference fixture failed");
    const auto reference_source = Read(root / "Content/Reference.scene");
    editor::AdditiveSceneSession documents(workspace, world);
    Require(documents.Attach(document, files).has_value(), "Borrowed primary attachment failed");
    const auto reference = documents.Open("Content/Reference.scene", false);
    Require(reference && !documents.EditableDocument(*reference) &&
                !documents.WritableFiles(*reference) && documents.Files(*reference) &&
                job.Start(*documents.Files(*reference)),
            "Const-only actual reference could not be inspected");
    const auto before_reference = Bytes(*documents.Document(*reference));
    Drain(job, *documents.Files(*reference));
    Require(job.Snapshot().phase == Phase::Ready && job.Snapshot().result->rows.empty() &&
                Bytes(*documents.Document(*reference)) == before_reference &&
                Read(root / "Content/Reference.scene") == reference_source,
            "Actual reference comparison mutated its document/source");
  }
  std::filesystem::remove(path);
  Require(job.Start(reader_files), "Deleted disk inspection was blocked");
  Drain(job, reader_files);
  Require(job.Snapshot().phase == Phase::Ready && !job.Snapshot().result->rows.empty() &&
              !std::filesystem::exists(path),
          "Disk deletion was conflated with empty/corrupt source");
  job.Shutdown();
  Require(!job.Start(reader_files), "Stopped owner accepted new work");
  jobs.Stop();
  std::filesystem::remove_all(root);
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Owning scene comparison job passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
