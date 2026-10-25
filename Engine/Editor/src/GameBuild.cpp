#include "Nexora/Editor/GameBuild.h"
#include "AtomicFile.h"
#include "GameBuildSDKFiles.h"
#include "Nexora/Editor/BuildProcess.h"
#include "Nexora/Runtime/GameLaunch.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <thread>

namespace nexora::editor {
namespace {
using Path = std::filesystem::path;
constexpr std::size_t kArtifactLimit = 512 * 1024 * 1024;
bool Fail(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
  return false;
}
std::string Text(const runtime::ByteBuffer &bytes) {
  return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
[[maybe_unused]] std::span<const std::byte> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::byte *>(text.data()), text.size()};
}
// Inventory corruption/change detector, not an authenticity signature or security boundary.
std::string Fingerprint(std::span<const std::byte> bytes) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto byte : bytes) {
    hash ^= std::to_integer<unsigned char>(byte);
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}
bool Ordinary(const Path &path, bool directory) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || (directory ? !is_directory(status) : !is_regular_file(status)))
    return false;
#if defined(_WIN32)
  const auto attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
    return false;
#endif
  return true;
}
bool Physical(const Path &path) {
  std::error_code ec;
  return path.is_absolute() && Ordinary(path, true) &&
         std::filesystem::canonical(path, ec) == path && !ec;
}
bool Missing(const Path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return ec == std::errc::no_such_file_or_directory || (!ec && !exists(status));
}
Path UniqueDirectory(const Path &parent, std::string_view prefix) {
  if (!Physical(parent))
    return {};
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  for (unsigned index = 0; index < 100; ++index) {
    const auto path =
        parent / (std::string(prefix) + std::to_string(stamp) + "-" + std::to_string(index));
    std::error_code ec;
    if (std::filesystem::create_directory(path, ec))
      return path;
    if (ec && ec != std::errc::file_exists)
      break;
  }
  return {};
}
// Private operation directories only, under the cooperative single-writer threat model.
// Refuse cleanup if any directory was replaced by an alias. Never follow a reparse point.
void Cleanup(const Path &root) noexcept {
  try {
    if (root.empty() || !Physical(root))
      return;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end && !ec;
         it.increment(ec))
      if (!Ordinary(it->path(), it->is_directory(ec)))
        return;
    if (!ec)
      std::filesystem::remove_all(root, ec);
  } catch (...) {
  }
}
struct DirectoryOwner final {
  Path path;
  ~DirectoryOwner() { Cleanup(path); }
};
[[maybe_unused]] bool Write(const Path &path, std::span<const std::byte> bytes,
                            std::string *error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec || !Missing(path))
    return Fail(error, "Build staging path is occupied or unavailable.");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  out.close();
  return static_cast<bool>(out) || Fail(error, "Could not write a complete staged game file.");
}
std::optional<runtime::ByteBuffer> Read(const Path &path, std::size_t limit, std::string *error) {
  std::error_code ec;
  const auto root = std::filesystem::canonical(path.parent_path(), ec);
  if (ec)
    return {};
  auto source = ReadGameBuildSource(root, path.filename(), limit, false, error);
  return source ? std::move(source->bytes) : std::nullopt;
}
std::optional<std::vector<Path>> ScriptFiles(const Path &root, const Path &directory,
                                             std::string *error) {
  if (!Physical(root) || directory.empty() || directory == "." || directory.is_absolute()) {
    Fail(error, "A script manifest must be inside a dedicated project-relative source directory.");
    return {};
  }
  // Validate every parent and reject aliases before starting a recursive traversal.
  if (!ReadGameBuildSource(root, directory / "__nexora_absence_probe__", 0, true, error) ||
      !Ordinary(root / directory, true))
    return {};
  std::vector<Path> result;
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(root / directory, ec), end;
       it != end && !ec; it.increment(ec)) {
    const auto path = it->path();
    if (it->is_directory(ec)) {
      if (!Ordinary(path, true)) {
        Fail(error, "Script sources contain a directory alias.");
        return {};
      }
      if (path.filename() == ".zig-cache" || path.filename() == "zig-out" ||
          path.filename() == ".git")
        it.disable_recursion_pending();
    } else {
      if (!Ordinary(path, false) || result.size() == kMaximumGameBuildSourceFiles) {
        Fail(error, "Script sources exceed admission limits or contain a special file.");
        return {};
      }
      result.push_back(path.lexically_relative(root));
    }
  }
  if (ec) {
    Fail(error, "Cannot enumerate all script sources.");
    return {};
  }
  std::ranges::sort(result);
  return result;
}
[[maybe_unused]] bool Run(core::JobSystem &jobs, const Path &executable, const Path &cwd,
                          std::vector<std::string> arguments, const core::CancellationToken &cancel,
                          std::string &log, std::string *error, std::string *commands) {
  if (cancel.IsCancellationRequested())
    return Fail(error, "Game build cancelled.");
  const auto quote = [](std::string_view text) {
    std::string result = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : text) {
      if (c == '\\' || c == '"')
        result += std::string("\\") + static_cast<char>(c);
      else if (c < 32)
        result += std::string("\\u00") + hex[c >> 4] + hex[c & 15];
      else
        result += static_cast<char>(c);
    }
    return result + '"';
  };
  *commands += "{\"cwd\":" + quote(detail::PathUtf8(cwd)) + ",\"argv\":[" +
               quote(detail::PathUtf8(executable));
  for (const auto &argument : arguments)
    *commands += "," + quote(argument);
  *commands += "]}\n";
  BuildProcess process(jobs);
  if (!process.Start({1, executable, cwd, std::move(arguments), BuildProcess::kMaximumOutputBytes},
                     error))
    return false;
  for (;;) {
    if (cancel.IsCancellationRequested())
      static_cast<void>(process.Cancel());
    process.Poll(1);
    if (!process.Busy())
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const auto result = process.Snapshot();
  log += result.output;
  if (log.size() > 1024 * 1024)
    log.erase(0, log.size() - 1024 * 1024);
  if (cancel.IsCancellationRequested())
    return Fail(error, "Game build cancelled; managed child processes drained.");
  if (result.phase == BuildProcessPhase::Exited && result.exit_code == 0U)
    return true;
  auto diagnostic = result.output;
  if (!foundation::IsValidUtf8(diagnostic))
    diagnostic = "Tool output contains invalid UTF-8; inspect the tool directly.";
  if (diagnostic.size() > 4096) {
    diagnostic.erase(0, diagnostic.size() - 4096);
    while (!foundation::IsValidUtf8(diagnostic))
      diagnostic.erase(0, 1);
  }
  return Fail(error, "Game build tool failed: " + result.message + "\n" + diagnostic);
}
std::optional<std::string> Inventory(const Path &directory, std::string_view project,
                                     std::string *error) {
  if (!Physical(directory))
    return {};
  std::vector<Path> files;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(directory, ec), end; it != end && !ec;
       it.increment(ec)) {
    if (!Ordinary(it->path(), false) || files.size() == 256) {
      Fail(error, "A game output contains a directory, alias or excess files.");
      return {};
    }
    if (it->path().filename() != "Game.inventory")
      files.push_back(it->path().filename());
  }
  if (ec)
    return {};
  std::ranges::sort(files);
  std::size_t total{};
  std::string result =
      "NEXORA_GAME_INVENTORY_1\nproject=" + std::string(project) + "\nintegrity=fnv1a64-size\n";
  for (const auto &file : files) {
    auto data = ReadGameBuildSource(directory, file, kArtifactLimit - total, false, error);
    if (!data || !data->bytes)
      return {};
    total += data->bytes->size();
    result += detail::PathUtf8(file) + "\t" + std::to_string(data->bytes->size()) + "\t" +
              Fingerprint(*data->bytes) + "\n";
  }
  return result;
}
bool OwnedOutput(const Path &output, std::string_view project, std::string *error) {
  const auto inventory = Inventory(output, project, error);
  const auto expected = Read(output / "Game.inventory", 64 * 1024, error);
  return (inventory && expected && *inventory == Text(*expected)) ||
         Fail(error, "Existing game output is not an unchanged engine build for this project.");
}
} // namespace

std::optional<GameBuildToolchain> ReadGameBuildToolchain(const Path &profile, const Path &cmake,
                                                         const Path &ninja, const Path &zig,
                                                         std::string *error) {
  if (error)
    error->clear();
  if (!profile.is_absolute() || !Physical(profile.parent_path())) {
    Fail(error, "The engine runtime profile must have a physical absolute parent directory.");
    return {};
  }
  const auto data =
      ReadGameBuildSource(profile.parent_path(), profile.filename(), 64 * 1024, false, error);
  if (!data || !data->bytes)
    return {};
  std::istringstream input(Text(*data->bytes));
  std::string line;
  if (!std::getline(input, line))
    return {};
  if (line.ends_with('\r'))
    line.pop_back();
  if (line != "NEXORA_GAME_RUNTIME_1") {
    Fail(error, "Unsupported engine runtime profile.");
    return {};
  }
  GameBuildToolchain tools;
  tools.cmake = cmake;
  tools.ninja = ninja;
  tools.zig = zig;
  while (std::getline(input, line)) {
    if (line.ends_with('\r'))
      line.pop_back();
    const bool player = line.starts_with("player=");
    if ((!player && !line.starts_with("runtime=")) || tools.runtime_files.size() >= 256) {
      Fail(error, "Invalid engine runtime inventory entry.");
      return {};
    }
    const auto name = line.substr(player ? 7 : 8);
    const Path relative(std::u8string(name.begin(), name.end()));
    if (relative.empty() || relative.has_parent_path() ||
        !ReadGameBuildSource(profile.parent_path(), relative, kArtifactLimit, false, error))
      return {};
    if (player) {
      if (!tools.player.empty()) {
        Fail(error, "Duplicate engine runtime Player.");
        return {};
      }
      tools.player = profile.parent_path() / relative;
    } else
      tools.runtime_files.push_back(profile.parent_path() / relative);
  }
  if (tools.player.empty()) {
    Fail(error, "Engine runtime profile has no Player.");
    return {};
  }
  return tools;
}

struct PreparedGameBuild::Implementation final {
  DirectoryOwner operation;
  Path package, root, script_directory;
  std::vector<Path> script_files;
  std::vector<GameBuildSource> sources;
  std::string project, log, inventory, commands;
  bool published{};
  bool Current(std::string *error) const {
    if (!GameBuildSourcesCurrent(root, sources, error))
      return false;
    if (!script_directory.empty()) {
      const auto files = ScriptFiles(root, script_directory, error);
      if (!files || *files != script_files)
        return Fail(error, "Script source inventory changed during the game build.");
    }
    return true;
  }
};
PreparedGameBuild::PreparedGameBuild(std::unique_ptr<Implementation> value)
    : implementation_(std::move(value)) {}
PreparedGameBuild::~PreparedGameBuild() = default;
PreparedGameBuild::PreparedGameBuild(PreparedGameBuild &&) noexcept = default;
PreparedGameBuild &PreparedGameBuild::operator=(PreparedGameBuild &&) noexcept = default;
const Path &PreparedGameBuild::Directory() const noexcept { return implementation_->package; }
const std::string &PreparedGameBuild::Log() const noexcept { return implementation_->log; }

std::optional<PreparedGameBuild> PrepareGameBuild(GameBuildRequest request,
                                                  const core::CancellationToken &cancel,
                                                  std::string *error) {
#if !NEXORA_ASSET_PIPELINE_ENABLED
  static_cast<void>(request);
  static_cast<void>(cancel);
  Fail(error, "Game builds require the optional Asset Pipeline.");
  return {};
#else
  if (error)
    error->clear();
  auto state = std::make_unique<PreparedGameBuild::Implementation>();
  state->root = request.project_root;
  state->project = request.capture.project.project.ToString();
  state->sources = std::move(request.capture.sources);
  if ((request.mode != GameBuildMode::SceneOnly && request.mode != GameBuildMode::ScriptProject) ||
      request.capture.project.project == runtime::AssetUuid{} || state->sources.empty() ||
      !state->Current(error) || cancel.IsCancellationRequested()) {
    Fail(error, "Game build source capture is invalid, stale or cancelled.");
    return {};
  }
  state->operation.path = UniqueDirectory(request.temporary_parent, "nexora-game-");
  if (state->operation.path.empty()) {
    Fail(error, "Cannot create a private game build operation directory.");
    return {};
  }
  const auto &operation = state->operation.path;
  state->package = operation / "package";
  std::filesystem::create_directory(state->package);
  const auto package = CookGameProject(request.capture.project, error);
  if (!package || cancel.IsCancellationRequested() ||
      !Write(state->package / runtime::kGamePackageFilename, *package, error))
    return {};
  const auto launch = runtime::EncodeGameLaunch(
      {request.mode == GameBuildMode::ScriptProject, request.maximum_frames}, error);
  if (!launch || !Write(state->package / runtime::kGameLaunchFilename, *launch, error))
    return {};
  if (request.tools.runtime_files.size() > 128) {
    Fail(error, "Game runtime inventory exceeds its library limit.");
    return {};
  }
  std::size_t artifact_bytes = package->size() + launch->size();
  std::set<std::string> filenames{
      "game.nxgame", "game.launch", "game.inventory", "build.sources", "build.commands",
      "build.tools", "license",     "game.dll",       "libgame.so",    "libgame.dylib"};
  const auto copy = [&](const Path &from, const Path &name) {
    auto key = detail::PathUtf8(name);
    for (auto &c : key)
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c + ('a' - 'A'));
    if (name.empty() || name.has_parent_path() || !filenames.insert(key).second)
      return Fail(error, "Duplicate or reserved game runtime filename.");
    auto bytes = Read(from, kArtifactLimit - artifact_bytes, error);
    if (!bytes || !Write(state->package / name, *bytes, error))
      return false;
    artifact_bytes += bytes->size();
    std::error_code ec;
    std::filesystem::permissions(state->package / name,
                                 std::filesystem::status(from, ec).permissions(), ec);
    return !ec || Fail(error, "Could not retain executable permissions.");
  };
#if defined(_WIN32)
  const Path player_name = "Game.exe";
#else
  const Path player_name = "Game";
#endif
  if (!copy(request.tools.player, player_name))
    return {};
  for (const auto &file : request.tools.runtime_files) {
    const auto name = detail::PathUtf8(file.filename());
    if (!(name.ends_with(".dll") || name.ends_with(".so") || name.find(".so.") != name.npos ||
          name.ends_with(".dylib")) ||
        !copy(file, file.filename())) {
      Fail(error, "Game runtime inventory contains an invalid or unavailable library.");
      return {};
    }
  }
  core::JobSystem processes(1);
  processes.Start();
  // This is CPU data admission by the same consumer that will run the built game. No window,
  // native device or authored module callback is entered during this verification.
  if (!Run(processes, request.tools.player, operation,
           {"--verify-native-game-package", detail::PathUtf8(state->package / "Game.nxgame")},
           cancel, state->log, error, &state->commands))
    return {};
  if (request.mode == GameBuildMode::ScriptProject) {
    state->script_directory = request.script_manifest.parent_path();
    const auto files = ScriptFiles(state->root, state->script_directory, error);
    if (!files)
      return {};
    state->script_files = *files;
    if (state->script_files.size() + state->sources.size() > kMaximumGameBuildSourceFiles) {
      Fail(error, "Combined game/script source count exceeds admission limits.");
      return {};
    }
    std::size_t source_bytes{};
    for (const auto &source : state->sources)
      if (source.bytes)
        source_bytes += source.bytes->size();
    for (const auto &path : *files) {
      auto source = ReadGameBuildSource(state->root, path,
                                        kMaximumGameBuildSourceBytes - source_bytes, false, error);
      if (!source || !source->bytes || !Write(operation / "source" / path, *source->bytes, error))
        return {};
      source_bytes += source->bytes->size();
      state->sources.push_back(std::move(*source));
    }
    if (!state->Current(error))
      return {};
    const auto sdk = operation / "sdk";
    for (const auto &file : detail::kGameSDKFiles)
      if (!Write(sdk / file.path, Bytes(file.bytes), error))
        return {};
    constexpr std::string_view cmake = R"cmake(cmake_minimum_required(VERSION 3.24)
project(NexoraInstalledGameBuild LANGUAGES NONE)
include("${CMAKE_CURRENT_LIST_DIR}/sdk/CMake/Modules/NexoraGameProject.cmake")
nexora_add_game_module(GameModule PROJECT "${NEXORA_GAME_MANIFEST}"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/module" ZIG "${NEXORA_GAME_ZIG}")
file(READ "${NEXORA_GAME_MANIFEST}" manifest)
string(JSON module_name GET "${manifest}" module_name)
if(WIN32)
  set(module_file "bin/${module_name}.dll")
elseif(APPLE)
  set(module_file "lib/lib${module_name}.dylib")
else()
  set(module_file "lib/lib${module_name}.so")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/module-file.txt" "${module_file}")
)cmake";
    if (!Write(operation / "CMakeLists.txt", Bytes(cmake), error) ||
        !Run(processes, request.tools.cmake, operation,
             {"-S", detail::PathUtf8(operation), "-B", detail::PathUtf8(operation / "compile"),
              "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + detail::PathUtf8(request.tools.ninja),
              "-DNEXORA_GAME_MANIFEST=" +
                  detail::PathUtf8(operation / "source" / request.script_manifest),
              "-DNEXORA_GAME_ZIG=" + detail::PathUtf8(request.tools.zig)},
             cancel, state->log, error, &state->commands) ||
        !Run(processes, request.tools.cmake, operation,
             {"--build", detail::PathUtf8(operation / "compile"), "--target", "GameModule"}, cancel,
             state->log, error, &state->commands))
      return {};
    const auto module_name = Read(operation / "compile/module-file.txt", 256, error);
    if (!module_name)
      return {};
    auto module = ReadGameBuildSource(
        operation / "compile/module",
        Path(std::u8string(reinterpret_cast<const char8_t *>(module_name->data()),
                           module_name->size())),
        64 * 1024 * 1024, false, error);
    if (!module || !module->bytes ||
        !Write(state->package / runtime::kGameModuleFilename, *module->bytes, error))
      return {};
    if (!Run(processes, request.tools.player, operation,
             {"--verify-game-module",
              detail::PathUtf8(state->package / runtime::kGameModuleFilename)},
             cancel, state->log, error, &state->commands))
      return {};
    // Zig entry tests run in the established adapter; actual module lifetime/rendering is an
    // integrated runtime acceptance gate, not inferred from compilation or this inventory.
  } else if (!request.script_manifest.empty()) {
    Fail(error, "Scene-only mode cannot silently ignore a configured script manifest.");
    return {};
  }
  if (!Run(processes, state->package / player_name, operation,
           {"--verify-native-game-package", detail::PathUtf8(state->package / "Game.nxgame")},
           cancel, state->log, error, &state->commands))
    return {};
  if (request.mode == GameBuildMode::ScriptProject &&
      !Run(
          processes, state->package / player_name, operation,
          {"--verify-game-module", detail::PathUtf8(state->package / runtime::kGameModuleFilename)},
          cancel, state->log, error, &state->commands))
    return {};
  for (const auto &file : detail::kGameSDKFiles)
    if (file.path == "LICENSE" && !Write(state->package / "LICENSE", Bytes(file.bytes), error))
      return {};
  std::string sources =
      "NEXORA_GAME_SOURCES_1\nproject=" + state->project +
      "\nstart_scene=" + request.capture.project.start_scene.ToString() +
      "\nmode=" + (request.mode == GameBuildMode::SceneOnly ? "scene-only" : "script-project") +
      "\n";
  for (const auto &scene : request.capture.project.scenes)
    sources += "scene=" + scene.entry.id.ToString() + "\t" + scene.entry.path + "\n";
  for (const auto &source : state->sources)
    sources +=
        detail::PathUtf8(source.relative) + "\t" +
        (source.bytes ? std::to_string(source.bytes->size()) + "\t" + Fingerprint(*source.bytes)
                      : "absent") +
        "\n";
  std::string tools = "NEXORA_GAME_BUILD_TOOLS_1\nintegrity=fnv1a64-size\n";
  if (request.mode == GameBuildMode::ScriptProject) {
    for (const auto &executable : {request.tools.cmake, request.tools.ninja, request.tools.zig}) {
      const auto data = Read(executable, 256 * 1024 * 1024, error);
      if (!data)
        return {};
      tools += detail::PathUtf8(executable.filename()) + "\t" + std::to_string(data->size()) +
               "\t" + Fingerprint(*data) + "\n";
    }
    for (const auto &file : detail::kGameSDKFiles)
      tools += std::string(file.path) + "\t" + std::to_string(file.bytes.size()) + "\t" +
               Fingerprint(Bytes(file.bytes)) + "\n";
  }
  if (!Write(state->package / "Build.commands", Bytes(state->commands), error) ||
      !Write(state->package / "Build.tools", Bytes(tools), error))
    return {};
  if (!Write(state->package / "Build.sources", Bytes(sources), error) || !state->Current(error) ||
      cancel.IsCancellationRequested())
    return {};
  const auto inventory = Inventory(state->package, state->project, error);
  if (!inventory || !Write(state->package / "Game.inventory", Bytes(*inventory), error))
    return {};
  state->inventory = *inventory;
  return PreparedGameBuild(std::move(state));
#endif
}

bool PreparedGameBuild::Publish(const Path &output, const core::CancellationToken &cancel,
                                const std::function<bool()> &owner_current, std::string *error) {
  if (error)
    error->clear();
  if (!implementation_ || implementation_->published)
    return Fail(error, "Game build has already been published or moved.");
  auto &state = *implementation_;
  const auto inside = [](const Path &path, const Path &directory) {
    const auto relative = path.lexically_relative(directory);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
  };
  const auto allowed = [&] {
    return !cancel.IsCancellationRequested() && owner_current && owner_current() &&
           state.Current(error);
  };
  if (!allowed())
    return Fail(error, "Game build publication cancelled or authoring state changed.");
  if (!output.is_absolute() || output.filename().empty() || !Physical(output.parent_path()) ||
      output.filename() == "." || output.filename() == ".." || output.lexically_normal() != output)
    return Fail(error,
                "Game output must have a physical parent and a new ordinary directory name.");
  if (inside(output, state.operation.path) ||
      (!state.script_directory.empty() && inside(output, state.root / state.script_directory)) ||
      !ReadGameBuildSource(output.parent_path(), output.filename() / "__nexora_output_probe__", 0,
                           true, error))
    return Fail(error, "Game output overlaps private build/script sources or uses an unsafe name.");
  const auto inventory = Inventory(state.package, state.project, error);
  const auto manifest = Read(state.package / "Game.inventory", 64 * 1024, error);
  if (!inventory || !manifest || *inventory != state.inventory ||
      Text(*manifest) != state.inventory)
    return Fail(error, "Prepared game artifacts changed before publication.");
  const bool previous = !Missing(output);
  if (previous && !OwnedOutput(output, state.project, error))
    return false;
  DirectoryOwner candidate{UniqueDirectory(output.parent_path(), ".nexora-game-publish-")};
  DirectoryOwner backup{UniqueDirectory(output.parent_path(), ".nexora-game-previous-")};
  if (candidate.path.empty() || backup.path.empty())
    return Fail(error, "Cannot reserve game publication directories.");
  std::error_code ec;
  for (const auto &file : std::filesystem::directory_iterator(state.package)) {
    std::filesystem::copy_file(file.path(), candidate.path / file.path().filename(), ec);
    if (ec)
      return Fail(error, "Cannot stage game output on its destination filesystem.");
  }
  if (!OwnedOutput(candidate.path, state.project, error) || !allowed() ||
      (previous ? !OwnedOutput(output, state.project, error) : !Missing(output)))
    return Fail(error, "Game publication became stale before the directory commit.");
  // Cancellation linearizes at the guard above. Cooperative writers must serialize publication.
  // The two renames are recoverable, not a power-loss-atomic replacement of a nonempty directory.
  std::filesystem::remove(backup.path, ec);
  if (ec)
    return Fail(error, "Cannot reserve the previous game output path.");
  if (previous) {
    std::filesystem::rename(output, backup.path, ec);
    if (ec)
      return Fail(error, "Cannot move the previous game output; it may still be running.");
  }
  std::filesystem::rename(candidate.path, output, ec);
  if (ec) {
    if (previous) {
      std::error_code rollback;
      std::filesystem::rename(backup.path, output, rollback);
      if (rollback) {
        const auto retained = detail::PathUtf8(backup.path);
        backup.path.clear();
        return Fail(error,
                    "Game publication rollback failed; previous game retained at " + retained);
      }
    }
    return Fail(error, "Cannot commit staged game output; previous output restored.");
  }
  candidate.path.clear();
  state.published = true;
  return true;
}
} // namespace nexora::editor
