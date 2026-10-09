#include "Nexora/Editor/StaticProjectExportJob.h"
#include "Nexora/Runtime/ProjectPackage.h"

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Phase = editor::StaticExportPhase;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-static-export-job-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  core::JobSystem jobs{1};
  editor::StaticProjectExportJob job{jobs};
  editor::ProjectWorkspace workspace;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  editor::MaterialAssetCatalog materials;
  runtime::World world;
  runtime::Id scene = world.LoadScene("Export job");
  editor::SceneDocument document{world, scene};
  runtime::AssetUuid scene_asset, mesh_asset;
  runtime::Id entity{};
  Fixture() {
    root += std::filesystem::path(u8" 資料 µ");
    Require(workspace.Create(root, "Export job"), "project creation failed");
    jobs.Start();
    std::ofstream(root / "Content/Triangle.obj")
        << "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n";
    std::ofstream(root / "Content/Paint.nmaterial")
        << "NEXORA_MATERIAL 1\nbase_color 0.2 0.4 0.6\nmetallic 0.25\nroughness 0.5\n"
           "occlusion 1\nemission 0 0 2\n";
    Require(
        assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "actual asset import failed");
    mesh_asset = assets.Search("Triangle").front()->id;
    const auto material_asset = assets.Search("Paint").front()->id;
    entity =
        document.CreateMesh("Triangle", {runtime::MeshResourceId(mesh_asset), {~runtime::Id{0}}});
    Require(
        entity && document.CreateCamera("Camera") &&
            document.SetOpaqueComponent(*document.Key(entity),
                                        editor::MaterialAssetReference(material_asset)) &&
            document.SetOpaqueComponent(*document.Key(entity), {919, "Absent plugin", {0, 255}}) &&
            document.Save(root / "Content/Main.scene") && assets.ImportSavedScene("Main.scene") &&
            content.Open(workspace, assets, 7) && meshes.PublishContent(content.Browser()) &&
            materials.PublishContent(content.Browser()),
        "export fixture failed");
    for (const auto &item : content.Browser().Items())
      if (item.type == ".scene")
        scene_asset = item.id;
    Require(scene_asset != runtime::AssetUuid{}, "scene identity missing");
  }
  ~Fixture() {
    job.Shutdown();
    jobs.Stop();
    workspace = editor::ProjectWorkspace{};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  std::filesystem::path Output() const { return root / ".nexora/exports/static-view.nxproject"; }
  std::filesystem::path Stage() const {
    auto path = Output();
    path += ".tmp";
    return path;
  }
  void Start() {
    std::string error = "stale";
    Require(job.Start(workspace, document, content, meshes, materials, scene_asset, &error) &&
                error.empty() && job.Busy() && job.Snapshot().operation,
            "job admission failed");
  }
  void Ready() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (job.Snapshot().phase != Phase::ReadyToPublish &&
           std::chrono::steady_clock::now() < deadline) {
      Require(job.Snapshot().phase != Phase::Failed, "worker failed before staging");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(job.Snapshot().phase == Phase::ReadyToPublish &&
                std::filesystem::is_regular_file(Stage()),
            "worker did not produce actual verified staging");
  }
  Phase Finish(const editor::ProjectWorkspace *current = nullptr,
               const editor::ProjectContentSession *current_content = nullptr) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (job.Busy() && std::chrono::steady_clock::now() < deadline) {
      static_cast<void>(job.Poll(current ? *current : workspace, document,
                                 current_content ? *current_content : content, meshes, materials));
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(!job.Busy() && !std::filesystem::exists(Stage()),
            "terminal job did not release staging");
    Require(!job.Poll(workspace, document, content, meshes, materials),
            "completion was emitted twice");
    return job.Snapshot().phase;
  }
};

void Run() {
  Fixture f;
  Require(f.document.Rename(*f.document.Key(f.entity), "Unsaved authoring name"),
          "dirty fixture failed");
  const auto before = f.document.PrepareSave();
  const auto capture = f.document.CaptureRuntimeScene();
  Require(before && capture && f.document.Dirty(), "owning pre-export capture failed");
  f.Start();
  std::string error;
  Require(!f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset,
                       &error) &&
              !error.empty(),
          "concurrent intake was accepted");
  f.Ready();
  Require(!std::filesystem::exists(f.Output()), "worker published without owner authorization");
  Require(f.Finish() == Phase::Published, "verified fresh package did not publish");
  const auto original = Read(f.Output());
  const auto bytes = std::as_bytes(std::span{original.data(), original.size()});
  auto loaded = runtime::DecodeStaticProjectPackage(bytes, &error);
  const auto snapshot = f.job.Snapshot();
  Require(loaded && loaded->SceneAssetId() == f.scene_asset && loaded->RenderItems().size() == 1 &&
              loaded->AssetCount() == 3 && loaded->RenderItems()[0].material &&
              loaded->RenderItems()[0].material->base_color == std::array{.2F, .4F, .6F} &&
              loaded->InactiveComponentCount() == 1 && snapshot.bytes == original.size() &&
              snapshot.checksum.size() == 16 &&
              snapshot.verify_command.find("--verify-package") != std::string::npos &&
              f.document.MatchesPreparedSave(*before) &&
              f.document.CaptureRuntimeScene() == capture && f.document.Dirty(),
          "publication lost Runtime content or changed authoring baseline");
  Require(f.document.Undo() && f.document.Redo() && f.document.MatchesPreparedSave(*before),
          "export changed real authoring Undo/Redo");
  const auto first_operation = snapshot.operation;
  f.Start();
  f.Ready();
  Require(f.Finish() == Phase::Published && Read(f.Output()) == original &&
              f.job.Snapshot().checksum == snapshot.checksum,
          "unchanged owning input did not reproduce the published package");

  // Cancellation before execution is deterministic: occupy the actual one-worker JobSystem.
  {
    std::promise<void> release, started;
    const auto gate = release.get_future().share();
    const auto blocker = f.jobs.Submit({[gate, &started](const core::CancellationToken &) {
                                          started.set_value();
                                          gate.wait();
                                        },
                                        core::JobPriority::Normal,
                                        {},
                                        "Export cancellation gate"});
    Require(started.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "actual blocker worker did not run");
    f.Start();
    Require(f.job.Snapshot().phase == Phase::Queued && f.job.Cancel(),
            "queued cancellation failed");
    release.set_value();
    f.jobs.Wait(blocker);
    Require(f.Finish() == Phase::Cancelled && Read(f.Output()) == original &&
                f.job.Snapshot().operation > first_operation,
            "queued cancellation replaced previous package");
  }
  f.Start();
  f.Ready();
  Require(f.job.Cancel() && f.Finish() == Phase::Cancelled && Read(f.Output()) == original,
          "ready-to-publish cancellation replaced previous package");

  f.Start();
  f.Ready();
  Require(f.document.Rename(*f.document.Key(f.entity), "Changed during export") &&
              f.Finish() == Phase::Stale && Read(f.Output()) == original,
          "authoring rename published stale data");
  Require(f.document.Undo(), "stale export changed authoring Undo");
  f.Start();
  f.Ready();
  auto &untracked = f.world.CreateEntity(f.scene);
  untracked.transform.x = 23;
  Require(f.Finish() == Phase::Stale && Read(f.Output()) == original,
          "untracked Runtime mutation published stale capture");
  runtime::WorldCommandBuffer removal;
  removal.DestroyEntity(untracked.id);
  Require(removal.Apply(f.world), "untracked fixture removal failed");

  f.Start();
  f.Ready();
  Require(f.content.Browser().Rename(f.mesh_asset, "Moved.obj") && f.Finish() == Phase::Stale &&
              Read(f.Output()) == original && f.content.Browser().Undo(),
          "content revision mutation published stale input or lost Undo");
  f.Start();
  f.Ready();
  f.meshes.Clear();
  Require(f.Finish() == Phase::Stale && Read(f.Output()) == original &&
              f.meshes.PublishContent(f.content.Browser()),
          "catalog invalidation published stale geometry");

  f.Start();
  f.Ready();
  editor::ProjectWorkspace reader;
  Require(reader.Open(f.root, editor::ProjectAccess::ReadOnly) &&
              f.Finish(&reader) == Phase::Stale && Read(f.Output()) == original,
          "read-only publication was authorized");
  std::string failure;
  Require(
      !f.job.Start(reader, f.document, f.content, f.meshes, f.materials, f.scene_asset, &failure),
      "read-only intake was authorized");
  reader = editor::ProjectWorkspace{};

  f.Start();
  f.Ready();
  std::ofstream(f.root / ".nexora/workspace.recovery") << "pending recovery";
  Require(
      f.Finish() == Phase::Stale && Read(f.Output()) == original &&
          !f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset),
      "recovery-pending export replaced prior package");
  std::filesystem::remove(f.root / ".nexora/workspace.recovery");

  std::ofstream(f.Stage()) << "foreign staging";
  Require(!f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset) &&
              Read(f.Stage()) == "foreign staging" && Read(f.Output()) == original,
          "occupied stage was overwritten or removed");
  std::filesystem::remove(f.Stage());
  std::filesystem::create_directory(f.Stage());
  Require(!f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset) &&
              std::filesystem::is_directory(f.Stage()),
          "occupied staging directory was removed");
  std::filesystem::remove(f.Stage());

  f.Start();
  f.Ready();
  std::ofstream(f.Stage(), std::ios::app) << "external stage mutation";
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (f.job.Busy() && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(f.job.Poll(f.workspace, f.document, f.content, f.meshes, f.materials));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(!f.job.Busy() && f.job.Snapshot().phase == Phase::Failed &&
              Read(f.Output()) == original && Read(f.Stage()).ends_with("external stage mutation"),
          "changed staging was published or its foreign modification removed");
  std::filesystem::remove(f.Stage());
  f.Start();
  f.Ready();
  const auto retained_path = f.Output().parent_path() / "retained.nxproject";
  std::filesystem::rename(f.Output(), retained_path);
  std::filesystem::create_directory(f.Output());
  Require(f.Finish() == Phase::Failed && Read(retained_path) == original &&
              std::filesystem::is_directory(f.Output()),
          "unsafe destination was removed or replaced during publication");
  std::filesystem::remove(f.Output());
  std::filesystem::rename(retained_path, f.Output());

  {
    Fixture other;
    f.Start();
    f.Ready();
    Require(f.Finish(&other.workspace, &other.content) == Phase::Stale &&
                Read(f.Output()) == original && !std::filesystem::exists(other.Output()),
            "project switch published the previous project's captured package");
  }
#if !defined(_WIN32)
  const auto foreign = f.root / "Foreign";
  std::filesystem::create_directory(foreign);
  std::ofstream(foreign / "sentinel") << "foreign data";
  std::filesystem::create_symlink(foreign / "sentinel", f.Stage());
  Require(!f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset) &&
              std::filesystem::is_symlink(f.Stage()) &&
              Read(foreign / "sentinel") == "foreign data",
          "preexisting staging alias was followed or removed");
  std::filesystem::remove(f.Stage());
  std::filesystem::remove(f.Output());
  std::filesystem::remove(f.Output().parent_path());
  std::filesystem::create_directory_symlink(foreign, f.Output().parent_path());
  Require(!f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset) &&
              Read(foreign / "sentinel") == "foreign data" &&
              !std::filesystem::exists(foreign / "static-view.nxproject"),
          "output directory alias was followed");
  std::filesystem::remove(f.Output().parent_path());
  std::filesystem::create_directory(f.Output().parent_path());
  std::ofstream(f.Output(), std::ios::binary)
      .write(original.data(), static_cast<std::streamsize>(original.size()));
#endif

  // Real producer failure: unresolved resource cannot report readiness/success or replace output.
  Require(f.document.SetMeshRenderer(*f.document.Key(f.entity), runtime::MeshComponent{9999, {}}),
          "invalid mesh fixture failed");
  f.Start();
  Require(f.Finish() == Phase::Failed && Read(f.Output()) == original &&
              !f.job.Snapshot().message.empty(),
          "failed cooker replaced previous package");
  Require(f.document.Undo(), "cooker failure changed Undo");
  f.Start();
  f.Ready();
  f.job.Shutdown();
  Require(
      !f.job.Busy() && f.job.Snapshot().phase == Phase::Cancelled && Read(f.Output()) == original &&
          !std::filesystem::exists(f.Stage()) &&
          !f.job.Start(f.workspace, f.document, f.content, f.meshes, f.materials, f.scene_asset),
      "shutdown failed to cancel/drain owned staging or stop intake");
}
} // namespace

int main() {
  try {
    Run();
    std::cout << "StaticView export worker/publication contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
