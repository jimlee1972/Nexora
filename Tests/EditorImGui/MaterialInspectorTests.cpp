#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void WriteMaterial(const std::filesystem::path &path, float red) {
  std::ofstream(path) << "NEXORA_MATERIAL 1 base_color " << red
                      << " 0.2 0.3 metallic 0.4 roughness 0.6 occlusion 1 emission 0 0 0\n";
}
std::string ReadBytes(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-material-inspector-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    using namespace nexora;
    using Access = editor::imgui::EditorImGuiTestAccess;
    editor::ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(root, "Material Inspector", &error), "project creation failed");
    const auto source = root / "Content/Red.nmaterial";
    WriteMaterial(source, .8F);
    std::ofstream(root / "Content/Triangle.obj") << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    editor::AssetWorkspace assets;
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite, &error),
            "source indexing failed");
    runtime::AssetUuid asset;
    for (const auto &entry : assets.Entries())
      if (entry.material)
        asset = entry.id;
    editor::ProjectContentSession content;
    Require(content.Open(workspace, assets, 7, true, &error), "content opening failed");
    editor::MaterialAssetCatalog materials;
    Require(materials.PublishContent(content.Browser(), &error) && materials.ResolveAsset(asset, 7),
            "real imported material missing from catalog");
    runtime::World world;
    const auto scene_id = world.LoadScene("Material Inspector");
    Require(world.Activate(scene_id), "scene activation failed");
    editor::SceneDocument scene(world, scene_id);
    const auto entity = scene.Create("Mesh object");
    auto key = *scene.Key(entity);
    const runtime::MeshComponent mesh{123, {0xfedcba9876543210ULL}};
    Require(scene.SetMeshRenderer(key, mesh) && scene.Select(std::array{entity}),
            "mesh fixture failed");
    const auto path = root / "Content/Test.scene";
    Require(scene.Save(path), "baseline save failed");
    editor::ProductShell shell;
    editor::imgui::EditorImGuiHost ui;
    float pointer_scale = 1;
    ui.SetDisplay(1280, 1600, pointer_scale);
    const auto draw = [&](const editor::MaterialAssetCatalog *catalog = nullptr,
                          bool content_available = true, bool catalog_available = true) {
      ui.BeginFrame();
      ui.DrawProductShell(shell, &scene, &workspace, content_available ? &content : nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          catalog_available ? (catalog ? catalog : &materials) : nullptr);
      static_cast<void>(ui.EndFrame());
    };
    draw();
    Require(Access::InspectorMaterialLabel(ui) == "Unassigned", "initial material label wrong");
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    Access::FocusInspector(ui);
    draw();
    draw();
    const auto click = [&](std::size_t control, bool catalog_available = true) {
      const auto point = Access::InspectorMaterialPosition(ui, control);
      Require(point.has_value(), "material control unavailable");
      Nexora::Window::WindowEvent pointer;
      pointer.type = Nexora::Window::WindowEventType::Pointer;
      pointer.value0 = static_cast<int>((*point)[0] * pointer_scale);
      pointer.value1 = static_cast<int>((*point)[1] * pointer_scale);
      Nexora::Window::WindowEvent button;
      button.type = Nexora::Window::WindowEventType::PointerButton;
      button.value0 = 0;
      button.value1 = 1;
      ui.ProcessEvents(std::array{pointer, button});
      draw(nullptr, true, catalog_available);
      button.value1 = 0;
      ui.ProcessEvents(std::array{button});
      draw(nullptr, true, catalog_available);
    };
    // The popup's owner may become unavailable between frames. Disabled styling is not an
    // ownership guard: do not dereference unavailable Content or revive a queued assignment.
    click(0);
    draw(nullptr, false);
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, key),
            "unavailable Content popup changed the scene");
    Nexora::Window::WindowEvent dismiss;
    dismiss.type = Nexora::Window::WindowEventType::Key;
    dismiss.value0 = static_cast<int>(Nexora::Window::Key::Escape);
    dismiss.value1 = 1;
    ui.ProcessEvents(std::array{dismiss});
    draw();
    dismiss.value1 = 0;
    ui.ProcessEvents(std::array{dismiss});
    draw();
    draw();
    click(0);
    draw();
    click(1);
    Require(editor::ReadMaterialAssetReference(scene, key) == asset && scene.Dirty(),
            "actual combo click did not assign imported material UUID");
    Require(scene.MeshRenderer(key)->material.shader == mesh.material.shader,
            "material asset assignment reinterpreted legacy shader ID");
    Require(scene.Undo() && !scene.Dirty() && !editor::ReadMaterialAssetReference(scene, key),
            "assignment Undo failed to restore clean baseline");
    // Exercise the same real dropdown using physical pointer coordinates at 2x DPI.
    pointer_scale = 2;
    ui.SetDisplay(1280, 1600, pointer_scale);
    draw();
    draw();
    click(0);
    draw();
    click(1);
    Require(editor::ReadMaterialAssetReference(scene, key) == asset && scene.Undo() &&
                !scene.Dirty(),
            "2x physical pointer assignment or Undo failed");
    // All rejected requests must preserve the existing Redo transaction.
    const auto reject = [&](editor::SceneDocument::NodeKey requested, runtime::AssetUuid id,
                            std::uint64_t generation,
                            const editor::MaterialAssetCatalog *catalog = nullptr) {
      Access::QueueInspectorMaterial(ui, requested, id, generation);
      draw(catalog);
      Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, key),
              "rejected material request changed scene content");
    };
    const auto require_cancelled = [&] {
      Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, key),
              "abandoned material request revived after its gate reopened");
    };
    // Native blur cancels immediately; regaining focus before the next frame cannot revive it.
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    auto lost = focus;
    lost.value0 = 0;
    ui.ProcessEvents(std::array{lost, focus});
    draw();
    require_cancelled();
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    Require(content.Open(workspace, assets, 7, false, &error), "read-only gate opening failed");
    draw();
    Require(content.Open(workspace, assets, 7, true, &error), "read-only gate restore failed");
    draw();
    require_cancelled();
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    Access::CollapseInspector(ui, true);
    draw();
    Access::CollapseInspector(ui, false);
    draw();
    draw();
    require_cancelled();
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    ui.RequestCloseConfirmation();
    draw();
    draw();
    require_cancelled();
    Nexora::Window::WindowEvent escape;
    escape.type = Nexora::Window::WindowEventType::Key;
    escape.value0 = static_cast<int>(Nexora::Window::Key::Escape);
    escape.value1 = 1;
    ui.ProcessEvents(std::array{escape});
    draw();
    escape.value1 = 0;
    ui.ProcessEvents(std::array{escape});
    draw();
    Require(ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
            "close modal did not restore editing");
    require_cancelled();
    reject(key, asset, 6);
    reject(key, runtime::AssetUuid{99, 77}, 7);
    auto stale_key = key;
    ++stale_key.entity_generation;
    reject(stale_key, asset, 7);
    Require(content.Open(workspace, assets, 7, false, &error), "read-only opening failed");
    reject(key, asset, 7);
    Require(content.Open(workspace, assets, 7, true, &error), "writable reopening failed");
    editor::MaterialAssetCatalog empty;
    reject(key, asset, 7, &empty);
    Require(content.Open(workspace, assets, 8, true, &error), "new project generation failed");
    editor::MaterialAssetCatalog next_generation;
    Require(next_generation.PublishContent(content.Browser(), &error),
            "next-generation catalog failed");
    Require(content.Open(workspace, assets, 7, true, &error), "original generation restore failed");
    reject(key, asset, 7, &next_generation);
    Require(scene.Redo() && editor::ReadMaterialAssetReference(scene, key) == asset &&
                scene.Undo() && !scene.Dirty(),
            "rejected requests discarded the pending Redo transaction");
    const auto other = scene.Create("Other mesh");
    const auto other_key = *scene.Key(other);
    Require(scene.SetMeshRenderer(other_key, mesh) && scene.Save(path),
            "second mesh fixture failed");
    Require(scene.SetMeshRenderer(other_key, std::nullopt) && scene.Select(std::array{other}),
            "missing Mesh Renderer fixture failed");
    Access::QueueInspectorMaterial(ui, other_key, asset, 7);
    draw();
    Require(!editor::ReadMaterialAssetReference(scene, other_key) &&
                Access::InspectorMaterialLabel(ui) == "No Mesh Renderer" && scene.Undo() &&
                !scene.Dirty(),
            "material assignment created a reference without a Mesh Renderer");
    Require(scene.Select(std::array{other}), "other selection failed");
    reject(key, asset, 7);
    Require(scene.Select(std::array{entity, other}), "multi-selection failed");
    reject(key, asset, 7);
    Require(Access::InspectorMaterialLabel(ui) == "Unassigned",
            "equal absent material references were incorrectly mixed");
    Require(!editor::ReadMaterialAssetReference(scene, other_key),
            "multi-selection partially assigned another entity");
    Require(scene.Select(std::array{entity}), "reselection failed");
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    draw();
    Require(scene.Undo() && scene.Redo() && editor::ReadMaterialAssetReference(scene, key) == asset,
            "single material transaction lost Undo/Redo");
    Require(scene.SetTransform(entity, runtime::Transform{5, 0, 0}) && scene.Undo(),
            "pending transform Redo fixture failed");
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    draw();
    Require(scene.Redo() && scene.Transform(entity)->x == 5 && scene.Undo(),
            "no-op material assignment discarded another transaction's Redo");
    Require(scene.Save(path) && scene.Reload(path) && scene.Select(std::array{entity}),
            "assigned scene persistence failed");
    const auto old_key = key;
    key = *scene.Key(entity);
    Require(editor::ReadMaterialAssetReference(scene, key) == asset &&
                scene.MeshRenderer(key)->material.shader == mesh.material.shader,
            "reopened scene lost material UUID or legacy shader ID");
    Access::QueueInspectorMaterial(ui, old_key, asset, 7);
    draw();
    Require(!scene.Dirty(), "stale document generation committed after reload");
    draw();
    Require(Access::InspectorMaterialLabel(ui).find("Red.nmaterial") != std::string_view::npos &&
                Access::InspectorOpaqueInfo(ui).empty(),
            "known material reference shown as missing plugin");
    // A successful same-generation reimport invalidates the old catalog, rather than allowing
    // assignment against a retained material pointer. Existing scene references remain intact.
    WriteMaterial(source, .1F);
    Require(content.Reimport(asset, &error), "material reimport failed");
    const auto current = content.Browser().Find(asset)->material;
    Require(current && current != materials.ResolveAsset(asset, 7)->material,
            "reimport did not publish a new immutable material");
    Require(scene.Select(std::array{other}), "reimport target selection failed");
    Access::QueueInspectorMaterial(ui, *scene.Key(other), asset, 7);
    draw();
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, *scene.Key(other)) &&
                editor::ReadMaterialAssetReference(scene, key) == asset,
            "stale catalog assigned a reimported material");
    Require(scene.Select(std::array{entity}), "reimport target reselection failed");
    Require(materials.PublishContent(content.Browser(), &error) &&
                materials.ResolveAsset(asset, 7)->material == current,
            "catalog did not publish reimported material");
    Require(content.Delete(std::array{asset}, &error) &&
                materials.PublishContent(content.Browser(), &error),
            "material deletion fixture failed");
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    draw();
    Require(!scene.Dirty() && editor::ReadMaterialAssetReference(scene, key) == asset &&
                Access::InspectorMaterialLabel(ui) == "Missing material",
            "missing material erased saved reference");
    Require(content.Undo(&error) && materials.PublishContent(content.Browser(), &error),
            "material content Undo failed");
    // Actual multi-selection widgets publish one metadata transaction, including initial
    // references, while preserving each object's native shader and mesh values.
    const auto multi_other = *scene.Key(other);
    Require(scene.Select(std::array{entity, other}), "material batch selection failed");
    draw();
    Require(Access::InspectorMaterialLabel(ui) == "Mixed", "mixed material references hidden");
    for (const float scale : {1.F, 2.F}) {
      pointer_scale = scale;
      ui.SetDisplay(1280, 1600, scale);
      Access::FocusInspector(ui);
      draw();
      draw();
      click(0);
      draw();
      click(1);
      Require(editor::ReadMaterialAssetReference(scene, key) == asset &&
                  editor::ReadMaterialAssetReference(scene, multi_other) == asset &&
                  scene.MeshRenderer(key)->mesh == mesh.mesh &&
                  scene.MeshRenderer(multi_other)->mesh == mesh.mesh &&
                  scene.MeshRenderer(key)->material.shader == mesh.material.shader &&
                  scene.MeshRenderer(multi_other)->material.shader == mesh.material.shader,
              "actual multi-material widget changed mesh/shader or failed assignment");
      Require(scene.Undo() && !scene.Dirty() &&
                  editor::ReadMaterialAssetReference(scene, key) == asset &&
                  !editor::ReadMaterialAssetReference(scene, multi_other),
              "multi-material assignment did not Undo atomically to clean baseline");
    }
    const std::array material_keys{key, multi_other};
    const auto reject_batch = [&](std::span<const editor::SceneDocument::NodeKey> requested,
                                  std::uint64_t generation) {
      Access::QueueInspectorMaterials(ui, requested, asset, generation);
      draw();
      Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, multi_other),
              "rejected material batch partially published");
    };
    reject_batch(std::array{key}, 7);
    reject_batch(std::array{key, key}, 7);
    reject_batch(std::array{old_key, multi_other}, 7);
    reject_batch(material_keys, 6);
    Access::QueueInspectorMaterials(ui, material_keys, asset, 7);
    Require(scene.Select(std::array{entity}), "batch stale selection fixture failed");
    draw();
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, multi_other),
            "changed selection revived a queued material batch");
    Require(scene.Select(std::array{entity, other}), "batch reselection failed");
    Access::QueueInspectorMaterials(ui, material_keys, asset, 7);
    ui.ProcessEvents(std::array{lost, focus});
    draw();
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, multi_other),
            "focus cancellation revived a queued material batch");
    Require(scene.Redo() && editor::ReadMaterialAssetReference(scene, multi_other) == asset,
            "rejected material batches consumed Redo");
    Require(scene.Save(path), "material batch save failed");
    Require(scene.SetTransform(other, runtime::Transform{8, 0, 0}) && scene.Undo(),
            "material batch no-op Redo fixture failed");
    Access::QueueInspectorMaterials(ui, material_keys, asset, 7);
    draw();
    Require(!scene.Dirty() && scene.Redo() && scene.Transform(other)->x == 8 && scene.Undo(),
            "equal material batch consumed unrelated Redo");
    Require(scene.Reload(path), "material batch reopen failed");
    key = *scene.Key(entity);
    Require(editor::ReadMaterialAssetReference(scene, key) == asset &&
                editor::ReadMaterialAssetReference(scene, *scene.Key(other)) == asset &&
                scene.MeshRenderer(key)->mesh == mesh.mesh &&
                scene.MeshRenderer(*scene.Key(other))->material.shader == mesh.material.shader,
            "material batch reopen changed UUIDs or mesh/shader values");
    const auto fresh = scene.Create("Unassigned batch target");
    const auto bad = scene.Create("Unsupported batch target");
    const std::array rejected_keys{*scene.Key(fresh), *scene.Key(bad)};
    Require(
        scene.SetMeshRenderers(rejected_keys, std::array{std::optional{mesh}, std::optional{mesh}}),
        "unsupported material batch mesh fixture failed");
    auto bad_reference = editor::MaterialAssetReference(asset);
    bad_reference.data[0] = 2;
    Require(scene.SetOpaqueComponent(rejected_keys[1], bad_reference) && scene.Save(path) &&
                scene.Select(std::array{fresh, bad}),
            "unsupported material batch fixture failed");
    Access::QueueInspectorMaterials(ui, rejected_keys, asset, 7);
    draw();
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, rejected_keys[0]) &&
                scene.OpaqueComponents(rejected_keys[1])->front() == bad_reference,
            "late unsupported target partially assigned material or erased unknown bytes");
    Require(scene.SetOpaqueComponent(rejected_keys[1], editor::MaterialAssetReference(asset)) &&
                scene.SetMeshRenderer(rejected_keys[1], std::nullopt) && scene.Save(path),
            "late missing Mesh Renderer fixture failed");
    Access::QueueInspectorMaterials(ui, rejected_keys, asset, 7);
    draw();
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, rejected_keys[0]) &&
                editor::ReadMaterialAssetReference(scene, rejected_keys[1]) == asset &&
                Access::InspectorMaterialLabel(ui) == "No Mesh Renderer",
            "late missing Mesh Renderer partially assigned a material batch");
    Require(scene.Select(std::array{entity}), "final material reselection failed");
    auto unsupported = editor::MaterialAssetReference(asset);
    unsupported.data.front() = 2;
    Require(scene.SetOpaqueComponent(key, unsupported) && scene.Save(path),
            "unknown-version fixture failed");
    Access::QueueInspectorMaterial(ui, key, asset, 7);
    draw();
    Require(!scene.Dirty() && !editor::ReadMaterialAssetReference(scene, key) &&
                Access::InspectorMaterialLabel(ui) == "Unsupported material reference" &&
                scene.OpaqueComponents(key)->front() == unsupported,
            "unsupported reference version was overwritten or hidden");
    Require(scene.Reload(path), "unknown-version persistence failed");
    key = *scene.Key(entity);
    Require(scene.OpaqueComponents(key)->front() == unsupported,
            "unknown-version bytes changed on reload");
    // Explicit removal uses document-owned supported references, including missing sources.
    const auto clear_first = scene.Create("Known clear target");
    const auto clear_second = scene.Create("Missing clear target");
    const std::array clear_keys{*scene.Key(clear_first), *scene.Key(clear_second)};
    const runtime::AssetUuid missing_asset{0x1122334455667788ULL, 0x99aabbccddeeff00ULL};
    const editor::OpaqueComponent other_payload{991, "Other plugin", {0, 7, 255}};
    Require(
        scene.SetMeshRenderers(clear_keys, std::array{std::optional{mesh}, std::optional{mesh}}) &&
            scene.SetOpaqueComponent(clear_keys[0], editor::MaterialAssetReference(asset)) &&
            scene.SetOpaqueComponent(clear_keys[1],
                                     editor::MaterialAssetReference(missing_asset)) &&
            scene.SetOpaqueComponent(clear_keys[0], other_payload) && scene.Save(path),
        "material clear owning fixture failed");
    const auto source_before_clear = ReadBytes(source);
    const auto first_before_clear = scene.OpaqueComponents(clear_keys[0]);
    const auto second_before_clear = scene.OpaqueComponents(clear_keys[1]);
    Require(scene.Select(std::array{clear_first}), "single clear selection failed");
    pointer_scale = 1;
    ui.SetDisplay(1280, 1600, pointer_scale);
    Access::FocusInspector(ui);
    draw();
    draw();
    click(0);
    draw();
    click(2);
    Require(!editor::ReadMaterialAssetReference(scene, clear_keys[0]) &&
                scene.OpaqueComponents(clear_keys[0]) == std::vector{other_payload} &&
                scene.Undo() && !scene.Dirty() &&
                scene.OpaqueComponents(clear_keys[0]) == first_before_clear,
            "actual single clear lost opaque data or failed Undo");
    Require(scene.Select(std::array{clear_first, clear_second}), "multi clear selection failed");
    for (const float scale : {1.F, 2.F}) {
      pointer_scale = scale;
      ui.SetDisplay(1280, 1600, scale);
      Access::FocusInspector(ui);
      draw();
      draw();
      Require(Access::InspectorMaterialLabel(ui) == "Mixed", "clear mixed label wrong");
      click(0);
      draw();
      click(2);
      Require(!editor::ReadMaterialAssetReference(scene, clear_keys[0]) &&
                  !editor::ReadMaterialAssetReference(scene, clear_keys[1]),
              "actual multi clear did not remove both supported references");
      Require(scene.OpaqueComponents(clear_keys[0]) == std::vector{other_payload} &&
                  scene.OpaqueComponents(clear_keys[1])->empty() &&
                  scene.MeshRenderer(clear_keys[0])->mesh == mesh.mesh &&
                  scene.MeshRenderer(clear_keys[1])->material.shader == mesh.material.shader,
              "actual multi clear changed unrelated opaque/mesh/shader values");
      // Deferred edits publish after this frame's widgets; the next frame observes their label.
      draw();
      Require(Access::InspectorMaterialLabel(ui) == "Unassigned",
              "actual multi clear label did not update on the next frame");
      Require(scene.Undo() && !scene.Dirty() &&
                  scene.OpaqueComponents(clear_keys[0]) == first_before_clear &&
                  scene.OpaqueComponents(clear_keys[1]) == second_before_clear && scene.Redo() &&
                  scene.Undo(),
              "actual multi clear failed one atomic Undo/Redo transaction");
    }
    const auto reject_clear = [&](std::span<const editor::SceneDocument::NodeKey> requested,
                                  std::uint64_t generation) {
      Access::QueueInspectorMaterialClear(ui, requested, generation);
      draw();
      Require(!scene.Dirty() && scene.OpaqueComponents(clear_keys[0]) == first_before_clear &&
                  scene.OpaqueComponents(clear_keys[1]) == second_before_clear,
              "rejected clear partially removed material references");
    };
    auto stale_clear = clear_keys[1];
    ++stale_clear.entity_generation;
    reject_clear(std::array{clear_keys[0]}, 7);
    reject_clear(std::array{clear_keys[0], clear_keys[0]}, 7);
    reject_clear(std::array{clear_keys[0], stale_clear}, 7);
    reject_clear(clear_keys, 6);
    Access::QueueInspectorMaterialClear(ui, clear_keys, 7);
    ui.ProcessEvents(std::array{lost, focus});
    draw();
    Require(!scene.Dirty() && scene.OpaqueComponents(clear_keys[0]) == first_before_clear,
            "clear revived after focus cancellation");
    Access::QueueInspectorMaterialClear(ui, clear_keys, 7);
    Require(content.Open(workspace, assets, 7, false, &error), "clear read-only fixture failed");
    draw();
    Require(content.Open(workspace, assets, 7, true, &error), "clear read-only restore failed");
    draw();
    Require(!scene.Dirty() && scene.OpaqueComponents(clear_keys[0]) == first_before_clear,
            "read-only clear revived or mutated metadata");
    Require(scene.Redo() && scene.Undo() && !scene.Dirty(),
            "rejected clears consumed pending material Redo");
    // Real popup clearing remains usable with no catalog borrow.
    Access::FocusInspector(ui);
    draw(nullptr, true, false);
    draw(nullptr, true, false);
    click(0, false);
    draw(nullptr, true, false);
    click(2, false);
    Require(!editor::ReadMaterialAssetReference(scene, clear_keys[0]) &&
                !editor::ReadMaterialAssetReference(scene, clear_keys[1]) &&
                scene.OpaqueComponents(clear_keys[0]) == std::vector{other_payload} &&
                scene.Save(path),
            "catalog-unavailable actual clearing or save failed");
    Require(scene.SetTransform(clear_first, runtime::Transform{9, 0, 0}) && scene.Undo(),
            "absent clear pending Redo fixture failed");
    Access::QueueInspectorMaterialClear(ui, clear_keys, 7);
    draw();
    Require(!scene.Dirty() && scene.Redo() && scene.Transform(clear_first)->x == 9 && scene.Undo(),
            "all-absent clear consumed unrelated Runtime Redo");
    Require(scene.Reload(path), "clear save/reopen failed");
    const std::array reopened_clear{*scene.Key(clear_first), *scene.Key(clear_second)};
    Require(!editor::ReadMaterialAssetReference(scene, reopened_clear[0]) &&
                !editor::ReadMaterialAssetReference(scene, reopened_clear[1]) &&
                scene.OpaqueComponents(reopened_clear[0]) == std::vector{other_payload},
            "reopen restored removed material or erased unrelated plugin data");
    Require(scene.SetOpaqueComponent(reopened_clear[0], editor::MaterialAssetReference(asset)) &&
                scene.Select(std::array{clear_first, entity}) && scene.Save(path),
            "late unsupported clear fixture failed");
    Access::QueueInspectorMaterialClear(ui, std::array{reopened_clear[0], *scene.Key(entity)}, 7);
    draw();
    Require(!scene.Dirty() &&
                editor::ReadMaterialAssetReference(scene, reopened_clear[0]) == asset &&
                scene.OpaqueComponents(*scene.Key(entity))->front() == unsupported,
            "late unsupported clear removed supported first target or unknown data");
    Require(scene.SetOpaqueComponent(reopened_clear[1],
                                     editor::MaterialAssetReference(missing_asset)) &&
                scene.SetMeshRenderer(reopened_clear[1], std::nullopt) &&
                scene.Select(std::array{clear_first, clear_second}) && scene.Save(path),
            "late missing Mesh Renderer clear fixture failed");
    Access::QueueInspectorMaterialClear(ui, reopened_clear, 7);
    draw();
    Require(!scene.Dirty() &&
                editor::ReadMaterialAssetReference(scene, reopened_clear[0]) == asset &&
                editor::ReadMaterialAssetReference(scene, reopened_clear[1]) == missing_asset &&
                ReadBytes(source) == source_before_clear,
            "late missing component clear changed references or source material bytes");
    workspace = {};
    std::filesystem::remove_all(root);
    std::cout << "Material Inspector contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::filesystem::remove_all(root);
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
