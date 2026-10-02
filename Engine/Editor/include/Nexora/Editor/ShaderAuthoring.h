#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/ShaderRuntime.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexora::editor {
enum class ShaderDiagnosticSeverity : std::uint8_t { Info, Warning, Error };
struct ShaderCompileDiagnostic final {
  ShaderDiagnosticSeverity severity{ShaderDiagnosticSeverity::Info};
  std::string file;
  std::uint32_t line{};
  std::uint32_t column{};
  std::string message;
  rhi::Backend backend{rhi::Backend::Null};
  std::string variant;
  ShaderCompileDiagnostic() = default;
  ShaderCompileDiagnostic(ShaderDiagnosticSeverity diagnostic_severity, std::string diagnostic_file,
                          std::uint32_t diagnostic_line, std::uint32_t diagnostic_column,
                          std::string diagnostic_message,
                          rhi::Backend diagnostic_backend = rhi::Backend::Null,
                          std::string diagnostic_variant = {})
      : severity(diagnostic_severity), file(std::move(diagnostic_file)), line(diagnostic_line),
        column(diagnostic_column), message(std::move(diagnostic_message)),
        backend(diagnostic_backend), variant(std::move(diagnostic_variant)) {}
};
struct ShaderCompileResult final {
  bool succeeded{};
  rhi::ShaderModuleArtifact artifact;
  std::vector<ShaderCompileDiagnostic> diagnostics;
};

struct ShaderCompileRequest final {
  std::string shader_id;
  std::filesystem::path source_path;
  std::filesystem::path output_path;
  std::vector<std::string> entry_points;
  std::vector<std::filesystem::path> include_directories;
  std::vector<std::string> defines;
  std::string profile;
  rhi::ShaderBinaryFormat format{rhi::ShaderBinaryFormat::SpirV};
  rhi::PipelineLayoutMetadata reflection;
  std::uint64_t retire_fence{};
  rhi::Backend backend{rhi::Backend::Null};
  std::string variant;
  std::vector<std::filesystem::path> dependencies;
};

struct ShaderCompilerOptions final {
  std::filesystem::path slangc_executable{"slangc"};
};

using ShaderProcessRunner = std::function<int(const std::vector<std::string> &, std::string &)>;

[[nodiscard]] NEXORA_EDITOR_API std::vector<ShaderCompileDiagnostic>
ParseShaderDiagnostics(std::string_view output);
[[nodiscard]] NEXORA_EDITOR_API std::vector<ShaderCompileDiagnostic>
ParseShaderDiagnostics(std::string_view output, rhi::Backend backend, std::string_view variant);
[[nodiscard]] NEXORA_EDITOR_API ShaderCompileResult
CompileSlang(const ShaderCompileRequest &request, const ShaderCompilerOptions &options,
             const ShaderProcessRunner &runner = {});

enum class ShaderReloadStatus : std::uint8_t { NoChange, Committed, Failed };

// File-change orchestration lives in Editor. The compiler callback is injectable for tests and
// lets platform hosts choose their process sandbox without moving Slang ownership into RHI.
class NEXORA_EDITOR_API ShaderHotReloadController final {
public:
  using CompileFunction = std::function<ShaderCompileResult(const ShaderCompileRequest &)>;

  explicit ShaderHotReloadController(CompileFunction compile);

  [[nodiscard]] ShaderReloadStatus Reload(const ShaderCompileRequest &request,
                                          runtime::ShaderArtifactSlot &slot, std::string &error);
  [[nodiscard]] ShaderReloadStatus ReloadIfChanged(const ShaderCompileRequest &request,
                                                   runtime::ShaderArtifactSlot &slot,
                                                   std::string &error);
  void Forget(const std::filesystem::path &source_path);

private:
  struct ObservedInputs final {
    std::string source;
    std::uint64_t fingerprint{};
  };
  CompileFunction compile_;
  // Keyed per compile request, fingerprinting the source and its declared dependencies.
  std::unordered_map<std::string, ObservedInputs> observed_inputs_;
};

struct ShaderVariantBudget final {
  std::size_t maximum{};
  std::size_t used{};
};

// Write times of a request's source and declared dependencies at one instant. Capture it *before*
// compiling and hand it to the cache with the result: stamping at store time would label output
// built from older inputs as current when a file is saved mid-compile.
struct ShaderInputSnapshot final {
  std::unordered_map<std::string, std::int64_t> write_times;
};
[[nodiscard]] NEXORA_EDITOR_API ShaderInputSnapshot
CaptureShaderInputs(const ShaderCompileRequest &request);

// Development-only, process-local cache. Entries own compiler results and dependency timestamps;
// a dependency change invalidates every variant which consumed it. Shipping never consults it.
class NEXORA_EDITOR_API DevelopmentShaderCache final {
public:
  explicit DevelopmentShaderCache(ShaderVariantBudget budget);
  [[nodiscard]] const ShaderCompileResult *Find(const ShaderCompileRequest &request);
  // Snapshot-less overload stamps the inputs at store time and is only safe when nothing can edit
  // them while the compile runs; prefer the overload taking a pre-compile snapshot.
  [[nodiscard]] bool Store(const ShaderCompileRequest &request, ShaderCompileResult result,
                           std::string &error);
  [[nodiscard]] bool Store(const ShaderCompileRequest &request, ShaderCompileResult result,
                           ShaderInputSnapshot inputs, std::string &error);
  std::size_t InvalidateDependency(const std::filesystem::path &dependency);
  [[nodiscard]] ShaderVariantBudget Budget() const noexcept;

private:
  struct Entry final {
    ShaderCompileResult result;
    std::unordered_map<std::string, std::int64_t> dependency_write_times;
  };
  ShaderVariantBudget budget_;
  std::unordered_map<std::string, Entry> entries_;
};

// Applies a successful compiler result to Runtime as one generation; failed results only carry
// diagnostics and leave the active shader untouched.
[[nodiscard]] NEXORA_EDITOR_API bool ApplyShaderCompileResult(const ShaderCompileResult &result,
                                                              runtime::ShaderArtifactSlot &slot,
                                                              std::string &error);
[[nodiscard]] NEXORA_EDITOR_API bool ApplyShaderCompileResult(const ShaderCompileResult &result,
                                                              runtime::ShaderArtifactSlot &slot,
                                                              std::uint64_t retire_fence,
                                                              std::string &error);
} // namespace nexora::editor
