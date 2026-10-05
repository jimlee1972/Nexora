#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/SceneFiles.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Status = editor::SceneFileStatus;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
void RunDiscovery() {
  editor::ContentBrowserModel browser;
  const runtime::AssetUuid old{1, 1}, added{2, 2};
  Require(browser.Reset(
              std::array{editor::ContentItem{old, "Content/Old.txt", ".txt", "old", {}, {}}}, 1) &&
              browser.Select(old) && browser.Rename(old, "Renamed.txt"),
          "Content Undo fixture failed");
  Require(browser.Discover({added, "Content/Saved.scene", ".scene", "saved", {}, {}}) &&
              browser.IsSelected(old) && browser.Find(added) &&
              !browser.Discover({added, "Content/Duplicate.scene", ".scene", "bad", {}, {}}) &&
              !browser.Discover({{3, 3}, "Content/Old.txt", ".txt", "bad", {}, {}}) &&
              browser.Undo() && browser.Find(old)->path == "Content/Old.txt" &&
              browser.Find(added) && browser.IsSelected(old),
          "Scene discovery lost content Undo or admitted duplicate identity/path");
}
void Run(const std::filesystem::path &root) {
  RunDiscovery();
  editor::ProjectWorkspace workspace, reader;
  Require(workspace.Create(root, "Scene files") &&
              reader.Open(root, editor::ProjectAccess::ReadOnly),
          "Workspace fixture failed");
  runtime::World world;
  const auto id = world.LoadScene("Authored", true), other = world.LoadScene("Other");
  Require(world.Activate(id), "Scene activation failed");
  editor::SceneDocument scene(world, id), unrelated(world, other);
  Require(unrelated.Create("Unrelated") != 0, "Other scene fixture failed");
  const auto unrelated_before = world.SaveScene(other);
  const auto camera = scene.CreateCamera("Camera");
  const auto light = scene.CreateLight("Light", camera);
  Require(camera && light && scene.SetEulerField(std::array{*scene.Key(camera)}, 1, 720) &&
              scene.Select(std::array{*scene.Key(camera)}) && scene.CopySelection(),
          "Scene fixture failed");
  editor::SceneFileSession files(workspace, scene);
  auto token = files.Token();
  const auto original = world.SaveScene(id);
  Require(files.New(token).status == Status::NeedsUnsavedChoice &&
              files.Save(token).status == Status::NeedsPath && world.SaveScene(id) == original,
          "Missing choices mutated the document");
  const auto first = std::filesystem::path("Content/First.scene");
  Require(files.SaveAs(token, first).Applied() && files.CurrentPath() == first && !scene.Dirty(),
          "Save As did not adopt the saved document");
  const auto first_bytes = Read(root / first);
  Require(scene.Rename(*scene.Key(camera), "Unsaved Camera"), "Dirty fixture failed");
  const auto dirty_world = world.SaveScene(id);
  const auto dirty_generation = scene.Generation();
  Require(files.Open(token, first).status == Status::NeedsUnsavedChoice &&
              scene.Name(camera) == "Unsaved Camera",
          "Open silently discarded edits");
  std::ofstream(root / "Content/Corrupt.scene") << "corrupt scene";
  Require(files.Open(token, "Content/Corrupt.scene", true).status == Status::Rejected &&
              files.CurrentPath() == first && world.SaveScene(id) == dirty_world &&
              scene.Generation() == dirty_generation && scene.Dirty() && scene.Undo() &&
              scene.Name(camera) == "Camera" && scene.Redo(),
          "Failed Open changed path, history, generation or dirty state");
  Require(files.New(token, true).Applied() && !files.CurrentPath() && scene.Nodes().empty() &&
              scene.Selection().empty() && scene.Dirty() && !scene.Undo() && !scene.Redo() &&
              !scene.Paste() && world.FindScene(id)->state == runtime::SceneState::Active &&
              world.FindScene(id)->name == "Authored" && world.FindScene(id)->persistent &&
              world.SaveScene(other) == unrelated_before,
          "New did not establish an empty document boundary preserving the live scene");
  Require(files.SaveAs(token, "Content/Stale.scene").status == Status::Rejected &&
              !std::filesystem::exists(root / "Content/Stale.scene"),
          "Previous document token saved the new document");
  token = files.Token();
  Require(files.SaveAs(token, first).status == Status::NeedsOverwrite &&
              Read(root / first) == first_bytes && scene.Dirty() && !files.CurrentPath(),
          "Save As replaced a different file without confirmation");
  Require(files.SaveAs(token, "Content/Empty.scene").Applied() && !scene.Dirty() &&
              files.Open(token, first).Applied() && scene.Camera(*scene.Key(camera)) &&
              scene.Light(*scene.Key(light)) && scene.Parent(light) == camera &&
              (*scene.EulerAngles(camera))[1] == 720 && !scene.Dirty() && !scene.Undo(),
          "Open lost hierarchy/components/Euler metadata or retained old history");
  token = files.Token();
  Require(scene.Rename(*scene.Key(camera), "Second Camera") &&
              files.SaveAs(token, "Content/Empty.scene", true).Applied() &&
              files.CurrentPath() == "Content/Empty.scene" && Read(root / first) == first_bytes,
          "Confirmed Save As did not adopt destination or modified the original file");
  Require(scene.Undo() && files.Save(token).Applied() && scene.Redo() &&
              scene.Name(camera) == "Second Camera",
          "Save consumed Redo history");

  const auto before_path = files.CurrentPath();
  const auto before_bytes = Read(root / *before_path);
  const auto before_world = world.SaveScene(id);
  const auto invalid = std::array<std::filesystem::path, 10>{
      root / "Absolute.scene",   "../Outside.scene",
      "Content/../Escape.scene", "Content/./Dot.scene",
      "Content/Bad.txt",         ".nexora/project.scene",
      "Content/Bad:Name.scene",  "Content/Bad\nName.scene",
      "Content/Bad\\Name.scene", std::string(1024, 'x') + ".scene"};
  for (const auto &path : invalid)
    Require(files.SaveAs(token, path, true).status == Status::Rejected &&
                files.Open(token, path, true).status == Status::Rejected &&
                files.CurrentPath() == before_path && world.SaveScene(id) == before_world &&
                Read(root / *before_path) == before_bytes,
            "Invalid path modified a managed document or file");
  const auto occupied = root / *before_path;
  auto temporary = occupied;
  temporary += ".tmp";
  std::ofstream(temporary) << "preexisting temporary sentinel";
  Require(files.Save(token).status == Status::Rejected &&
              Read(temporary) == "preexisting temporary sentinel" &&
              Read(occupied) == before_bytes && world.SaveScene(id) == before_world &&
              files.CurrentPath() == before_path,
          "Save truncated a preexisting temporary file or changed the committed document");
  std::filesystem::remove(temporary);
  std::filesystem::create_directories(root / "Content/Directory.scene");
  Require(!scene.Save(root / "Content/Directory.scene") &&
              std::filesystem::is_directory(root / "Content/Directory.scene") &&
              !std::filesystem::exists(root / "Content/Directory.scene.tmp") &&
              world.SaveScene(id) == before_world,
          "Failed atomic replacement deleted the original directory or left its owned temporary");
  Require(files.SaveAs(token, "Content/Directory.scene", true).status == Status::Rejected &&
              files.SaveAs(token, "Content/First.scene/Child.scene").status == Status::Rejected &&
              files.CurrentPath() == before_path,
          "Failed destination changed current path");
  const auto outside = root.parent_path() / (root.filename().string() + "-outside");
  std::filesystem::create_directories(outside);
  std::ofstream(outside / "Keep.scene") << "outside sentinel";
  std::error_code error;
  std::filesystem::create_directory_symlink(outside, root / "Content/Escape", error);
  if (!error) {
    std::filesystem::create_symlink(outside / "Keep.scene", temporary, error);
    Require(!error && files.Save(token).status == Status::Rejected &&
                std::filesystem::is_symlink(temporary) &&
                Read(outside / "Keep.scene") == "outside sentinel" &&
                Read(occupied) == before_bytes,
            "Save followed or removed a preexisting temporary symlink");
    std::filesystem::remove(temporary);
    Require(files.SaveAs(token, "Content/Escape/Keep.scene", true).status == Status::Rejected &&
                files.Open(token, "Content/Escape/Keep.scene", true).status == Status::Rejected &&
                Read(outside / "Keep.scene") == "outside sentinel",
            "Symlink escaped the project scope");
  }
  std::filesystem::remove_all(outside);
  Require(files.SaveAs(token, std::filesystem::path(u8"Content/場景.scene")).Applied(),
          "UTF-8 scene path failed");
  editor::SceneFileSession observer(reader, scene);
  Require(observer.BindCurrent(first) &&
              observer.New(observer.Token(), true).status == Status::Rejected &&
              observer.Save(observer.Token()).status == Status::Rejected &&
              observer.SaveAs(observer.Token(), "Content/ReadOnly.scene").status ==
                  Status::Rejected &&
              observer.Open(observer.Token(), first, true).Applied() &&
              !std::filesystem::exists(root / "Content/ReadOnly.scene"),
          "Read-only access blocked Open or allowed a write");
  Require(files.Save(files.Token()).status == Status::Rejected,
          "External Reload did not invalidate the old session");
  editor::SceneFileSession protected_file(workspace, scene);
  Require(protected_file.BindCurrent("Content/Corrupt.scene", true) &&
              protected_file.Save(protected_file.Token()).status == Status::Rejected &&
              protected_file.SaveAs(protected_file.Token(), "Content/Corrupt.scene").status ==
                  Status::NeedsOverwrite &&
              Read(root / "Content/Corrupt.scene") == "corrupt scene" &&
              protected_file.New(protected_file.Token(), true).Applied() &&
              !protected_file.SaveBlocked(),
          "Failed-load destination was overwritten");
  runtime::World play(runtime::WorldKind::Play);
  const auto play_id = play.LoadScene("Play");
  editor::SceneDocument play_scene(play, play_id);
  const auto play_before = play.SaveScene(play_id);
  const auto generation_before = play_scene.Generation();
  Require(!play_scene.NewScene() && play.SaveScene(play_id) == play_before &&
              play_scene.Generation() == generation_before,
          "New mutated a Play World");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-scene-files-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root);
    std::filesystem::remove_all(root);
    std::cout << "Scene file document boundaries and failure contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
