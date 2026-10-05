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
      ("nexora-mesh-drop-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace, observer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  runtime::AssetUuid asset{}, text_asset{};
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Mesh drop");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  float scale;
  const editor::MeshAssetCatalog *catalog = &meshes;
  explicit Fixture(float dpi = 1) : scale(dpi) {
    Require(workspace.Create(root, "Mesh Drop") &&
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
    ui.SetDisplay(1280, 900, scale);
    Access::ConfigureSyntheticInput(ui);
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
  void Hover(bool native = false) {
    const auto viewport = ui.SceneCanvasViewport();
    Require(viewport.has_value(), "Scene canvas is absent");
    Move((viewport->x + viewport->width * 0.5F) / scale + (native ? 0 : 64),
         (viewport->y + viewport->height * 0.5F) / scale + (native ? 0 : 32));
    Draw();
    Unchanged(); // Acceptance preview must never author an entity or consume history.
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
    Require(world.SaveScene(scene_id) == baseline && scene.Nodes().empty() && !scene.Dirty(),
            "blocked mesh creation mutated the document");
  }
};
} // namespace
int main() {
  try {
    for (float dpi : {1.0F, 2.0F}) {
      for (bool native : {false, true}) {
        Fixture f(dpi);
        if (native) {
          f.ui.SetNativeScenePreview(true);
          Require(f.ui.SetNativeSceneOrbit({0.7, 0.4, 12, 0}), "orbit setup failed");
          f.Draw();
        }
        f.Start(f.asset);
        f.Hover(native);
        f.Button(false);
        Require(f.scene.Nodes().size() == 1 && f.scene.Selection().size() == 1,
                "pointer mesh delivery did not create/select a root");
        const auto id = f.scene.Selection().front();
        const auto pose = f.scene.Transform(id);
        Require(pose && f.scene.Name(id) == "Triangle" &&
                    f.scene.MeshRenderer(*f.scene.Key(id))->mesh == editor::MeshResourceId(f.asset),
                "mesh drop lost stable asset identity or name");
        const double expected_x = native ? 4 : 6;
        const double expected_z = native ? -3 : -2;
        Require(std::abs(pose->x - expected_x) < 0.06 && pose->y == 0 &&
                    std::abs(pose->z - expected_z) < 0.06,
                "mesh drop used the wrong Scene point or DPI conversion");
        const auto created = f.world.SaveScene(f.scene_id);
        Require(f.scene.Undo(), "mesh drop Undo failed");
        f.Unchanged();
        Require(!f.scene.Undo() && f.scene.Redo() && f.scene.Name(id) == "Triangle" &&
                    f.world.SaveScene(f.scene_id) == created,
                "mesh drop was not one initialized Undo/Redo transaction");
        Require(f.scene.Save(f.root / "Content/Dropped.scene") &&
                    f.scene.Reload(f.root / "Content/Dropped.scene") &&
                    f.scene.Transform(id) == pose &&
                    f.scene.MeshRenderer(*f.scene.Key(id))->mesh == editor::MeshResourceId(f.asset),
                "dropped mesh failed save/reload");
      }
    }
    for (int gate = 0; gate < 15; ++gate) {
      Fixture f;
      // Keep an existing Redo branch: rejected deliveries may not consume it.
      const auto sentinel = f.scene.Create("History sentinel");
      Require(sentinel && f.scene.Undo(), "history fixture failed");
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
                "project generation switch failed");
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
      if (gate == 12) {
        f.active = &f.observer;
        f.Draw();
        f.active = &f.workspace;
      }
      if (gate == 13) {
        Require(f.content.Open(f.workspace, f.assets, 7, false), "content access change failed");
        f.Draw();
        Require(f.content.Open(f.workspace, f.assets, 7, true), "content access restore failed");
      }
      const bool native = gate == 10 || gate == 11 || gate == 14;
      if (native) {
        f.ui.SetNativeScenePreview(true);
        Require(f.ui.SetNativeSceneOrbit(
                    {0.7, 0.4, 12, gate == 10 ? -20.0 : (gate == 14 ? 1000.0 : 100000.0)}),
                "invalid ground fixture failed");
      }
      f.Draw();
      f.Hover(native);
      f.Button(false);
      f.Unchanged();
      Require(f.scene.Redo() && f.scene.Name(sentinel) == "History sentinel",
              "rejected mesh drop discarded Redo");
    }
    std::cout << "Typed mesh Scene drag/drop contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
