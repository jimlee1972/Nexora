#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/SceneMergeCandidateJob.h"
#include "SceneMergeTestAccess.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Phase = editor::SceneMergeCandidatePhase;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Bytes(const editor::SceneDocument &document) { return document.PrepareSave()->Bytes(); }
void Drain(editor::SceneMergeCandidateJob &job, std::uint64_t identity) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!job.Poll(identity)) {
    Require(std::chrono::steady_clock::now() < deadline, "Candidate worker did not drain");
    std::this_thread::yield();
  }
}
void Run() {
  runtime::World world;
  editor::SceneDocument document(world, world.LoadScene("Captured candidate"));
  const auto entity = document.Create("Base");
  const editor::OpaqueComponent unknown{701, "Unavailable.Provider", {0, 255, 17}};
  Require(entity && document.SetOpaqueComponent(*document.Key(entity), unknown), "Fixture failed");
  auto base = Bytes(document);
  Require(document.Rename(*document.Key(entity), "Local"), "Local fixture failed");
  auto local = Bytes(document);
  runtime::World disk_world;
  editor::SceneDocument disk(disk_world, disk_world.LoadScene("Disk"));
  Require(disk.ReloadBytes(base) && disk.Rename(*disk.Key(entity), "Disk"), "Disk fixture failed");
  auto pose = *disk.Transform(entity);
  pose.z = 7;
  Require(disk.SetTransform(entity, pose), "Independent disk pose failed");
  auto remote = Bytes(disk);
  const auto original_base = base, original_local = local, original_remote = remote;
  const auto baseline = document.PrepareSave();
  auto review = editor::SceneMerge::Prepare(base, local, remote);
  Require(review && review->Comparison().conflicts == 1, "Review fixture failed");
  const auto identity = review->Identity();
  const auto &rows = review->Comparison().rows;
  const auto found = std::ranges::find(rows, "entities/" + std::to_string(entity) + "/name",
                                       &editor::SceneComparisonRow::stable_path);
  Require(found != rows.end(), "Name conflict missing");
  editor::SceneMergeRequest request{
      1,
      identity,
      {{static_cast<std::size_t>(found - rows.begin()), editor::SceneMergeChoice::Local}}};
  core::JobSystem jobs(1);
  jobs.Start();
  const auto blocked = [&](const auto &action) {
    std::promise<void> entered, release;
    auto released = release.get_future().share();
    const auto gate = jobs.Submit({[&](const core::CancellationToken &) {
                                     entered.set_value();
                                     released.wait();
                                   },
                                   core::JobPriority::Normal,
                                   {},
                                   "candidate worker gate"});
    entered.get_future().wait();
    try {
      action();
    } catch (...) {
      release.set_value();
      jobs.Wait(gate);
      throw;
    }
    release.set_value();
    jobs.Wait(gate);
  };
  editor::SceneMergeCandidateJob job(jobs);
  blocked([&] {
    Require(job.Start(*review, request) && job.Busy() && !job.Start(*review, request),
            "Candidate replacement admitted while busy");
    base = local = remote = "caller buffers replaced";
    review.reset();
    request.choices.clear();
  });
  Drain(job, identity);
  const auto ready = job.Snapshot();
  Require(ready.phase == Phase::Ready && ready.review == identity && ready.operation &&
              ready.result && ready.result->bytes,
          "Owning candidate did not publish");
  runtime::World candidate_world;
  editor::SceneDocument candidate(candidate_world, candidate_world.LoadScene("Reload"));
  Require(candidate.ReloadBytes(*ready.result->bytes) && candidate.Name(entity) == "Local" &&
              candidate.Transform(entity)->z == 7 && candidate.Nodes().size() == 1 &&
              candidate.OpaqueComponents(*candidate.Key(entity)) ==
                  std::optional<std::vector<editor::OpaqueComponent>>({unknown}) &&
              document.MatchesPreparedSave(*baseline) && Bytes(document) == original_local &&
              Bytes(disk) == original_remote,
          "Async selected candidate lost literal values or changed source/history");
  review = editor::SceneMerge::Prepare(original_base, original_local, original_remote);
  request = {1, review->Identity(), {}};
  for (std::size_t i = 0; i < review->Comparison().rows.size(); ++i)
    if (review->Comparison().rows[i].choice == editor::SceneComparisonChoice::Unresolved)
      request.choices.push_back({i, editor::SceneMergeChoice::Local});
  auto invalid = request;
  invalid.schema = 2;
  Require(!job.Start(*review, invalid) && !job.Snapshot().result, "Unknown schema retained output");
  invalid = request;
  ++invalid.review;
  Require(!job.Start(*review, invalid), "Stale review identity admitted");
  invalid = request;
  invalid.choices.push_back(invalid.choices.front());
  Require(job.Start(*review, invalid), "Bounded invalid worker fixture rejected before submission");
  Drain(job, review->Identity());
  Require(job.Snapshot().phase == Phase::Failed && !job.Snapshot().result,
          "Duplicate choices produced a candidate");
  invalid = request;
  invalid.choices.front().choice = static_cast<editor::SceneMergeChoice>(255);
  Require(job.Start(*review, invalid), "Unknown choice worker fixture did not start");
  Drain(job, review->Identity());
  Require(job.Snapshot().phase == Phase::Failed && !job.Snapshot().result,
          "Unknown choice produced a candidate");
  blocked([&] {
    Require(job.Start(*review, request) && job.Cancel() && !job.Start(*review, request),
            "Queued cancellation contract failed");
  });
  Drain(job, review->Identity());
  Require(job.Snapshot().phase == Phase::Cancelled && !job.Snapshot().result,
          "Cancelled worker published output");
  Require(job.Start(*review, request), "Stale publication fixture failed");
  Drain(job, 0);
  Require(job.Snapshot().phase == Phase::Stale && !job.Snapshot().result,
          "Changed current review published a candidate");
  const auto inject = jobs.Submit({[](const core::CancellationToken &) {
                                     editor::detail::SceneMergeTestAccess::FailNextAfterLoad();
                                   },
                                   core::JobPriority::Normal,
                                   {},
                                   "candidate allocation fault"});
  jobs.Wait(inject);
  Require(job.Start(*review, request), "Allocation fault fixture failed");
  Drain(job, review->Identity());
  Require(job.Snapshot().phase == Phase::Failed && !job.Snapshot().result &&
              !job.Snapshot().message.empty() && document.MatchesPreparedSave(*baseline),
          "Worker allocation failure exposed output or changed source");
  Require(job.Start(*review, request), "Post-failure retry failed");
  Drain(job, review->Identity());
  Require(job.Snapshot().phase == Phase::Ready && job.Snapshot().result,
          "Failed worker did not recover");
  auto absent = editor::SceneMerge::Prepare({}, {}, {});
  Require(absent && job.Start(*absent, {1, absent->Identity(), {}}), "Absent fixture failed");
  Drain(job, absent->Identity());
  Require(job.Snapshot().phase == Phase::Ready && job.Snapshot().result &&
              !job.Snapshot().result->bytes,
          "Absent candidate was confused with failure");
  std::promise<void> shutdown_entered;
  std::future<void> shutdown;
  bool waited{};
  blocked([&] {
    Require(job.Start(*review, request), "Queued shutdown fixture failed");
    shutdown = std::async(std::launch::async, [&] {
      shutdown_entered.set_value();
      job.Shutdown();
    });
    shutdown_entered.get_future().wait();
    waited = shutdown.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
  });
  shutdown.get();
  Require(waited && job.Snapshot().phase == Phase::Cancelled && !job.Snapshot().result &&
              !job.Start(*review, request),
          "Shutdown failed to drain/revoke output");
  jobs.Stop();
  Require(ready.result->bytes && document.MatchesPreparedSave(*baseline) &&
              Bytes(document) == original_local && Bytes(disk) == original_remote &&
              document.Undo() && document.Redo() && document.MatchesPreparedSave(*baseline),
          "Retained candidate/authoring history was lost after teardown");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Owning asynchronous scene candidate passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
