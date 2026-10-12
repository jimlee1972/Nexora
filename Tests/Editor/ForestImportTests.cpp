#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  runtime::World source_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  const auto parent = source.Create("Prefab root"), second = source.Create("Second root");
  const auto child = source.Create("Child", parent);
  const auto camera = source.CreateCamera("Camera", child);
  const auto light = source.CreateLight("Light", parent);
  const auto mesh = source.Create("Mesh", parent);
  const auto parent_key = *source.Key(parent), child_key = *source.Key(child);
  const std::array parent_keys{parent_key};
  Require(
      source.SetTransform(parent, {2, 3, 4}) && source.SetEulerField(parent_keys, 1, 720) &&
          source.SetCamera(*source.Key(camera), runtime::CameraComponent{47, 0.3, 900}) &&
          source.SetLight(*source.Key(light), runtime::LightComponent{2.5F}) &&
          source.SetOpaqueComponent(child_key, {77, "Unavailable.Provider", {0, 255, 27, 127}}) &&
          source.SetMeshRenderer(
              *source.Key(mesh),
              runtime::MeshComponent{std::numeric_limits<runtime::Id>::max() - 1,
                                     {std::numeric_limits<runtime::Id>::max() - 2}}) &&
          source.Move(second, 0, 0),
      "Complete ordered source fixture failed");
  const std::string bytes = source.PrepareSave()->Bytes();
  const auto source_generation = source.Generation();

  runtime::World target_world;
  editor::SceneDocument target(target_world, target_world.LoadScene("Target"));
  const auto seed = target.Create("Clipboard seed");
  const auto seed_key = *target.Key(seed);
  const std::array seed_ids{seed};
  Require(target.Select(seed_ids) && target.CopySelection() && target.Rename(seed_key, "Changed") &&
              target.Undo(),
          "Target clipboard/Redo fixture failed");
  const auto expected = *target.PrepareSave();
  const auto original_selection = std::vector(target.Selection().begin(), target.Selection().end());
  Require(!target.ImportForestBytes(expected, bytes, false) &&
              !target.ImportForestBytes(expected, "corrupt", true) &&
              !target.ImportForestBytes(
                  expected, std::string_view(bytes).substr(0, bytes.size() / 2), true) &&
              !target.ImportForestBytes(
                  expected,
                  std::string(editor::SceneDocument::kMaximumImportedForestBytes + 1, 'x'), true) &&
              target.PrepareSave()->Bytes() == expected.Bytes() &&
              std::ranges::equal(target.Selection(), original_selection) && target.Redo() &&
              target.Name(seed) == "Changed" && target.Undo(),
          "Rejected import changed source/selection or erased Redo");
  editor::SceneDocument other(target_world, target_world.LoadScene("Other document"));
  const auto other_expected = *other.PrepareSave();
  Require(!target.ImportForestBytes(other_expected, bytes, true),
          "Foreign document generation authorized import");
  Require(target.Rename(seed_key, "Stale") && !target.ImportForestBytes(expected, bytes, true) &&
              target.Undo(),
          "Stale content authorized import");

  const auto imported = target.ImportForestBytes(expected, bytes, true);
  Require(imported && imported->size() == 6 && imported->front().source == second &&
              target.Nodes().size() == 7,
          "Atomic forest import lost source sibling order or nodes");
  std::map<runtime::Id, editor::SceneDocument::NodeKey> mapped;
  for (const auto &entry : *imported) {
    Require(target.Key(entry.target.id) == entry.target &&
                mapped.emplace(entry.source, entry.target).second && entry.target.id != seed,
            "Imported map contains missing/duplicate/foreign target keys");
    Require(target.Name(entry.target.id) == source.Name(entry.source),
            "Source name changed during import");
  }
  const auto actual_camera = target.Camera(mapped.at(camera));
  const auto actual_light = target.Light(mapped.at(light));
  const auto actual_mesh = target.MeshRenderer(mapped.at(mesh));
  Require(actual_camera && actual_camera->vertical_field_of_view == 47 &&
              actual_camera->near_plane == 0.3 && actual_camera->far_plane == 900 && actual_light &&
              actual_light->intensity == 2.5F && actual_mesh &&
              actual_mesh->mesh == std::numeric_limits<runtime::Id>::max() - 1 &&
              actual_mesh->material.shader == std::numeric_limits<runtime::Id>::max() - 2 &&
              target.Parent(mapped.at(parent).id) == 0 &&
              target.Parent(mapped.at(child).id) == mapped.at(parent).id &&
              target.Parent(mapped.at(camera).id) == mapped.at(child).id &&
              target.Parent(mapped.at(light).id) == mapped.at(parent).id &&
              target.Parent(mapped.at(mesh).id) == mapped.at(parent).id &&
              target.Transform(mapped.at(parent).id) == source.Transform(parent) &&
              std::abs((*target.EulerAngles(mapped.at(parent).id))[1] - 720) < 1e-9 &&
              target.OpaqueComponents(mapped.at(child)) == source.OpaqueComponents(child_key),
          "Forest remap lost parent, components, full-width resource IDs, Euler or opaque data");
  const auto imported_bytes = target.PrepareSave()->Bytes();
  Require(target.Paste() && target.Name(target.Selection().front()) == "Clipboard seed Copy" &&
              target.Undo() && target.PrepareSave()->Bytes() == imported_bytes && target.Undo() &&
              target.PrepareSave()->Bytes() == expected.Bytes() &&
              std::ranges::equal(target.Selection(), original_selection) &&
              !target.Key(mapped.at(child).id) && target.Redo() &&
              target.PrepareSave()->Bytes() == imported_bytes &&
              target.Key(mapped.at(child).id) == mapped.at(child),
          "Import changed clipboard or was not one independent Undo/Redo transaction");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
  Require(reopened.ReloadBytes(imported_bytes) &&
              reopened.Name(mapped.at(parent).id) == "Prefab root" &&
              reopened.OpaqueComponents(*reopened.Key(mapped.at(child).id)) ==
                  source.OpaqueComponents(child_key) &&
              source.PrepareSave()->Bytes() == bytes && source.Generation() == source_generation,
          "Actual scene reopen lost imported data or changed source document");

  editor::SceneDocument cut_target(target_world, target_world.LoadScene("Pending cut"));
  const auto cut_seed = cut_target.Create("Cut root");
  const std::array cut_ids{cut_seed};
  Require(cut_target.Select(cut_ids) && cut_target.CutSelection(), "Pending cut fixture failed");
  const auto cut_expected = *cut_target.PrepareSave();
  Require(cut_target.ImportForestBytes(cut_expected, bytes, true).has_value() &&
              cut_target.Paste() && cut_target.Name(cut_target.Selection().front()) == "Cut root" &&
              cut_target.Paste() &&
              cut_target.Name(cut_target.Selection().front()) == "Cut root Copy",
          "Import consumed pending-cut clipboard state");

  runtime::World large_world;
  const auto large_scene = large_world.LoadScene("Overbudget source");
  runtime::SceneEditor large_editor(large_world);
  std::vector<runtime::Entity> prototypes(editor::SceneDocument::kMaximumImportedForestNodes + 1);
  for (std::size_t i = 0; i < prototypes.size(); ++i)
    prototypes[i].id = i + 1;
  const auto large_ids = large_editor.CloneEntityForest(large_scene, prototypes);
  Require(large_ids.size() == prototypes.size(), "Large source fixture failed");
  std::ostringstream oversized;
  oversized << "NEXORA_EDITOR_SCENE 3\n";
  for (const auto id : large_ids)
    oversized << "node " << id << " 0 Generated\n";
  oversized << "world\n" << *large_world.SaveScene(large_scene);
  const auto unchanged = *target.PrepareSave();
  Require(!target.ImportForestBytes(unchanged, oversized.str(), true) &&
              target.PrepareSave()->Bytes() == unchanged.Bytes(),
          "Overbudget forest mutated target before rejection");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
