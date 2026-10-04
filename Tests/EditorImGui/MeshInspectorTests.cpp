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
