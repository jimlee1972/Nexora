#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace nexora::editor;
using Access = imgui::EditorImGuiTestAccess;
using Nexora::Window::WindowEvent;
using Nexora::Window::WindowEventType;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-chrome-controls-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  nexora::editor::test::TemporaryDirectoryCleanup cleanup{root};
  ProjectWorkspace first, second, observer;
  Require(first.Create(root / "First", "First") && second.Create(root / "Second", "Second"),
          "Project fixture failed");
  auto *workspace = &first;
  ProfileSession profile;
  Require(profile.Add({1, 2, 0, 0}), "Live profile fixture failed");
  imgui::EditorImGuiHost host;
  host.SetDisplay(1400 * scale, 1000 * scale, scale);
  Access::ConfigureSyntheticInput(host);
  WindowEvent focus;
  focus.type = WindowEventType::FocusChanged;
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  ProductShell shell;
  const auto draw = [&] {
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, workspace, nullptr, nullptr, nullptr, nullptr, nullptr,
                          &profile);
    Require(host.EndFrame().vertices > 0, "Chrome trace controls did not render");
  };
  draw();
  host.OpenChromeTraceImport();
  draw();
  draw();
  const auto click = [&](std::size_t control) {
    auto point = Access::ChromeTraceControlPosition(host, control);
    Require(point.has_value(), "Actual Chrome trace control absent");
    WindowEvent pointer, button;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    host.ProcessEvents(std::array{pointer});
    draw();
    button.type = WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    host.ProcessEvents(std::array{button});
    draw();
    button.value1 = 0;
    host.ProcessEvents(std::array{button});
    draw();
    draw();
  };
  click(3);
  auto request = host.TakeChromeTraceImportRequest();
  Require(request && request->project == first.Project().id && request->root == first.Root() &&
              request->selection.event_name == "Frame" && request->selection.process == 0 &&
              request->selection.thread == 0 && !host.TakeChromeTraceImportRequest() &&
              !host.TakeGpuImportRequest() && !host.TakeMemoryImportRequest(),
          "Actual Chrome import did not copy explicit scope/selection exactly once");
  const ChromeTraceCapture capture{{0, 0, "Frame"}, {{1, 100, 1.25}, {2, 200, 0}}, 0};
  Require(host.SetImportedChromeTrace(capture), "Valid external static publication failed");
  auto copy = capture;
  copy.selection.event_name = "Changed";
  copy.samples[0].duration_milliseconds = 99;
  const auto imported = [&] { return Access::ImportedChromeTrace(host); };
  Require(imported() && imported()->selection.event_name == "Frame" &&
              imported()->samples.front().duration_milliseconds == 1.25 &&
              profile.Samples().size() == 1,
          "External view borrowed caller or changed live frame measurements");
  for (int kind = 0; kind < 5; ++kind) {
    auto bad = capture;
    if (kind == 0)
      bad.samples.clear();
    if (kind == 1)
      bad.samples[1].sequence = 1;
    if (kind == 2)
      bad.samples[1].start_microseconds = 99;
    if (kind == 3)
      bad.samples[0].duration_milliseconds = std::numeric_limits<double>::infinity();
    if (kind == 4)
      bad.selection.event_name = "bad\n";
    Require(!host.SetImportedChromeTrace(std::move(bad)) && imported() &&
                imported()->samples.front().duration_milliseconds == 1.25,
            "Invalid external publication erased the last-good view");
  }
  Require(host.SetImportedProfileCapture({{{1, 1, 0, 0}}, 0}) &&
              host.SetImportedMemoryCapture({{{1, 0, 1}}, 0}),
          "Independent static fixtures failed");
  profile.Clear();
  draw();
  Require(imported() && profile.Samples().empty(), "Live Clear erased external static capture");
  click(4);
  Require(!imported() && Access::ImportedProfileCapture(host) &&
              Access::ImportedMemoryCapture(host),
          "External Clear erased other static imports");
  Require(host.SetImportedChromeTrace(capture) &&
              observer.Open(first.Root(), ProjectAccess::ReadOnly),
          "Read-only fixture failed");
  workspace = &observer;
  draw();
  click(3);
  Require(host.TakeChromeTraceImportRequest().has_value(),
          "Read-only inspection blocked external import");
  workspace = &first;
  draw();
  host.RequestCloseConfirmation();
  draw();
  click(3);
  Require(!host.TakeChromeTraceImportRequest(), "Close modal emitted external import");
  workspace = &second;
  draw();
  Require(!imported() && !host.TakeChromeTraceImportRequest() &&
              !Access::ChromeTraceControlPosition(host, 3),
          "Project switch retained external view/request/window");
  host.OpenChromeTraceImport();
  draw();
  draw();
  Require(host.SetImportedChromeTrace(capture), "Second project static publication failed");
  workspace = nullptr;
  draw();
  Require(!imported() && !host.TakeChromeTraceImportRequest() &&
              !host.SetImportedChromeTrace(capture),
          "Detached scope retained external capture or accepted publication");
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
