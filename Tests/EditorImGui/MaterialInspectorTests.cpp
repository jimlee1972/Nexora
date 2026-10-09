#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
#include <iostream>
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
                          bool content_available = true) {
      ui.BeginFrame();
      ui.DrawProductShell(shell, &scene, &workspace, content_available ? &content : nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          catalog ? catalog : &materials);
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
    const auto click = [&](std::size_t control) {
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
      draw();
      button.value1 = 0;
      ui.ProcessEvents(std::array{button});
      draw();
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
    Require(Access::InspectorMaterialLabel(ui) == "Single selection required",
            "multi-selection did not explain material limitation");
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
