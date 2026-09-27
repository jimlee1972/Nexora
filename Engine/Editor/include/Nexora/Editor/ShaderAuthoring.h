#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/ShaderRuntime.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexora::editor {
enum class ShaderDiagnosticSeverity : std::uint8_t { Info, Warning, Error };
struct ShaderCompileDiagnostic final {
  ShaderDiagnosticSeverity severity{ShaderDiagnosticSeverity::Info};
  std::string file;
  std::uint32_t line{};
  std::uint32_t column{};
  std::string message;
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
  std::string profile;
  rhi::ShaderBinaryFormat format{rhi::ShaderBinaryFormat::SpirV};
  rhi::PipelineLayoutMetadata reflection;
  std::uint64_t retire_fence{};
};

struct ShaderCompilerOptions final {
  std::filesystem::path slangc_executable{"slangc"};
};

using ShaderProcessRunner = std::function<int(const std::vector<std::string> &, std::string &)>;

[[nodiscard]] NEXORA_EDITOR_API std::vector<ShaderCompileDiagnostic>
ParseShaderDiagnostics(std::string_view output);
[[nodiscard]] NEXORA_EDITOR_API ShaderCompileResult CompileSlang(
    const ShaderCompileRequest &request, const ShaderCompilerOptions &options,
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
  CompileFunction compile_;
  std::unordered_map<std::string, std::int64_t> observed_write_times_;
};

// Applies a successful compiler result to Runtime as one generation; failed results only carry
// diagnostics and leave the active shader untouched.
[[nodiscard]] NEXORA_EDITOR_API bool ApplyShaderCompileResult(
    const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot, std::string &error);
[[nodiscard]] NEXORA_EDITOR_API bool ApplyShaderCompileResult(
    const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot,
    std::uint64_t retire_fence, std::string &error);
} // namespace nexora::editor
