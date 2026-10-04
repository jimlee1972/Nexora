#include "EditorImGuiTestAccess.h"
#include "GameViewPreview.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-mesh-creation-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace, observer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  runtime::AssetUuid asset{}, text_asset{};
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Mesh creation");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  Fixture() {
    Require(workspace.Create(root, "Mesh Creation") &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "mesh creation workspace failed");
    std::ofstream(root / "Content/Triangle.obj") << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::ofstream(root / "Content/Notes.txt") << "Not a mesh\n";
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite) &&
                content.Open(workspace, assets, 7, true) && meshes.Publish(assets.Entries(), 7),
            "asset publication failed");
    for (const auto &entry : assets.Entries()) {
      if (entry.type == ".obj")
        asset = entry.id;
      if (entry.type == ".txt")
        text_asset = entry.id;
    }
    Require(meshes.ResolveAsset(asset, 7).has_value() && content.Browser().Select(asset) &&
                scene.Save(root / "Content/Main.scene"),
            "mesh fixture failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, 1);
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    Require(ui.SetSceneOverviewCamera({4, -3, 32}), "Scene center failed");
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusContent(ui);
    Draw();
    Draw();
  }
  ~Fixture() { std::filesystem::remove_all(root); }
  void Draw(const editor::MeshAssetCatalog *catalog = nullptr) {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, &content, nullptr, nullptr, nullptr, nullptr,
                        nullptr, catalog ? catalog : &meshes);
    static_cast<void>(ui.EndFrame());
  }
  void Click(const editor::MeshAssetCatalog *catalog = nullptr) {
    const auto point = Access::ContentAddMeshPosition(ui);
    Require(point.has_value(), "Content Add mesh button is absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0]);
    pointer.value1 = static_cast<int>((*point)[1]);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw(catalog);
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw(catalog);
  }
  void Unchanged() const {
    Require(world.SaveScene(scene_id) == baseline && scene.Nodes().empty() && !scene.Dirty(),
            "blocked mesh creation mutated the document");
  }
};
} // namespace
int main() {
  try {
    {
      runtime::World world;
      const auto scene_id = world.LoadScene("Runtime create");
      runtime::SceneEditor editor(world);
      const runtime::MeshComponent mesh{41, {std::numeric_limits<runtime::Id>::max()}};
      runtime::Transform pose{3, 2, -1, 0, 0, 0, 2, -2, 3, 4};
      const auto id = editor.CreateMeshEntity(scene_id, mesh, pose);
      Require(id && editor.UndoDepth() == 1 && world.FindEntity(id)->mesh_renderer &&
                  world.FindEntity(id)->transform.qw == 1 &&
                  world.FindEntity(id)->mesh_data.material.shader == mesh.material.shader,
              "initialized creation did not normalize/retain payloads");
      const auto created = world.SaveScene(scene_id);
      Require(editor.Undo() && !world.FindEntity(id), "Runtime create Undo failed");
      pose.sx = 0;
      Require(!editor.CreateMeshEntity(scene_id, mesh, pose) &&
                  !editor.CreateMeshEntity(scene_id, {0, {}}) &&
                  !editor.CreateMeshEntity(99999, mesh) && editor.UndoDepth() == 0 &&
                  editor.Redo() && world.SaveScene(scene_id) == created,
              "failed create discarded Runtime Redo");
      Require(world.RequestUnload(scene_id) && !editor.CreateMeshEntity(scene_id, mesh) &&
                  !editor.CreateEntity(scene_id) && editor.UndoDepth() == 1,
              "unloading scene accepted creation");
    }
    {
      Fixture f;
      const runtime::MeshComponent mesh{editor::MeshResourceId(f.asset),
                                        {std::numeric_limits<runtime::Id>::max()}};
      const runtime::Transform pose{3, 2, -1, 0, 0, 0, 1, -2, 3, 4};
      const auto id = f.scene.CreateMesh("Triangle", mesh, pose);
      Require(id && f.scene.Name(id) == "Triangle" && f.scene.Transform(id) == pose &&
                  f.scene.MeshRenderer(*f.scene.Key(id))->material.shader == mesh.material.shader,
              "mesh document creation lost metadata/pose");
      const auto created = f.world.SaveScene(f.scene_id);
      Require(f.scene.Undo(), "mesh document Undo failed");
      f.Unchanged();
      auto invalid = pose;
      invalid.x = std::numeric_limits<double>::infinity();
      Require(!f.scene.CreateMesh("Bad\nName", mesh) && !f.scene.CreateMesh("", mesh) &&
                  !f.scene.CreateMesh("Invalid", mesh, invalid) && f.scene.Redo() &&
                  f.world.SaveScene(f.scene_id) == created && f.scene.Name(id) == "Triangle",
              "failed create discarded document Redo or stable identity");
      Require(f.scene.Save(f.root / "Content/Created.scene") &&
                  f.scene.Reload(f.root / "Content/Created.scene") &&
                  f.scene.Transform(id) == pose &&
                  f.scene.MeshRenderer(*f.scene.Key(id))->material.shader == mesh.material.shader,
              "initialized mesh did not survive save/reload");
    }
    for (int gate = 0; gate < 11; ++gate) {
      Fixture f;
      editor::MeshAssetCatalog stale;
      const editor::MeshAssetCatalog *catalog = &f.meshes;
      if (gate == 1)
        f.active = &f.observer;
      if (gate == 2) {
        Require(stale.Publish(f.assets.Entries(), 8), "stale catalog failed");
        catalog = &stale;
      }
      if (gate == 3)
        Require(f.content.Browser().Select(f.text_asset), "text selection failed");
      if (gate == 4)
        std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
      if (gate == 5)
        f.ui.RequestCloseConfirmation();
      if (gate == 6)
        f.meshes.Clear();
      if (gate == 7)
        Require(f.content.Browser().Select(f.text_asset, true), "multi-selection failed");
      if (gate == 8)
        Require(f.content.Open(f.workspace, f.assets, 7, false) &&
                    f.content.Browser().Select(f.asset),
                "read-only content failed");
      if (gate >= 9) {
        f.ui.SetNativeScenePreview(true);
        Require(f.ui.SetNativeSceneOrbit({0.7, 0.4, 12, 2}), "native Scene center failed");
        if (gate == 10)
          Require(f.ui.SetSceneOverviewCamera({200000, -300000, 32}), "wide native center failed");
      }
      f.Draw(catalog);
      f.Click(catalog);
      if (gate && gate < 9)
        f.Unchanged();
      else {
        Require(f.scene.Nodes().size() == 1 && f.scene.Selection().size() == 1,
                "Add mesh did not create/select one root");
        const auto id = f.scene.Selection().front();
        const double expected_x = gate == 10 ? 100000 : 4;
        const double expected_z = gate == 10 ? -100000 : -3;
        Require(f.scene.Name(id) == "Triangle" && f.scene.Transform(id)->x == expected_x &&
                    f.scene.Transform(id)->y == (gate >= 9 ? 2 : 0) &&
                    f.scene.Transform(id)->z == expected_z &&
                    f.scene.MeshRenderer(*f.scene.Key(id))->mesh == editor::MeshResourceId(f.asset),
                "Add mesh lost the asset reference or Scene center");
        runtime::PlaySession play(f.world);
        Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
                "Play failed");
        const auto camera = play.PlayWorld()->CreateEntity(f.scene_id).id;
        runtime::WorldCommandBuffer setup;
        setup.SetCamera(camera, runtime::CameraComponent{});
        setup.SetTransform(camera, {4, 0, 5});
        Require(setup.Apply(*play.PlayWorld()), "Game camera setup failed");
        const auto frame =
            editor::preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), f.meshes, 2);
        Require(frame.instances.size() == 1 && frame.geometry.vertices.size() == 3 &&
                    frame.geometry.indices.size() == 3 && frame.instances.front().model_transform &&
                    frame.instances.front().model_transform->at(3) == expected_x &&
                    frame.instances.front().model_transform->at(11) == expected_z,
                "created mesh did not resolve into Game geometry");
        Require(f.scene.Undo(), "UI Add mesh Undo failed");
        f.Unchanged();
        Require(f.scene.Redo() && f.scene.Name(id) == "Triangle" &&
                    f.scene.MeshRenderer(*f.scene.Key(id))->mesh == editor::MeshResourceId(f.asset),
                "UI Add mesh Redo lost initialized payload/identity");
      }
    }
    std::cout << "Mesh scene instantiation contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
