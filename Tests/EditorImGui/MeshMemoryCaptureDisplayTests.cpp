#include "EditorImGuiTestAccess.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float dpi) {
  using namespace nexora::editor;
  using imgui::EditorImGuiTestAccess;
  using Nexora::Window::WindowEvent;
  using Nexora::Window::WindowEventType;
  struct Temporary {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("nexora-mesh-memory-ui-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Temporary() {
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
    }
  } temp;
  ProjectWorkspace first, second, observer;
  std::string error;
  Require(first.Create(temp.root / "first", "First", &error) &&
              second.Create(temp.root / "second", "Second", &error),
          "project create failed");
  ProjectWorkspace *project = &first;
  ProfileSession profile;
  const auto origin = std::chrono::steady_clock::time_point{};
  Require(profile.ObserveMeshMemory(1, origin, 0, 0) &&
              profile.ObserveMeshMemory(1, origin + std::chrono::milliseconds(250), std::nullopt,
                                        std::nullopt),
          "owner fixture failed");
  imgui::EditorImGuiHost host;
  host.SetDisplay(1280 * dpi, 900 * dpi, dpi);
  EditorImGuiTestAccess::ConfigureSyntheticInput(host);
  WindowEvent focus;
  focus.type = WindowEventType::FocusChanged;
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  ProductShell shell;
  const auto draw = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, project, nullptr, nullptr, nullptr, nullptr, nullptr,
                          &profile);
    Require(host.EndFrame().vertices > 0, "capture panel did not draw");
  };
  draw();
  draw();
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  const auto click = [&](std::size_t control) {
    EditorImGuiTestAccess::ScrollProfilerToMeshControls(host);
    draw();
    draw();
    const auto point = EditorImGuiTestAccess::MeshMemoryControlPosition(host, control);
    Require(point.has_value(), "mesh memory control missing");
    WindowEvent pointer;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    WindowEvent button;
    button.type = WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    host.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    host.ProcessEvents(std::array{button});
    draw();
  };
  const imgui::MeshMemoryCaptureRequest scope{first.Project().id, first.Root()};
  const auto imported = [&] { return EditorImGuiTestAccess::ImportedMeshMemoryCapture(host); };
  click(0);
  auto export_request = host.TakeMeshMemoryExportRequest();
  Require(export_request && export_request->project == scope.project &&
              export_request->root == scope.root && !host.TakeMeshMemoryExportRequest() &&
              !host.TakeMemoryExportRequest() && !host.TakeGpuExportRequest(),
          "export not scoped/owning/independent/one-shot");
  Require(first.ExportMeshMemoryJson(profile.MeshMemorySamples(), 7, &error),
          "owner export failed");
  click(1);
  auto import_request = host.TakeMeshMemoryImportRequest();
  Require(import_request && import_request->root == scope.root &&
              import_request->project == scope.project && !host.TakeMeshMemoryImportRequest() &&
              !host.TakeMemoryImportRequest() && !host.TakeGpuImportRequest(),
          "import not scoped/independent/one-shot");
  auto capture = first.ImportMeshMemoryJson(&error);
  Require(capture && host.SetImportedMeshMemoryCapture(*import_request, *capture),
          "static publication failed");
  capture->samples[0].native_allocation_bytes = 99;
  Require(imported() && imported()->samples[0].native_allocation_bytes == 0 &&
              imported()->samples[0].cpu_key_payload_bytes == 0 &&
              !imported()->samples[1].native_allocation_bytes &&
              !imported()->samples[1].cpu_key_payload_bytes &&
              imported()->older_samples_dropped == 7 && profile.MeshMemorySamples().size() == 2,
          "publication borrowed data or changed live state");
  Require(host.SetImportedProfileCapture({{{1, 1, 0, 0}}, 0}) &&
              host.SetImportedMemoryCapture({{{1, 0, 1}}, 0}) &&
              host.SetImportedGpuCapture({GpuProfileSource::VulkanTimestamps, true, {{1, 0}}, 0}),
          "other imports failed");
  for (auto bad : {MeshMemoryCapture{{}, 0}, MeshMemoryCapture{{{1, 0, 1, std::nullopt}}, 0},
                   MeshMemoryCapture{{{2, 0, 1, 1}, {1, 250, 1, 1}}, 0}})
    Require(!host.SetImportedMeshMemoryCapture(scope, std::move(bad)) &&
                imported()->samples[0].native_allocation_bytes == 0,
            "bad publication erased prior static state");
  auto stale = scope;
  stale.root = second.Root();
  Require(!host.SetImportedMeshMemoryCapture(stale, *capture) &&
              imported()->samples[0].native_allocation_bytes == 0,
          "foreign root published");
  stale = scope;
  stale.project = second.Project().id;
  Require(!host.SetImportedMeshMemoryCapture(stale, *capture), "foreign UUID published");
  std::ofstream(first.Root() / ".nexora/mesh-memory.json") << "corrupt";
  click(1);
  Require(host.TakeMeshMemoryImportRequest() && !first.ImportMeshMemoryJson(&error) && imported() &&
              imported()->samples[0].native_allocation_bytes == 0,
          "failed owner read erased prior import");
  profile.Clear();
  draw();
  Require(imported()->samples.size() == 2 && profile.MeshMemorySamples().empty(),
          "live Clear erased static");
  click(0);
  Require(!host.TakeMeshMemoryExportRequest(), "empty live history exported");
  click(1);
  Require(host.TakeMeshMemoryImportRequest().has_value(), "empty live history blocked import");
  click(2);
  Require(!imported() && EditorImGuiTestAccess::ImportedMemoryCapture(host) &&
              EditorImGuiTestAccess::ImportedProfileCapture(host) &&
              EditorImGuiTestAccess::ImportedGpuCapture(host),
          "mesh clear erased independent imports");
  Require(host.SetImportedMeshMemoryCapture(scope, *capture), "republish failed");
  Require(profile.ObserveMeshMemory(1, origin + std::chrono::milliseconds(500), 4096, 512),
          "new observation failed");
  draw();
  Require(observer.Open(first.Root(), ProjectAccess::ReadOnly, &error), "observer failed");
  click(0); // A previously queued writer action must lose authority on read-only transition.
  project = &observer;
  draw();
  click(0);
  click(1);
  Require(!host.TakeMeshMemoryExportRequest() && host.TakeMeshMemoryImportRequest(),
          "readonly gates failed");
  project = &first;
  draw();
  click(0);
  click(1);
  focus.value0 = 0;
  host.ProcessEvents(std::array{focus});
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest() &&
              imported() && !host.SetImportedMeshMemoryCapture(scope, *capture),
          "focus loss retained authority or erased static state");
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  draw();
  draw();
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  click(0);
  click(1);
  host.RequestCloseConfirmation();
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest() && imported(),
          "close retained pending authority");
  draw();
  click(0);
  click(1);
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest(),
          "close modal emitted requests");
  WindowEvent escape;
  escape.type = WindowEventType::Key;
  escape.value0 = static_cast<int>(Nexora::Window::Key::Escape);
  escape.value1 = 1;
  host.ProcessEvents(std::array{escape});
  draw();
  escape.value1 = 0;
  host.ProcessEvents(std::array{escape});
  draw();
  Require(host.TakeCloseChoice() == imgui::CloseChoice::Cancel, "close cancel failed");
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  click(0);
  click(1);
  std::ofstream(first.Root() / ".nexora/workspace.recovery") << "schema=1\n";
  draw();
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest() && imported(),
          "recovery retained pending authority");
  click(0);
  click(1);
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest(),
          "recovery emitted requests");
  Require(first.DiscardRecovery(&error), "recovery discard failed");
  draw();
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  click(0);
  click(1);
  project = &second;
  draw();
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest() &&
              !imported() && profile.MeshMemorySamples().size() == 1 &&
              !host.SetImportedMeshMemoryCapture(scope, *capture),
          "project switch retained authority/static or erased live");
  // A press originating in one project must not complete a file action after a scope change.
  project = &first;
  draw();
  for (std::size_t control = 0; control < 2; ++control) {
    EditorImGuiTestAccess::ScrollProfilerToMeshControls(host);
    draw();
    draw();
    const auto point = EditorImGuiTestAccess::MeshMemoryControlPosition(host, control);
    Require(point.has_value(), "held request control missing");
    WindowEvent pointer;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    WindowEvent button;
    button.type = WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    host.ProcessEvents(std::array{pointer, button});
    draw();
    project = &second;
    draw();
    button.value1 = 0;
    host.ProcessEvents(std::array{button});
    draw();
    Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest(),
            "old held press completed against the new project");
    project = &first;
    draw();
  }
  project = &second;
  draw();
  const imgui::MeshMemoryCaptureRequest second_scope{second.Project().id, second.Root()};
  Require(
      host.SetImportedMeshMemoryCapture(second_scope, {{{1, 0, std::nullopt, std::nullopt}}, 0}),
      "unavailable static rejected");
  draw();
  Require(imported() && !imported()->samples[0].native_allocation_bytes, "static fabricated bytes");
  ProjectWorkspace closed;
  project = &closed;
  draw();
  click(0);
  click(1);
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest() && !imported(),
          "closed project emitted requests or retained static state");
  project = nullptr;
  draw();
  click(0);
  click(1);
  Require(!host.TakeMeshMemoryExportRequest() && !host.TakeMeshMemoryImportRequest() &&
              !imported() && !host.SetImportedMeshMemoryCapture(second_scope, *capture),
          "detached gates failed");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
