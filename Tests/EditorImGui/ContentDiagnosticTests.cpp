#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float dpi) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-content-diagnostics-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace, observer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content, readonly;
  Require(workspace.Create(root, "Import diagnostics"), "project creation failed");
  std::ofstream(root / "Content/Broken.obj") << "v invalid 0 0\nf 1 2 3\n";
  std::ofstream(root / "Content/Good.txt") << "valid ordinary source";
  Require(
      assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite) &&
          assets.Entries().size() == 2 && content.Open(workspace, assets, 7) &&
          observer.Open(root, editor::ProjectAccess::ReadOnly) &&
          readonly.Open(observer, assets, 8, false),
      "content indexing/open failed");
  const auto broken = assets.Entries()[0].id, good = assets.Entries()[1].id;
  const auto diagnostic = assets.Find(broken)->error;
  Require(!diagnostic.empty() && assets.Find(broken)->state == editor::ImportState::Failed &&
              content.Browser().Find(broken)->import_diagnostic == diagnostic,
          "index failure text was discarded");
  editor::imgui::EditorImGuiHost ui;
  editor::ProductShell shell;
  editor::ProjectWorkspace *active_workspace = &workspace;
  editor::ProjectContentSession *active_content = &content;
  editor::AssetImportQueue *active_imports = nullptr;
  ui.SetDisplay(1400 * dpi, 1000 * dpi, dpi);
  Access::ConfigureSyntheticInput(ui);
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, nullptr, active_workspace, active_content, nullptr, active_imports);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  Access::FocusContent(ui);
  draw();
  draw();
  const auto click = [&](runtime::AssetUuid asset) {
    const auto position = Access::ContentAssetPosition(ui, asset);
    Require(position.has_value(), "asset row is not visible");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*position)[0] * dpi);
    pointer.value1 = static_cast<int>((*position)[1] * dpi);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    draw();
  };
  click(broken);
  Require(Access::ContentAssetDiagnostic(ui) == diagnostic,
          "real asset selection did not show initial import failure");
  Require(content.Browser().Select(good, true), "multi-selection failed");
  draw();
  Require(Access::ContentAssetDiagnostic(ui).empty(), "multiple selection retained stale failure");
  click(good);
  Require(Access::ContentAssetDiagnostic(ui).empty(), "healthy selection retained stale failure");
  active_workspace = &observer;
  active_content = &readonly;
  draw();
  click(broken);
  const auto revision = readonly.Browser().Revision();
  Require(Access::ContentAssetDiagnostic(ui) == diagnostic && !readonly.Reimport(broken) &&
              readonly.Browser().Revision() == revision,
          "read-only inspection lost diagnostics or admitted mutation");
  active_workspace = &workspace;
  active_content = &content;
  draw();
  click(broken);
  Require(content.Rename(broken, "Repaired.obj"), "rename failed");
  Require(!content.Reimport(broken) &&
              content.Browser().Find(broken)->import_diagnostic == diagnostic,
          "failed reimport erased initial diagnostic");
  std::ofstream(root / "Content/Repaired.obj") << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
  Require(content.Reimport(broken) && content.Browser().Find(broken)->import_diagnostic.empty(),
          "successful reimport did not resolve initial failure");
  draw();
  Require(Access::ContentAssetDiagnostic(ui).empty() && content.Undo() &&
              content.Browser().Find(broken)->import_diagnostic.empty() &&
              content.Browser().Find(broken)->thumbnail == editor::ThumbnailState::Ready,
          "content Undo resurrected resolved failure");
  active_content = nullptr;
  draw();
  Require(Access::ContentAssetDiagnostic(ui).empty(), "closed content retained diagnostic");
  // A retained read-only observer owns its original index snapshot independently of reimport.
  Require(readonly.Browser().Find(broken)->import_diagnostic == diagnostic,
          "publication mutated another session's diagnostic snapshot");
  core::JobSystem jobs{1};
  jobs.Start();
  editor::AssetImportQueue imports{jobs};
  editor::ProjectContentSession queued;
  Require(queued.Open(workspace, assets, 9) && queued.Browser().Select(broken) &&
              queued.Browser().Find(broken)->import_diagnostic == diagnostic,
          "queued repair fixture lost initial diagnostic");
  active_content = &queued;
  active_imports = &imports;
  draw();
  Access::FocusContent(ui);
  draw();
  draw();
  Require(Access::ContentAssetDiagnostic(ui) == diagnostic,
          "reattached content did not display its initial failure");
  std::string import_error;
  editor::ReimportJobRequest invalid_request{
      9, broken, root / "Content/Broken.obj", "", "default-v1", {}, ".obj"};
  Require(imports.Start(invalid_request, &import_error) == 0,
          "normal reimport admitted a missing baseline without explicit initial recovery");
  invalid_request.initial_import = true;
  invalid_request.previous_artifact = "existing-artifact";
  Require(imports.Start(invalid_request, &import_error) == 0,
          "initial recovery admitted a contradictory existing baseline");
  const bool started = queued.BeginReimport(imports, broken, &import_error);
  Require(started, import_error.c_str());
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (queued.ReimportBusy() && std::chrono::steady_clock::now() < deadline)
    draw(); // The actual Content draw polls and publishes the owning worker result.
  Require(!queued.ReimportBusy() && queued.Browser().Find(broken)->import_diagnostic.empty() &&
              queued.Browser().Find(broken)->mesh && Access::ContentAssetDiagnostic(ui).empty(),
          "background repair did not clear the model and graphical failure together");
  active_imports = nullptr;
  Require(
      assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite) &&
          assets.Find(broken) && assets.Find(broken)->error.empty() &&
          content.Open(workspace, assets, 10) &&
          content.Browser().Find(broken)->import_diagnostic.empty(),
      "reindex/reopen resurrected a repaired source failure");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Content diagnostic selection, repair and read-only UI contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
