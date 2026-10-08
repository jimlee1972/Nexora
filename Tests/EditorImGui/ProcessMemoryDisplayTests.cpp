#include "EditorImGuiTestAccess.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
std::optional<std::uint64_t> reading;
std::uint64_t reads = 0;
std::optional<std::uint64_t> Read() noexcept {
  ++reads;
  return reading;
}
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float dpi) {
  using namespace nexora::editor;
  using imgui::EditorImGuiTestAccess;
  using Nexora::Window::WindowEvent;
  using Nexora::Window::WindowEventType;
  using namespace std::chrono_literals;
  struct TemporaryProjects final {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("nexora-memory-display-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryProjects() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } temporary;
  ProjectWorkspace first, second;
  std::string error;
  Require(first.Create(temporary.root / "first", "First", &error) &&
              second.Create(temporary.root / "second", "Second", &error),
          "temporary project creation failed");
  ProfileSession profile;
  ProfileSession *active_profile = &profile;
  ProjectWorkspace *active_project = &first;
  imgui::EditorImGuiHost host;
  host.SetDisplay(1280 * dpi, 900 * dpi, dpi);
  EditorImGuiTestAccess::ConfigureSyntheticInput(host);
  WindowEvent focus;
  focus.type = WindowEventType::FocusChanged;
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  ProductShell shell;
  const auto draw = [&] {
    const auto before = reads;
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, active_project, nullptr, nullptr, nullptr, nullptr,
                          nullptr, active_profile);
    const auto frame = host.EndFrame();
    Require(frame.vertices > 0 && reads == before, "widgets performed OS reads or did not draw");
  };
  draw();
  draw();
  EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  Require(!EditorImGuiTestAccess::ProfileMemory(host).resident_bytes,
          "unsampled memory fabricated a value");
  const auto start = std::chrono::steady_clock::time_point{};
  reading = 123456789;
  Require(profile.SampleProcessMemory(start, Read) && profile.Add({1, 4.5, 0, 0}),
          "owner memory sample failed");
  draw();
  const auto copied = EditorImGuiTestAccess::ProfileMemory(host);
  reading = 987654321;
  Require(profile.SampleProcessMemory(start + 250ms, Read) &&
              EditorImGuiTestAccess::ProfileMemory(host).resident_bytes == copied.resident_bytes,
          "host retained a mutable owner observation borrow");
  draw();
  Require(EditorImGuiTestAccess::ProfileMemory(host).resident_bytes == reading &&
              EditorImGuiTestAccess::ProfileMemory(host).observed_peak_bytes == reading,
          "next frame did not publish copied observation");
  const auto click = [&](bool clear) {
    const auto point = clear ? EditorImGuiTestAccess::ProfileClearPosition(host)
                             : EditorImGuiTestAccess::ProfileCapturePosition(host);
    Require(point.has_value(), "Profiler memory control was not visible");
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
  Require(!profile.Capturing() && !profile.SampleProcessMemory(start + 1s, Read) &&
              EditorImGuiTestAccess::ProfileMemory(host).resident_bytes == reading,
          "Capture control did not pause memory while retaining its value");
  click(true);
  const auto cleared = EditorImGuiTestAccess::ProfileMemory(host);
  Require(!cleared.resident_bytes && !cleared.observed_peak_bytes && cleared.attempts == 0 &&
              !profile.Capturing() && profile.Samples().empty(),
          "Clear control did not reset memory and wall history while staying paused");
  click(false);
  reading = 10;
  Require(profile.SampleProcessMemory(start + 1s, Read), "resume did not resample cleared history");
  draw();
  active_project = &second;
  draw();
  Require(EditorImGuiTestAccess::ProfileMemory(host).resident_bytes == 10 &&
              EditorImGuiTestAccess::ProfileMemory(host).observed_peak_bytes == 10,
          "project switch reset process-wide memory observations");
  active_project = nullptr;
  draw();
  Require(EditorImGuiTestAccess::ProfileMemory(host).resident_bytes == 10,
          "detaching project discarded process-wide observation");
  reading.reset();
  Require(profile.SampleProcessMemory(start + 1250ms, Read), "unavailable owner read rejected");
  draw();
  Require(!EditorImGuiTestAccess::ProfileMemory(host).resident_bytes &&
              EditorImGuiTestAccess::ProfileMemory(host).observed_peak_bytes == 10,
          "unavailable observation was displayed as latest historical success");
  active_profile = nullptr;
  draw();
  Require(!EditorImGuiTestAccess::ProfileMemory(host).observed_peak_bytes &&
              EditorImGuiTestAccess::ProfileMemory(host).attempts == 0,
          "detaching session retained stale memory observations");
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
