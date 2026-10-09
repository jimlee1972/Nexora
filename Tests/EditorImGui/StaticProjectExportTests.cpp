#include "EditorImGuiTestAccess.h"
#include "Nexora/Runtime/ProjectPackage.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Phase = editor::StaticExportPhase;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-static-export-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  core::JobSystem jobs{1};
  editor::StaticProjectExportJob job{jobs};
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  editor::MaterialAssetCatalog materials;
  runtime::World world;
  runtime::Id id = world.LoadScene("UI export");
  editor::SceneDocument scene{world, id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::AssetUuid scene_asset;
  float scale{};
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "UI export"), "UI project creation failed");
    Require(scene.CreateCamera("Camera") && scene.Save(root / "Content/Main.scene") &&
                assets.ImportTree(root / "Content", {}, {},
                                  editor::AssetIdentityMode::PersistentReadWrite) &&
                content.Open(writer, assets, 4) && meshes.PublishContent(content.Browser()) &&
                materials.PublishContent(content.Browser()),
            "UI real asset fixture failed");
    scene_asset = content.Browser().Items().front().id;
    jobs.Start();
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
    job.Shutdown();
    jobs.Stop();
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  void Draw() {
    ui.SetSceneFileContext({active->Project().id, scene.Generation()}, "Content/Main.scene");
    ui.SetStaticExportStatus(job.Snapshot(), job.Busy());
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, &content, nullptr, nullptr, nullptr, &play, nullptr,
                        &meshes, &materials);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::size_t control) {
    const auto point = Access::StaticExportPosition(ui, control);
    Require(point.has_value(), "StaticView menu control absent");
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
  void StartFromUi() {
    Click(0);
    Click(1);
    auto request = ui.TakeStaticExportRequest();
    Require(request && !request->cancel && request->token.project == writer.Project().id &&
                request->token.document_generation == scene.Generation() &&
                !ui.TakeStaticExportRequest(),
            "UI export request scope/one-shot failed");
    Require(job.Start(writer, scene, content, meshes, materials, scene_asset),
            "UI actual job start failed");
    Draw();
  }
  void Ready() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (job.Snapshot().phase != Phase::ReadyToPublish &&
           std::chrono::steady_clock::now() < deadline) {
      Require(job.Snapshot().phase != Phase::Failed, "UI worker failed");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(job.Snapshot().phase == Phase::ReadyToPublish,
            "UI worker did not stage actual package");
  }
  void Finish() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (job.Busy() && std::chrono::steady_clock::now() < deadline) {
      static_cast<void>(job.Poll(*active, scene, content, meshes, materials));
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(!job.Busy(), "UI job did not terminate");
    Draw();
  }
};
void Run(float dpi) {
  Fixture f{dpi};
  const auto before = f.scene.PrepareSave();
  f.StartFromUi();
  f.Ready();
  f.Finish();
  const auto status = Access::StaticExportStatus(f.ui);
  Require(status.phase == Phase::Published && status.bytes > 0 && status.checksum.size() == 16 &&
              status.verify_command.find("--verify-package") != std::string::npos &&
              f.scene.MatchesPreparedSave(*before),
          "UI published status or authoring preservation failed");
  const auto output = f.root / status.relative_path;
  std::ifstream input(output, std::ios::binary);
  const std::string original{std::istreambuf_iterator<char>(input), {}};
  input.close();
  Require(runtime::DecodeStaticProjectPackage(
              std::as_bytes(std::span{original.data(), original.size()}))
              .has_value(),
          "UI produced artifact did not load in actual Runtime consumer");
  f.StartFromUi();
  f.Ready();
  f.Click(0);
  f.Click(2);
  const auto cancel = f.ui.TakeStaticExportRequest();
  Require(cancel && cancel->cancel && f.job.Cancel() && !f.ui.TakeStaticExportRequest(),
          "UI actual cancel action failed");
  f.Finish();
  Require(Access::StaticExportStatus(f.ui).phase == Phase::Cancelled,
          "UI cancelled export reported success");
  std::ifstream retained(output, std::ios::binary);
  Require(std::string(std::istreambuf_iterator<char>(retained), {}) == original,
          "UI cancellation replaced last package");
  retained.close();
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "UI read-only fixture failed");
  f.active = &f.reader;
  f.Draw();
  f.Click(0);
  f.Click(1);
  Require(!f.ui.TakeStaticExportRequest(), "read-only UI emitted export");
  f.ui.SetSceneFileContext({f.writer.Project().id, f.scene.Generation() + 1},
                           "Content/Other.scene");
  f.ui.SetStaticExportStatus(status, false);
  Require(Access::StaticExportStatus(f.ui).phase == Phase::Idle &&
              Access::StaticExportStatus(f.ui).verify_command.empty(),
          "stale UI scope displayed another scene's success");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "StaticView export 1x/2x UI requests and actual job contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
