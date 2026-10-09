#include "Nexora/Editor/EditorWorkspace.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

struct Scratch final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-prepared-save-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Scratch() { Require(std::filesystem::create_directory(root), "scratch directory was not new"); }
  ~Scratch() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
};

void Run() {
  Scratch scratch;
  const auto path = scratch.root / "Authoring.scene";
  const auto deferred = scratch.root / "Deferred.scene";
  runtime::World world;
  const auto scene_id = world.LoadScene("Prepared save");
  editor::SceneDocument scene(world, scene_id);
  const auto parent = scene.Create("Parent");
  const auto child = scene.CreateCamera("Child", parent);
  const auto key = *scene.Key(parent);
  const std::array keys{key};
  const editor::OpaqueComponent opaque{91, "Unavailable plugin", {0, 127, 255, 0}};
  Require(scene.SetTransform(parent, {1, 2, 3}) && scene.SetEulerField(keys, 1, 720) &&
              scene.Select(std::array{parent, child}) && scene.SetOpaqueComponent(key, opaque),
          "authoring fixture failed");
  const auto world_before = world.SaveScene(scene_id);
  const auto selected = std::vector(scene.Selection().begin(), scene.Selection().end());
  const auto prepared = scene.PrepareSave();
  Require(prepared && prepared->Generation() == scene.Generation() && scene.Dirty() &&
              world.SaveScene(scene_id) == world_before &&
              std::ranges::equal(scene.Selection(), selected) &&
              std::filesystem::is_empty(scratch.root) &&
              prepared->Bytes().starts_with("NEXORA_EDITOR_SCENE 3\n"),
          "preparation changed state/baseline or touched the filesystem");
  const auto retained_bytes = prepared->Bytes();
  auto copied = *prepared;
  Require(scene.SavePrepared(path, copied) && Read(path) == retained_bytes && !scene.Dirty(),
          "matching prepared scene did not publish and advance the baseline");

  // Ordinary edits keep the document generation unchanged, so generation alone is insufficient.
  const auto generation = scene.Generation();
  Require(scene.SetTransform(parent, {8, 9, 10}) && scene.Generation() == generation &&
              scene.Dirty() && !scene.SavePrepared(deferred, *prepared) &&
              !std::filesystem::exists(deferred) && Read(path) == retained_bytes &&
              prepared->Bytes() == retained_bytes &&
              std::ranges::equal(scene.Selection(), selected),
          "stale prepared content reached IO or borrowed live authoring data");
  Require(scene.Undo() && !scene.Dirty() && scene.SavePrepared(deferred, *prepared) &&
              Read(deferred) == retained_bytes && scene.Redo() && scene.Dirty(),
          "preparation/publication consumed authoring history or rejected restored content");
  Require(scene.Undo() && !scene.Dirty(), "Redo preservation fixture failed");
  auto stage = path;
  stage += ".tmp";
  std::ofstream(stage) << "occupied staging";
  Require(!scene.SavePrepared(path, *prepared) && Read(stage) == "occupied staging" &&
              Read(path) == retained_bytes && !scene.Dirty() && scene.Redo() && scene.Dirty(),
          "failed publication changed files, clean baseline or Redo");
  std::filesystem::remove(stage);
  Require(scene.Save(path) && !scene.Dirty(), "ordinary Save compatibility failed");
  const auto last_good = Read(path);

  const auto before_opaque = scene.PrepareSave();
  auto changed_opaque = opaque;
  changed_opaque.data.push_back(42);
  Require(before_opaque && scene.SetOpaqueComponent(key, changed_opaque) && scene.Dirty() &&
              !scene.SavePrepared(path, *before_opaque) && Read(path) == last_good &&
              scene.Undo() && !scene.Dirty() && scene.Redo() && scene.Dirty(),
          "opaque-only edits were omitted from stale-save validation");
  const auto dirty_prepared = scene.PrepareSave();
  std::ofstream(stage) << "blocked dirty save";
  Require(dirty_prepared && !scene.SavePrepared(path, *dirty_prepared) && scene.Dirty() &&
              Read(path) == last_good,
          "failed publication marked unsaved opaque data clean");
  std::filesystem::remove(stage);

  // Full turns retain an identical runtime rotation but different authoring metadata.
  Require(scene.Undo() && scene.SetEulerField(keys, 1, 1080) && scene.Dirty() &&
              !scene.SavePrepared(path, *before_opaque) && Read(path) == last_good,
          "authored Euler turns were omitted from stale-save validation");
  const auto with_turns = scene.PrepareSave();
  Require(with_turns && scene.SavePrepared(path, *with_turns) && !scene.Dirty(),
          "current Euler/opaque snapshot did not publish");

  runtime::World reopened_world;
  const auto reopened_id = reopened_world.LoadScene("Reopen");
  editor::SceneDocument reopened(reopened_world, reopened_id);
  Require(reopened.Reload(path) &&
              reopened.OpaqueComponents(*reopened.Key(parent)) == scene.OpaqueComponents(key) &&
              reopened.EulerAngles(parent) == scene.EulerAngles(parent) &&
              reopened.Camera(*reopened.Key(child)) && scene.Camera(*scene.Key(child)) &&
              reopened.Camera(*reopened.Key(child))->vertical_field_of_view ==
                  scene.Camera(*scene.Key(child))->vertical_field_of_view &&
              reopened.Camera(*reopened.Key(child))->near_plane ==
                  scene.Camera(*scene.Key(child))->near_plane &&
              reopened.Camera(*reopened.Key(child))->far_plane ==
                  scene.Camera(*scene.Key(child))->far_plane &&
              !reopened.SavePrepared(deferred, *with_turns) && Read(deferred) == retained_bytes,
          "round trip lost metadata or a different document accepted the snapshot");
  Require(scene.Reload(path) && scene.Generation() != with_turns->Generation() &&
              !scene.SavePrepared(path, *with_turns) && Read(path) == with_turns->Bytes(),
          "Reload revived a snapshot from an older document generation");
  const auto before_new = scene.PrepareSave();
  Require(before_new && scene.NewScene() && !scene.SavePrepared(path, *before_new) &&
              Read(path) == before_new->Bytes() && copied.Bytes() == retained_bytes,
          "NewScene revived stale identity or destroyed owning prepared storage");
  Require(world.RequestUnload(scene_id), "Runtime unload fixture failed");
  world.EndFrame();
  Require(!scene.PrepareSave() && !scene.SavePrepared(path, *before_new) &&
              Read(path) == before_new->Bytes(),
          "missing Runtime scene admitted serialization/publication");
}
} // namespace

int main() {
  try {
    Run();
    std::cout << "Prepared scene-save contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
