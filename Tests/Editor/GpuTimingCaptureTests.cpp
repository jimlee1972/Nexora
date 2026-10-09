#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <array>
#include <chrono>
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
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}
struct CommaDecimal : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
};
void Run(const std::filesystem::path &fixture) {
  using namespace nexora::editor;
  namespace fs = std::filesystem;
  struct Temporary {
    fs::path root = fs::temp_directory_path() /
                    ("nexora-gpu-capture-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Temporary() {
      std::error_code ec;
      fs::remove_all(root, ec);
    }
  } temp;
  ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(temp.root, "GPU", &error), "create failed");
  const auto output = temp.root / ".nexora/gpu-timing.json";
  const std::array samples{
      GpuProfileSample{9007199254740993ULL, 0}, GpuProfileSample{9007199254740994ULL, std::nullopt},
      GpuProfileSample{std::numeric_limits<std::uint64_t>::max(), 250.12345678901234}};
  const auto old_locale = std::locale();
  std::locale::global(std::locale(old_locale, new CommaDecimal));
  const bool exported =
      workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples,
                                    std::numeric_limits<std::uint64_t>::max(), &error);
  std::locale::global(old_locale);
  Require(exported && error.empty(), "export failed");
  const auto original = Read(output);
  const std::array wall{FrameSample{1, 1, 0, 0}};
  const std::array memory{ProcessMemorySample{1, 0, 0}};
  Require(workspace.ExportEditorFrameProcessingJson(wall, 0, &error) &&
              workspace.ExportProcessMemoryJson(memory, 0, &error),
          "independent capture fixtures failed");
  const auto wall_bytes = Read(temp.root / ".nexora/frame-processing.json");
  const auto memory_bytes = Read(temp.root / ".nexora/process-memory.json");
  if (!fixture.empty())
    fs::copy_file(output, fixture);
  auto capture = workspace.ImportGpuTimingJson(&error);
  Require(capture && capture->samples.size() == 3 && capture->samples[0].milliseconds == 0 &&
              !capture->samples[1].milliseconds &&
              capture->source == GpuProfileSource::VulkanTimestamps &&
              capture->software_rasterizer &&
              capture->samples[2].milliseconds == samples[2].milliseconds &&
              capture->older_samples_dropped == std::numeric_limits<std::uint64_t>::max() &&
              error.empty(),
          "round trip precision or unavailable semantics failed");
  auto stage = output;
  stage += ".tmp";
  std::ofstream(stage) << "unrelated stage";
  Require(!workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                         &error) &&
              Read(output) == original && Read(stage) == "unrelated stage",
          "export overwrote occupied stage or last-good file");
  const auto write = [&](std::string_view bytes) {
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  };
  const auto reject = [&](const std::string &bytes) {
    write(bytes);
    error = "stale";
    Require(!workspace.ImportGpuTimingJson(&error) && !error.empty() && error != "stale" &&
                Read(output) == bytes && Read(stage) == "unrelated stage" &&
                capture->samples[2].milliseconds == samples[2].milliseconds,
            "corrupt import accepted or mutated source/staging/prior capture");
  };
  const auto replace = [&](const char *from, const char *to) {
    auto text = original;
    auto at = text.find(from);
    Require(at != std::string::npos, "mutation seed missing");
    text.replace(at, std::string_view(from).size(), to);
    reject(text);
  };
  for (std::size_t length = 0; length < original.find_last_of('}') + 1; ++length)
    reject(original.substr(0, length));
  replace("\"schema\": 1", "\"schema\": 2");
  replace("\"source\": \"NexoraEditor\"", "\"source\": \"Other\"");
  replace("\"metric\": \"completed_gpu_timing\"", "\"metric\": \"cpu_time\"");
  replace("\"scope\": \"native_command_buffer_interval\"", "\"scope\": \"scene\"");
  replace("\"unit\": \"milliseconds\"", "\"unit\": \"seconds\"");
  replace("\"sequence_axis\": \"native_completed_submission_id\"",
          "\"sequence_axis\": \"cpu_frame\"");
  replace("\"timing_source\": \"vulkan_timestamps\"", "\"timing_source\": \"unknown\"");
  replace("\"software_rasterizer\": true", "\"software_rasterizer\": 1");
  replace("\"software_rasterizer\": true", "\"software_rasterizer\": null");
  replace("\"sample_count\": 3", "\"sample_count\": 4");
  replace("\"sample_count\": 3", "\"sample_count\": 3, \"sample_count\": 3");
  replace("\"software_rasterizer\": true", "\"software_rasterizer\": true, \"unknown\": false");
  replace("\"source\": \"NexoraEditor\",", "");
  replace("\"milliseconds\": 0", "\"milliseconds\": -1");
  replace("\"milliseconds\": 0", "\"milliseconds\": 1e999");
  replace("\"milliseconds\": 0", "\"milliseconds\": nan");
  replace("\"milliseconds\": 0", "\"milliseconds\": 01");
  replace("\"submission\": \"9007199254740993\"", "\"submission\": \"0\"");
  replace("\"submission\": \"9007199254740993\"", "\"submission\": \"9007199254740994\"");
  replace("\"milliseconds\": 0", "\"milliseconds\": \"0\"");
  replace("\"submission\": \"9007199254740993\"", "\"submission\": 9007199254740993");
  replace("\"submission\": \"9007199254740993\"", "\"submission\": \"+1\"");
  replace("\"submission\": \"9007199254740993\"", "\"submission\": \"18446744073709551616\"");
  reject(original + "{}");
  reject(original + std::string(1, '\0'));
  auto wrong_project = original;
  auto id = wrong_project.find(workspace.Project().id.ToString());
  wrong_project[id] = wrong_project[id] == '0' ? '1' : '0';
  reject(wrong_project);
  write(original +
        std::string(ProjectWorkspace::kMaximumGpuTimingJsonBytes - original.size(), ' '));
  Require(workspace.ImportGpuTimingJson(&error).has_value(), "exact byte budget rejected");
  reject(original +
         std::string(ProjectWorkspace::kMaximumGpuTimingJsonBytes - original.size() + 1, ' '));
  for (std::size_t i = 0; i < original.size(); ++i) {
    auto mutated = original;
    mutated[i] ^= 1;
    write(mutated);
    auto attempt = workspace.ImportGpuTimingJson(&error);
    Require((attempt || !error.empty()) && Read(output) == mutated &&
                Read(stage) == "unrelated stage",
            "mutation import failed preservation/error contract");
  }
  auto escaped = original;
  const auto source_at = escaped.find("NexoraEditor");
  escaped.replace(source_at, 1, "\\u004e");
  write(escaped);
  Require(workspace.ImportGpuTimingJson(&error).has_value(), "equivalent JSON escape rejected");
  write(original);
  fs::remove(stage);
  for (const auto &bad :
       {std::vector<GpuProfileSample>{}, std::vector<GpuProfileSample>{{0, 1}},
        std::vector<GpuProfileSample>{{1, -1}},
        std::vector<GpuProfileSample>{{1, std::numeric_limits<double>::infinity()}},
        std::vector<GpuProfileSample>{{1, std::numeric_limits<double>::quiet_NaN()}},
        std::vector<GpuProfileSample>{{1, 0}, {1, 2}},
        std::vector<GpuProfileSample>{{2, 0}, {1, 2}}, std::vector<GpuProfileSample>(601, {1, 0})})
    Require(
        !workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, bad, 0, &error) &&
            Read(output) == original,
        "invalid export changed last-good file");
  for (auto bad_source : {GpuProfileSource::Unavailable, static_cast<GpuProfileSource>(255)})
    Require(!workspace.ExportGpuTimingJson(bad_source, false, samples, 0, &error) &&
                Read(output) == original,
            "unsupported source exported or changed last-good file");
  std::vector<GpuProfileSample> maximum;
  for (std::uint64_t i = 1; i <= 600; ++i)
    maximum.push_back({i, std::numeric_limits<double>::max()});
  Require(
      workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, maximum, 3, &error) &&
          workspace.ImportGpuTimingJson(&error)->samples.size() == 600 &&
          workspace.ImportGpuTimingJson(&error)->samples.back().milliseconds ==
              std::numeric_limits<double>::max(),
      "maximum sample/finite numeric budget rejected");
  auto too_many = Read(output);
  auto at = too_many.rfind(']');
  too_many.insert(at, ",{\"submission\":\"601\",\"milliseconds\":0}");
  std::ofstream(stage) << "unrelated stage";
  reject(too_many); // A valid declared count cannot permit an oversized actual array.
  at = too_many.find("\"sample_count\": 600");
  too_many.replace(at, 19, "\"sample_count\": 601");
  reject(too_many);
  fs::remove(stage);
  write(original);
  ProjectWorkspace observer;
  Require(observer.Open(temp.root, ProjectAccess::ReadOnly, &error) &&
              observer.ImportGpuTimingJson(&error) &&
              !observer.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                            &error) &&
              Read(output) == original,
          "read-only contract failed");
  std::ofstream(temp.root / ".nexora/workspace.recovery") << "schema=1\n";
  Require(!workspace.ImportGpuTimingJson(&error) &&
              !workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                             &error) &&
              Read(output) == original && workspace.DiscardRecovery(&error),
          "recovery guard failed");
  const auto backup = temp.root / "backup.json";
  fs::rename(output, backup);
  fs::create_directory(output);
  Require(!workspace.ImportGpuTimingJson(&error) &&
              !workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                             &error) &&
              fs::is_directory(output) && Read(backup) == original && !fs::exists(stage),
          "nonregular path or failed replacement contract failed");
  fs::remove(output);
  std::error_code ec;
  fs::create_symlink(backup, output, ec);
  if (!ec)
    Require(!workspace.ImportGpuTimingJson(&error) &&
                !workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                               &error) &&
                Read(backup) == original,
            "aliased file imported");
  fs::remove(output, ec);
  fs::rename(backup, output);
  const std::array unavailable{GpuProfileSample{1, std::nullopt}};
  for (const auto source : {GpuProfileSource::VulkanTimestamps, GpuProfileSource::Dx12Timestamps,
                            GpuProfileSource::MetalCommandBuffer}) {
    Require(workspace.ExportGpuTimingJson(source, false, unavailable, 0, &error),
            "unavailable-only native trace export rejected");
    auto unavailable_capture = workspace.ImportGpuTimingJson(&error);
    Require(unavailable_capture && unavailable_capture->source == source &&
                !unavailable_capture->software_rasterizer &&
                !unavailable_capture->samples.front().milliseconds,
            "unavailable-only trace fabricated timing or source/software identity");
  }
  const auto last_good = Read(output);
  fs::create_directory(stage);
  std::ofstream(stage / "keep") << "unrelated directory";
  Require(!workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                         &error) &&
              Read(output) == last_good && Read(stage / "keep") == "unrelated directory",
          "occupied staging directory or last-good file was changed");
  fs::remove_all(stage);
  Require(Read(temp.root / ".nexora/frame-processing.json") == wall_bytes &&
              Read(temp.root / ".nexora/process-memory.json") == memory_bytes,
          "GPU capture changed independent wall/RSS capture files");
#if !defined(_WIN32)
  // Windows writer leases do not share directory deletion/rename; native aliases also require
  // privileges.
  const auto metadata_backup = temp.root / "metadata-backup";
  observer = ProjectWorkspace{};
  fs::rename(temp.root / ".nexora", metadata_backup);
  fs::create_directory_symlink(metadata_backup, temp.root / ".nexora", ec);
  if (!ec) {
    Require(!workspace.ImportGpuTimingJson(&error) &&
                !workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                               &error),
            "aliased metadata admitted memory I/O");
    fs::remove(temp.root / ".nexora");
  }
  fs::rename(metadata_backup, temp.root / ".nexora");
#endif
  observer = ProjectWorkspace{};
  workspace = ProjectWorkspace{};
  Require(!workspace.ImportGpuTimingJson(&error) &&
              !workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, true, samples, 0,
                                             &error),
          "closed workspace admitted I/O");
}
} // namespace
int main(int argc, char **argv) {
  try {
    Run(argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{});
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
