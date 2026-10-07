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
  std::ifstream file(path, std::ios::binary);
  std::ostringstream value;
  value << file.rdbuf();
  return value.str();
}
void VerifyCsvImport(nexora::editor::ProjectWorkspace &workspace) {
  using namespace nexora::editor;
  namespace fs = std::filesystem;
  const auto output = workspace.Root() / ".nexora/frame-processing.csv";
  const auto original = Read(output);
  const std::string header =
      "frame,frame_processing_wall_ms,older_frames_dropped,gpu_ms,memory_bytes\n";
  auto stage = output;
  stage += ".tmp";
  std::ofstream(stage, std::ios::binary) << "unrelated staging";
  const auto write = [&](std::string_view bytes) {
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  };
  std::string error = "stale";
  const auto ordinary = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(ordinary && error.empty() && ordinary->samples.size() == 2 &&
              ordinary->samples.front().frame == 2 && ordinary->samples.front().cpu_ms == 4.25 &&
              ordinary->older_frames_dropped == 1 && Read(output) == original &&
              Read(stage) == "unrelated staging",
          "CSV import round trip, stale error or non-mutating ownership failed");
  const auto reject = [&](const std::string &bytes) {
    write(bytes);
    error = "stale";
    Require(!workspace.ImportEditorFrameProcessingCsv(&error) && !error.empty() &&
                error != "stale" && Read(output) == bytes && Read(stage) == "unrelated staging",
            "invalid CSV was accepted or import mutated source/staging");
  };
  for (const auto *rows : {"",
                           "0,1,0,,\n",
                           "1,-1,0,,\n",
                           "1,nan,0,,\n",
                           "1,inf,0,,\n",
                           "1,1e999,0,,\n",
                           "1,1.5suffix,0,,\n",
                           "1,+1,0,,\n",
                           " 1,1,0,,\n",
                           "1, 1,0,,\n",
                           "1,1, 0,,\n",
                           "1,1,0,0,\n",
                           "1,1,0,,0\n",
                           "1,1,0,,,\n",
                           "1,1,0,\n",
                           "1,1,0,,",
                           "\n",
                           "\"1\",1,0,,\n",
                           "18446744073709551616,1,0,,\n",
                           "1,1,18446744073709551616,,\n",
                           "1,1,0,,\n1,2,0,,\n",
                           "2,1,0,,\n1,2,0,,\n",
                           "1,1,0,,\n2,2,1,,\n"})
    reject(header + rows);
  reject("");
  reject("unsupported\n1,1,0,,\n");
  reject(header + std::string("1,1\0,0,,\n", 9));
  std::string maximum = header;
  for (std::uint64_t frame = 1; frame <= 600; ++frame)
    maximum += std::to_string(frame) + ",0.25,0,,\n";
  write(maximum);
  auto capture = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(capture && capture->samples.size() == 600, "600-frame import rejected");
  reject(maximum + "601,0.25,0,,\n");
  const auto exact = header + "1," +
                     std::string(ProjectWorkspace::kMaximumFrameProcessingCsvBytes - header.size() -
                                     std::string_view("1,,0,,\n").size(),
                                 '0') +
                     ",0,,\n";
  write(exact);
  capture = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(capture && capture->samples.front().cpu_ms == 0 &&
              exact.size() == ProjectWorkspace::kMaximumFrameProcessingCsvBytes,
          "exact-byte-limit CSV was rejected");
  reject(exact + "\n");
  const std::array precise{
      FrameSample{9007199254740993ULL, 1.2345678901234567, 99, 99},
      FrameSample{9007199254740994ULL, std::numeric_limits<double>::min(), 99, 99},
      FrameSample{9007199254740995ULL, std::numeric_limits<double>::denorm_min(), 99, 99},
      FrameSample{std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<double>::max(), 99,
                  99}};
  fs::remove(stage);
  Require(workspace.ExportEditorFrameProcessing(precise, std::numeric_limits<std::uint64_t>::max(),
                                                &error),
          "precise CSV fixture export failed");
  const auto previous_locale = std::locale();
  std::locale::global(std::locale(previous_locale, new CommaDecimal));
  capture = workspace.ImportEditorFrameProcessingCsv(&error);
  std::locale::global(previous_locale);
  Require(capture && capture->samples.size() == precise.size() &&
              capture->older_frames_dropped == std::numeric_limits<std::uint64_t>::max(),
          "lossless uint64/locale-independent CSV import failed");
  for (std::size_t index = 0; index < precise.size(); ++index)
    Require(capture->samples[index].frame == precise[index].frame &&
                capture->samples[index].cpu_ms == precise[index].cpu_ms &&
                capture->samples[index].gpu_ms == 0 && capture->samples[index].memory_bytes == 0,
            "CSV import lost double precision or invented GPU/memory data");
  write("frame,frame_processing_wall_ms,older_frames_dropped,gpu_ms,memory_bytes\r\n1,1.5,0,,\r\n");
  capture = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(capture && capture->samples.front().cpu_ms == 1.5, "CRLF CSV rejected");
  const auto backup = workspace.Root() / "csv-backup";
  fs::rename(output, backup);
  Require(!workspace.ImportEditorFrameProcessingCsv(&error) && !error.empty(),
          "missing CSV accepted");
  fs::create_directory(output);
  Require(!workspace.ImportEditorFrameProcessingCsv(&error) && fs::is_directory(output),
          "CSV directory accepted or removed");
  fs::remove(output);
#if !defined(_WIN32)
  for (const auto &target : {backup, workspace.Root() / "missing-csv"}) {
    fs::create_symlink(target, output);
    Require(!workspace.ImportEditorFrameProcessingCsv(&error) && fs::is_symlink(output),
            "aliased CSV accepted or removed");
    fs::remove(output);
  }
#endif
  fs::rename(backup, output);
#if !defined(_WIN32)
  const auto metadata = workspace.Root() / ".nexora";
  const auto metadata_backup = workspace.Root() / "metadata-backup";
  fs::rename(metadata, metadata_backup);
  fs::create_directory_symlink(metadata_backup, metadata);
  Require(!workspace.ImportEditorFrameProcessingCsv(&error) && fs::is_symlink(metadata),
          "aliased CSV parent accepted or removed");
  fs::remove(metadata);
  fs::rename(metadata_backup, metadata);
#endif
  write(original);
  Require(workspace.ImportEditorFrameProcessingCsv(&error) && error.empty(),
          "valid CSV retry failed");
  ProjectWorkspace empty;
  Require(!empty.ImportEditorFrameProcessingCsv(&error) && !error.empty(),
          "closed workspace imported a CWD-relative CSV");
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
  VerifyCsvImport(workspace);
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
  Require(!workspace.ImportEditorFrameProcessingCsv(&error) && Read(output) == expected,
          "CSV import bypassed recovery gating");
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
  Require(observer.ImportEditorFrameProcessingCsv(&error) && error.empty() &&
              Read(output) == expected,
          "read-only observer could not import without writes");
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
  const auto click = [&](int control = 0) {
    const auto point = control == 3 ? imgui::EditorImGuiTestAccess::ProfileImportClearPosition(host)
                       : control == 2 ? imgui::EditorImGuiTestAccess::ProfileCsvImportPosition(host)
                       : control == 1
                           ? imgui::EditorImGuiTestAccess::ProfileJsonExportPosition(host)
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
  click(2);
  Require(host.TakeProfileCsvImportRequest() && !host.TakeProfileCsvImportRequest() &&
              !host.TakeProfileExportRequest() && !host.TakeProfileJsonExportRequest(),
          "CSV import click was not an independent one-shot request");
  auto loaded = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(loaded && host.SetImportedProfileCapture(*loaded), "valid snapshot publication failed");
  loaded->samples.front().cpu_ms = 999;
  draw();
  const auto *imported = imgui::EditorImGuiTestAccess::ImportedProfileCapture(host);
  Require(imported && imported->samples.front().cpu_ms == 4.25 &&
              imported->older_frames_dropped == 1 && profile.Samples().size() == 2 &&
              profile.Samples().front().cpu_ms == 4.25 && profile.Capturing(),
          "import borrowed caller data or changed live capture");
  Require(!host.SetImportedProfileCapture({{{1, -1, 0, 0}}, 0}) &&
              imgui::EditorImGuiTestAccess::ImportedProfileCapture(host)->samples.front().cpu_ms ==
                  4.25,
          "invalid snapshot replaced the previous imported trace");
  click(3);
  Require(!imgui::EditorImGuiTestAccess::ImportedProfileCapture(host) &&
              profile.Samples().size() == 2 && profile.DroppedCount() == 1,
          "clear imported changed live history");
  host.RequestCloseConfirmation();
  draw();
  click(true);
  Require(!host.TakeProfileJsonExportRequest(), "close confirmation emitted JSON export");
  click(2);
  Require(!host.TakeProfileCsvImportRequest(), "close confirmation emitted CSV import");
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
  click(2);
  Require(host.TakeProfileCsvImportRequest(), "read-only CSV import button was disabled");
  auto observed = observer.ImportEditorFrameProcessingCsv(&error);
  Require(observed && host.SetImportedProfileCapture(*observed),
          "read-only snapshot publication failed");
  click(2);
  active = nullptr;
  draw();
  Require(!imgui::EditorImGuiTestAccess::ImportedProfileCapture(host) &&
              !host.TakeProfileCsvImportRequest(),
          "project detach retained the prior imported trace or stale request");
  click(2);
  Require(!host.TakeProfileCsvImportRequest(), "no-project CSV import button was enabled");
  active = &workspace;
  profile.Clear();
  draw();
  click();
  Require(!host.TakeProfileExportRequest(), "empty capture emitted export");
  click(true);
  Require(!host.TakeProfileJsonExportRequest(), "empty capture emitted JSON export");
  click(2);
  Require(host.TakeProfileCsvImportRequest(), "empty live history prevented saved CSV import");
  loaded = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(loaded && host.SetImportedProfileCapture(*loaded) && profile.Samples().empty(),
          "saved CSV import populated live history");
  const auto imported_cpu =
      imgui::EditorImGuiTestAccess::ImportedProfileCapture(host)->samples.front().cpu_ms;
  std::ofstream(output, std::ios::binary | std::ios::trunc) << "corrupt CSV\n";
  loaded = workspace.ImportEditorFrameProcessingCsv(&error);
  Require(!loaded &&
              imgui::EditorImGuiTestAccess::ImportedProfileCapture(host)->samples.front().cpu_ms ==
                  imported_cpu,
          "failed CSV load changed the prior imported trace");
  std::ofstream(output, std::ios::binary | std::ios::trunc) << expected;
  Require(profile.Add({4, 1, 0, 0}), "recovery sample fixture failed");
  if (dpi == 1)
    std::ofstream(temporary.root / ".nexora/workspace.recovery") << "schema=1\n";
  else
    std::filesystem::create_directory(temporary.root / ".nexora/workspace.recovery");
  draw();
  click(true);
  click(2);
  Require(!host.TakeProfileCsvImportRequest(), "recovery prompt emitted CSV import");
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
