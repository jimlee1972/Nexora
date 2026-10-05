#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/ProjectContent.h"
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
void RunSavedSceneImport(const std::filesystem::path &content) {
  std::filesystem::create_directories(content);
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(content.parent_path(), "Single import"),
          "Single import workspace failed");
  const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
  std::ofstream(content / "Triangle.obj") << obj;
  editor::AssetWorkspace assets;
  Require(assets.ImportTree(content, {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
          "Saved scene import fixture failed");
  const auto original_mesh = assets.Entries().front();
  const auto identity = Read(content / "Triangle.obj.meta");
  Require(original_mesh.mesh != nullptr, "Saved scene import mesh fixture failed");
  // A full tree import cannot pass this unrelated broken source/identity; single-scene work must.
  std::filesystem::remove(content / "Triangle.obj");
  std::ofstream(content / "Triangle.obj.meta") << "poisoned unrelated identity";
  std::ofstream(content / "Unrelated.obj") << "poisoned unrelated mesh";
  std::ofstream(content / "Unrelated.obj.meta") << "poisoned unrelated identity";
  runtime::World world;
  const auto id = world.LoadScene("Saved source");
  editor::SceneDocument document(world, id);
  Require(document.Create("First") && document.Save(content / "Saved.scene") &&
              assets.ImportSavedScene("Saved.scene") && assets.Entries().size() == 2 &&
              assets.Find(original_mesh.id)->mesh == original_mesh.mesh &&
              assets.Find(original_mesh.id)->artifact_hash == original_mesh.artifact_hash &&
              Read(content / "Triangle.obj.meta") == "poisoned unrelated identity",
          "Saving a scene re-read or replaced unrelated content/geometry");
  const auto saved = assets.Search("Saved").front()->id;
  const auto first_hash = assets.Find(saved)->artifact_hash;
  const auto saved_identity = Read(content / "Saved.scene.meta");
  Require(document.Create("Second") && document.Save(content / "Saved.scene") &&
              assets.ImportSavedScene("Saved.scene") &&
              assets.Find(saved)->artifact_hash != first_hash &&
              Read(content / "Saved.scene.meta") == saved_identity &&
              assets.Find(original_mesh.id)->mesh == original_mesh.mesh,
          "Single-scene update lost stable identity or replaced unrelated mesh ownership");
  const auto updated_hash = assets.Find(saved)->artifact_hash;
  std::ofstream(content / "Saved.scene.meta")
      << "schema=1\nuuid=" << original_mesh.id.ToString() << "\ntype=.scene\n";
  Require(!assets.ImportSavedScene("Saved.scene") &&
              assets.Find(saved)->artifact_hash == updated_hash && assets.Entries().size() == 2,
          "Duplicate identity changed the saved scene index");
  std::ofstream(content / "Saved.scene.meta") << saved_identity;
  std::ofstream(content / "TooLarge.scene") << "sparse";
  std::filesystem::resize_file(content / "TooLarge.scene", 64 * 1024 * 1024 + 1);
  Require(!assets.ImportSavedScene("TooLarge.scene") &&
              !std::filesystem::exists(content / "TooLarge.scene.meta") &&
              assets.Entries().size() == 2 && !assets.ImportSavedScene("../Escape.scene") &&
              !assets.ImportSavedScene("Triangle.obj"),
          "Unbounded or invalid scene import published an asset");
  std::filesystem::remove(content / "TooLarge.scene");
  std::ofstream(content / "Triangle.obj") << obj;
  std::ofstream(content / "Triangle.obj.meta") << identity;
  std::filesystem::remove(content / "Unrelated.obj");
  std::filesystem::remove(content / "Unrelated.obj.meta");
  const auto unicode = std::filesystem::path(u8"場景.scene");
  Require(document.Save(content / unicode) && assets.ImportSavedScene(unicode),
          "Unicode saved-scene import failed");
  const auto unicode_id = assets.Entries().back().id;
  editor::AssetWorkspace reader;
  Require(reader.ImportTree(content, {}, {}, editor::AssetIdentityMode::PersistentReadOnly) &&
              reader.Find(saved) && reader.Find(saved)->artifact_hash == updated_hash &&
              !reader.ImportSavedScene("Saved.scene") &&
              Read(content / "Saved.scene.meta") == saved_identity,
          "Single-scene hash disagrees with tree import or read-only index wrote a file");
  editor::ProjectContentSession browser;
  Require(reader.Find(unicode_id) && browser.Open(workspace, reader, 1, false) &&
              browser.Browser().Find(unicode_id) &&
              browser.Browser().Find(unicode_id)->path ==
                  std::filesystem::path("Content") / unicode,
          "UTF-8 scene identity lost its native browser path on reopen");
}
void Run(const std::filesystem::path &root) {
  RunDiscovery();
  RunSavedSceneImport(root / "SingleImport/Content");
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
  const auto invalid = std::array<std::filesystem::path, 12>{
      root / "Absolute.scene",   "../Outside.scene",        "Content/../Escape.scene",
      "Content/./Dot.scene",     "Content/Bad.txt",         ".nexora/project.scene",
      "Content/Bad:Name.scene",  "Content/Bad\nName.scene", ".NEXORA/Internal.scene",
      ".nexora./Internal.scene", ".nexora /Internal.scene", std::string(1024, 'x') + ".scene"};
  for (const auto &path : invalid)
    Require(files.SaveAs(token, path, true).status == Status::Rejected &&
                files.Open(token, path, true).status == Status::Rejected &&
                files.CurrentPath() == before_path && world.SaveScene(id) == before_world &&
                Read(root / *before_path) == before_bytes,
            "Invalid path modified a managed document or file");
#if !defined(_WIN32)
  // Backslash is a nonportable filename character on POSIX; Windows treats it as a separator.
  Require(files.SaveAs(token, "Content/Bad\\Name.scene").status == Status::Rejected &&
              files.Open(token, "Content/Bad\\Name.scene", true).status == Status::Rejected &&
              files.CurrentPath() == before_path && world.SaveScene(id) == before_world,
          "POSIX backslash filename was admitted");
#endif
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
    std::filesystem::create_directory_symlink(root / ".nexora", root / "Alias", error);
    Require(!error && files.SaveAs(token, "Alias/Internal.scene").status == Status::Rejected &&
                files.Open(token, "Alias/Internal.scene", true).status == Status::Rejected &&
                !std::filesystem::exists(root / ".nexora/Internal.scene"),
            "Canonical alias bypassed the reserved metadata namespace");
    Require(files.SaveAs(token, "Alias/scenes/Aliased.scene").Applied() &&
                files.CurrentPath() == ".nexora/scenes/Aliased.scene" &&
                files.Open(token, "Alias/scenes/Aliased.scene").Applied(),
            "Allowed metadata alias did not adopt the canonical current path");
    token = files.Token();
    std::ofstream(root / "Content/Keep.obj") << "mesh source sentinel";
    std::filesystem::create_symlink(root / "Content/Keep.obj", root / "Content/Wrong.scene", error);
    Require(!error && files.SaveAs(token, "Content/Wrong.scene", true).status == Status::Rejected &&
                files.Open(token, "Content/Wrong.scene", true).status == Status::Rejected &&
                Read(root / "Content/Keep.obj") == "mesh source sentinel",
            "Scene alias overwrote a different source file type");
    Require(files.SaveAs(token, "Content/Escape/Keep.scene", true).status == Status::Rejected &&
                files.Open(token, "Content/Escape/Keep.scene", true).status == Status::Rejected &&
                Read(outside / "Keep.scene") == "outside sentinel",
            "Symlink escaped the project scope");
  }
  std::filesystem::remove_all(outside);
  Require(files.SaveAs(token, std::filesystem::path(u8"Content/場景.scene")).Applied(),
          "UTF-8 scene path failed");
  const auto native_nested = std::filesystem::path("Content") / "Native" / "Nested.scene";
  Require(files.SaveAs(token, native_nested).Applied() && files.CurrentPath() == native_nested,
          "Native nested path separators failed");
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
