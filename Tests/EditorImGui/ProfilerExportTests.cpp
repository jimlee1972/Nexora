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
#include <vector>

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
void Run(float dpi, const std::filesystem::path &fixture = {}) {
  using namespace nexora;
  using namespace editor;
  TemporaryProject temporary;
  ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(temporary.root, "Profiler", &error), "project create failed");
  ProfileSession profile{2};
  Require(profile.Add({1, 3, 0, 0}) && profile.Add({2, 4.25, 0, 0}) && profile.Add({3, 2.5, 0, 0}),
          "samples failed");
  const auto previous_locale = std::locale();
  std::locale::global(std::locale(previous_locale, new CommaDecimal));
  const bool exported =
      workspace.ExportEditorFrameProcessing(profile.Samples(), profile.DroppedCount(), &error);
  const bool json_exported =
      workspace.ExportEditorFrameProcessingJson(profile.Samples(), profile.DroppedCount(), &error);
  std::locale::global(previous_locale);
  const auto output = temporary.root / ".nexora/frame-processing.csv";
  const auto expected = "frame,frame_processing_wall_ms,older_frames_dropped,gpu_ms,memory_bytes\n"
                        "2,4.25,1,,\n3,2.5,1,,\n";
  Require(exported && Read(output) == expected, "CSV precision, scope or unavailable cells failed");
  const auto json_output = temporary.root / ".nexora/frame-processing.json";
  const auto json_expected = Read(json_output);
  Require(json_exported && json_expected.find("\"schema\": 1") != std::string::npos &&
              json_expected.find("\"project_uuid\": \"" + workspace.Project().id.ToString()) !=
                  std::string::npos &&
              json_expected.find("\"frame_processing_wall_ms\": 4.25") != std::string::npos &&
              json_expected.find("\"gpu_ms\": null") != std::string::npos,
          "JSON locale, schema, project identity or unavailable data failed");
  Require(!workspace.ExportEditorFrameProcessingJson({}, 0, &error) &&
              Read(json_output) == json_expected,
          "empty JSON export changed the last-good file");
  for (const auto &bad :
       {std::vector<FrameSample>{{0, 1, 0, 0}}, std::vector<FrameSample>{{1, -1, 0, 0}},
        std::vector<FrameSample>{{1, std::numeric_limits<double>::infinity(), 0, 0}},
        std::vector<FrameSample>{{1, std::numeric_limits<double>::quiet_NaN(), 0, 0}},
        std::vector<FrameSample>{{1, 1, 0, 0}, {1, 2, 0, 0}},
        std::vector<FrameSample>{{2, 1, 0, 0}, {1, 2, 0, 0}},
        std::vector<FrameSample>(601, {1, 1, 0, 0})}) {
    Require(!workspace.ExportEditorFrameProcessingJson(bad, 1, &error) && !error.empty() &&
                Read(json_output) == json_expected,
            "invalid JSON samples replaced the last-good file");
  }
  const auto json_stage = json_output.string() + ".tmp";
  std::ofstream(json_stage) << "occupied JSON staging";
  Require(!workspace.ExportEditorFrameProcessingJson(profile.Samples(), 1, &error) &&
              Read(json_stage) == "occupied JSON staging" && Read(json_output) == json_expected,
          "JSON export overwrote an occupied stage or last-good file");
  std::filesystem::remove(json_stage);
  const auto backup = temporary.root / "last-good.json";
  std::filesystem::rename(json_output, backup);
  std::filesystem::create_directory(json_output);
  Require(!workspace.ExportEditorFrameProcessingJson(profile.Samples(), 1, &error) &&
              std::filesystem::is_directory(json_output) && Read(backup) == json_expected &&
              !std::filesystem::exists(json_stage),
          "failed JSON replacement removed the destination or left owned staging");
  std::filesystem::remove(json_output);
  std::filesystem::rename(backup, json_output);
  std::vector<FrameSample> maximum;
  for (std::uint64_t frame = 1; frame <= 600; ++frame)
    maximum.push_back({frame, 0.125, 999, 999});
  Require(workspace.ExportEditorFrameProcessingJson(maximum, 1, &error) &&
              Read(json_output).find("\"sample_count\": 600") != std::string::npos &&
              workspace.ExportEditorFrameProcessingJson(profile.Samples(), 1, &error),
          "maximum JSON capture or retry failed");
  Require(!workspace.ExportEditorFrameProcessing({}, 0, &error), "empty export accepted");
  const std::array invalid{FrameSample{4, std::numeric_limits<double>::quiet_NaN(), 0, 0}};
  Require(!workspace.ExportEditorFrameProcessing(invalid, 0, &error) && Read(output) == expected,
          "invalid export replaced the last good file");
  std::ofstream(temporary.root / ".nexora/workspace.recovery") << "schema=1\n";
  Require(!workspace.ExportEditorFrameProcessingJson(profile.Samples(), 1, &error) &&
              Read(json_output) == json_expected,
          "JSON export bypassed recovery gating");
  Require(!workspace.ExportEditorFrameProcessing(profile.Samples(), 1, &error) &&
              Read(output) == expected && workspace.DiscardRecovery(&error),
          "recovery export mutated the file");
  ProjectWorkspace observer;
  Require(observer.Open(temporary.root, ProjectAccess::ReadOnly, &error) &&
              !observer.ExportEditorFrameProcessing(profile.Samples(), 1, &error) &&
              Read(output) == expected,
          "read-only export wrote a file");
  Require(!observer.ExportEditorFrameProcessingJson(profile.Samples(), 1, &error) &&
              Read(json_output) == json_expected,
          "read-only JSON export wrote a file");
  imgui::EditorImGuiHost host;
  host.SetDisplay(1280 * dpi, 900 * dpi, dpi);
  imgui::EditorImGuiTestAccess::ConfigureSyntheticInput(host);
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
  const auto click = [&](bool json = false) {
    const auto point = json ? imgui::EditorImGuiTestAccess::ProfileJsonExportPosition(host)
                            : imgui::EditorImGuiTestAccess::ProfileExportPosition(host);
    Require(point.has_value(), "Profiler export button not visible");
    Nexora::Window::WindowEvent pointer;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
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
  Require(!host.TakeProfileJsonExportRequest(), "CSV click emitted a JSON request");
  click(true);
  Require(host.TakeProfileJsonExportRequest() && !host.TakeProfileJsonExportRequest() &&
              !host.TakeProfileExportRequest(),
          "JSON click was not an independent one-shot request");
  host.RequestCloseConfirmation();
  draw();
  click(true);
  Require(!host.TakeProfileJsonExportRequest(), "close confirmation emitted JSON export");
  Nexora::Window::WindowEvent escape;
  escape.type = Nexora::Window::WindowEventType::Key;
  escape.value0 = static_cast<int>(Nexora::Window::Key::Escape);
  escape.value1 = 1;
  host.ProcessEvents(std::array{escape});
  draw();
  escape.value1 = 0;
  host.ProcessEvents(std::array{escape});
  draw();
  Require(host.TakeCloseChoice() == imgui::CloseChoice::Cancel,
          "close-confirmation fixture did not cancel");
  imgui::EditorImGuiTestAccess::FocusProfiler(host);
  draw();
  draw();
  active = &observer;
  draw();
  click();
  Require(!host.TakeProfileExportRequest(), "read-only button emitted export");
  click(true);
  Require(!host.TakeProfileJsonExportRequest(), "read-only button emitted JSON export");
  active = &workspace;
  profile.Clear();
  draw();
  click();
  Require(!host.TakeProfileExportRequest(), "empty capture emitted export");
  click(true);
  Require(!host.TakeProfileJsonExportRequest(), "empty capture emitted JSON export");
  Require(profile.Add({4, 1, 0, 0}), "recovery sample fixture failed");
  if (dpi == 1)
    std::ofstream(temporary.root / ".nexora/workspace.recovery") << "schema=1\n";
  else
    std::filesystem::create_directory(temporary.root / ".nexora/workspace.recovery");
  draw();
  click(true);
  Require(!host.TakeProfileJsonExportRequest() && workspace.DiscardRecovery(&error),
          "recovery prompt emitted JSON export");
  if (!fixture.empty()) {
    const std::array precise{
        FrameSample{9007199254740993ULL, 1.2345678901234567, 99, 99},
        FrameSample{9007199254740994ULL, std::numeric_limits<double>::min(), 99, 99},
        FrameSample{std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<double>::max(),
                    99, 99}};
    std::locale::global(std::locale(previous_locale, new CommaDecimal));
    const auto result = workspace.ExportEditorFrameProcessingJson(
        precise, std::numeric_limits<std::uint64_t>::max(), &error);
    std::locale::global(previous_locale);
    Require(result, "precise JSON fixture export failed");
    std::filesystem::copy_file(json_output, fixture);
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    Run(1, argc == 2 ? std::filesystem::path{argv[1]} : std::filesystem::path{});
    Run(2);
    std::cout << "Profiler CSV/JSON ownership and UI contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
