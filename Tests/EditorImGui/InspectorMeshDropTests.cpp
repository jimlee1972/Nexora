#include "EditorImGuiTestAccess.h"
#include <cmath>

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
      ("nexora-inspector-mesh-drop-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace, observer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  runtime::AssetUuid asset{}, text_asset{};
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Inspector mesh drop");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  runtime::Id first{}, second{};
  float scale;
  const editor::MeshAssetCatalog *catalog = &meshes;
  explicit Fixture(float dpi = 1) : scale(dpi) {
    Require(workspace.Create(root, "Inspector Mesh Drop") &&
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
    first = scene.Create("Existing mesh");
    second = scene.Create("Missing mesh");
    Require(scene.SetMeshRenderer(*scene.Key(first), runtime::MeshComponent{51, {99}}) &&
                scene.Select(std::array{first, second}),
            "selection setup failed");
    Require(meshes.ResolveAsset(asset, 7).has_value() && content.Browser().Select(asset) &&
                scene.Save(root / "Content/Main.scene"),
            "mesh fixture failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, scale);
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
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, &content, nullptr, nullptr, nullptr, nullptr,
                        nullptr, catalog);
    static_cast<void>(ui.EndFrame());
  }
  void Move(float x, float y) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Pointer;
    event.value0 = static_cast<int>(x * scale);
    event.value1 = static_cast<int>(y * scale);
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Button(bool down) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::PointerButton;
    event.value0 = 0;
    event.value1 = down;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Start(runtime::AssetUuid source) {
    const auto point = Access::ContentAssetPosition(ui, source);
    Require(point.has_value(), "Content source is absent");
    Move((*point)[0], (*point)[1]);
    Button(true);
    Move((*point)[0] + 12, (*point)[1]);
    Draw();
    Require(Access::ContentDragActive(ui), "pointer asset drag did not start");
  }
  void Hover() {
    const auto point = Access::InspectorMeshPosition(ui, 0);
    Require(point.has_value(), "Inspector Mesh field is absent");
    Move((*point)[0], (*point)[1]);
    Draw();
    Unchanged();
  }
  void Focus(bool focused) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = focused;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Escape() {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(Nexora::Window::Key::Escape);
    event.value1 = 1;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Unchanged() const {
    Require(world.SaveScene(scene_id) == baseline && scene.Nodes().size() == 2 && !scene.Dirty(),
            "mesh hover or rejected drop mutated the document");
  }
};
} // namespace
int main() {
  try {
    for (float dpi : {1.0F, 2.0F}) {
      Fixture f(dpi);
      f.Start(f.asset);
      f.Hover();
      f.Button(false);
      const auto first = *f.scene.Key(f.first), second = *f.scene.Key(f.second);
      Require(f.scene.MeshRenderer(first)->mesh == editor::MeshResourceId(f.asset) &&
                  f.scene.MeshRenderer(first)->material.shader == 99 &&
                  f.scene.MeshRenderer(second)->mesh == editor::MeshResourceId(f.asset) &&
                  f.scene.MeshRenderer(second)->material.shader == 0 &&
                  f.scene.Nodes().size() == 2 && f.scene.Selection().size() == 2,
              "Inspector mesh drop did not assign the whole selection and preserve materials");
      const auto assigned = f.world.SaveScene(f.scene_id);
      Require(f.scene.Undo(), "mesh drop Undo failed");
      f.Unchanged();
      Require(f.scene.Redo() && f.world.SaveScene(f.scene_id) == assigned,
              "mesh drop Redo lost initialized references");
      Require(f.scene.Save(f.root / "Content/Main.scene") &&
                  f.scene.Reload(f.root / "Content/Main.scene") &&
                  f.scene.MeshRenderer(*f.scene.Key(f.first))->material.shader == 99 &&
                  f.scene.MeshRenderer(*f.scene.Key(f.second))->mesh ==
                      editor::MeshResourceId(f.asset),
              "Inspector drop references failed persistence");
    }
    for (int gate = 0; gate < 12; ++gate) {
      Fixture f;
      const auto sentinel = f.scene.Create("History sentinel");
      Require(sentinel && f.scene.Undo(), "Redo fixture failed");
      f.Start(gate == 4 ? f.text_asset : f.asset);
      editor::MeshAssetCatalog stale;
      if (gate == 0)
        f.active = &f.observer;
      if (gate == 1)
        Require(f.content.Open(f.workspace, f.assets, 7, false), "read-only content failed");
      if (gate == 2) {
        Require(stale.Publish(f.assets.Entries(), 8), "stale catalog failed");
        f.catalog = &stale;
      }
      if (gate == 3)
        f.meshes.Clear();
      if (gate == 5)
        Require(f.content.Open(f.workspace, f.assets, 8, true) &&
                    f.meshes.Publish(f.assets.Entries(), 8),
                "generation switch failed");
      if (gate == 6)
        std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
      if (gate == 7)
        f.ui.RequestCloseConfirmation();
      if (gate == 8)
        f.Escape();
      if (gate == 9) {
        f.Focus(false);
        f.Focus(true);
      }
      if (gate == 11) {
        f.active = &f.observer;
        f.Draw();
        f.active = &f.workspace;
      }
      f.Draw();
      f.Hover();
      if (gate == 10) {
        Require(f.scene.Select(std::span<const runtime::Id>{}), "deselection failed");
        f.Draw();
      }
      f.Button(false);
      f.Unchanged();
      Require(f.scene.Redo() && f.scene.Name(sentinel) == "History sentinel",
              "rejected Inspector drop consumed Redo");
    }
    std::cout << "Typed Content-to-Inspector mesh drop contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
