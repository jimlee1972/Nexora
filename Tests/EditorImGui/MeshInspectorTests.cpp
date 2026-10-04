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
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-mesh-inspector-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    using namespace nexora;
    editor::ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(root, "Mesh Inspector", &error), "project creation failed");
    std::ofstream(root / "Content/Triangle.obj") << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    editor::AssetWorkspace assets;
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite, &error),
            "mesh indexing failed");
    editor::ProjectContentSession content;
    Require(content.Open(workspace, assets, 7, true, &error), "content opening failed");
    editor::MeshAssetCatalog meshes;
    Require(meshes.Publish(assets.Entries(), 7, &error), "catalog publication failed");
    const auto asset = assets.Entries().front().id;
    runtime::World world;
    const auto scene_id = world.LoadScene("Mesh Inspector");
    Require(world.Activate(scene_id), "scene activation failed");
    editor::SceneDocument scene(world, scene_id);
    const auto entity = scene.Create("Mesh object");
    const auto key = *scene.Key(entity);
    Require(scene.Select(std::array{entity}), "selection failed");
    editor::ProductShell shell;
    editor::imgui::EditorImGuiHost ui;
    ui.SetDisplay(1280, 720, 1);
    const auto draw = [&](const editor::MeshAssetCatalog *catalog = nullptr) {
      ui.BeginFrame();
      ui.DrawProductShell(shell, &scene, &workspace, &content, nullptr, nullptr, nullptr, nullptr,
                          nullptr, catalog ? catalog : &meshes);
      static_cast<void>(ui.EndFrame());
    };
    using Access = editor::imgui::EditorImGuiTestAccess;
    draw();
    Access::QueueInspectorMesh(ui, key, asset, 7);
    draw();
    Require(scene.MeshRenderer(key) &&
                scene.MeshRenderer(key)->mesh == editor::MeshResourceId(asset),
            "Inspector did not assign an imported mesh");
    Require(scene.Undo() && !scene.MeshRenderer(key) && scene.Redo(),
            "Inspector mesh assignment was not one Undo step");
    auto component = *scene.MeshRenderer(key);
    component.material.shader = 0xffffffffffffffffULL;
    Require(scene.SetMeshRenderer(key, component), "material fixture failed");
    Access::QueueInspectorMesh(ui, key, asset, 7);
    draw();
    Require(scene.MeshRenderer(key)->material.shader == component.material.shader,
            "mesh replacement lost the material reference");
    const auto path = root / "Content/Test.scene";
    Require(scene.Save(path) && !scene.Dirty(), "mesh scene save failed");
    Access::QueueInspectorMesh(ui, key, asset, 6);
    draw();
    Require(!scene.Dirty(), "stale project request mutated the scene");
    Access::QueueInspectorMesh(ui, key, runtime::AssetUuid{99, 77}, 7);
    draw();
    Require(!scene.Dirty(), "missing mesh request mutated the scene");
    Require(content.Open(workspace, assets, 7, false, &error), "read-only content opening failed");
    Access::QueueInspectorMesh(ui, key, std::nullopt, 7);
    draw();
    Require(scene.MeshRenderer(key) && !scene.Dirty(), "read-only content allowed mesh removal");
    Require(content.Open(workspace, assets, 7, true, &error), "writable content reopening failed");
    editor::MeshAssetCatalog replacement;
    Require(replacement.Publish(assets.Entries(), 8), "replacement catalog failed");
    Access::QueueInspectorMesh(ui, key, asset, 7);
    draw(&replacement);
    Require(!scene.Dirty(), "stale catalog request mutated the scene");
    Access::QueueInspectorMesh(ui, key, std::nullopt, 7);
    draw();
    Require(!scene.MeshRenderer(key) && scene.Undo() && scene.MeshRenderer(key) && !scene.Dirty(),
            "mesh removal did not restore presence and clean baseline in one Undo");
    Require(scene.Reload(path), "mesh scene reload failed");
    const auto reopened = *scene.Key(entity);
    Require(scene.MeshRenderer(reopened)->mesh == editor::MeshResourceId(asset) &&
                meshes.ResolveResource(scene.MeshRenderer(reopened)->mesh, 7),
            "saved mesh reference no longer resolved after reload");
    Require(scene.Select(std::array{entity}), "reloaded selection failed");
    Access::QueueInspectorMesh(ui, key, std::nullopt, 7);
    draw();
    Require(scene.MeshRenderer(reopened) && !scene.Dirty(),
            "stale entity request removed a reloaded component");
    const auto other = scene.Create("Other mesh");
    const auto empty = scene.Create("No mesh");
    const std::array batch{reopened, *scene.Key(other), *scene.Key(empty)};
    const auto original = *scene.MeshRenderer(reopened);
    const runtime::MeshComponent unresolved{123456789, {0x123456789abcdef0ULL}};
    Require(scene.SetMeshRenderer(batch[1], unresolved) && scene.Select(batch),
            "mixed fixture failed");
    Require(scene.Save(path), "batch baseline save failed");
    draw();
    Require(Access::InspectorMeshLabel(ui) == "Mixed", "mixed meshes/presence not displayed");
    const std::array<std::optional<runtime::MeshComponent>, 3> originals{original, unresolved,
                                                                         std::nullopt};
    const std::array<std::optional<runtime::MeshComponent>, 3> assigned{
        original, runtime::MeshComponent{original.mesh, unresolved.material},
        runtime::MeshComponent{original.mesh, {}}};
    auto invalid = batch;
    ++invalid.back().entity_generation;
    Require(!scene.SetMeshRenderers(invalid, assigned) && !scene.Dirty(),
            "invalid last key partially committed");
    const std::array duplicate{batch[0], batch[0], batch[2]};
    Require(!scene.SetMeshRenderers(duplicate, assigned) && !scene.Dirty(),
            "duplicate key batch accepted");
    Require(!scene.SetMeshRenderers({}, {}) &&
                !scene.SetMeshRenderers(batch, std::span(assigned).first(2)),
            "invalid batch shape accepted");
    Access::QueueInspectorMeshes(ui, invalid, asset, 7);
    draw();
    Require(!scene.Dirty(), "stale queued key partially committed");
    Access::QueueInspectorMeshes(ui, batch, asset, 6);
    draw();
    Require(!scene.Dirty(), "stale project batch committed");
    Require(content.Open(workspace, assets, 7, false, &error), "batch read-only fixture failed");
    Access::QueueInspectorMeshes(ui, batch, asset, 7);
    draw();
    Require(!scene.Dirty(), "read-only batch committed");
    Require(content.Open(workspace, assets, 7, true, &error), "batch writable fixture failed");
    std::ofstream(root / ".nexora/workspace.recovery") << "schema=1\n";
    Access::QueueInspectorMeshes(ui, batch, std::nullopt, 7);
    draw();
    Require(!scene.Dirty() && scene.MeshRenderer(batch[1]) && workspace.DiscardRecovery(&error),
            "recovery allowed a queued mesh batch");
    Access::QueueInspectorMeshes(ui, batch, asset, 7);
    Require(scene.Select(std::array{entity}), "changed selection failed");
    draw();
    Require(!scene.Dirty(), "abandoned selection request committed");
    Require(scene.Select(batch), "batch reselection failed");
    Access::QueueInspectorMeshes(ui, batch, asset, 7);
    draw();
    for (const auto key : batch)
      Require(scene.MeshRenderer(key) && scene.MeshRenderer(key)->mesh == original.mesh,
              "batch assignment failed");
    Require(scene.MeshRenderer(batch[0])->material.shader == original.material.shader &&
                scene.MeshRenderer(batch[1])->material.shader == unresolved.material.shader &&
                scene.MeshRenderer(batch[2])->material.shader == 0,
            "batch assignment copied another material");
    for (int replay = 0; replay < 3; ++replay) {
      Require(scene.Undo() && !scene.Dirty() && !scene.MeshRenderer(batch[2]) &&
                  scene.MeshRenderer(batch[1])->mesh == unresolved.mesh && scene.Redo(),
              "batch Undo/Redo lost immutable state");
    }
    Require(scene.Undo() && scene.SetMeshRenderers(batch, originals) && scene.Redo(),
            "no-op batch discarded Redo");
    draw();
    Require(Access::InspectorMeshLabel(ui) != "Mixed", "different materials mixed the mesh label");
    Require(scene.Save(path), "assigned batch save failed");
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    Access::FocusInspector(ui);
    draw();
    draw();
    const auto click = [&](std::size_t control) {
      const auto point = Access::InspectorMeshPosition(ui, control);
      if (!point)
        throw std::runtime_error("mesh control unavailable: " + std::to_string(control));
      Nexora::Window::WindowEvent pointer;
      pointer.type = Nexora::Window::WindowEventType::Pointer;
      pointer.value0 = static_cast<int>((*point)[0]);
      pointer.value1 = static_cast<int>((*point)[1]);
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
    click(2);
    for (const auto key : batch)
      Require(!scene.MeshRenderer(key), "Remove button did not remove the entire selection");
    Require(scene.Undo() && !scene.Dirty() && scene.Redo(), "batch removal was not one Undo step");
    draw();
    Require(Access::InspectorMeshLabel(ui) == "None", "absent batch not displayed as None");
    click(0);
    draw();
    click(1);
    for (const auto key : batch)
      Require(scene.MeshRenderer(key) && scene.MeshRenderer(key)->mesh == original.mesh,
              "combo click did not assign to all entities");
    Require(scene.Undo() && !scene.MeshRenderer(batch[0]) && !scene.MeshRenderer(batch[1]) &&
                !scene.MeshRenderer(batch[2]) && scene.Redo(),
            "combo batch was not one Undo step");
    Require(scene.Save(path) && scene.Reload(path), "batch persistence failed");
    for (const auto key : batch)
      Require(scene.MeshRenderer(*scene.Key(key.id))->mesh == original.mesh,
              "reopened batch lost mesh reference");
    Access::QueueInspectorMeshes(ui, batch, std::nullopt, 7);
    Require(scene.Select(std::array{entity, other, empty}), "reopened batch selection failed");
    draw();
    Require(!scene.Dirty(), "stale document batch committed after reload");

    runtime::World runtime_world;
    const auto runtime_scene = runtime_world.LoadScene("Batch");
    runtime::SceneEditor runtime_editor(runtime_world);
    const auto ra = runtime_editor.CreateEntity(runtime_scene);
    const auto rb = runtime_editor.CreateEntity(runtime_scene);
    const std::array runtime_ids{ra, rb};
    const std::array<std::optional<runtime::MeshComponent>, 2> runtime_meshes{original, unresolved};
    Require(!runtime_editor.SetMeshRenderers(std::array{ra, runtime::Id{0}}, runtime_meshes) &&
                !runtime_world.FindEntity(ra)->mesh_renderer,
            "Runtime partially committed missing last entity");
    Require(!runtime_editor.SetMeshRenderers(std::array{ra, ra}, runtime_meshes) &&
                !runtime_editor.SetMeshRenderers({}, {}) &&
                !runtime_editor.SetMeshRenderers(runtime_ids, std::span(runtime_meshes).first(1)),
            "Runtime accepted invalid batch");
    Require(runtime_editor.SetMeshRenderers(runtime_ids, runtime_meshes), "Runtime batch failed");
    for (int replay = 0; replay < 3; ++replay)
      Require(runtime_editor.Undo() && !runtime_world.FindEntity(ra)->mesh_renderer &&
                  !runtime_world.FindEntity(rb)->mesh_renderer && runtime_editor.Redo() &&
                  runtime_world.FindEntity(rb)->mesh_data.material.shader ==
                      unresolved.material.shader,
              "Runtime batch replay failed");
    workspace = {};
    std::filesystem::remove_all(root);
    std::cout << "Mesh Inspector contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::filesystem::remove_all(root);
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
