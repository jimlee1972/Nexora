#include "GameBuildCoordinator.h"
#include "Nexora/Editor/BuildProcess.h"
#include "Nexora/Editor/GameBuild.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Runtime/GameLaunch.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::string_view text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.close();
  Require(static_cast<bool>(out), "fixture write");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
std::filesystem::path Path(const char *text) {
  const std::string_view value(text);
  return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
std::string Utf8(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return {text.begin(), text.end()};
}
std::uint32_t Command(const std::filesystem::path &executable, const std::filesystem::path &cwd,
                      std::vector<std::string> arguments) {
  core::JobSystem jobs(1);
  jobs.Start();
  editor::BuildProcess process(jobs);
  std::string error;
  Require(process.Start({1, executable, cwd, std::move(arguments)}, &error), error.c_str());
  while (process.Busy()) {
    process.Poll(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const auto result = process.Snapshot();
  if (result.exit_code != 0U)
    std::cerr << result.output << '\n';
  Require(result.exit_code.has_value(), "child did not exit normally");
  return *result.exit_code;
}
std::optional<editor::PreparedGameBuild> PrepareOnWorker(const editor::GameBuildRequest &request,
                                                         std::string &error) {
  editor::preview::GameBuildCoordinator worker;
  Require(worker.Start(request) && worker.Busy() && !worker.Start(request),
          "worker duplicated a live build");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto result = worker.Poll()) {
      Require(!worker.Busy(), "completed worker retained a live future");
      error = std::move(result->error);
      return std::move(result->prepared);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  throw std::runtime_error("graphical build worker deadline exceeded");
}
void Run(const std::filesystem::path &root, int argc, char **argv) {
  Require(argc >= 7, "builder test arguments");
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Shared game build"), "project");
  Write(root / "Content/Triangle.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
  editor::AssetWorkspace assets;
  Require(
      assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
      "persistent shared mesh import");
  editor::ProjectContentSession content_session;
  Require(content_session.Open(workspace, assets, 1), "content session");
  auto &content = content_session.Browser();
  editor::MeshAssetCatalog meshes;
  editor::MaterialAssetCatalog materials;
  Require(meshes.PublishContent(content) && materials.PublishContent(content), "catalogs");
  runtime::World a, b;
  editor::SceneDocument title(a, a.LoadScene("Title")), level(b, b.LoadScene("Level01"));
  const auto title_camera = title.CreateCamera("Camera"),
             level_camera = level.CreateCamera("Camera");
  Require(title_camera && level_camera && title.SetTransform(title_camera, {0, 0, 5}) &&
              level.SetTransform(level_camera, {10, 0, 5}),
          "cameras");
  for (auto *document : {&title, &level}) {
    const auto entity = document->Create("Shared triangle");
    Require(document->SetMeshRenderer(
                *document->Key(entity),
                runtime::MeshComponent{runtime::MeshResourceId(assets.Entries().front().id), {0}}),
            "shared source geometry");
    Require(document->SetTransform(entity, {document == &title ? 1.0 : 9.0, 0, 0}),
            "distinct scene poses");
  }
  Require(title.Save(root / "Title.scene") && level.Save(root / "Level01.scene"), "saved scenes");
  editor::ProjectBuildSettingsSession settings(workspace);
  Require(settings.Reload(), "settings");
  auto first = settings.Enroll("Title.scene", content);
  auto second = settings.Enroll("Level01.scene", content);
  Require(first && second, "enrolled root scenes");
  first->id = {1, 1};
  second->id = {1, 2};
  Require(first && second && settings.Save({second->id, {*first, *second}}, content), "build list");
  std::string error;
  auto capture = editor::CaptureGameBuild(workspace, content, meshes, materials, &error);
  Require(capture.has_value(), error.c_str());
  editor::GameBuildRequest request;
  request.project_root = root;
  request.temporary_parent = root.parent_path();
  request.capture = *capture;
  request.tools.player = Path(argv[1]);
  request.tools.cmake = Path(argv[3]);
  request.tools.ninja = Path(argv[4]);
  request.tools.zig = Path(argv[5]);
  request.maximum_frames = 100;
  for (int index = 7; index < argc; ++index)
    request.tools.runtime_files.push_back(Path(argv[index]));
  const auto output = root / "BuiltGame";
  auto prepared = PrepareOnWorker(request, error);
  Require(prepared.has_value(), error.c_str());
  Require(!prepared->Publish(output, {}, [] { return false; }) && !exists(output), "owner guard");
  core::CancellationSource cancelled;
  cancelled.Cancel();
  Require(!prepared->Publish(output, cancelled.Token(), [] { return true; }) && !exists(output),
          "cancel guard");
  const auto original = Read(root / "Title.scene");
  Write(root / "Title.scene", original + "\n");
  Require(!prepared->Publish(output, {}, [] { return true; }) && !exists(output), "stale scene");
  Write(root / "Title.scene", original);
  Require(prepared->Publish(output, {}, [] { return true; }, &error), error.c_str());
  const auto old_inventory = Read(output / "Game.inventory");
#if defined(_WIN32)
  const auto installed = output / "Game.exe";
#else
  const auto installed = output / "Game";
#endif
  Require(Command(installed, root,
                  {"--verify-native-game-package", Utf8(output / "Game.nxgame")}) == 0U,
          "staged runtime dependency closure is incomplete");
  std::string flat_profile =
      "NEXORA_GAME_RUNTIME_1\r\nplayer=" + Utf8(installed.filename()) + "\r\n";
  for (const auto &library : request.tools.runtime_files)
    flat_profile += "runtime=" + Utf8(library.filename()) + "\r\n";
  Write(output / "FlatRuntime.profile", flat_profile);
  const auto flat_tools =
      editor::ReadGameBuildToolchain(output / "FlatRuntime.profile", request.tools.cmake,
                                     request.tools.ninja, request.tools.zig, &error);
  Require(flat_tools && flat_tools->player == installed &&
              flat_tools->runtime_files.size() == request.tools.runtime_files.size(),
          "flat installed/CRLF profile compatibility");
  std::filesystem::remove(output / "FlatRuntime.profile");
  Require(Command(Path(argv[6]), root,
                  {"--build-game=" + Utf8(root), "--output=" + Utf8(root / "CliGame"),
                   "--runtime-profile=" +
                       Utf8(request.tools.player.parent_path() / "GameRuntime.profile"),
                   "--scene-only"}) == 0U &&
              exists(root / "CliGame/Game.inventory"),
          "Editor CLI did not use the shared build service");
  Require(Command(Path(argv[6]), root,
                  {"--build-game=" + Utf8(root), "--output=" + Utf8(root / "InstalledCliGame"),
                   "--scene-only"}) == 0U &&
              exists(root / "InstalledCliGame/Game.inventory"),
          "Editor CLI could not resolve its installed/developer runtime and tool configuration");
  Require(!old_inventory.empty() && exists(output / "Game.launch") &&
              exists(output / "Game.nxgame") && !exists(output / "Title.scene") &&
              !exists(output / "sdk") && !exists(output / "source"),
          "source-free artifact");
  auto changed = editor::PrepareGameBuild(request, {}, &error);
  Require(changed.has_value(), error.c_str());
  Write(output / "keep-user-file.txt", "user data");
  Require(!changed->Publish(output, {}, [] { return true; }) &&
              Read(output / "Game.inventory") == old_inventory &&
              exists(output / "keep-user-file.txt"),
          "foreign output overwritten");
  std::filesystem::remove(output / "keep-user-file.txt");
  Write(changed->Directory() / "Game.launch", "tampered");
  Require(!changed->Publish(output, {}, [] { return true; }) &&
              Read(output / "Game.inventory") == old_inventory,
          "tampered artifact published");
  std::filesystem::create_directory(root / "Scripts");
  std::filesystem::copy_file(Path(argv[2]), root / "Scripts/game.zig");
  Write(root / "Scripts/project.json",
        R"({"schema":1,"module_name":"BuiltGameFixture","script":"game.zig"})");
  request.mode = editor::GameBuildMode::ScriptProject;
  request.script_manifest = "Scripts/project.json";
  auto scripted = PrepareOnWorker(request, error);
  Require(scripted.has_value(), error.c_str());
  Require(scripted->Log().find("NEXORA_SCRIPT_TESTS_PASSED") != std::string::npos,
          "SDK tests not run");
  Write(root / "Scripts/new.zig", "// new source\n");
  Require(!scripted->Publish(output, {}, [] { return true; }) &&
              Read(output / "Game.inventory") == old_inventory,
          "script inventory change escaped guard");
  std::filesystem::remove(root / "Scripts/new.zig");
  Require(scripted->Publish(output, {}, [] { return true; }, &error), error.c_str());
  Require(exists(output / runtime::kGameModuleFilename) &&
              Read(output / "Build.sources").find("mode=script-project") != std::string::npos,
          "compiled module missing");
  const auto scripted_inventory = Read(output / "Game.inventory");
  Write(root / "Scripts/game.zig", "invalid Zig source\n");
  Require(!editor::PrepareGameBuild(request, {}, &error) &&
              Read(output / "Game.inventory") == scripted_inventory,
          "failed compile replaced previous game");
  Require(!editor::PrepareGameBuild(request, cancelled.Token()), "pre-cancelled build ran");
  {
    editor::preview::GameBuildCoordinator worker;
    Require(worker.Start(request), "cancel worker start");
    worker.Cancel();
    std::optional<editor::preview::GameBuildCoordinator::Result> result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!result && std::chrono::steady_clock::now() < deadline) {
      result = worker.Poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Require(result && !result->prepared && !worker.Busy() &&
                Read(output / "Game.inventory") == scripted_inventory,
            "cancelled graphical worker published or retained a live operation");
  }
  workspace = editor::ProjectWorkspace{};
}
} // namespace
int main(int argc, char **argv) {
  auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
              ("nexora-shared-game-build-" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  bool retain{};
  if (argc > 1 && std::string_view(argv[argc - 1]).starts_with("--retain=")) {
    root = Path(argv[argc - 1] + 9);
    if (!root.is_absolute() || exists(root))
      return 2;
    retain = true;
    --argc;
  }
  try {
    Run(root, argc, argv);
    if (!retain)
      std::filesystem::remove_all(root);
    std::cout
        << "Actual SDK/CMake game build, source-free inventory and guarded publication passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\nFixture retained at " << root << '\n';
    return 1;
  }
}
