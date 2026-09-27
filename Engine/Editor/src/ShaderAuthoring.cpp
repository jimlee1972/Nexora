#include "Nexora/Editor/ShaderAuthoring.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

namespace nexora::editor {
namespace {
std::string QuoteArg(const std::string &value) {
  std::string quoted{"\""};
  for (const char character : value) {
    if (character == '\\' || character == '"')
      quoted.push_back('\\');
    quoted.push_back(character);
  }
  quoted.push_back('"');
  return quoted;
}

int RunProcess(const std::vector<std::string> &arguments, std::string &output) {
  if (arguments.empty())
    return -1;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto log_path = std::filesystem::temp_directory_path() /
                        ("nexora-slang-" + std::to_string(stamp) + ".log");
  std::string command;
  for (const auto &argument : arguments) {
    if (!command.empty())
      command.push_back(' ');
    command += QuoteArg(argument);
  }
  command += " >" + QuoteArg(log_path.string()) + " 2>&1";
  const auto result = std::system(command.c_str());
  std::ifstream log(log_path, std::ios::binary);
  output.assign(std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>());
  std::error_code ignored;
  std::filesystem::remove(log_path, ignored);
  return result;
}

std::string TargetName(rhi::ShaderBinaryFormat format) {
  switch (format) {
  case rhi::ShaderBinaryFormat::Dxil:
    return "dxil";
  case rhi::ShaderBinaryFormat::SpirV:
    return "spirv";
  case rhi::ShaderBinaryFormat::MetalSource:
    return "metal";
  }
  return {};
}

std::string NormalizePath(const std::filesystem::path &path) {
  return path.lexically_normal().generic_string();
}
} // namespace

std::vector<ShaderCompileDiagnostic> ParseShaderDiagnostics(std::string_view output) {
  std::vector<ShaderCompileDiagnostic> diagnostics;
  const std::regex location(R"(^(.+?):([0-9]+):([0-9]+):\s*(.*)$)");
  std::istringstream lines{std::string(output)};
  std::string line;
  while (std::getline(lines, line)) {
    std::smatch match;
    if (!std::regex_match(line, match, location))
      continue;
    ShaderCompileDiagnostic diagnostic;
    diagnostic.file = match[1].str();
    diagnostic.line = static_cast<std::uint32_t>(std::stoul(match[2].str()));
    diagnostic.column = static_cast<std::uint32_t>(std::stoul(match[3].str()));
    diagnostic.message = match[4].str();
    const auto lower = diagnostic.message;
    if (lower.find("error") != std::string::npos)
      diagnostic.severity = ShaderDiagnosticSeverity::Error;
    else if (lower.find("warning") != std::string::npos)
      diagnostic.severity = ShaderDiagnosticSeverity::Warning;
    else
      diagnostic.severity = ShaderDiagnosticSeverity::Info;
    diagnostics.push_back(std::move(diagnostic));
  }
  return diagnostics;
}

ShaderCompileResult CompileSlang(const ShaderCompileRequest &request,
                                 const ShaderCompilerOptions &options,
                                 const ShaderProcessRunner &runner) {
  ShaderCompileResult result;
  if (request.shader_id.empty() || request.source_path.empty() || request.output_path.empty() ||
      request.entry_points.empty() || request.reflection.layout_hash == 0) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.source_path.string(), 0, 0,
                                  "invalid Slang compile request"});
    return result;
  }
  const auto target = TargetName(request.format);
  if (target.empty()) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.source_path.string(), 0, 0,
                                  "unsupported Slang target"});
    return result;
  }
  std::vector<std::string> arguments{options.slangc_executable.string(), request.source_path.string(),
                                     "-target", target};
  if (!request.profile.empty())
    arguments.insert(arguments.end(), {"-profile", request.profile});
  if (request.format == rhi::ShaderBinaryFormat::SpirV)
    arguments.push_back("-fvk-use-entrypoint-name");
  for (const auto &include : request.include_directories)
    arguments.insert(arguments.end(), {"-I", include.string()});
  for (const auto &entry : request.entry_points)
    arguments.insert(arguments.end(), {"-entry", entry});
  arguments.insert(arguments.end(), {"-o", request.output_path.string()});

  std::string process_output;
  const auto exit_code = runner ? runner(arguments, process_output) : RunProcess(arguments, process_output);
  result.diagnostics = ParseShaderDiagnostics(process_output);
  if (exit_code != 0) {
    if (result.diagnostics.empty())
      result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.source_path.string(), 0, 0,
                                    "slangc failed without a parseable diagnostic"});
    return result;
  }
  std::ifstream output(request.output_path, std::ios::binary | std::ios::ate);
  if (!output) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.output_path.string(), 0, 0,
                                  "slangc succeeded but did not produce an artifact"});
    return result;
  }
  const auto size = output.tellg();
  if (size <= 0) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.output_path.string(), 0, 0,
                                  "slangc produced an empty artifact"});
    return result;
  }
  result.artifact.shader_id = request.shader_id;
  result.artifact.format = request.format;
  result.artifact.entry_point = request.entry_points.front();
  result.artifact.reflection = request.reflection;
  result.artifact.binary.resize(static_cast<std::size_t>(size));
  output.seekg(0);
  output.read(reinterpret_cast<char *>(result.artifact.binary.data()), size);
  if (!output) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.output_path.string(), 0, 0,
                                  "unable to read the compiled shader artifact"});
    result.artifact = {};
    return result;
  }
  result.succeeded = true;
  return result;
}

ShaderHotReloadController::ShaderHotReloadController(CompileFunction compile)
    : compile_(std::move(compile)) {}

ShaderReloadStatus ShaderHotReloadController::Reload(const ShaderCompileRequest &request,
                                                     runtime::ShaderArtifactSlot &slot,
                                                     std::string &error) {
  if (!compile_) {
    error = "shader hot reload has no compiler callback";
    return ShaderReloadStatus::Failed;
  }
  const auto result = compile_(request);
  if (!ApplyShaderCompileResult(result, slot, request.retire_fence, error))
    return ShaderReloadStatus::Failed;
  return ShaderReloadStatus::Committed;
}

ShaderReloadStatus ShaderHotReloadController::ReloadIfChanged(const ShaderCompileRequest &request,
                                                              runtime::ShaderArtifactSlot &slot,
                                                              std::string &error) {
  std::error_code filesystem_error;
  const auto write_time = std::filesystem::last_write_time(request.source_path, filesystem_error);
  if (filesystem_error) {
    error = "unable to inspect shader source timestamp: " + request.source_path.string();
    return ShaderReloadStatus::Failed;
  }
  const auto stamp = write_time.time_since_epoch().count();
  const auto key = NormalizePath(request.source_path);
  const auto found = observed_write_times_.find(key);
  if (found != observed_write_times_.end() && found->second == stamp)
    return ShaderReloadStatus::NoChange;
  const auto status = Reload(request, slot, error);
  if (status != ShaderReloadStatus::Failed)
    observed_write_times_[key] = stamp;
  return status;
}

void ShaderHotReloadController::Forget(const std::filesystem::path &source_path) {
  observed_write_times_.erase(NormalizePath(source_path));
}

bool ApplyShaderCompileResult(const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot,
                              std::string &error) {
  return ApplyShaderCompileResult(result, slot, 0, error);
}

bool ApplyShaderCompileResult(const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot,
                              std::uint64_t retire_fence, std::string &error) {
  const bool has_error = std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                     [](const ShaderCompileDiagnostic &diagnostic) {
                                       return diagnostic.severity ==
                                              ShaderDiagnosticSeverity::Error;
                                     });
  if (!result.succeeded || has_error) {
    slot.DiscardStaged();
    error = "shader compilation failed; active artifact was preserved";
    return false;
  }
  if (!slot.Stage(result.artifact, runtime::ShaderArtifactSource::DynamicCompile, error)) {
    slot.DiscardStaged();
    return false;
  }
  return slot.Commit(retire_fence, error);
}
} // namespace nexora::editor
