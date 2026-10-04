#include "EditorImGuiTestAccess.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path &path, const std::string &text) {
  std::ofstream(path, std::ios::binary) << text;
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-unknown-inspector-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    using namespace nexora;
    editor::ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(root, "Unknown Components", &error), "workspace failed");
    const auto path = root / "opaque.scene";
    runtime::World world;
    const auto scene_id = world.LoadScene("Unknown components");
    Require(world.Activate(scene_id), "scene activation failed");
    editor::SceneDocument document(world, scene_id);
    const auto entity = document.Create("Missing plugin");
    auto key = *document.Key(entity);
    editor::OpaqueComponent component{
        std::numeric_limits<runtime::TypeId>::max(), "Plugin.未知 \"quoted\" %s", {0, 1, 127, 255}};
    Require(document.SetOpaqueComponent(key, component) && document.Dirty(), "opaque edit failed");
    Require(document.Undo() && document.OpaqueComponents(key)->empty() && document.Redo() &&
                document.OpaqueComponents(key)->front() == component,
            "opaque edit did not have independent undo/redo");
    Require(document.SetTransform(entity, {4, 5, 6}) && document.Rename(key, "Renamed") &&
                document.Undo() && document.Undo() && document.Undo() &&
                document.OpaqueComponents(key)->empty() && document.Transform(entity)->x == 0 &&
                document.Redo() && document.Redo() && document.Redo() &&
                document.Transform(entity)->x == 4 && document.Name(entity) == "Renamed",
            "metadata undo corrupted runtime history ordering");
    editor::OpaqueComponent large{7, "Plugin.Large", std::vector<std::uint8_t>(128, 255)};
    Require(document.SetOpaqueComponent(key, large), "second payload failed");
    Require(document.Save(path) && !document.Dirty(), "save failed");
    auto edited = component;
    edited.data.push_back(42);
    Require(document.SetOpaqueComponent(key, edited) && document.Dirty() && document.Undo() &&
                !document.Dirty() && document.Redo() && document.Dirty() && document.Undo() &&
                !document.Dirty(),
            "opaque dirty baseline did not survive Undo/Redo");
    const auto good = Read(path);
    Require(good.starts_with("NEXORA_EDITOR_SCENE 3\n") &&
                good.find("00017fff") != std::string::npos,
            "versioned opaque bytes were not serialized");
    Require(document.Select(std::array{entity}) && document.CopySelection() &&
                document.DuplicateSelection(),
            "copy/duplicate failed");
    const auto duplicate = document.Selection().front();
    Require(document.OpaqueComponents(*document.Key(duplicate)) == document.OpaqueComponents(key),
            "duplicate dropped opaque data");
    Require(document.Undo() && !document.Key(duplicate) && document.Redo() &&
                document.OpaqueComponents(*document.Key(duplicate))->size() == 2 &&
                document.DeleteSelection() && document.Undo() &&
                document.OpaqueComponents(*document.Key(duplicate))->size() == 2 &&
                document.Redo() && !document.Key(duplicate) && document.Paste(),
            "creation/delete/clipboard did not retain opaque bytes");
    Require(document.Reload(path) && !document.Dirty() &&
                document.OpaqueComponents(*document.Key(entity))->size() == 2 &&
                !document.OpaqueComponents(key) && !document.SetOpaqueComponent(key, component),
            "reload did not preserve bytes or invalidate stale keys");
    key = *document.Key(entity);
    Require(document.Select(std::array{entity}), "reloaded selection failed");
    const auto generation = document.Generation();
    const auto runtime_before = world.SaveScene(scene_id);
    const auto before = document.OpaqueComponents(key);
    const auto reject = [&](std::string bad) {
      Write(root / "bad.scene", bad);
      Require(!document.Reload(root / "bad.scene") && document.Generation() == generation &&
                  world.SaveScene(scene_id) == runtime_before &&
                  document.OpaqueComponents(key) == before && document.Selection().size() == 1 &&
                  !document.Dirty(),
              "malformed reload changed the live document");
    };
    const auto world_marker = good.find("world\n");
    auto bad = good;
    bad.insert(world_marker, "opaque 999999 1 \"Orphan\" ff\n");
    reject(bad);
    bad = good;
    bad.insert(world_marker, "node 999999 0 Ghost\nopaque 999999 1 \"Ghost\" ff\n");
    reject(bad);
    bad = good;
    bad.insert(world_marker, "opaque " + std::to_string(entity) + " 7 \"Duplicate\" ff\n");
    reject(bad);
    bad = good;
    bad.insert(world_marker, "opaque " + std::to_string(entity) + " 8 \"Invalid\" zx\n");
    reject(bad);
    bad = good;
    bad.replace(0, std::string("NEXORA_EDITOR_SCENE 3").size(), "NEXORA_EDITOR_SCENE 2");
    reject(bad);
    bad = good;
    bad.replace(0, std::string("NEXORA_EDITOR_SCENE 3").size(), "NEXORA_EDITOR_SCENE 99");
    reject(bad);
    auto invalid_key = key;
    ++invalid_key.entity_generation;
    Require(!document.SetOpaqueComponent(invalid_key, component) &&
                !document.SetOpaqueComponent(key, {9, "line\nbreak", {}}) &&
                !document.SetOpaqueComponent(key, {9, "nul" + std::string(1, '\0'), {}}) &&
                !document.SetOpaqueComponent(
                    key, {9, "Too big",
                          std::vector<std::uint8_t>(
                              editor::UnknownComponentStore::kMaximumComponentBytes + 1)}) &&
                document.OpaqueComponents(key) == before,
            "invalid import mutated opaque metadata");
    const auto inspection = document.InspectOpaqueComponents(key);
    Require(inspection && inspection->size() == 2 && inspection->front().type == 7 &&
                inspection->front().preview.size() == 64 && inspection->front().byte_count == 128,
            "inspection copied an unbounded payload or lost metadata");
    editor::ProductShell shell;
    editor::imgui::EditorImGuiHost ui;
    ui.SetDisplay(1280, 720, 1);
    const auto draw = [&] {
      ui.BeginFrame();
      ui.DrawProductShell(shell, &document, &workspace);
      static_cast<void>(ui.EndFrame());
    };
    for (int i = 0; i < 3; ++i)
      draw();
    using Access = editor::imgui::EditorImGuiTestAccess;
    const auto rendered = Access::InspectorOpaqueInfo(ui);
    Require(rendered.size() == 2 && rendered.front().entity == entity &&
                rendered.back().type == component.type &&
                rendered.back().type_name == component.type_name &&
                rendered.back().preview == component.data &&
                document.OpaqueComponents(key) == before,
            "Inspector did not expose read-only missing-plugin metadata");
    Require(document.Select(std::span<const runtime::Id>{}), "deselect failed");
    draw();
    Require(Access::InspectorOpaqueInfo(ui).empty(), "Inspector retained an old selection");
    Require(document.Select(std::array{entity}) && document.DeleteSelection(), "delete failed");
    Require(inspection->front().preview.size() == 64 && rendered.back().preview == component.data,
            "inspection snapshots borrowed removed storage");
    Require(document.Undo() && document.Save(path) && Read(path) == good,
            "delete undo did not save the exact original bytes");
    editor::ProjectWorkspace read_only;
    Require(read_only.Open(root, editor::ProjectAccess::ReadOnly, &error), "read-only open failed");
    editor::imgui::EditorImGuiHost viewer;
    viewer.SetDisplay(1280, 720, 1);
    viewer.BeginFrame();
    viewer.DrawProductShell(shell, &document, &read_only);
    static_cast<void>(viewer.EndFrame());
    Require(Access::InspectorOpaqueInfo(viewer).size() == 2 && Read(path) == good,
            "read-only inspector hid or rewrote payloads");
    editor::UnknownComponentStore store;
    for (std::size_t i = 0; i < editor::UnknownComponentStore::kMaximumComponentsPerEntity; ++i)
      Require(store.Set(1, {i + 1, "Bounded", {}}), "bounded store staging failed");
    const auto previous = store.Serialize();
    Require(!store.Set(1, {1000, "Overflow", {}}) && store.Serialize() == previous &&
                !store.Deserialize("NEXORA_OPAQUE_COMPONENTS 1\n1 1 \"X\" ff\n1 1 \"X\" ff\n") &&
                store.Serialize() == previous,
            "store limits or failed deserialize changed the last good state");
    editor::UnknownComponentStore ordered_a, ordered_b;
    Require(ordered_a.Set(1, {9, "B", {}}) && ordered_a.Set(1, {3, "A", {}}) &&
                ordered_b.Set(1, {3, "A", {}}) && ordered_b.Set(1, {9, "B", {}}) &&
                ordered_a.Serialize() == ordered_b.Serialize(),
            "opaque serialization depended on import order");
    editor::UnknownComponentStore moved(std::move(ordered_a));
    Require(ordered_a.Set(1, {1, "Reused", {}}) && moved.Serialize() == ordered_b.Serialize(),
            "moved store retained stale budget counters");
    editor::UnknownComponentStore full;
    for (std::size_t i = 0; i < editor::UnknownComponentStore::kMaximumComponents; ++i)
      Require(full.Set(i / 64 + 1, {i + 1, "Count", {}}), "record budget staging failed");
    Require(!full.Set(100, {1, "Overflow", {}}), "global record budget was not enforced");
    runtime::World budget_world;
    editor::SceneDocument budget(budget_world, budget_world.LoadScene("Budget"));
    const auto budget_entity = budget.Create("Budget");
    const auto budget_key = *budget.Key(budget_entity);
    const auto chunk_size = editor::UnknownComponentStore::kMaximumComponentBytes - 256;
    for (runtime::TypeId type = 1; type <= 16; ++type)
      Require(budget.SetOpaqueComponent(budget_key,
                                        {type, "Budget", std::vector<std::uint8_t>(chunk_size)}),
              "document byte budget staging failed");
    Require(!budget.SetOpaqueComponent(budget_key,
                                       {17, "Overflow", std::vector<std::uint8_t>(chunk_size)}) &&
                budget.Select(std::array{budget_entity}) && budget.CopySelection() &&
                !budget.Paste() && budget.Nodes().size() == 1 &&
                budget.OpaqueComponents(budget_key)->size() == 16,
            "payload-budget failure partially created a copied entity");
    Require(!full.Deserialize(
                std::string(editor::UnknownComponentStore::kMaximumSerializedBytes + 1, 'x')),
            "oversized serialized payload was accepted");
    runtime::World legacy_world;
    const auto legacy_scene = legacy_world.LoadScene("Legacy");
    editor::SceneDocument legacy(legacy_world, legacy_scene);
    Require(legacy.Create("Legacy") && legacy.Save(root / "legacy.scene") &&
                Read(root / "legacy.scene").starts_with("NEXORA_EDITOR_SCENE 2\n") &&
                legacy.Reload(root / "legacy.scene"),
            "opaque-free schema compatibility changed");
    std::filesystem::remove_all(root);
    std::cout << "Unknown-component inspector and persistence contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove_all(root);
    return 1;
  }
}
