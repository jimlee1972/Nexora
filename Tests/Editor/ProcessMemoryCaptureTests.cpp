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
                    ("nexora-memory-capture-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Temporary() {
      std::error_code ec;
      fs::remove_all(root, ec);
    }
  } temp;
  ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(temp.root, "Memory", &error), "create failed");
  const auto output = temp.root / ".nexora/process-memory.json";
  const std::array samples{
      ProcessMemorySample{9007199254740993ULL, 0, 0},
      ProcessMemorySample{9007199254740994ULL, 250.12345678901234, std::nullopt},
      ProcessMemorySample{std::numeric_limits<std::uint64_t>::max(), 1000,
                          std::numeric_limits<std::uint64_t>::max()}};
  const auto old_locale = std::locale();
  std::locale::global(std::locale(old_locale, new CommaDecimal));
  const bool exported =
      workspace.ExportProcessMemoryJson(samples, std::numeric_limits<std::uint64_t>::max(), &error);
  std::locale::global(old_locale);
  Require(exported && error.empty(), "export failed");
  const auto original = Read(output);
  if (!fixture.empty())
    fs::copy_file(output, fixture);
  auto capture = workspace.ImportProcessMemoryJson(&error);
  Require(capture && capture->samples.size() == 3 && capture->samples[0].resident_bytes == 0 &&
              !capture->samples[1].resident_bytes &&
              capture->samples[1].elapsed_ms == samples[1].elapsed_ms &&
              capture->samples[2].resident_bytes == samples[2].resident_bytes &&
              capture->older_samples_dropped == std::numeric_limits<std::uint64_t>::max() &&
              error.empty(),
          "round trip precision or unavailable semantics failed");
  auto stage = output;
  stage += ".tmp";
  std::ofstream(stage) << "unrelated stage";
  Require(!workspace.ExportProcessMemoryJson(samples, 0, &error) && Read(output) == original &&
              Read(stage) == "unrelated stage",
          "export overwrote occupied stage or last-good file");
  const auto write = [&](std::string_view bytes) {
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  };
  const auto reject = [&](const std::string &bytes) {
    write(bytes);
    error = "stale";
    Require(!workspace.ImportProcessMemoryJson(&error) && !error.empty() && error != "stale" &&
                Read(output) == bytes && Read(stage) == "unrelated stage" &&
                capture->samples[2].resident_bytes == samples[2].resident_bytes,
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
  replace("\"metric\": \"process_resident_memory\"", "\"metric\": \"gpu_memory\"");
  replace("\"scope\": \"current_process_including_shared_resident_pages\"", "\"scope\": \"scene\"");
  replace("\"time_unit\": \"milliseconds\"", "\"time_unit\": \"seconds\"");
  replace("\"time_origin\": \"first_observation_since_clear\"", "\"time_origin\": \"boot\"");
  replace("\"unit\": \"bytes\"", "\"unit\": \"megabytes\"");
  replace("\"sample_count\": 3", "\"sample_count\": 4");
  replace("\"sample_count\": 3", "\"sample_count\": 3, \"sample_count\": 3");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": -1");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": 1e999");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": nan");
  replace("\"elapsed_ms\": 0", "\"elapsed_ms\": 1000");
  replace("\"sequence\": \"9007199254740993\"", "\"sequence\": \"0\"");
  replace("\"sequence\": \"9007199254740993\"", "\"sequence\": \"9007199254740994\"");
  replace("\"resident_bytes\": \"0\"", "\"resident_bytes\": 0");
  replace("\"resident_bytes\": \"0\"", "\"resident_bytes\": \"-1\"");
  replace("\"resident_bytes\": \"0\"", "\"resident_bytes\": \"+1\"");
  replace("\"resident_bytes\": \"0\"", "\"resident_bytes\": \"18446744073709551616\"");
  reject(original + "{}");
  reject(original + std::string(1, '\0'));
  auto wrong_project = original;
  auto id = wrong_project.find(workspace.Project().id.ToString());
  wrong_project[id] = wrong_project[id] == '0' ? '1' : '0';
  reject(wrong_project);
  write(original +
        std::string(ProjectWorkspace::kMaximumProcessMemoryJsonBytes - original.size(), ' '));
  Require(workspace.ImportProcessMemoryJson(&error).has_value(), "exact byte budget rejected");
  reject(original +
         std::string(ProjectWorkspace::kMaximumProcessMemoryJsonBytes - original.size() + 1, ' '));
  for (std::size_t i = 0; i < original.size(); ++i) {
    auto mutated = original;
    mutated[i] ^= 1;
    write(mutated);
    auto attempt = workspace.ImportProcessMemoryJson(&error);
    Require((attempt || !error.empty()) && Read(output) == mutated &&
                Read(stage) == "unrelated stage",
            "mutation import failed preservation/error contract");
  }
  auto escaped = original;
  const auto source_at = escaped.find("NexoraEditor");
  escaped.replace(source_at, 1, "\\u004e");
  write(escaped);
  Require(workspace.ImportProcessMemoryJson(&error).has_value(), "equivalent JSON escape rejected");
  write(original);
  fs::remove(stage);
  for (const auto &bad :
       {std::vector<ProcessMemorySample>{}, std::vector<ProcessMemorySample>{{0, 0, 1}},
        std::vector<ProcessMemorySample>{{1, -1, 1}},
        std::vector<ProcessMemorySample>{{1, std::numeric_limits<double>::infinity(), 1}},
        std::vector<ProcessMemorySample>{{1, 0, 1}, {2, 0, 2}},
        std::vector<ProcessMemorySample>{{2, 0, 1}, {1, 250, 2}},
        std::vector<ProcessMemorySample>(601, {1, 0, 1})})
    Require(!workspace.ExportProcessMemoryJson(bad, 0, &error) && Read(output) == original,
            "invalid export changed last-good file");
  std::vector<ProcessMemorySample> maximum;
  for (std::uint64_t i = 1; i <= 600; ++i)
    maximum.push_back({i, static_cast<double>(i * 250), i});
  Require(workspace.ExportProcessMemoryJson(maximum, 3, &error) &&
              workspace.ImportProcessMemoryJson(&error)->samples.size() == 600,
          "maximum sample budget rejected");
  auto too_many = Read(output);
  auto at = too_many.find("\"sample_count\": 600");
  too_many.replace(at, 19, "\"sample_count\": 601");
  at = too_many.rfind(']');
  too_many.insert(at, ",{\"sequence\":\"601\",\"elapsed_ms\":150250,\"resident_bytes\":\"601\"}");
  std::ofstream(stage) << "unrelated stage";
  reject(too_many);
  fs::remove(stage);
  write(original);
  ProjectWorkspace observer;
  Require(observer.Open(temp.root, ProjectAccess::ReadOnly, &error) &&
              observer.ImportProcessMemoryJson(&error) &&
              !observer.ExportProcessMemoryJson(samples, 0, &error) && Read(output) == original,
          "read-only contract failed");
  std::ofstream(temp.root / ".nexora/workspace.recovery") << "schema=1\n";
  Require(!workspace.ImportProcessMemoryJson(&error) &&
              !workspace.ExportProcessMemoryJson(samples, 0, &error) && Read(output) == original &&
              workspace.DiscardRecovery(&error),
          "recovery guard failed");
  const auto backup = temp.root / "backup.json";
  fs::rename(output, backup);
  fs::create_directory(output);
  Require(!workspace.ImportProcessMemoryJson(&error) &&
              !workspace.ExportProcessMemoryJson(samples, 0, &error) && fs::is_directory(output) &&
              Read(backup) == original && !fs::exists(stage),
          "nonregular path or failed replacement contract failed");
  fs::remove(output);
  std::error_code ec;
  fs::create_symlink(backup, output, ec);
  if (!ec)
    Require(!workspace.ImportProcessMemoryJson(&error) && Read(backup) == original,
            "aliased file imported");
  fs::remove(output, ec);
  fs::rename(backup, output);
  const std::array unavailable{ProcessMemorySample{1, 0, std::nullopt}};
  Require(workspace.ExportProcessMemoryJson(unavailable, 0, &error),
          "unavailable-only trace export rejected");
  auto unavailable_capture = workspace.ImportProcessMemoryJson(&error);
  Require(unavailable_capture && !unavailable_capture->samples.front().resident_bytes,
          "unavailable-only trace fabricated memory");
#if !defined(_WIN32)
  // Windows writer leases do not share directory deletion/rename; native aliases also require
  // privileges.
  const auto metadata_backup = temp.root / "metadata-backup";
  observer = ProjectWorkspace{};
  fs::rename(temp.root / ".nexora", metadata_backup);
  fs::create_directory_symlink(metadata_backup, temp.root / ".nexora", ec);
  if (!ec) {
    Require(!workspace.ImportProcessMemoryJson(&error) &&
                !workspace.ExportProcessMemoryJson(samples, 0, &error),
            "aliased metadata admitted memory I/O");
    fs::remove(temp.root / ".nexora");
  }
  fs::rename(metadata_backup, temp.root / ".nexora");
#endif
  observer = ProjectWorkspace{};
  workspace = ProjectWorkspace{};
  Require(!workspace.ImportProcessMemoryJson(&error) &&
              !workspace.ExportProcessMemoryJson(samples, 0, &error),
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
