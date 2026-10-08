#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SceneFiles.h"

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
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(static_cast<bool>(output), "External fixture write failed");
}
std::string RenameBytes(std::string bytes, std::string_view name) {
  for (auto at = bytes.find("Camera"); at != std::string::npos; at = bytes.find("Camera", at + 6))
    bytes.replace(at, 6, name);
  return bytes;
}
void RunRelocation(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Relocated saves"), "Relocation workspace failed");
  runtime::World world;
  const auto id = world.LoadScene("Relocated");
  editor::SceneDocument scene(world, id);
  const auto camera = scene.CreateCamera("Camera");
  editor::SceneFileSession files(workspace, scene);
  Require(camera && files.SaveAs(files.Token(), "Content/Original.scene").Applied(),
          "Relocation scene failed");
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  Require(
      assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite) &&
          content.Open(workspace, assets, 7),
      "Relocation Content failed");
  const auto asset = content.Browser().Items().front().id;
  const auto token = files.Token();
  Require(files.SynchronizeContent(token, content).Applied() &&
              scene.Rename(*scene.Key(camera), "Local moved"),
          "Relocation association failed");
  const auto local = world.SaveScene(id);
  const auto external = RenameBytes(Read(root / "Content/Original.scene"), "DiskAA");
  Write(root / "Content/Original.scene", external);
  const auto old_approval = files.Save(token).overwrite_token;
  Require(old_approval.has_value() && content.Rename(asset, "Renamed.scene") &&
              files.SynchronizeContent(token, content).Applied() &&
              files.CurrentPath() == "Content/Renamed.scene" && files.Token() == token &&
              files.Save(token, old_approval).status == Status::Rejected &&
              !std::filesystem::exists(root / "Content/Original.scene") &&
              Read(root / "Content/Renamed.scene") == external && world.SaveScene(id) == local,
          "Content rename reused old approval, reset baseline, or changed the document");
  const auto moved_conflict = files.Save(token);
  Require(moved_conflict.status == Status::NeedsOverwrite && moved_conflict.overwrite_token &&
              files.Save(token, moved_conflict.overwrite_token).Applied() && scene.Undo() &&
              scene.Redo() && content.Undo() &&
              files.SynchronizeContent(token, content).Applied() &&
              files.CurrentPath() == "Content/Original.scene" && files.Save(token).Applied(),
          "Content rename/Undo lost exact baseline, identity, or document history");
}
void Run(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "External saves"), "Workspace failed");
  runtime::World world;
  const auto id = world.LoadScene("External save");
  editor::SceneDocument scene(world, id);
  const auto camera = scene.CreateCamera("Camera");
  Require(camera && scene.Select(std::array{*scene.Key(camera)}) &&
              scene.SetEulerField(std::array{*scene.Key(camera)}, 1, 720),
          "Document fixture failed");
  editor::SceneFileSession files(workspace, scene);
  const std::filesystem::path relative(u8"Content/場景.scene");
  const auto path = root / relative;
  Require(files.SaveAs(files.Token(), relative).Applied(), "Initial Save failed");
  const auto original = Read(path);
  const auto mtime = std::filesystem::last_write_time(path);
  const auto disk_a = RenameBytes(original, "DiskAA");
  const auto disk_b = RenameBytes(original, "DiskBB");
  Require(disk_a.size() == original.size() && disk_a != original, "Same-size fixture failed");
  const auto token = files.Token();
  Require(scene.Rename(*scene.Key(camera), "Local A") &&
              scene.Rename(*scene.Key(camera), "Local B") && scene.Undo(),
          "Local/Redo fixture failed");
  const auto local = world.SaveScene(id);
  Write(path, disk_a);
  std::filesystem::last_write_time(path, mtime);
  auto conflict = files.Save(token);
  Require(conflict.status == Status::NeedsOverwrite && conflict.overwrite_token &&
              conflict.message.find("outside") != std::string::npos && Read(path) == disk_a &&
              std::filesystem::last_write_time(path) == mtime && files.Token() == token &&
              files.CurrentPath() == relative && world.SaveScene(id) == local && scene.Dirty() &&
              scene.Selection().size() == 1 && (*scene.EulerAngles(camera))[1] == 720 &&
              !std::filesystem::exists(path.string() + ".tmp") && scene.Redo() && scene.Undo() &&
              world.SaveScene(id) == local,
          "Same-size/restored-mtime conflict changed disk/document/history or was missed");
  const auto first_approval = *conflict.overwrite_token;
  Write(path, disk_b);
  auto refreshed = files.Save(token, first_approval);
  Require(refreshed.status == Status::NeedsOverwrite && refreshed.overwrite_token &&
              refreshed.overwrite_token != conflict.overwrite_token && Read(path) == disk_b &&
              files.Save(token, first_approval).status == Status::Rejected && scene.Dirty(),
          "A second external edit used the previous approval");
  const auto approval = *refreshed.overwrite_token;
  Write(path.string() + ".tmp", "occupied temporary file");
  Require(files.Save(token, approval).status == Status::Rejected && Read(path) == disk_b &&
              Read(path.string() + ".tmp") == "occupied temporary file" && scene.Dirty() &&
              world.SaveScene(id) == local,
          "Failed atomic save consumed the scene or overwrote a destination");
  std::filesystem::remove(path.string() + ".tmp");
  Require(files.Save(token, approval).Applied() && !scene.Dirty() &&
              Read(path).find("Local A") != std::string::npos && scene.Redo() && scene.Undo() &&
              files.Save(token).Applied() && files.Save(token, approval).status == Status::Rejected,
          "Confirmed Save lost history, failed to refresh baseline, or replayed approval");

  // A reload adopts the actual disk revision, and failed Open/Bind keep the old baseline/document.
  Write(path, disk_a);
  Require(files.Open(token, relative, true).Applied() && scene.Name(camera) == "DiskAA" &&
              files.Save(files.Token()).Applied(),
          "Open did not establish the new disk baseline");
  auto current_token = files.Token();
  const auto loaded = world.SaveScene(id);
  const auto before_bad = Read(path);
  const auto oversized = root / "Content/Oversized.scene";
  Write(oversized, "invalid");
  std::filesystem::resize_file(oversized, editor::SceneFileSession::kMaximumDiskBaselineBytes + 1);
  const auto directory = root / "Content/Directory.scene";
  std::filesystem::create_directory(directory);
  Require(
      !files.BindCurrent("Content/Oversized.scene") &&
          !files.BindCurrent("Content/Directory.scene") &&
          files.Open(current_token, "Content/Oversized.scene", true).status == Status::Rejected &&
          files.Open(current_token, "Content/Directory.scene", true).status == Status::Rejected &&
          world.SaveScene(id) == loaded && files.Token() == current_token &&
          files.CurrentPath() == relative && files.Save(current_token).Applied() &&
          Read(path) == before_bad,
      "Unobtainable baseline replaced the live good document or association");
  // Boundary admission is inclusive: 64 MiB can be inspected; one byte more fails before
  // allocation.
  const auto boundary = root / "Content/Boundary.scene";
  Write(boundary, "boundary");
  std::filesystem::resize_file(boundary, editor::SceneFileSession::kMaximumDiskBaselineBytes);
  {
    editor::SceneFileSession bounded(workspace, scene);
    Require(bounded.BindCurrent("Content/Boundary.scene"), "Exact 64 MiB baseline was rejected");
    std::filesystem::resize_file(boundary, editor::SceneFileSession::kMaximumDiskBaselineBytes + 1);
    Require(bounded.Save(bounded.Token()).status == Status::Rejected,
            "Oversized external edit was overwritten");
  }
  std::filesystem::remove(oversized);
  std::filesystem::remove(boundary);

  // A missing-at-bind destination is deliberate; deleting an existing loaded source is not.
  const auto before_delete = Read(path);
  std::filesystem::remove(path);
  Require(files.Save(current_token).status == Status::Rejected && !std::filesystem::exists(path),
          "Ordinary Save recreated a deleted existing source");
  Write(path, before_delete);
  editor::SceneFileSession initial(workspace, scene);
  Require(initial.BindCurrent("Content/InitiallyMissing.scene") &&
              initial.Save(initial.Token()).Applied(),
          "Missing-at-bind first Save was rejected");
  editor::SceneFileSession appeared(workspace, scene);
  Require(appeared.BindCurrent("Content/Appeared.scene"), "Absent association failed");
  Write(root / "Content/Appeared.scene", disk_b);
  Require(appeared.Save(appeared.Token()).status == Status::NeedsOverwrite &&
              Read(root / "Content/Appeared.scene") == disk_b,
          "A file created after Bind was overwritten silently");

  // Confirmations bind session, path, project and document generation, and recovery is rechecked.
  Write(path, disk_b);
  conflict = files.Save(current_token);
  Require(conflict.overwrite_token.has_value(), "Lifecycle conflict failed");
  editor::SceneFileSession other(workspace, scene);
  Require(other.BindCurrent(relative) &&
              other.Save(current_token, conflict.overwrite_token).status == Status::Rejected &&
              files.SaveAs(current_token, "Content/Appeared.scene", true, conflict.overwrite_token)
                      .status == Status::Rejected,
          "Approval crossed a session or destination");
  Write(root / ".nexora/workspace.recovery", "pending recovery");
  Require(files.Save(current_token, conflict.overwrite_token).status == Status::Rejected &&
              files.SaveAs(current_token, "Content/Recovery.scene", true).status ==
                  Status::Rejected &&
              !std::filesystem::exists(root / "Content/Recovery.scene") && Read(path) == disk_b,
          "Replacement bypassed recovery policy");
  std::filesystem::remove(root / ".nexora/workspace.recovery");
  Require(files.BindCurrent(relative) &&
              files.Save(current_token, conflict.overwrite_token).status == Status::Rejected,
          "Bind revived a previous replacement approval");
  Write(path, disk_a);
  conflict = files.Save(current_token);
  Require(conflict.overwrite_token && files.New(current_token, true).Applied() &&
              files.SaveAs(current_token, relative, true, conflict.overwrite_token).status ==
                  Status::Rejected,
          "Approval crossed New's document generation");
  // Explicit noninteractive replacement is retained for callers that own the destructive policy.
  Require(files.SaveAs(files.Token(), relative, true).Applied(), "Explicit Save As policy changed");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-external-scene-save-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root);
    RunRelocation(root / "Relocation");
    std::filesystem::remove_all(root);
    std::cout << "Exact disk baseline and confirmed scene replacement contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
