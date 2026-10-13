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
                    ("nexora-mesh-memory-capture-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Temporary() {
      std::error_code ec;
      fs::remove_all(root, ec);
    }
  } temp;
  ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(temp.root, "Memory", &error), "create failed");
  const auto output = temp.root / ".nexora/mesh-memory.json";
  const std::array samples{
      MeshMemorySample{9007199254740993ULL, 0, 0, 0},
      MeshMemorySample{9007199254740994ULL, 250.12345678901234, std::nullopt, std::nullopt},
      MeshMemorySample{std::numeric_limits<std::uint64_t>::max(), 1000,
                       std::numeric_limits<std::uint64_t>::max(), 9007199254740993ULL}};
  const auto old_locale = std::locale();
  std::locale::global(std::locale(old_locale, new CommaDecimal));
  const bool exported =
      workspace.ExportMeshMemoryJson(samples, std::numeric_limits<std::uint64_t>::max(), &error);
  std::locale::global(old_locale);
  Require(exported && error.empty(), "export failed");
  const auto original = Read(output);
  const std::array wall{FrameSample{1, 1, 0, 0}};
  const std::array rss{ProcessMemorySample{1, 0, 1}};
  const std::array gpu{GpuProfileSample{1, 1}};
  Require(
      workspace.ExportEditorFrameProcessingJson(wall, 0, &error) &&
          workspace.ExportProcessMemoryJson(rss, 0, &error) &&
          workspace.ExportGpuTimingJson(GpuProfileSource::VulkanTimestamps, false, gpu, 0, &error),
      "independent capture fixtures failed");
  const auto wall_bytes = Read(temp.root / ".nexora/frame-processing.json");
  const auto rss_bytes = Read(temp.root / ".nexora/process-memory.json");
  const auto gpu_bytes = Read(temp.root / ".nexora/gpu-timing.json");
  if (!fixture.empty())
    fs::copy_file(output, fixture);
  auto capture = workspace.ImportMeshMemoryJson(&error);
  Require(capture && capture->samples.size() == 3 &&
              capture->samples[0].native_allocation_bytes == 0 &&
              !capture->samples[1].native_allocation_bytes &&
              !capture->samples[1].cpu_key_payload_bytes &&
              capture->samples[0].cpu_key_payload_bytes == 0 &&
              capture->samples[2].cpu_key_payload_bytes == 9007199254740993ULL &&
              capture->samples[1].elapsed_ms == samples[1].elapsed_ms &&
              capture->samples[2].native_allocation_bytes == samples[2].native_allocation_bytes &&
              capture->older_samples_dropped == std::numeric_limits<std::uint64_t>::max() &&
              error.empty(),
          "round trip precision or unavailable semantics failed");
  auto stage = output;
  stage += ".tmp";
  std::ofstream(stage) << "unrelated stage";
  Require(!workspace.ExportMeshMemoryJson(samples, 0, &error) && Read(output) == original &&
              Read(stage) == "unrelated stage",
          "export overwrote occupied stage or last-good file");
  const auto write = [&](std::string_view bytes) {
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  };
  const auto reject = [&](const std::string &bytes) {
    write(bytes);
    error = "stale";
    Require(!workspace.ImportMeshMemoryJson(&error) && !error.empty() && error != "stale" &&
                Read(output) == bytes && Read(stage) == "unrelated stage" &&
                capture->samples[2].native_allocation_bytes == samples[2].native_allocation_bytes,
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
  replace("\"metric\": \"retained_mesh_memory\"", "\"metric\": \"gpu_memory\"");
  replace("\"scope\": \"current_native_surface_retained_geometry\"", "\"scope\": \"scene\"");
  replace("\"time_unit\": \"milliseconds\"", "\"time_unit\": \"seconds\"");
  replace("\"time_origin\": \"first_observation_since_clear_or_domain_change\"",
          "\"time_origin\": \"boot\"");
  replace("\"unit\": \"bytes\"", "\"unit\": \"megabytes\"");
  replace("\"sample_count\": 3", "\"sample_count\": 4");
  replace("\"sample_count\": 3", "\"sample_count\": 3, \"sample_count\": 3");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": -1");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": 1e999");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": nan");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": 1000");
  replace("\"sequence\": \"9007199254740993\"", "\"sequence\": \"0\"");
  replace("\"sequence\": \"9007199254740993\"", "\"sequence\": \"9007199254740994\"");
  replace("\"native_allocation_bytes\": \"0\"", "\"native_allocation_bytes\": 0");
  replace("\"native_allocation_bytes\": \"0\"", "\"native_allocation_bytes\": \"-1\"");
  replace("\"native_allocation_bytes\": \"0\"", "\"native_allocation_bytes\": \"+1\"");
  replace("\"native_allocation_bytes\": \"0\"",
          "\"native_allocation_bytes\": \"18446744073709551616\"");
  replace("\"cpu_scope\": \"owned_comparison_key_payload_excluding_allocator_overhead\"",
          "\"cpu_scope\": \"rss\"");
  replace("\"cpu_key_payload_bytes\": \"0\"", "\"cpu_key_payload_bytes\": null");
  replace("\"native_allocation_bytes\": \"0\"", "\"native_allocation_bytes\": null");
  replace("\"cpu_key_payload_bytes\": \"0\"", "\"cpu_key_payload_bytes\": 0");
  replace("\"source\": \"NexoraEditor\",", "");
  replace("\"sample_count\": 3", "\"sample_count\": 3, \"surface_domain\": \"7\"");
  reject(original + "{}");
  reject(original + std::string(1, '\0'));
  auto wrong_project = original;
  auto id = wrong_project.find(workspace.Project().id.ToString());
  wrong_project[id] = wrong_project[id] == '0' ? '1' : '0';
  reject(wrong_project);
  write(original +
        std::string(ProjectWorkspace::kMaximumMeshMemoryJsonBytes - original.size(), ' '));
  Require(workspace.ImportMeshMemoryJson(&error).has_value(), "exact byte budget rejected");
  reject(original +
         std::string(ProjectWorkspace::kMaximumMeshMemoryJsonBytes - original.size() + 1, ' '));
  for (std::size_t i = 0; i < original.size(); ++i) {
    auto mutated = original;
    mutated[i] ^= 1;
    write(mutated);
    auto attempt = workspace.ImportMeshMemoryJson(&error);
    Require((attempt || !error.empty()) && Read(output) == mutated &&
                Read(stage) == "unrelated stage",
            "mutation import failed preservation/error contract");
  }
  auto escaped = original;
  const auto source_at = escaped.find("NexoraEditor");
  escaped.replace(source_at, 1, "\\u004e");
  write(escaped);
  Require(workspace.ImportMeshMemoryJson(&error).has_value(), "equivalent JSON escape rejected");
  auto reordered = original;
  const std::string source_field = "  \"source\": \"NexoraEditor\",\n";
  const auto field_at = reordered.find(source_field);
  Require(field_at != std::string::npos, "reorder seed missing");
  reordered.erase(field_at, source_field.size());
  reordered.insert(reordered.find("  \"samples\""), source_field);
  write(reordered);
  Require(workspace.ImportMeshMemoryJson(&error).has_value(), "reordered fields rejected");
  write(original);
  fs::remove(stage);
  for (const auto &bad :
       {std::vector<MeshMemorySample>{}, std::vector<MeshMemorySample>{{1, 0, 1, std::nullopt}},
        std::vector<MeshMemorySample>{{1, 0, std::nullopt, 1}},
        std::vector<MeshMemorySample>{{0, 0, 1, 1}}, std::vector<MeshMemorySample>{{1, -1, 1, 1}},
        std::vector<MeshMemorySample>{{1, std::numeric_limits<double>::infinity(), 1, 1}},
        std::vector<MeshMemorySample>{{1, 0, 1, 1}, {2, 0, 2, 2}},
        std::vector<MeshMemorySample>{{2, 0, 1, 1}, {1, 250, 2, 2}},
        std::vector<MeshMemorySample>(601, {1, 0, 1, 1})})
    Require(!workspace.ExportMeshMemoryJson(bad, 0, &error) && Read(output) == original,
            "invalid export changed last-good file");
  std::vector<MeshMemorySample> maximum;
  for (std::uint64_t i = 1; i <= 600; ++i)
    maximum.push_back({std::numeric_limits<std::uint64_t>::max() - 600 + i,
                       i == 600 ? std::numeric_limits<double>::max() : static_cast<double>(i * 250),
                       std::numeric_limits<std::uint64_t>::max(),
                       std::numeric_limits<std::uint64_t>::max()});
  Require(workspace.ExportMeshMemoryJson(maximum, 3, &error) &&
              workspace.ImportMeshMemoryJson(&error)->samples.size() == 600,
          "maximum sample budget rejected");
  auto too_many = Read(output);
  auto at = too_many.rfind(']');
  too_many.insert(at, ",{\"sequence\":\"601\",\"elapsed_ms\":150250,\"native_allocation_bytes\":"
                      "\"601\",\"cpu_key_payload_bytes\":\"601\"}");
  std::ofstream(stage) << "unrelated stage";
  reject(too_many);
  fs::remove(stage);
  write(original);
  ProjectWorkspace observer;
  Require(observer.Open(temp.root, ProjectAccess::ReadOnly, &error) &&
              observer.ImportMeshMemoryJson(&error) &&
              !observer.ExportMeshMemoryJson(samples, 0, &error) && Read(output) == original,
          "read-only contract failed");
  std::ofstream(temp.root / ".nexora/workspace.recovery") << "schema=1\n";
  Require(!workspace.ImportMeshMemoryJson(&error) &&
              !workspace.ExportMeshMemoryJson(samples, 0, &error) && Read(output) == original &&
              workspace.DiscardRecovery(&error),
          "recovery guard failed");
  const auto backup = temp.root / "backup.json";
  fs::rename(output, backup);
  fs::create_directory(output);
  Require(!workspace.ImportMeshMemoryJson(&error) &&
              !workspace.ExportMeshMemoryJson(samples, 0, &error) && fs::is_directory(output) &&
              Read(backup) == original && !fs::exists(stage),
          "nonregular path or failed replacement contract failed");
  fs::remove(output);
  std::error_code ec;
  fs::create_symlink(backup, output, ec);
  if (!ec)
    Require(!workspace.ImportMeshMemoryJson(&error) &&
                !workspace.ExportMeshMemoryJson(samples, 0, &error) && Read(backup) == original,
            "aliased file imported");
  fs::remove(output, ec);
  fs::rename(backup, output);
  const std::array unavailable{MeshMemorySample{1, 0, std::nullopt, std::nullopt}};
  Require(workspace.ExportMeshMemoryJson(unavailable, 0, &error),
          "unavailable-only trace export rejected");
  auto unavailable_capture = workspace.ImportMeshMemoryJson(&error);
  Require(unavailable_capture && !unavailable_capture->samples.front().native_allocation_bytes,
          "unavailable-only trace fabricated memory");
#if !defined(_WIN32)
  // Windows writer leases do not share directory deletion/rename; native aliases also require
  // privileges.
  const auto metadata_backup = temp.root / "metadata-backup";
  observer = ProjectWorkspace{};
  fs::rename(temp.root / ".nexora", metadata_backup);
  fs::create_directory_symlink(metadata_backup, temp.root / ".nexora", ec);
  if (!ec) {
    Require(!workspace.ImportMeshMemoryJson(&error) &&
                !workspace.ExportMeshMemoryJson(samples, 0, &error),
            "aliased metadata admitted memory I/O");
    fs::remove(temp.root / ".nexora");
  }
  fs::rename(metadata_backup, temp.root / ".nexora");
#endif
  Require(Read(temp.root / ".nexora/frame-processing.json") == wall_bytes &&
              Read(temp.root / ".nexora/process-memory.json") == rss_bytes &&
              Read(temp.root / ".nexora/gpu-timing.json") == gpu_bytes,
          "mesh interchange changed independent captures");
  observer = ProjectWorkspace{};
  workspace = ProjectWorkspace{};
  Require(!workspace.ImportMeshMemoryJson(&error) &&
              !workspace.ExportMeshMemoryJson(samples, 0, &error),
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
