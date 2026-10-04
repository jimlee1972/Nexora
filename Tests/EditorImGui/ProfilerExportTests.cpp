#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/EditorProduction.h"
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct TemporaryProject final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-profiler-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TemporaryProject() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};
struct CommaDecimal final : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
};
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::ostringstream value;
  value << file.rdbuf();
  return value.str();
}
} // namespace
int main() {
  try {
    using namespace nexora;
    using namespace editor;
    TemporaryProject temporary;
    ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(temporary.root, "Profiler", &error), "project create failed");
    ProfileSession profile{2};
    Require(profile.Add({1, 3, 0, 0}) && profile.Add({2, 4.25, 0, 0}) &&
                profile.Add({3, 2.5, 0, 0}),
            "samples failed");
    const auto previous_locale = std::locale();
    std::locale::global(std::locale(previous_locale, new CommaDecimal));
    const bool exported =
        workspace.ExportEditorFrameProcessing(profile.Samples(), profile.DroppedCount(), &error);
    std::locale::global(previous_locale);
    const auto output = temporary.root / ".nexora/frame-processing.csv";
    const auto expected =
        "frame,frame_processing_wall_ms,older_frames_dropped,gpu_ms,memory_bytes\n"
        "2,4.25,1,,\n3,2.5,1,,\n";
    Require(exported && Read(output) == expected,
            "CSV precision, scope or unavailable cells failed");
    Require(!workspace.ExportEditorFrameProcessing({}, 0, &error), "empty export accepted");
    const std::array invalid{FrameSample{4, std::numeric_limits<double>::quiet_NaN(), 0, 0}};
    Require(!workspace.ExportEditorFrameProcessing(invalid, 0, &error) && Read(output) == expected,
            "invalid export replaced the last good file");
    std::ofstream(temporary.root / ".nexora/workspace.recovery") << "schema=1\n";
    Require(!workspace.ExportEditorFrameProcessing(profile.Samples(), 1, &error) &&
                Read(output) == expected && workspace.DiscardRecovery(&error),
            "recovery export mutated the file");
    ProjectWorkspace observer;
    Require(observer.Open(temporary.root, ProjectAccess::ReadOnly, &error) &&
                !observer.ExportEditorFrameProcessing(profile.Samples(), 1, &error) &&
                Read(output) == expected,
            "read-only export wrote a file");
    imgui::EditorImGuiHost host;
    host.SetDisplay(1280, 900, 1);
    imgui::EditorImGuiTestAccess::SetInputTrickle(host, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    host.ProcessEvents(std::array{focus});
    ProductShell shell;
    ProjectWorkspace *active = &workspace;
    const auto draw = [&] {
      host.BeginFrame();
      host.DrawProductShell(shell, nullptr, active, nullptr, nullptr, nullptr, nullptr, nullptr,
                            &profile);
      static_cast<void>(host.EndFrame());
    };
    draw();
    draw();
    imgui::EditorImGuiTestAccess::FocusProfiler(host);
    draw();
    draw();
    const auto click = [&] {
      const auto point = imgui::EditorImGuiTestAccess::ProfileExportPosition(host);
      Require(point.has_value(), "Profiler export button not visible");
      Nexora::Window::WindowEvent pointer;
      pointer.type = Nexora::Window::WindowEventType::Pointer;
      pointer.value0 = static_cast<int>((*point)[0]);
      pointer.value1 = static_cast<int>((*point)[1]);
      Nexora::Window::WindowEvent button;
      button.type = Nexora::Window::WindowEventType::PointerButton;
      button.value0 = 0;
      button.value1 = 1;
      host.ProcessEvents(std::array{pointer, button});
      draw();
      button.value1 = 0;
      host.ProcessEvents(std::array{button});
      draw();
    };
    click();
    Require(host.TakeProfileExportRequest() && !host.TakeProfileExportRequest(),
            "Export click was not one-shot");
    active = &observer;
    draw();
    click();
    Require(!host.TakeProfileExportRequest(), "read-only button emitted export");
    active = &workspace;
    profile.Clear();
    draw();
    click();
    Require(!host.TakeProfileExportRequest(), "empty capture emitted export");
    std::cout << "Profiler CSV ownership and UI contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
