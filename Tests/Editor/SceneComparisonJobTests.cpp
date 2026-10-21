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
  {
    editor::ProjectWorkspace workspace;
    Require(workspace.Create(root, "Compare"), "Project creation failed");
    runtime::World world;
    const auto scene = world.LoadScene("Compare");
    editor::SceneDocument document(world, scene);
    editor::SceneFileSession files(workspace, document);
    const auto entity = document.Create("Base");
    const editor::OpaqueComponent opaque{501, "Unavailable.Provider", {0x14, 0, 0xff}};
    Require(entity && document.SetOpaqueComponent(*document.Key(entity), opaque) &&
                files.SaveAs(files.Token(), "Content/Primary.scene").Applied(),
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
                Read(path) == disk && document.Undo() && Bytes(document) == base &&
                document.Redo() && Bytes(document) == local,
            "Read-only worker changed source/history or lost conflicting revisions");
    Require(result.review && result.review->Identity() &&
                result.result.get() == &result.review->Comparison() &&
                result.review->BaseBytes() == base && result.review->LocalBytes() == local &&
                result.review->RemoteBytes() == disk,
            "Ready result did not retain exact owning raw sources with an aliased comparison");
    const auto resolve = [&](editor::SceneMergeChoice side, std::string_view expected_name) {
      editor::SceneMergeRequest request;
      request.review = result.review->Identity();
      for (std::size_t i = 0; i < result.result->rows.size(); ++i)
        request.choices.push_back({i, side});
      const auto candidate = editor::SceneMerge::Resolve(*result.review, request);
      runtime::World candidate_world;
      editor::SceneDocument merged(candidate_world, candidate_world.LoadScene("Candidate"));
      Require(candidate && candidate->bytes && merged.ReloadBytes(*candidate->bytes) &&
                  merged.Name(entity) == expected_name && merged.Nodes().size() == 1 &&
                  merged.OpaqueComponents(*merged.Key(entity)) ==
                      std::optional<std::vector<editor::OpaqueComponent>>({opaque}),
              "Captured asynchronous review lost an explicitly selected name or opaque bytes");
    };
    resolve(editor::SceneMergeChoice::Local, "Local");
    resolve(editor::SceneMergeChoice::Remote, "Disk");
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
                !job.Snapshot().review && result.result->conflicts == 1,
            "Cancellation exposed output or invalidated owned prior result");
    blocked([&] {
      Require(job.Start(files) && document.Rename(key, "Changed after capture"),
              "Stale content fixture failed");
    });
    Drain(job, files);
    Require(job.Snapshot().phase == Phase::Stale && !job.Snapshot().result && Read(path) == disk,
            "Changed local content accepted stale publication");
    Require(!job.Snapshot().review, "Stale local source exposed a retained merge review");
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
                !job.Snapshot().review && Read(path) == "corrupt",
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
                job.Snapshot().review && job.Snapshot().review->BaseBytes() == disk &&
                job.Snapshot().review->LocalBytes() == disk &&
                job.Snapshot().review->RemoteBytes() == disk && Read(path) == disk,
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
                job.Snapshot().review && !job.Snapshot().review->RemoteBytes() &&
                !std::filesystem::exists(path),
            "Disk deletion was conflated with empty/corrupt source");
    editor::SceneComparisonSnapshot after_owner;
    {
      editor::SceneComparisonJob temporary_owner(jobs);
      Require(temporary_owner.Start(reader_files), "Temporary owner did not start");
      Drain(temporary_owner, reader_files);
      after_owner = temporary_owner.Snapshot();
    }
    editor::SceneComparisonJob draining_owner(jobs);
    std::promise<void> shutdown_entered;
    std::future<void> shutdown;
    bool waited_for_worker{};
    blocked([&] {
      Require(draining_owner.Start(reader_files), "Queued teardown fixture did not start");
      shutdown = std::async(std::launch::async, [&] {
        shutdown_entered.set_value();
        draining_owner.Shutdown();
      });
      shutdown_entered.get_future().wait();
      waited_for_worker =
          shutdown.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
    }); // Only now release the actual sole worker so Shutdown can finish draining.
    shutdown.get();
    Require(waited_for_worker && draining_owner.Snapshot().phase == Phase::Cancelled &&
                !draining_owner.Snapshot().result && !draining_owner.Snapshot().review,
            "Shutdown abandoned queued work or exposed cancelled review data");
    job.Shutdown();
    Require(job.Snapshot().phase == Phase::Cancelled && !job.Snapshot().result &&
                !job.Snapshot().review,
            "Stopped owner continued to expose a ready comparison/review");
    Require(!job.Start(reader_files), "Stopped owner accepted new work");
    jobs.Stop();
    // The old immutable review owns all sources independently of newer jobs, disk and local state.
    resolve(editor::SceneMergeChoice::Local, "Local");
    resolve(editor::SceneMergeChoice::Remote, "Disk");
    Require(after_owner.review && after_owner.result && after_owner.review->BaseBytes() == disk &&
                after_owner.review->LocalBytes() == disk && !after_owner.review->RemoteBytes(),
            "Destroyed worker owner invalidated retained source snapshots");
    editor::SceneMergeRequest retained_request;
    retained_request.review = after_owner.review->Identity();
    for (std::size_t i = 0; i < after_owner.result->rows.size(); ++i)
      retained_request.choices.push_back({i, editor::SceneMergeChoice::Local});
    const auto retained_candidate =
        editor::SceneMerge::Resolve(*after_owner.review, retained_request);
    runtime::World retained_world;
    editor::SceneDocument retained(retained_world, retained_world.LoadScene("Retained candidate"));
    // Canonical migration can add the existing default Euler record; raw source stays exact above.
    Require(retained_candidate && retained_candidate->bytes &&
                retained.ReloadBytes(*retained_candidate->bytes) && retained.Nodes().size() == 1 &&
                retained.Name(entity) == "Disk" &&
                retained.OpaqueComponents(*retained.Key(entity)) ==
                    std::optional<std::vector<editor::OpaqueComponent>>({opaque}),
            "Destroyed owner review lost selected local name or exact unknown payload");
    Require(!std::filesystem::exists(path), "Data-only resolution recreated the deleted source");
  } // Release the actual Windows writer lease before deleting fixture data.
  std::filesystem::remove_all(root);
  Require(!std::filesystem::exists(root), "Fixture cleanup retained project data");
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
