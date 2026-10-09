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
        ("nexora-gpu-capture-ui-" +
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
  constexpr auto source = GpuProfileSource::VulkanTimestamps;
  Require(profile.ObserveGpuFrame(1, source, true, {1, 0}) &&
              profile.ObserveGpuFrame(1, source, true, {3, std::nullopt}),
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
    const auto point = EditorImGuiTestAccess::GpuControlPosition(host, control);
    Require(point.has_value(), "GPU capture control missing");
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
  click(0);
  Require(host.TakeGpuExportRequest() && !host.TakeGpuExportRequest() &&
              !host.TakeMemoryExportRequest() && !host.TakeProfileJsonExportRequest(),
          "GPU export not independent/one-shot");
  Require(first.ExportGpuTimingJson(source, true, profile.GpuSamples(), 7, &error),
          "GPU export failed");
  click(1);
  Require(host.TakeGpuImportRequest() && !host.TakeGpuImportRequest() &&
              !host.TakeMemoryImportRequest() && !host.TakeProfileJsonImportRequest(),
          "GPU import not independent/one-shot");
  auto capture = first.ImportGpuTimingJson(&error);
  Require(capture && host.SetImportedGpuCapture(*capture), "static publication failed");
  capture->samples[0].milliseconds = 99;
  const auto imported = [&]() { return EditorImGuiTestAccess::ImportedGpuCapture(host); };
  Require(imported()->source == source && imported()->software_rasterizer &&
              imported()->samples[0].milliseconds == 0 && !imported()->samples[1].milliseconds &&
              imported()->older_samples_dropped == 7 && profile.GpuSamples().size() == 2,
          "capture borrowed input or mutated live ownership");
  Require(host.SetImportedProfileCapture({{{1, 1, 0, 0}}, 0}) &&
              host.SetImportedMemoryCapture({{{1, 0, 1}}, 0}),
          "other imports failed");
  for (auto bad : {GpuTimingCapture{GpuProfileSource::Unavailable, false, {{1, 0}}, 0},
                   GpuTimingCapture{source, false, {{0, 0}}, 0},
                   GpuTimingCapture{source, false, {{2, 0}, {1, 0}}, 0}})
    Require(!host.SetImportedGpuCapture(std::move(bad)) && imported()->samples[0].milliseconds == 0,
            "rejected publication erased last-good static capture");
  profile.Clear();
  draw();
  Require(imported()->samples.size() == 2 && profile.GpuSamples().empty(),
          "live Clear erased static capture");
  click(0);
  Require(!host.TakeGpuExportRequest(), "empty live GPU history exported");
  click(1);
  Require(host.TakeGpuImportRequest(), "empty live history blocked static import");
  click(2);
  Require(!imported() && EditorImGuiTestAccess::ImportedMemoryCapture(host) &&
              EditorImGuiTestAccess::ImportedProfileCapture(host),
          "GPU clear erased independent imports");
  Require(host.SetImportedGpuCapture(*capture), "GPU republish failed");
  Require(host.SetImportedGpuCapture(
              {GpuProfileSource::MetalCommandBuffer, false, {{1, std::nullopt}}, 0}),
          "all-unavailable static capture rejected");
  draw();
  Require(imported()->source == GpuProfileSource::MetalCommandBuffer &&
              !imported()->software_rasterizer && !imported()->samples.front().milliseconds &&
              profile.GpuSamples().empty(),
          "static source transition fabricated timing or mutated live state");
  Require(profile.ObserveGpuFrame(1, source, true, {4, 1}), "new completion failed");
  draw();
  Require(observer.Open(first.Root(), ProjectAccess::ReadOnly, &error), "observer open failed");
  project = &observer;
  draw();
  click(0);
  click(1);
  Require(!host.TakeGpuExportRequest() && host.TakeGpuImportRequest() &&
              observer.ImportGpuTimingJson(&error),
          "read-only capture gates incorrect");
  project = &first;
  draw();
  host.RequestCloseConfirmation();
  draw();
  click(0);
  click(1);
  Require(!host.TakeGpuExportRequest() && !host.TakeGpuImportRequest(),
          "close modal emitted GPU requests");
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
  std::ofstream(first.Root() / ".nexora/workspace.recovery") << "schema=1\n";
  draw();
  click(0);
  click(1);
  Require(!host.TakeGpuExportRequest() && !host.TakeGpuImportRequest(),
          "recovery modal emitted GPU requests");
  Require(first.DiscardRecovery(&error), "recovery discard failed");
  draw();
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  click(0);
  click(1);
  project = &second;
  draw();
  Require(!host.TakeGpuExportRequest() && !host.TakeGpuImportRequest() && !imported() &&
              profile.GpuSamples().size() == 1,
          "project switch retained requests/static capture or cleared live history");
  Require(host.SetImportedGpuCapture(*capture), "second project owning publication failed");
  project = nullptr;
  draw();
  click(0);
  click(1);
  Require(!host.TakeGpuExportRequest() && !host.TakeGpuImportRequest() && !imported() &&
              !host.SetImportedGpuCapture(*capture),
          "detached capture gates incorrect");
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
