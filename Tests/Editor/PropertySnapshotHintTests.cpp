#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabPlacementRebase.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::map<std::filesystem::path, std::string> Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file()) {
      std::ifstream input(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string{std::istreambuf_iterator<char>(input), {}});
    }
  return result;
}
std::string Revision(std::string bytes, const editor::SceneDocument::PrefabPlacement &placement) {
  const auto prefix =
      "prefab-placement " + placement.instance.ToString() + ' ' + placement.source.ToString() + ' ';
  const auto old = prefix + std::to_string(placement.revision) + '\n';
  const auto position = bytes.find(old);
  Require(position != bytes.npos, "Placement revision record absent");
  bytes.replace(position, old.size(), prefix + std::to_string(placement.revision + 1) + '\n');
  return bytes;
}
void Away(editor::SceneDocument &scene, editor::SceneDocument::NodeKey key, double yaw) {
  Require(scene.SetEulerField(std::array{key}, 1, yaw), "Authored hint failed");
  auto pose = *scene.Transform(key.id);
  pose.qx = pose.qy = pose.qw = 0;
  pose.qz = 1;
  Require(scene.SetTransform(key.id, pose), "Rotation away failed");
}
void Revive(editor::SceneDocument &scene, runtime::Id id, double yaw) {
  const auto before = *scene.PrepareSave();
  auto pose = *scene.Transform(id);
  pose.qx = pose.qy = pose.qz = 0;
  pose.qw = 1;
  Require(scene.SetTransform(id, pose) && scene.EulerAngles(id)->at(1) == yaw,
          "Unchanged snapshot rotation lost latent authored Euler hint");
  Require(scene.Undo() && scene.MatchesPreparedSave(before), "Hint revival Undo changed snapshot");
}
void Run(int mode) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-snapshot-hints-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace writer;
  Require(writer.Create(root, "Snapshot hints"), "Project failed");
  runtime::World world, source_world;
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  runtime::Id id{};
  if (mode) {
    const auto source_id = source.Create("Root");
    std::uint64_t serial{};
    const auto factory = [&] { return foundation::Uuid{8940, ++serial}; };
    const auto asset = editor::PrefabAssets::Capture({8941, 1}, source, factory);
    Require(asset && editor::PrefabAssets::Publish(writer, *asset), "Root source failed");
    const auto prepared = editor::ProjectPrefabPlacement::Prepare(writer, asset->id, scene);
    const auto placed =
        prepared
            ? editor::ProjectPrefabPlacement::Instantiate(writer, scene, *prepared, {8942, 1}, true)
            : std::nullopt;
    Require(placed && placed->size() == 1, "Placement failed");
    id = placed->front().target.id;
    Require(source.Rename(*source.Key(source_id), "Changed name"), "New source failed");
    const auto advanced = editor::PrefabAssets::Capture(asset->id, source, factory, &*asset);
    Require(advanced && editor::PrefabAssets::Publish(writer, *advanced, &*asset),
            "Source advance failed");
  } else
    id = scene.Create("Root");
  const auto other = scene.Create("Unbound");
  const auto key = *scene.Key(id);
  Away(scene, key, 720);
  Away(scene, *scene.Key(other), 1440);
  Require(scene.Save(root / "Main.scene"), "Baseline failed");
  const auto before = *scene.PrepareSave();
  const auto files = Files(root);
  if (mode == 2) {
    const auto review = editor::PrefabPlacementRebase::Prepare(writer, scene, key);
    Require(review && review->Ready() && review->Unresolved() == 0 &&
                editor::PrefabPlacementRebase::Apply(writer, scene, *review, true),
            "Source-only name rebase failed");
  } else {
    runtime::World staged_world;
    editor::SceneDocument staged(staged_world, staged_world.LoadScene("Staged"));
    Require(staged.ReloadBytes(before.Bytes()) && staged.Rename(*staged.Key(id), "Changed name"),
            "Candidate failed");
    auto candidate = staged.PrepareSave()->Bytes();
    const bool applied =
        mode ? scene.ApplyPrefabPlacementSnapshot(
                   before, Revision(candidate, scene.PrefabPlacements().front()), {8942, 1}, true)
             : scene.ApplyPropertySnapshot(before, candidate, true);
    Require(applied, "Name-only snapshot failed");
  }
  const auto after = *scene.PrepareSave();
  Revive(scene, id, 720);
  Revive(scene, other, 1440);
  for (int i = 0; i < 20; ++i)
    Require(scene.Undo() && scene.MatchesPreparedSave(before) && !scene.Dirty() && scene.Redo() &&
                scene.MatchesPreparedSave(after),
            "Hint snapshot history diverged");
  Revive(scene, id, 720);
  Require(Files(root) == files && scene.Key(id) == key, "Hint snapshot changed files/identity");
  runtime::World changed_world;
  editor::SceneDocument changed(changed_world, changed_world.LoadScene("Changed"));
  Require(changed.ReloadBytes(after.Bytes()), "Changed rotation staging failed");
  auto changed_pose = *changed.Transform(id);
  changed_pose.qx = 1;
  changed_pose.qy = changed_pose.qz = changed_pose.qw = 0;
  Require(changed.SetTransform(id, changed_pose) &&
              scene.ApplyPropertySnapshot(after, changed.PrepareSave()->Bytes(), true),
          "Explicit runtime rotation change rejected");
  const auto rotated = *scene.PrepareSave();
  Revive(scene, id, 0);
  Revive(scene, other, 1440);
  Require(scene.Undo() && scene.MatchesPreparedSave(after), "Rotation snapshot Undo failed");
  const auto pending = *scene.PrepareSave();
  Require(scene.ApplyPropertySnapshot(pending, pending.Bytes(), true) && scene.Redo() &&
              scene.MatchesPreparedSave(rotated),
          "Canonical no-op lost pending snapshot Redo");
  Revive(scene, id, 0);
  Require(scene.SetEulerField(std::array{key}, 0, 540), "Explicit authored hint failed");
  const auto hinted = *scene.PrepareSave();
  auto removed = hinted.Bytes();
  const auto marker = "euler " + std::to_string(id) + ' ';
  const auto offset = removed.find(marker);
  Require(offset != removed.npos, "Explicit authored record absent");
  removed.erase(offset, removed.find('\n', offset) - offset + 1);
  Require(scene.ApplyPropertySnapshot(hinted, removed, true) &&
              scene.PrepareSave()->Bytes() == removed && scene.EulerAngles(id)->at(0) != 540 &&
              scene.Undo() && scene.MatchesPreparedSave(hinted) &&
              scene.EulerAngles(id)->at(0) == 540 && scene.Redo() &&
              scene.PrepareSave()->Bytes() == removed && scene.EulerAngles(id)->at(0) != 540 &&
              Files(root) == files,
          "Explicit authored hint removal was ignored or history/files changed");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2)
      Run(std::stoi(argv[1]));
    else
      for (int mode = 0; mode < 3; ++mode)
        Run(mode);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
