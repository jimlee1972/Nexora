#if !defined(_GNU_SOURCE) && defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "Nexora/Editor/ShaderAuthoring.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(_WIN32) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
#define NEXORA_SHADER_PROCESS_SPAWN 1
#include "ProcessLaunch.h"
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char **environ;
#endif
#endif

namespace nexora::editor {
namespace {
// The default runner launches the compiler directly from an argument vector and never through a
// shell. Request fields (source/include paths, entry points, profile, variant) come from project
// content, so routing them through `std::system` would let shell syntax such as `$(...)` or `%VAR%`
// in a path execute arbitrary commands.
#if defined(_WIN32)
std::wstring Widen(const std::string &value) {
  if (value.empty())
    return {};
  // std::filesystem::path::string() yields the active code page on Windows, so decode with it.
  const int size =
      MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(std::max(size, 0)), L'\0');
  if (size > 0)
    MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), wide.data(), size);
  return wide;
}

// Quotes one argument so CommandLineToArgvW/the MSVC runtime reproduce it exactly. CreateProcessW
// does not involve cmd.exe, so no shell metacharacter is interpreted.
void AppendWindowsArgument(std::wstring &command_line, const std::wstring &argument) {
  if (!command_line.empty())
    command_line.push_back(L' ');
  if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    command_line += argument;
    return;
  }
  command_line.push_back(L'"');
  for (auto it = argument.begin();; ++it) {
    std::size_t backslashes = 0;
    while (it != argument.end() && *it == L'\\') {
      ++it;
      ++backslashes;
    }
    if (it == argument.end()) {
      command_line.append(backslashes * 2, L'\\');
      break;
    }
    if (*it == L'"') {
      command_line.append(backslashes * 2 + 1, L'\\');
      command_line.push_back(L'"');
    } else {
      command_line.append(backslashes, L'\\');
      command_line.push_back(*it);
    }
  }
  command_line.push_back(L'"');
}

int RunProcess(const std::vector<std::string> &arguments, std::string &output) {
  output.clear();
  if (arguments.empty())
    return -1;
  std::wstring command_line;
  for (const auto &argument : arguments)
    AppendWindowsArgument(command_line, Widen(argument));

  SECURITY_ATTRIBUTES inheritable{};
  inheritable.nLength = sizeof(inheritable);
  inheritable.bInheritHandle = TRUE;
  HANDLE read_end = nullptr;
  HANDLE write_end = nullptr;
  if (!CreatePipe(&read_end, &write_end, &inheritable, 0)) {
    output = "unable to create the shader compiler output pipe";
    return -1;
  }
  SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = nullptr;
  startup.hStdOutput = write_end;
  startup.hStdError = write_end;
  PROCESS_INFORMATION process{};
  const BOOL launched = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
  CloseHandle(write_end);
  if (!launched) {
    CloseHandle(read_end);
    output =
        "unable to launch " + arguments.front() + " (error " + std::to_string(GetLastError()) + ")";
    return -1;
  }
  char buffer[4096];
  DWORD read = 0;
  while (ReadFile(read_end, buffer, sizeof(buffer), &read, nullptr) && read > 0)
    output.append(buffer, read);
  CloseHandle(read_end);
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 1;
  if (!GetExitCodeProcess(process.hProcess, &exit_code))
    exit_code = 1;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return static_cast<int>(exit_code);
}
#elif defined(NEXORA_SHADER_PROCESS_SPAWN)
char **ProcessEnvironment() {
#if defined(__APPLE__)
  return *_NSGetEnviron();
#else
  return environ;
#endif
}

int RunProcess(const std::vector<std::string> &arguments, std::string &output) {
  output.clear();
  if (arguments.empty())
    return -1;
  std::unique_lock launch{detail::ProcessLaunchMutex()};
  int pipe_fds[2]{-1, -1};
  if (detail::ProcessOutputPipe(pipe_fds) != 0) {
    output = "unable to create the shader compiler output pipe";
    return -1;
  }
  // Both endpoints are protected before a cooperating built-in launcher can spawn.

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
  if (pipe_fds[1] > STDERR_FILENO)
    posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1);
  for (const auto &argument : arguments)
    argv.push_back(const_cast<char *>(argument.c_str()));
  argv.push_back(nullptr);
  pid_t child = 0;
  const int spawn_error =
      posix_spawnp(&child, argv.front(), &actions, nullptr, argv.data(), ProcessEnvironment());
  posix_spawn_file_actions_destroy(&actions);
  close(pipe_fds[1]);
  launch.unlock();
  if (spawn_error != 0) {
    close(pipe_fds[0]);
    output = "unable to launch " + arguments.front() + ": " + std::strerror(spawn_error);
    return -1;
  }
  // Drain before waiting so a compiler that writes more than the pipe buffer cannot deadlock.
  char buffer[4096];
  for (;;) {
    const auto count = read(pipe_fds[0], buffer, sizeof(buffer));
    if (count > 0) {
      output.append(buffer, static_cast<std::size_t>(count));
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    break;
  }
  close(pipe_fds[0]);
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#else
int RunProcess(const std::vector<std::string> &, std::string &output) {
  output = "this platform cannot launch slangc; inject a ShaderProcessRunner";
  return -1;
}
#endif

std::optional<std::uint32_t> ParseUnsigned(std::string_view text) {
  std::uint32_t value{};
  const auto *end = text.data() + text.size();
  const auto [last, status] = std::from_chars(text.data(), end, value);
  if (status != std::errc{} || last != end)
    return std::nullopt;
  return value;
}

std::optional<ShaderDiagnosticSeverity> ParseSeverity(std::string token) {
  std::transform(token.begin(), token.end(), token.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  if (token == "error" || token == "fatal error" || token == "internal error")
    return ShaderDiagnosticSeverity::Error;
  if (token == "warning")
    return ShaderDiagnosticSeverity::Warning;
  if (token == "note" || token == "info" || token == "remark")
    return ShaderDiagnosticSeverity::Info;
  return std::nullopt;
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

// Identifies one compile output. Every input that changes the produced binary or its reflection is
// part of the key, so two requests differing only in include paths, dependencies, or the expected
// layout never share a cached artifact.
std::string CacheKey(const ShaderCompileRequest &request) {
  std::string key = request.shader_id + "\n" + NormalizePath(request.source_path) + "\n" +
                    std::to_string(static_cast<unsigned>(request.format)) + "\n" + request.profile +
                    "\n" + request.variant +
                    "\nschema=" + std::to_string(request.reflection.schema_version) +
                    "\nlayout=" + std::to_string(request.reflection.layout_hash);
  for (const auto &entry : request.entry_points)
    key += "\nentry=" + entry;
  for (const auto &include : request.include_directories)
    key += "\ninclude=" + NormalizePath(include);
  for (const auto &define : request.defines)
    key += "\ndefine=" + define;
  for (const auto &dependency : request.dependencies)
    key += "\ndependency=" + NormalizePath(dependency);
  return key;
}

std::int64_t WriteTime(const std::filesystem::path &path) {
  std::error_code error;
  const auto value = std::filesystem::last_write_time(path, error);
  return error ? -1 : value.time_since_epoch().count();
}

// Folds the source and every declared dependency timestamp into one value, so editing an included
// file triggers a reload just like editing the source itself.
std::uint64_t InputFingerprint(std::int64_t source_stamp, const ShaderCompileRequest &request) {
  std::uint64_t hash = 14695981039346656037ull;
  const auto mix = [&hash](std::int64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
      hash ^= (static_cast<std::uint64_t>(value) >> shift) & 0xffU;
      hash *= 1099511628211ull;
    }
  };
  mix(source_stamp);
  for (const auto &dependency : request.dependencies)
    mix(WriteTime(dependency));
  return hash;
}
} // namespace

std::vector<ShaderCompileDiagnostic> ParseShaderDiagnostics(std::string_view output) {
  return ParseShaderDiagnostics(output, rhi::Backend::Null, {});
}

// Accepts both the native Slang 2026.x layout
//   error[E30015]: undefined identifier
//    --> Shaders/A.slang:5:15
// and the single-line `file:line:col: severity: message` layout. Severity always comes from the
// explicit severity token, never from words inside the message. Echoed source and gutter lines
// are skipped so text quoted from the shader cannot masquerade as a diagnostic, and out-of-range
// numbers degrade to an unknown position instead of throwing.
std::vector<ShaderCompileDiagnostic>
ParseShaderDiagnostics(std::string_view output, rhi::Backend backend, std::string_view variant) {
  std::vector<ShaderCompileDiagnostic> diagnostics;
  constexpr auto kSeverities = "fatal error|internal error|error|warning|note|info|remark";
  const std::regex header(std::string(R"(^\s*()") + kSeverities +
                              R"()(?:\[[A-Za-z0-9_]+\])?:\s*(.*)$)",
                          std::regex::icase);
  const std::regex arrow(R"(^\s*-->\s*(.+):([0-9]+):([0-9]+)\s*$)");
  const std::regex single_line(std::string(R"(^(.+?):([0-9]+):([0-9]+):\s*()") + kSeverities +
                                   R"():\s*(.*)$)",
                               std::regex::icase);
  const std::regex gutter(R"(^\s*[0-9]*\s*\|)");

  std::optional<ShaderCompileDiagnostic> pending;
  const auto flush = [&] {
    if (pending)
      diagnostics.push_back(std::move(*pending));
    pending.reset();
  };
  const auto start = [&](ShaderDiagnosticSeverity severity, std::string message) {
    flush();
    pending.emplace(severity, std::string{}, 0, 0, std::move(message), backend,
                    std::string(variant));
  };

  std::istringstream lines{std::string(output)};
  std::string line;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (std::regex_search(line, gutter))
      continue;
    std::smatch match;
    if (std::regex_match(line, match, arrow)) {
      if (pending) {
        pending->file = match[1].str();
        pending->line = ParseUnsigned(match[2].str()).value_or(0);
        pending->column = ParseUnsigned(match[3].str()).value_or(0);
        flush();
      }
      continue;
    }
    if (std::regex_match(line, match, single_line)) {
      const auto severity = ParseSeverity(match[4].str());
      start(severity.value_or(ShaderDiagnosticSeverity::Info), match[5].str());
      pending->file = match[1].str();
      pending->line = ParseUnsigned(match[2].str()).value_or(0);
      pending->column = ParseUnsigned(match[3].str()).value_or(0);
      flush();
      continue;
    }
    if (std::regex_match(line, match, header)) {
      const auto severity = ParseSeverity(match[1].str());
      start(severity.value_or(ShaderDiagnosticSeverity::Info), match[2].str());
    }
  }
  flush();
  return diagnostics;
}

ShaderCompileResult CompileSlang(const ShaderCompileRequest &request,
                                 const ShaderCompilerOptions &options,
                                 const ShaderProcessRunner &runner) {
  ShaderCompileResult result;
  if (request.shader_id.empty() || request.source_path.empty() || request.output_path.empty() ||
      request.entry_points.empty() || request.reflection.layout_hash == 0) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.source_path.string(), 0,
                                  0, "invalid Slang compile request"});
    return result;
  }
  const auto target = TargetName(request.format);
  if (target.empty()) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.source_path.string(), 0,
                                  0, "unsupported Slang target"});
    return result;
  }
  std::vector<std::string> arguments{options.slangc_executable.string(),
                                     request.source_path.string(), "-target", target};
  if (!request.profile.empty())
    arguments.insert(arguments.end(), {"-profile", request.profile});
  if (request.format == rhi::ShaderBinaryFormat::SpirV)
    arguments.push_back("-fvk-use-entrypoint-name");
  for (const auto &include : request.include_directories)
    arguments.insert(arguments.end(), {"-I", include.string()});
  for (const auto &define : request.defines)
    arguments.insert(arguments.end(), {"-D", define});
  for (const auto &entry : request.entry_points)
    arguments.insert(arguments.end(), {"-entry", entry});
  arguments.insert(arguments.end(), {"-o", request.output_path.string()});

  std::error_code remove_error;
  std::filesystem::remove(request.output_path, remove_error);
  if (remove_error) {
    result.diagnostics.push_back(
        {ShaderDiagnosticSeverity::Error, request.output_path.string(), 0, 0,
         "unable to clear the previous shader artifact: " + remove_error.message(), request.backend,
         request.variant});
    return result;
  }
  std::string process_output;
  const auto exit_code =
      runner ? runner(arguments, process_output) : RunProcess(arguments, process_output);
  result.diagnostics = ParseShaderDiagnostics(process_output, request.backend, request.variant);
  if (exit_code != 0) {
    const bool has_error =
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                    [](const ShaderCompileDiagnostic &diagnostic) {
                      return diagnostic.severity == ShaderDiagnosticSeverity::Error;
                    });
    if (!has_error) {
      // Surface the first line of raw output (for example a launch failure) rather than hiding it.
      std::string detail;
      std::istringstream raw{process_output};
      while (detail.empty() && std::getline(raw, detail))
        detail.erase(0, detail.find_first_not_of(" \t\r"));
      result.diagnostics.push_back(
          {ShaderDiagnosticSeverity::Error, request.source_path.string(), 0, 0,
           detail.empty() ? "slangc failed without a parseable diagnostic"
                          : "slangc failed without a parseable diagnostic: " + detail,
           request.backend, request.variant});
    }
    return result;
  }
  std::ifstream output(request.output_path, std::ios::binary | std::ios::ate);
  if (!output) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.output_path.string(), 0,
                                  0, "slangc succeeded but did not produce an artifact"});
    return result;
  }
  const auto size = output.tellg();
  if (size <= 0) {
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.output_path.string(), 0,
                                  0, "slangc produced an empty artifact"});
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
    result.diagnostics.push_back({ShaderDiagnosticSeverity::Error, request.output_path.string(), 0,
                                  0, "unable to read the compiled shader artifact"});
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
  // Track each request (shader/variant/backend/target) separately: keying by source path alone let
  // the first request polled after an edit consume the change, so other variants built from the
  // same source were never filled or rebuilt. The fingerprint is captured before compiling, so a
  // save that lands mid-compile still differs on the next poll and triggers another rebuild.
  const auto fingerprint = InputFingerprint(write_time.time_since_epoch().count(), request);
  const auto key = CacheKey(request);
  const auto found = observed_inputs_.find(key);
  if (found != observed_inputs_.end() && found->second.fingerprint == fingerprint)
    return ShaderReloadStatus::NoChange;
  const auto status = Reload(request, slot, error);
  if (status != ShaderReloadStatus::Failed)
    observed_inputs_.insert_or_assign(
        key, ObservedInputs{NormalizePath(request.source_path), fingerprint});
  return status;
}

void ShaderHotReloadController::Forget(const std::filesystem::path &source_path) {
  const auto source = NormalizePath(source_path);
  std::erase_if(observed_inputs_,
                [&source](const auto &entry) { return entry.second.source == source; });
}

DevelopmentShaderCache::DevelopmentShaderCache(ShaderVariantBudget budget) : budget_(budget) {
  budget_.used = 0;
}

const ShaderCompileResult *DevelopmentShaderCache::Find(const ShaderCompileRequest &request) {
  const auto found = entries_.find(CacheKey(request));
  if (found == entries_.end())
    return nullptr;
  for (const auto &[path, stamp] : found->second.dependency_write_times) {
    if (WriteTime(path) != stamp) {
      entries_.erase(found);
      budget_.used = entries_.size();
      return nullptr;
    }
  }
  return &found->second.result;
}

ShaderInputSnapshot CaptureShaderInputs(const ShaderCompileRequest &request) {
  ShaderInputSnapshot snapshot;
  snapshot.write_times.emplace(NormalizePath(request.source_path), WriteTime(request.source_path));
  for (const auto &dependency : request.dependencies)
    snapshot.write_times.emplace(NormalizePath(dependency), WriteTime(dependency));
  return snapshot;
}

bool DevelopmentShaderCache::Store(const ShaderCompileRequest &request, ShaderCompileResult result,
                                   std::string &error) {
  return Store(request, std::move(result), CaptureShaderInputs(request), error);
}

bool DevelopmentShaderCache::Store(const ShaderCompileRequest &request, ShaderCompileResult result,
                                   ShaderInputSnapshot inputs, std::string &error) {
  if (!result.succeeded) {
    error = "failed shader results are not cacheable";
    return false;
  }
  const auto key = CacheKey(request);
  if (!entries_.contains(key) && entries_.size() >= budget_.maximum) {
    error = "shader variant budget exhausted";
    return false;
  }
  Entry entry;
  entry.result = std::move(result);
  // Inputs edited after the snapshot mean the result is already stale: refuse it (and drop any
  // older entry) rather than let it occupy budget until the next Find notices.
  for (const auto &[path, stamp] : inputs.write_times) {
    if (WriteTime(path) != stamp) {
      entries_.erase(key);
      budget_.used = entries_.size();
      error = "shader inputs changed while compiling; result discarded as stale";
      return false;
    }
  }
  entry.dependency_write_times = std::move(inputs.write_times);
  entries_.insert_or_assign(key, std::move(entry));
  budget_.used = entries_.size();
  error.clear();
  return true;
}

std::size_t DevelopmentShaderCache::InvalidateDependency(const std::filesystem::path &dependency) {
  const auto path = NormalizePath(dependency);
  const auto before = entries_.size();
  std::erase_if(entries_, [&path](const auto &item) {
    return item.second.dependency_write_times.contains(path);
  });
  budget_.used = entries_.size();
  return before - entries_.size();
}

ShaderVariantBudget DevelopmentShaderCache::Budget() const noexcept { return budget_; }

bool ApplyShaderCompileResult(const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot,
                              std::string &error) {
  return ApplyShaderCompileResult(result, slot, 0, error);
}

bool ApplyShaderCompileResult(const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot,
                              std::uint64_t retire_fence, std::string &error) {
  const bool has_error =
      std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                  [](const ShaderCompileDiagnostic &diagnostic) {
                    return diagnostic.severity == ShaderDiagnosticSeverity::Error;
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
