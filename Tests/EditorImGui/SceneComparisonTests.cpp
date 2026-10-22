#include "EditorImGuiTestAccess.h"
#include "Nexora/Cryptography/Signature.h"
#include "Nexora/Editor/SceneMergePublication.h"

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-semantic-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  runtime::World world;
  runtime::Id id = world.LoadScene("Comparison");
  editor::SceneDocument scene{world, id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::SceneComparisonSnapshot observation;
  std::shared_ptr<const editor::SceneMergeReview> merge_review;
  editor::SceneMergeCandidateSnapshot candidate;
  bool candidate_busy{}, publication_allowed{};
  std::uint64_t candidate_draft{};
  float scale;
  bool busy{};
  // File session must be constructed after the workspace opens.
  std::unique_ptr<editor::SceneFileSession> current;
  explicit Fixture(float dpi, bool unknown = false) : scale(dpi) {
    Require(writer.Create(root, "Semantic UI"), "Project creation failed");
    const auto entity = scene.Create("Base");
    auto bound = scene.PrepareSave()->Bytes();
    const std::string plain_header = "NEXORA_EDITOR_SCENE 2\n";
    const std::string bound_header = "NEXORA_EDITOR_SCENE 4\n";
    Require(bound.starts_with(plain_header), "Initial scene schema changed");
    bound.replace(0, plain_header.size(), bound_header);
    bound.insert(bound.find("world\n"), "prefab-placement 10000000-0000-0000-0000-000000000001 "
                                        "20000000-0000-0000-0000-000000000001 7\n"
                                        "prefab-node 10000000-0000-0000-0000-000000000001 0 "
                                        "30000000-0000-0000-0000-000000000001 " +
                                            std::to_string(entity) + '\n');
    Require(scene.ReloadBytes(bound), "Persistent binding fixture failed");
    if (unknown)
      Require(
          scene.SetOpaqueComponent(*scene.Key(entity), {701, "Unavailable.Provider", {0, 255, 17}}),
          "Unknown fixture failed");
    current = std::make_unique<editor::SceneFileSession>(writer, scene);
    Require(entity && current->SaveAs(current->Token(), "Content/Main.scene").Applied(),
            "Base save failed");
    const auto base = scene.PrepareSave()->Bytes();
    Require(scene.Rename(*scene.Key(entity), "Local"), "Local fixture failed");
    const auto local = scene.PrepareSave()->Bytes();
    runtime::World remote_world;
    editor::SceneDocument remote(remote_world, remote_world.LoadScene("Remote"));
    Require(remote.ReloadBytes(base) && remote.Rename(*remote.Key(entity), "Disk"),
            "Remote fixture failed");
    auto disk = remote.PrepareSave()->Bytes();
    const auto binding = disk.find("prefab-placement ");
    Require(binding != disk.npos, "Remote binding was lost before the independent deletion");
    disk.erase(binding, disk.find("world\n", binding) - binding);
    disk.replace(0, bound_header.size(), unknown ? "NEXORA_EDITOR_SCENE 3\n" : plain_header);
    std::ofstream(root / "Content/Main.scene", std::ios::binary | std::ios::trunc) << disk;
    const auto comparison = editor::CompareSceneRevisions(base, local, disk);
    Require(comparison && comparison->conflicts == 1 && comparison->rows.size() == 7 &&
                std::ranges::count_if(comparison->rows,
                                      [](const auto &row) {
                                        return row.stable_path.starts_with("prefabs/") &&
                                               row.base && row.local && !row.remote &&
                                               row.choice == editor::SceneComparisonChoice::Remote;
                                      }) == 6,
            "Actual comparison fixture lost persistent binding deletion rows");
    observation = {1,
                   editor::SceneComparisonPhase::Ready,
                   current->Token(),
                   "Content/Main.scene",
                   "Captured comparison",
                   std::make_shared<const editor::SceneComparison>(*comparison)};
    auto prepared_review = editor::SceneMerge::Prepare(base, local, disk);
    Require(prepared_review.has_value(), "Owning UI review failed");
    merge_review = std::make_shared<const editor::SceneMergeReview>(std::move(*prepared_review));
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
  }
  ~Fixture() {
    current.reset();
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  void Draw() {
    ui.SetSceneFileContext(current->Token(), "Content/Main.scene");
    ui.SetSceneComparisonStatus(observation, busy);
    ui.SetSceneMergeCandidateStatus(candidate, candidate_busy, candidate_draft);
    ui.SetSceneMergePublicationAllowed(publication_allowed);
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Click(const std::optional<std::array<float, 2>> &point) {
    Require(point.has_value(), "Comparison control missing");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    Draw();
  }
};
void Run(float dpi) {
  Fixture f(dpi);
  const auto original = f.scene.PrepareSave();
  f.Click(Access::SceneFilePosition(f.ui, 0));
  f.Click(Access::SceneComparisonPosition(f.ui, 0));
  Require(f.ui.TakeSceneComparisonRequest() == f.current->Token() &&
              !f.ui.TakeSceneComparisonRequest(),
          "File menu comparison request lost scope/one-shot semantics");
  f.Draw();
  Require(Access::SceneComparisonRenderedRows(f.ui) == 7 &&
              Access::SceneComparisonStatus(f.ui).result->conflicts == 1 &&
              f.scene.MatchesPreparedSave(*original),
          "Actual semantic rows were not displayed read-only");
  const auto conflict = f.current->Save(f.current->Token());
  Require(conflict.status == editor::SceneFileStatus::NeedsOverwrite,
          "External-source conflict missing");
  editor::imgui::SceneFileRequest request{editor::imgui::SceneFileAction::SaveAs,
                                          f.current->Token(), "Content/Main.scene"};
  request.overwrite_token = conflict.overwrite_token;
  f.ui.RequestSceneOverwrite(request);
  f.Draw();
  f.Draw();
  f.Click(Access::SceneComparisonPosition(f.ui, 1));
  Require(f.ui.TakeSceneComparisonRequest() == f.current->Token(),
          "Overwrite compare action lost scope");
  f.Draw();
  Require(Access::SceneComparisonRenderedRows(f.ui) > 0 && !f.ui.TakeSceneFileRequest(),
          "Overwrite comparison emitted a destructive decision");
  f.busy = true;
  f.Draw();
  f.Click(Access::SceneComparisonPosition(f.ui, 2));
  Require(f.ui.TakeSceneComparisonCancelRequest() && !f.ui.TakeSceneComparisonCancelRequest(),
          "Comparison cancellation was not one-shot");
  f.busy = false;
  f.Draw();
  f.Click(Access::SceneComparisonPosition(f.ui, 3));
  f.Click(Access::SceneFilePosition(f.ui, 6));
  Require(!f.ui.TakeSceneFileRequest() && f.scene.MatchesPreparedSave(*original),
          "Comparison/back/cancel changed source intent or authoring content");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "Read-only project failed");
  f.active = &f.reader;
  f.Draw();
  f.Click(Access::SceneFilePosition(f.ui, 0));
  f.Click(Access::SceneComparisonPosition(f.ui, 0));
  Require(f.ui.TakeSceneComparisonRequest() == f.current->Token(),
          "Read-only comparison was blocked");
  f.Draw();
  auto unsafe = std::make_shared<editor::SceneComparison>();
  unsafe->rows.push_back(
      {"entities/1/name", {}, std::string(65537, 'x'), {}, editor::SceneComparisonChoice::Local});
  auto malformed = f.observation;
  malformed.result = unsafe;
  f.ui.SetSceneComparisonStatus(malformed, false);
  Require(Access::SceneComparisonStatus(f.ui).phase == editor::SceneComparisonPhase::Failed &&
              !Access::SceneComparisonStatus(f.ui).result,
          "Over-budget widget observation was retained");
  f.ui.SetSceneFileContext({f.writer.Project().id, f.scene.Generation() + 1}, "Content/Main.scene");
  f.ui.SetSceneComparisonStatus(f.observation, false);
  Require(!Access::SceneComparisonStatus(f.ui).result && !f.ui.TakeSceneComparisonRequest(),
          "Old document comparison resurrected after scope replacement");
}
void RunMerge(float dpi) {
  std::cout << "Merge controls dpi=" << dpi << std::endl;
  Fixture f(dpi, true);
  f.observation.review = f.merge_review;
  f.observation.result =
      std::shared_ptr<const editor::SceneComparison>(f.merge_review, &f.merge_review->Comparison());
  const auto source_entity = f.scene.Nodes().front().id;
  const std::array source_selection{*f.scene.Key(source_entity)};
  Require(f.scene.Select(source_selection), "Source selection fixture failed");
  const auto source_generation = f.scene.Generation();
  const auto original = f.scene.PrepareSave();
  const auto disk_bytes = [&] {
    std::ifstream input(f.root / "Content/Main.scene", std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  };
  const auto original_disk = disk_bytes();
  f.Click(Access::SceneFilePosition(f.ui, 0));
  f.Click(Access::SceneComparisonPosition(f.ui, 0));
  Require(f.ui.TakeSceneComparisonRequest().has_value(), "Merge inspection did not open");
  f.Draw();
  const auto &rows = f.merge_review->Comparison().rows;
  const auto conflict = std::ranges::find(rows, editor::SceneComparisonChoice::Unresolved,
                                          &editor::SceneComparisonRow::choice);
  Require(conflict != rows.end(), "Actual name conflict missing");
  const auto row = static_cast<std::size_t>(conflict - rows.begin());
  f.Click(Access::SceneMergePosition(f.ui, 0));
  Require(!f.ui.TakeSceneMergeCandidateRequest(), "Unresolved choices emitted a request");
  f.Click(Access::SceneMergeChoicePosition(f.ui, row, 1));
  f.Click(Access::SceneMergePosition(f.ui, 0));
  auto request = f.ui.TakeSceneMergeCandidateRequest();
  Require(request && request->token == f.current->Token() && request->draft &&
              request->comparison_operation == f.observation.operation &&
              request->candidate.review == f.merge_review->Identity() &&
              request->candidate.choices.size() == 1 &&
              request->candidate.choices.front().row == row &&
              request->candidate.choices.front().choice == editor::SceneMergeChoice::Local &&
              !f.ui.TakeSceneMergeCandidateRequest() && !f.ui.TakeSceneFileRequest(),
          "Actual merge choice widgets lost scoped owning one-shot semantics");
  core::JobSystem jobs(1);
  jobs.Start();
  editor::SceneMergeCandidateJob job(jobs);
  const auto drain = [&] {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (!job.Poll(f.merge_review->Identity())) {
      Require(std::chrono::steady_clock::now() < deadline, "UI candidate did not drain");
      std::this_thread::yield();
    }
  };
  Require(job.Start(*f.merge_review, request->candidate), "Actual UI request did not start");
  drain();
  f.candidate = job.Snapshot();
  f.candidate_draft = request->draft;
  f.Draw();
  Require(Access::SceneMergeStatus(f.ui).result && f.candidate.result->bytes,
          "Validated candidate was not presented");
  auto allocation_review = editor::SceneMerge::Prepare(
      f.merge_review->BaseBytes(), f.merge_review->LocalBytes(), f.merge_review->RemoteBytes());
  Require(allocation_review.has_value(), "Allocation review fixture failed");
  auto allocation_observation = f.observation;
  allocation_observation.review =
      std::make_shared<const editor::SceneMergeReview>(std::move(*allocation_review));
  allocation_observation.result = std::shared_ptr<const editor::SceneComparison>(
      allocation_observation.review, &allocation_observation.review->Comparison());
  ++allocation_observation.operation;
  Access::FailNextSceneMergeChoiceAllocation(f.ui);
  bool allocation_failed{};
  try {
    f.ui.SetSceneComparisonStatus(allocation_observation, false);
  } catch (const std::bad_alloc &) {
    allocation_failed = true;
  }
  Require(allocation_failed && Access::SceneComparisonStatus(f.ui).review == f.merge_review,
          "Choice allocation failure published replacement review identity");
  f.Draw();
  f.Click(Access::SceneMergePosition(f.ui, 0));
  const auto after_allocation = f.ui.TakeSceneMergeCandidateRequest();
  Require(after_allocation && after_allocation->candidate.review == request->candidate.review &&
              after_allocation->draft == request->draft &&
              after_allocation->candidate.choices.size() == 1 &&
              after_allocation->candidate.choices.front().choice == editor::SceneMergeChoice::Local,
          "Choice allocation failure lost prior choices or mixed old/new review state");
  runtime::World world;
  editor::SceneDocument merged(world, world.LoadScene("UI candidate"));
  const auto entity = f.scene.Nodes().front().id;
  Require(merged.ReloadBytes(*f.candidate.result->bytes) && merged.Name(entity) == "Local" &&
              merged.PrefabPlacements().empty() &&
              merged.OpaqueComponents(*merged.Key(entity)) ==
                  std::optional<std::vector<editor::OpaqueComponent>>(
                      {{701, "Unavailable.Provider", {0, 255, 17}}}) &&
              f.scene.MatchesPreparedSave(*original) && disk_bytes() == original_disk,
          "UI candidate lost literal selections/unknown data or changed source/history");
  f.Click(Access::SceneMergeChoicePosition(f.ui, row, 2));
  f.Draw();
  f.Draw();
  Require(!Access::SceneMergeStatus(f.ui).result,
          "Old successful response resurrected after a draft choice changed");
  f.Click(Access::SceneMergePosition(f.ui, 0));
  request = f.ui.TakeSceneMergeCandidateRequest();
  Require(request && request->draft != f.candidate_draft &&
              request->candidate.choices.front().choice == editor::SceneMergeChoice::Remote,
          "Changed draft did not own the selected disk choice");
  // Hold a real mouse-down across a new immutable review with identical row positions.
  const auto point = Access::SceneMergeChoicePosition(f.ui, row, 1);
  Require(point.has_value(), "Held choice position missing");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>((*point)[0] * dpi);
  pointer.value1 = static_cast<int>((*point)[1] * dpi);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer, button});
  f.Draw();
  auto replacement = editor::SceneMerge::Prepare(
      f.merge_review->BaseBytes(), f.merge_review->LocalBytes(), f.merge_review->RemoteBytes());
  Require(replacement.has_value(), "Replacement owning review failed");
  f.merge_review = std::make_shared<const editor::SceneMergeReview>(std::move(*replacement));
  f.observation.review = f.merge_review;
  f.observation.result =
      std::shared_ptr<const editor::SceneComparison>(f.merge_review, &f.merge_review->Comparison());
  ++f.observation.operation;
  f.Draw();
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  f.Click(Access::SceneMergePosition(f.ui, 0));
  Require(!f.ui.TakeSceneMergeCandidateRequest(), "Held release retargeted another review");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "Readonly merge fixture failed");
  f.active = &f.reader;
  f.Draw();
  f.Click(Access::SceneMergeChoicePosition(f.ui, row, 1));
  f.Click(Access::SceneMergePosition(f.ui, 0));
  request = f.ui.TakeSceneMergeCandidateRequest();
  Require(request && !f.reader.Writable() && f.ui.SceneMergeInteractionAllowed(),
          "Read-only captured-data candidate review was blocked");
  // Use the actual queued worker to exercise the graphical one-shot Cancel control.
  std::promise<void> entered, release;
  auto released = release.get_future().share();
  const auto gate = jobs.Submit({[&](const core::CancellationToken &) {
                                   entered.set_value();
                                   released.wait();
                                 },
                                 core::JobPriority::Normal,
                                 {},
                                 "UI candidate gate"});
  entered.get_future().wait();
  try {
    Require(job.Start(*f.merge_review, request->candidate), "Queued UI candidate failed");
    f.candidate = job.Snapshot();
    f.candidate_draft = request->draft;
    f.candidate_busy = true;
    f.Draw();
    f.Click(Access::SceneMergePosition(f.ui, 1));
    Require(f.ui.TakeSceneMergeCandidateCancelRequest() &&
                !f.ui.TakeSceneMergeCandidateCancelRequest() && job.Cancel(),
            "Candidate Cancel control lost one-shot semantics");
  } catch (...) {
    release.set_value();
    jobs.Wait(gate);
    throw;
  }
  release.set_value();
  jobs.Wait(gate);
  drain();
  f.candidate = job.Snapshot();
  f.candidate_busy = false;
  f.Draw();
  Require(!Access::SceneMergeStatus(f.ui).result, "Cancelled candidate UI retained output");
  auto detached = f.observation;
  detached.result = std::make_shared<const editor::SceneComparison>(*detached.result);
  f.ui.SetSceneComparisonStatus(detached, false);
  Require(!Access::SceneComparisonStatus(f.ui).review,
          "Unbound comparison rows acquired owning merge review authority");
  f.Draw();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  Require(!f.ui.SceneMergeInteractionAllowed() && !f.ui.TakeSceneMergeCandidateRequest() &&
              !Access::SceneMergeStatus(f.ui).result && f.scene.MatchesPreparedSave(*original) &&
              disk_bytes() == original_disk && !f.ui.TakeSceneFileRequest(),
          "Blocked UI retained candidate/authoring authority or changed exact sources");
  job.Shutdown();
  jobs.Stop();
  Require(f.scene.Generation() == source_generation && f.scene.Selection().size() == 1 &&
              f.scene.Selection().front() == source_entity && f.scene.Undo() &&
              f.scene.Name(source_entity) == "Base" && f.scene.Redo() &&
              f.scene.Name(source_entity) == "Local" && f.scene.MatchesPreparedSave(*original) &&
              disk_bytes() == original_disk,
          "Graphical candidate validation changed generation, selection or real Undo/Redo history");
}
void RunPublication(float dpi) {
  Fixture f(dpi, true);
  f.publication_allowed = true;
  f.observation.review = f.merge_review;
  f.observation.result =
      std::shared_ptr<const editor::SceneComparison>(f.merge_review, &f.merge_review->Comparison());
  const auto original = *f.scene.PrepareSave();
  const auto disk = std::string(*f.merge_review->RemoteBytes());
  const auto source = f.root / "Content/Main.scene";
  const auto read = [&] {
    std::ifstream input(source, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  };
  if (dpi == 2) {
    const auto conflict = f.current->Save(f.current->Token());
    Require(conflict.status == editor::SceneFileStatus::NeedsOverwrite && conflict.overwrite_token,
            "Overwrite-to-publication fixture did not require confirmation");
    editor::imgui::SceneFileRequest intent{editor::imgui::SceneFileAction::SaveAs,
                                           f.current->Token(), "Content/Main.scene"};
    intent.overwrite_token = conflict.overwrite_token;
    f.ui.RequestSceneOverwrite(intent);
    f.Draw();
    f.Draw();
    f.Click(Access::SceneComparisonPosition(f.ui, 1));
  } else {
    f.Click(Access::SceneFilePosition(f.ui, 0));
    f.Click(Access::SceneComparisonPosition(f.ui, 0));
  }
  Require(f.ui.TakeSceneComparisonRequest().has_value(), "Publication comparison did not open");
  f.Draw();
  const auto &rows = f.merge_review->Comparison().rows;
  const auto found = std::ranges::find(rows, editor::SceneComparisonChoice::Unresolved,
                                       &editor::SceneComparisonRow::choice);
  Require(found != rows.end(), "Publication name conflict missing");
  f.Click(
      Access::SceneMergeChoicePosition(f.ui, static_cast<std::size_t>(found - rows.begin()), 1));
  f.Click(Access::SceneMergePosition(f.ui, 0));
  auto draft = f.ui.TakeSceneMergeCandidateRequest();
  Require(draft.has_value(), "Publication candidate request missing");
  core::JobSystem jobs(1);
  jobs.Start();
  editor::SceneMergeCandidateJob job(jobs);
  Require(job.Start(*f.merge_review, draft->candidate), "Publication candidate worker failed");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!job.Poll(f.merge_review->Identity())) {
    Require(std::chrono::steady_clock::now() < deadline, "Publication candidate did not drain");
    std::this_thread::yield();
  }
  f.candidate = job.Snapshot();
  f.candidate_draft = draft->draft;
  f.Draw();
  Require(f.candidate.result && f.candidate.result->bytes, "Publication candidate unavailable");
  std::uint64_t identity{};
  const auto preview = [&] {
    return editor::imgui::SceneMergePublicationPreview{1,
                                                       ++identity,
                                                       f.current->Token(),
                                                       f.observation.operation,
                                                       f.merge_review->Identity(),
                                                       f.candidate_draft,
                                                       "Content/Main.scene",
                                                       f.candidate.result->bytes->size()};
  };
  const auto review = [&] {
    f.Click(Access::SceneMergePublicationPosition(f.ui, 0));
    auto request = f.ui.TakeSceneMergePublicationRequest();
    Require(request && request->draft == draft->draft && !f.ui.TakeSceneMergePublicationRequest() &&
                !f.ui.TakeSceneFileRequest(),
            "Publication request lost one-shot scope or emitted an ordinary Save");
    return request->candidate;
  };
  auto request = review();
  auto capability =
      editor::SceneMergePublisher::Prepare(*f.current, *f.merge_review, request, true);
  if (cryptography::ActiveProvider() == cryptography::Provider::Unavailable) {
    Require(!capability && read() == disk, "Unavailable backend changed source");
    job.Shutdown();
    jobs.Stop();
    return;
  }
  Require(capability.has_value(), "Actual publication prepare failed");
  auto scope = preview();
  auto invalid = scope;
  invalid.schema = 9;
  Require(!f.ui.SetSceneMergePublicationPreview(invalid), "Unknown confirmation schema accepted");
  Require(f.ui.SetSceneMergePublicationPreview(scope), "Current publication preview rejected");
  f.Draw();
  f.Draw();
  Require(f.ui.SceneMergePublicationPreviewCurrent(scope) && read() == disk,
          "Preview changed source before confirmation");
  f.Click(Access::SceneMergePublicationPosition(f.ui, 2));
  auto cancelled = f.ui.TakeSceneMergePublicationDecision();
  Require(cancelled && !cancelled->confirmed && cancelled->scope == scope &&
              !f.ui.TakeSceneMergePublicationDecision() && read() == disk &&
              f.scene.MatchesPreparedSave(original) && !f.ui.TakeSceneFileRequest(),
          "Real Cancel lost scope or changed source/history");
  f.Draw();
  // A held press belongs to one full confirmation identity and cannot retarget a replacement.
  request = review();
  scope = preview();
  Require(f.ui.SetSceneMergePublicationPreview(scope), "Held-confirm preview failed");
  f.Draw();
  f.Draw();
  const auto held = Access::SceneMergePublicationPosition(f.ui, 1);
  Require(held.has_value(), "Held-confirm control missing");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>((*held)[0] * dpi);
  pointer.value1 = static_cast<int>((*held)[1] * dpi);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer, button});
  f.Draw();
  scope = preview();
  Require(f.ui.SetSceneMergePublicationPreview(scope), "Replacement confirmation rejected");
  f.Draw();
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  f.Draw();
  Require(!f.ui.TakeSceneMergePublicationDecision() &&
              f.ui.SceneMergePublicationPreviewCurrent(scope) && read() == disk,
          "Held release retargeted a new confirmation identity");
  f.Click(Access::SceneMergePublicationPosition(f.ui, 2));
  Require(f.ui.TakeSceneMergePublicationDecision().has_value(),
          "Held-confirm cleanup did not cancel");
  f.Draw();
  request = review();
  scope = preview();
  Require(f.ui.SetSceneMergePublicationPreview(scope), "Revocation preview failed");
  f.Draw();
  f.Draw();
  f.publication_allowed = false;
  f.Draw();
  Require(!f.ui.SceneMergePublicationPreviewCurrent(scope) &&
              !f.ui.TakeSceneMergePublicationDecision() &&
              !Access::SceneMergePublicationPosition(f.ui, 1) && read() == disk,
          "Revoked publication retained confirmation authority");
  f.publication_allowed = true;
  f.Draw();
  // A changed disk while the confirmation is open must be rejected by the real publisher.
  request = review();
  capability = editor::SceneMergePublisher::Prepare(*f.current, *f.merge_review, request, true);
  Require(capability.has_value(), "Stale-disk preparation failed");
  scope = preview();
  Require(f.ui.SetSceneMergePublicationPreview(scope), "Stale-disk preview rejected too early");
  f.Draw();
  f.Draw();
  std::ofstream(source, std::ios::binary | std::ios::trunc) << *f.merge_review->BaseBytes();
  f.Click(Access::SceneMergePublicationPosition(f.ui, 1));
  auto stale = f.ui.TakeSceneMergePublicationDecision();
  Require(stale && stale->confirmed &&
              editor::SceneMergePublisher::Publish(*f.current, *capability, true).status ==
                  editor::SceneMergePublishStatus::Rejected &&
              read() == *f.merge_review->BaseBytes(),
          "Stale disk crossed actual graphical confirmation");
  std::ofstream(source, std::ios::binary | std::ios::trunc) << disk;
  f.Draw();
  request = review();
  capability = editor::SceneMergePublisher::Prepare(*f.current, *f.merge_review, request, true);
  Require(capability.has_value(), "Successful publication preparation failed");
  scope = preview();
  Require(f.ui.SetSceneMergePublicationPreview(scope), "Successful publication preview failed");
  f.Draw();
  f.Draw();
  f.Click(Access::SceneMergePublicationPosition(f.ui, 1));
  auto confirmed = f.ui.TakeSceneMergePublicationDecision();
  Require(confirmed && confirmed->confirmed && confirmed->scope == scope &&
              !f.ui.TakeSceneMergePublicationDecision(),
          "Real Confirm lost scope or duplicated");
  const auto published = editor::SceneMergePublisher::Publish(*f.current, *capability, true);
  Require(published.Published() && read() == *f.candidate.result->bytes &&
              f.scene.MatchesPreparedSave(original) &&
              editor::SceneMergePublisher::Inspect(f.writer, published.archive).has_value(),
          "Graphical confirmation did not publish exact candidate with retained provenance");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Published UI"));
  const auto entity = f.scene.Nodes().front().id;
  Require(reopened.ReloadBytes(read()) && reopened.Name(entity) == "Local" &&
              reopened.PrefabPlacements().empty() &&
              reopened.OpaqueComponents(*reopened.Key(entity))->front().data ==
                  std::vector<std::uint8_t>({0, 255, 17}) &&
              f.scene.Undo() && f.scene.Name(entity) == "Base" && f.scene.Redo() &&
              f.scene.Name(entity) == "Local" && f.scene.MatchesPreparedSave(original) &&
              f.current->Save(f.current->Token()).status == editor::SceneFileStatus::NeedsOverwrite,
          "Published UI file lost unknown bytes/reopen or changed live history/baseline");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly),
          "Publication read-only fixture failed");
  f.active = &f.reader;
  f.Draw();
  f.Click(Access::SceneMergePublicationPosition(f.ui, 0));
  Require(!f.ui.TakeSceneMergePublicationRequest() &&
              !f.ui.SetSceneMergePublicationPreview(preview()),
          "Read-only graphical publication accepted");
  f.active = &f.writer;
  const std::array tabs{editor::imgui::SceneTabItem{
      7, f.current->Token(), "Reference", "Content/Main.scene", false, false, false, true}};
  Require(f.ui.SetSceneTabs(tabs, 7), "Reference publication tab fixture failed");
  f.Draw();
  f.Click(Access::SceneMergePublicationPosition(f.ui, 0));
  Require(!f.ui.TakeSceneMergePublicationRequest() &&
              !f.ui.SetSceneMergePublicationPreview(preview()),
          "Reference graphical publication accepted");
  job.Shutdown();
  jobs.Stop();
}

} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    RunMerge(1);
    RunMerge(2);
    RunPublication(1);
    RunPublication(2);
    std::cout << "Graphical semantic scene comparison passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
