#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("nexora-property-snapshot-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(directory);
  editor::test::TemporaryDirectoryCleanup cleanup{directory};
  runtime::World world, source_world;
  editor::SceneDocument document(world, world.LoadScene("Atomic properties"));
  const auto root = document.Create("Root"), child = document.CreateCamera("Child", root);
  const auto root_key = *document.Key(root), child_key = *document.Key(child);
  const std::array selected{root};
  Require(document.SetOpaqueComponent(child_key, {91, "Missing", {0, 255, 27}}) &&
              document.Select(selected) && document.CopySelection() &&
              document.Save(directory / "Source.scene") && !document.Dirty(),
          "Baseline fixture failed");
  const auto before = *document.PrepareSave();
  const auto generation = document.Generation();
  const auto source_scene = source_world.LoadScene("Source");
  editor::SceneDocument source(source_world, source_scene);
  Require(source.ReloadBytes(before.Bytes()) && source.Rename(*source.Key(root), "Changed root") &&
              source.SetTransform(root, {9, 8, 7}) &&
              source.SetEulerField(std::array{*source.Key(root)}, 1, 720) &&
              source.Move(child, 0, 0) &&
              source.SetLight(*source.Key(child), runtime::LightComponent{3.5F}) &&
              source.SetMeshRenderer(*source.Key(root), runtime::MeshComponent{42, {44}}) &&
              source.SetCamera(*source.Key(root), runtime::CameraComponent{75, .2, 250}) &&
              source.SetOpaqueComponent(*source.Key(child), {91, "Missing", {0, 255, 28}}),
          "Complete property source fixture failed");
  // Preserve a disabled component's nondefault stored values using the official snapshot parser.
  std::istringstream input(*source_world.SaveScene(source_scene));
  std::string line, snapshot;
  std::getline(input, line);
  snapshot = line + '\n';
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::vector<std::string> fields;
    for (std::string field; row >> field;)
      fields.push_back(std::move(field));
    Require(fields.size() == 21, "Runtime fixture row changed");
    if (fields[0] == std::to_string(root))
      fields[12] = "0";
    for (std::size_t i = 0; i < fields.size(); ++i)
      snapshot += (i ? " " : "") + fields[i];
    snapshot += '\n';
  }
  Require(source_world.ReplaceSceneSnapshot(source_scene, snapshot), "Disabled fixture rejected");
  const auto changed = *source.PrepareSave();
  Require(document.Rename(root_key, "Pending redo") && document.Undo(), "Redo fixture failed");
  Require(document.ApplyPropertySnapshot(before, before.Bytes(), true) && document.Redo() &&
              document.Undo(),
          "No-op snapshot erased Redo");
  Require(
      !document.ApplyPropertySnapshot(before, changed.Bytes(), false) &&
          !document.ApplyPropertySnapshot(before, "corrupt", true) &&
          !document.ApplyPropertySnapshot(before, std::string(8 * 1024 * 1024 + 1, 'x'), true) &&
          !document.ApplyPropertySnapshot(changed, changed.Bytes(), true) &&
          document.PrepareSave()->Bytes() == before.Bytes() && document.Redo() && document.Undo(),
      "Rejected input mutated bytes/history");
  const auto additional = source.Create("Forbidden identity");
  Require(!document.ApplyPropertySnapshot(before, source.PrepareSave()->Bytes(), true) &&
              source.Undo() && !source.Key(additional),
          "Identity-changing snapshot applied");
  Require(document.ApplyPropertySnapshot(before, changed.Bytes(), true) &&
              document.PrepareSave()->Bytes() == changed.Bytes() && document.Dirty() &&
              document.Generation() == generation && document.Key(root) == root_key &&
              document.Key(child) == child_key && document.Selection().size() == 1 &&
              document.Selection().front() == root && document.Parent(child) == 0 &&
              document.EulerAngles(root) == source.EulerAngles(root) &&
              !document.Camera(root_key) &&
              world.FindEntity(root)->camera_data.vertical_field_of_view == 75 &&
              document.OpaqueComponents(child_key) == source.OpaqueComponents(*source.Key(child)),
          "Atomic snapshot lost values, keys, selection or baseline");
  Require(document.SetTransform(root, runtime::WithPosition(*document.Transform(root), 99, 8, 7)) &&
              document.Rename(root_key, "Later ordinary edit"),
          "Interleaved edits failed");
  const std::string later = document.PrepareSave()->Bytes();
  Require(document.Undo() && document.Undo() &&
              document.PrepareSave()->Bytes() == changed.Bytes() && document.Undo() &&
              document.PrepareSave()->Bytes() == before.Bytes() && !document.Dirty() &&
              document.Redo() && document.PrepareSave()->Bytes() == changed.Bytes() &&
              document.Redo() && document.Redo() && document.PrepareSave()->Bytes() == later &&
              document.Undo() && document.Undo(),
          "Snapshot/Runtime/rename histories diverged");
  const auto restore = *document.Transform(root);
  runtime::WorldCommandBuffer foreign;
  foreign.SetTransform(root, runtime::WithPosition(restore, -7, 8, 7));
  Require(foreign.Apply(world), "Foreign mutation fixture failed");
  const std::string foreign_bytes = document.PrepareSave()->Bytes();
  Require(!document.Undo() && document.PrepareSave()->Bytes() == foreign_bytes,
          "Stale snapshot replay overwrote foreign content");
  foreign.SetTransform(root, restore);
  Require(foreign.Apply(world) && document.Undo() &&
              document.PrepareSave()->Bytes() == before.Bytes() && document.Redo() &&
              document.PrepareSave()->Bytes() == changed.Bytes() && document.Paste() &&
              document.Name(document.Selection().front()) == "Root Copy" && document.Undo() &&
              document.PrepareSave()->Bytes() == changed.Bytes(),
          "Rejected replay consumed cursor or snapshot replaced clipboard");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Atomic document property snapshots passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
