#include "EditorImGuiTestAccess.h"
#include <array>
#include <chrono>
#include <filesystem>
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
        ("nexora-gpu-display-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Temporary() {
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
    }
  } temp;
  ProjectWorkspace first, second;
  std::string error;
  Require(first.Create(temp.root / "first", "First", &error) &&
              second.Create(temp.root / "second", "Second", &error),
          "create failed");
  ProfileSession profile;
  auto *active_profile = &profile;
  auto *project = &first;
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
                          active_profile);
    Require(host.EndFrame().vertices > 0, "GPU panel did not draw");
  };
  draw();
  draw();
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  Require(!EditorImGuiTestAccess::ProfileGpu(host).milliseconds,
          "unsampled GPU fabricated a value");
  constexpr auto source = GpuProfileSource::VulkanTimestamps;
  Require(profile.ObserveGpuFrame(1, source, true, {1, 1.25}),
          "initial GPU owner observation failed");
  draw();
  const auto copied = EditorImGuiTestAccess::ProfileGpu(host);
  Require(profile.ObserveGpuFrame(1, source, true, {2, 2.5}) &&
              EditorImGuiTestAccess::ProfileGpu(host).milliseconds == 1.25,
          "host retained mutable observation borrow");
  draw();
  Require(EditorImGuiTestAccess::ProfileGpu(host).milliseconds == 2.5 &&
              copied.milliseconds == 1.25 && copied.source == source && copied.software_rasterizer,
          "copied source/value/ownership failed");
  const auto click = [&](bool clear) {
    const auto point = clear ? EditorImGuiTestAccess::ProfileClearPosition(host)
                             : EditorImGuiTestAccess::ProfileCapturePosition(host);
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
  click(false);
  Require(!profile.Capturing() && !profile.ObserveGpuFrame(1, source, true, {3, 9}) &&
              profile.GpuSamples().size() == 2,
          "Capture did not pause GPU ingestion");
  click(true);
  Require(!profile.Capturing() && profile.GpuSamples().empty() &&
              !EditorImGuiTestAccess::ProfileGpu(host).milliseconds &&
              !EditorImGuiTestAccess::ProfileGpu(host).observed_peak_ms,
          "Clear did not reset GPU history while paused");
  click(false);
  Require(!profile.ObserveGpuFrame(1, source, true, {3, 9}) &&
              profile.ObserveGpuFrame(1, source, true, {4, 0}),
          "resume replayed old completion or confused zero/unavailable");
  draw();
  Require(EditorImGuiTestAccess::ProfileGpu(host).milliseconds == 0,
          "measured GPU zero displayed unavailable");
  Require(profile.ObserveGpuFrame(1, source, true, {5, std::nullopt}),
          "unavailable completion rejected");
  draw();
  Require(!EditorImGuiTestAccess::ProfileGpu(host).milliseconds &&
              EditorImGuiTestAccess::ProfileGpu(host).observed_peak_ms == 0,
          "unavailable result displayed old measurement or lost historical peak");
  project = &second;
  draw();
  project = nullptr;
  draw();
  Require(profile.GpuSamples().size() == 2 &&
              EditorImGuiTestAccess::ProfileGpu(host).completed_submission == 5,
          "project switch/detach cleared process surface history");
  Require(profile.ObserveGpuFrame(2, GpuProfileSource::MetalCommandBuffer, false, {1, 7}),
          "new domain completion failed");
  draw();
  Require(EditorImGuiTestAccess::ProfileGpu(host).source == GpuProfileSource::MetalCommandBuffer &&
              !EditorImGuiTestAccess::ProfileGpu(host).software_rasterizer &&
              profile.GpuSamples().size() == 1,
          "domain/source transition retained previous stream");
  active_profile = nullptr;
  draw();
  Require(!EditorImGuiTestAccess::ProfileGpu(host).milliseconds &&
              EditorImGuiTestAccess::ProfileGpu(host).source == GpuProfileSource::Unavailable,
          "detached owner retained stale measurements");
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
