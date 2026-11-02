#include "Nexora/Editor/GameScriptBuild.h"
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace nexora;
using Path = std::filesystem::path;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const Path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  Require(static_cast<bool>(output), "fixture write failed");
}
std::string Read(const Path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "fixture read failed");
  return {std::istreambuf_iterator<char>(input), {}};
}
auto Files(const Path &root) {
  std::map<Path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file())
      result.emplace(entry.path().lexically_relative(root), Read(entry.path()));
  return result;
}
void Run(const Path &fixture, int argc, char **argv) {
  Require(argc == 5, "expected Player/CMake/Ninja/Zig paths");
  const auto project = fixture / Path{u8"專案 scripts"};
  const auto temporary = fixture / "operations";
  std::filesystem::create_directories(project / "Code");
  std::filesystem::create_directory(temporary);
  const std::string script = R"zig(const nx = @import("nexora");
pub const Game = struct {
    pub const reload_lifecycle = true;
    pub const state_version: u32 = 1;
    counter: u64 = 1,
    pub fn update(self: *@This(), _: *const nx.game.Context, _: f64) !void { self.counter += 1; }
    pub fn saveState(self: *const @This(), writer: *nx.game.StateWriter) !void {
        try writer.int(u64, self.counter);
    }
    pub fn loadState(self: *@This(), reader: *nx.game.StateReader) !void {
        self.counter = try reader.int(u64);
        try reader.finish();
    }
};
)zig";
  Write(project / "Code/game.zig", script);
  Write(project / "Code/project.json",
        R"({"schema":1,"module_name":"ScriptOnlyFixture","script":"game.zig"})");
  editor::GameScriptBuildRequest request;
  request.project_root = project;
  request.temporary_parent = temporary;
  request.script_manifest = "Code/project.json";
  request.tools.player = Path(argv[1]);
  request.tools.cmake = Path(argv[2]);
  request.tools.ninja = Path(argv[3]);
  request.tools.zig = Path(argv[4]);
  const auto original = Files(project);
  std::string error;
  auto worker = std::async(std::launch::async, [request, &error] {
    return editor::PrepareGameScript(request, {}, &error);
  });
  auto first = worker.get();
  Require(first.has_value(), error.c_str());
  Require(first->Current(&error), error.c_str());
  Require(first->Log().find("NEXORA_SCRIPT_TESTS_PASSED") != std::string::npos,
          "canonical SDK tests did not execute");
  Require(Files(project) == original && !exists(project / "project.nexora"),
          "script-only compilation required scene/project cooking or changed authored sources");
  auto second = editor::PrepareGameScript(request, {}, &error);
  Require(second.has_value(), error.c_str());
  Require(first->Library() != second->Library() &&
              first->Library().filename() != second->Library().filename() && first->Current() &&
              second->Current(),
          "candidate generations reused a library identity");
  const auto first_bytes = Read(first->Library());
  Require(!first_bytes.empty(), "compiled library is empty");
  auto corrupted = first_bytes;
  corrupted[0] ^= 1;
  Write(first->Library(), corrupted);
  Require(!first->Current() && second->Current(), "candidate artifact guard escaped");
  Write(first->Library(), first_bytes);
  Require(first->Current(), "restored fixture artifact did not match its original fingerprint");
  Write(project / "Code/new.zig", "// new source\n");
  Require(!first->Current() && !second->Current(), "new source escaped inventory guard");
  std::filesystem::remove(project / "Code/new.zig");
  const auto script_time = std::filesystem::last_write_time(project / "Code/game.zig");
  auto changed = script;
  const auto counter = changed.find("counter: u64 = 1");
  Require(counter != std::string::npos, "counter fixture missing");
  changed[counter + std::string_view("counter: u64 = ").size()] = '2';
  Write(project / "Code/game.zig", changed);
  std::filesystem::last_write_time(project / "Code/game.zig", script_time);
  Require(!first->Current() && !second->Current(),
          "same-size source edit with preserved timestamp escaped byte guard");
  Write(project / "Code/game.zig", "invalid Zig source\n");
  Require(!first->Current(), "changed source escaped candidate guard");
  Require(!editor::PrepareGameScript(request, {}, &error) && !error.empty() &&
              Read(first->Library()) == first_bytes,
          "failed compilation changed a prior candidate or lost diagnostics");
  Write(project / "Code/game.zig", script);
  Require(first->Current() && second->Current() && Files(project) == original,
          "compiler failed to preserve authored input or prior candidates");
  core::CancellationSource cancelled;
  cancelled.Cancel();
  Require(!editor::PrepareGameScript(request, cancelled.Token(), &error),
          "pre-cancelled script compilation ran");
  auto invalid = request;
  invalid.maximum_source_files = 0;
  Require(!editor::PrepareGameScript(invalid, {}, &error) && !error.empty(),
          "source budget failure omitted diagnostics");
  invalid = request;
  invalid.temporary_parent = project;
  Require(!editor::PrepareGameScript(invalid, {}, &error) && Files(project) == original,
          "compiler wrote private operations into authored project");
}
} // namespace

int main(int argc, char **argv) {
  const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
  const auto fixture =
      parent / ("nexora-script-only-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Require(std::filesystem::create_directory(fixture), "fixture directory creation failed");
    Run(fixture, argc, argv);
    const auto resolved = std::filesystem::canonical(fixture);
    Require(resolved == fixture && resolved.parent_path() == parent,
            "fixture cleanup escaped root");
    std::filesystem::remove_all(resolved);
    std::cout << "Standalone canonical SDK compilation, unique candidates, source/artifact guards "
                 "and failure conservation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\nFixture retained at " << fixture << '\n';
    return 1;
  }
}
