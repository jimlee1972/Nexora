#include "EditorImGuiTestAccess.h"
#include "ScenePreviewCandidates.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool Near(double a, double b) { return std::abs(a - b) < 1e-5; }
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-scene-frame-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MeshAssetCatalog meshes;
  editor::ProductShell shell;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Frame geometry");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id ancestor{}, parent{}, leaf{}, other{}, redo_entity{};
  editor::imgui::EditorImGuiHost ui;
  float scale;
  editor::ProjectWorkspace *active = &writer;
  const editor::MeshAssetCatalog *catalog = &meshes;
  bool consume_framing = true;
  std::optional<std::string> original;
  explicit Fixture(float dpi, bool wide = false) : scale(dpi) {
    Require(writer.Create(root, "Frame geometry") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "frame project fixture failed");
    std::ofstream(root / "Content/Offset.obj") << "v 4 2 -1\nv 8 2 -1\nv 4 6 3\nf 1 2 3\n";
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite) &&
                content.Open(writer, assets, 7, true) && meshes.Publish(assets.Entries(), 7),
            "frame catalog fixture failed");
    ancestor = scene.Create("Stretch");
    parent = scene.Create("Turn", ancestor);
    leaf = scene.Create("Offset mesh", parent);
    runtime::Transform stretch{10, -3, 4};
    stretch.sx = -2;
    stretch.sy = 3;
    stretch.sz = 0.5;
    runtime::Transform turn;
    turn.qz = std::sin(std::numbers::pi / 8);
    turn.qw = std::cos(std::numbers::pi / 8);
    Require(
        scene.SetTransforms(std::array{*scene.Key(ancestor), *scene.Key(parent), *scene.Key(leaf)},
                            std::array{stretch, turn, runtime::Transform{1, 0, 2}}) &&
            scene.SetMeshRenderer(
                *scene.Key(leaf),
                runtime::MeshComponent{editor::MeshResourceId(assets.Entries()[0].id), {}}) &&
            scene.Select(std::array{leaf}),
        "frame mirrored/sheared hierarchy fixture failed");
    if (wide) {
      other = scene.Create("Other root");
      Require(scene.SetTransform(other, {-20, -5, -8}) && scene.Select(std::array{leaf}),
              "frame all root fixture failed");
    }
    Require(scene.Save(root / "Content/Frame.scene"), "frame baseline save failed");
    redo_entity = scene.Create("Retained Redo");
    Require(scene.Undo(), "frame Redo fixture failed");
    original = world.SaveScene(scene_id);
    ui.SetDisplay(1600, 1000, scale);
    ui.SetNativeScenePreview(true);
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int index = 0; index < 4; ++index)
      Draw();
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, &content, nullptr, nullptr, nullptr, nullptr,
                        nullptr, catalog);
    static_cast<void>(ui.EndFrame());
    if (consume_framing) {
      if (const auto request = ui.TakeNativeSceneFrameAllRequest()) {
        Require(request->project == active->Project().id &&
                    request->document_generation == scene.Generation(),
                "Frame all emitted a stale scope");
        const auto prepared = editor::preview::PrepareNativeSceneMeshes(
            scene, *catalog, content, content.Browser().ProjectGeneration());
        const auto candidates = editor::preview::NativeSceneProxyCandidates(scene, &prepared);
        static_cast<void>(ui.ApplyNativeSceneFrameAll(*request, candidates));
      }
    }
  }
  void Key(Nexora::Window::Key key) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Frame() {
    Access::FocusScene(ui);
    Draw();
    Key(Nexora::Window::Key::F);
  }
  editor::SceneFileToken RequestAll() {
    Require(!consume_framing, "request fixture consumed Frame all prematurely");
    Access::FocusScene(ui);
    Draw();
    Nexora::Window::WindowEvent key;
    key.type = Nexora::Window::WindowEventType::Key;
    key.value0 = static_cast<int>(Nexora::Window::Key::Home);
    key.value1 = 1;
    ui.ProcessEvents(std::array{key});
    Draw();
    const auto request = ui.TakeNativeSceneFrameAllRequest();
    Require(request.has_value(), "Home did not emit a current-frame request");
    key.value1 = 0;
    ui.ProcessEvents(std::array{key}); // Release is processed in the next GUI frame.
    return *request;
  }
  void ClickFrame(bool all = false) {
    const auto position = all ? Access::SceneFrameAllPosition(ui) : Access::SceneFramePosition(ui);
    Require(position.has_value(), "frame button absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*position)[0] * scale);
    pointer.value1 = static_cast<int>((*position)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Verify(double x, double y, double z, double radius) {
    const auto view = ui.NativeScenePreviewViewport();
    Require(view && view->width && view->height, "framed viewport absent");
    const double aspect = static_cast<double>(view->width) / view->height;
    const double half_fov = std::min(0.425, std::atan(std::tan(0.425) * aspect));
    const auto camera = ui.GetSceneOverviewCamera();
    const auto orbit = ui.GetNativeSceneOrbit();
    Require(Near(camera.x, x) && Near(camera.z, z) && Near(orbit.target_y, y) &&
                Near(orbit.distance, std::clamp(1.5 * radius / std::sin(half_fov), 2.0, 100.0)),
            "frame did not use exact world bounds and the narrower viewport FOV");
    Require(original == world.SaveScene(scene_id) && !scene.Dirty(),
            "camera navigation mutated scene content or dirty state");
  }
};
void RunAll(float dpi) {
  Fixture f(dpi, true);
  const double c = std::sqrt(0.5);
  // Independent union of the sheared mesh, rotated ancestor proxies and the other root proxy.
  const double x = (-20.45 + 10 + 2.25 * c) * 0.5;
  const double y = (-4.95 - 3 + 45 * c) * 0.5;
  const double z = (-8.45 + 6.5) * 0.5;
  const double radius =
      std::hypot((20.45 + 10 + 2.25 * c) * 0.5, (4.95 - 3 + 45 * c) * 0.5, (8.45 + 6.5) * 0.5);
  const auto home = [&] {
    Access::FocusScene(f.ui);
    f.Draw();
    f.Key(Nexora::Window::Key::Home);
  };
  home();
  f.Verify(x, y, z, radius);
  Require(std::ranges::equal(f.scene.Selection(), std::array{f.leaf}),
          "Home replaced the selected mesh with the entire scene");
  Require(f.ui.SetNativeSceneOrbit({0.3, 0.6, 25, 0}), "all button camera reset failed");
  f.ClickFrame(true);
  f.Verify(x, y, z, radius);
  Require(f.scene.Select(std::span<const runtime::Id>{}), "clear frame selection failed");
  f.active = &f.reader;
  Require(f.ui.SetNativeSceneOrbit({0.3, 0.6, 25, 0}), "readonly all camera reset failed");
  home();
  f.Verify(x, y, z, radius);
  Require(f.scene.Selection().empty(), "Frame all introduced selection into an empty selection");
  Access::FocusHierarchy(f.ui);
  f.Draw();
  Require(f.ui.SetNativeSceneOrbit({0.3, 0.6, 25, 0}), "panel gate camera reset failed");
  f.Key(Nexora::Window::Key::Home);
  Require(f.ui.GetNativeSceneOrbit().target_y == 0 && f.ui.GetNativeSceneOrbit().distance == 25,
          "Home in another panel navigated the scene");
  Access::FocusScene(f.ui);
  f.Draw();
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  f.Key(Nexora::Window::Key::Home);
  Require(f.ui.GetNativeSceneOrbit().target_y == 0 && f.ui.GetNativeSceneOrbit().distance == 25,
          "Home without application focus navigated the scene");
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus});
  f.active = &f.writer;
  Require(f.scene.Select(std::array{f.leaf}), "all drag selection failed");
  f.Draw();
  const auto viewport = f.ui.NativeScenePreviewViewport();
  Require(viewport.has_value(), "all drag viewport missing");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = viewport->x + viewport->width / 2;
  pointer.value1 = viewport->y + viewport->height / 2;
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer, button});
  f.Draw();
  f.Key(Nexora::Window::Key::Home);
  Require(f.ui.GetNativeSceneOrbit().target_y == 0 && f.ui.GetNativeSceneOrbit().distance == 25,
          "Home interrupted an active native Scene gesture");
  button.value1 = 0;
  pointer.value0 += static_cast<int>(40 * dpi);
  pointer.value1 += static_cast<int>(20 * dpi);
  Nexora::Window::WindowEvent home_key;
  home_key.type = Nexora::Window::WindowEventType::Key;
  home_key.value0 = static_cast<int>(Nexora::Window::Key::Home);
  home_key.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer, button, home_key});
  f.Draw();
  Require(f.ui.NativeSceneDrag().has_value() && f.ui.GetNativeSceneOrbit().target_y == 0 &&
              f.ui.GetNativeSceneOrbit().distance == 25,
          "Home navigated before a released Scene drag could commit");
  home_key.value1 = 0;
  f.ui.ProcessEvents(std::array{home_key});
  f.Draw();
  Require(f.scene.Select(std::span<const runtime::Id>{}), "restore empty all selection failed");
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Key(Nexora::Window::Key::Home);
  f.ClickFrame(true);
  Require(f.ui.GetNativeSceneOrbit().target_y == 0 && f.ui.GetNativeSceneOrbit().distance == 25,
          "Frame all bypassed the close modal");
  f.Key(Nexora::Window::Key::Escape);
  Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
          "frame all close modal did not cancel");
  home();
  const auto camera = f.ui.GetSceneOverviewCamera();
  const auto orbit = f.ui.GetNativeSceneOrbit();
  f.consume_framing = false;
  const std::array outside{editor::PickCandidate{f.other, {1000000, 0, 0}, {1000001, 1, 1}}};
  Require(!f.ui.ApplyNativeSceneFrameAll(f.RequestAll(), outside),
          "out-of-range submission bounds were accepted");
  Require(f.ui.GetSceneOverviewCamera().x == camera.x &&
              f.ui.GetSceneOverviewCamera().z == camera.z &&
              f.ui.GetNativeSceneOrbit().target_y == orbit.target_y &&
              f.ui.GetNativeSceneOrbit().distance == orbit.distance,
          "out-of-range Frame all partially published camera state");
  const std::array valid{editor::PickCandidate{f.other, {-1, -1, -1}, {1, 1, 1}}};
  auto stale = f.RequestAll();
  ++stale.document_generation;
  Require(!f.ui.ApplyNativeSceneFrameAll(stale, valid), "stale Frame all scope was accepted");
  const auto replay = f.RequestAll();
  f.Draw();
  Require(!f.ui.ApplyNativeSceneFrameAll(replay, valid), "Frame all request survived a GUI frame");
  const auto single = f.RequestAll();
  Require(f.ui.ApplyNativeSceneFrameAll(single, valid) &&
              !f.ui.ApplyNativeSceneFrameAll(single, outside),
          "Frame all did not consume its application intent exactly once");
  const std::array malformed{editor::PickCandidate{f.other, {2, 0, 0}, {1, 1, 1}}};
  Require(!f.ui.ApplyNativeSceneFrameAll(f.RequestAll(), malformed),
          "malformed Frame all bounds were accepted");
  const auto closing = f.RequestAll();
  f.ui.RequestCloseConfirmation();
  Require(!f.ui.ApplyNativeSceneFrameAll(closing, valid),
          "close confirmation did not invalidate a deferred frame request");
  f.Draw();
  f.Key(Nexora::Window::Key::Escape);
  Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
          "deferred frame close fixture did not cancel");
  f.consume_framing = true;
  f.ui.SetNativeScenePreview(false);
  f.active = &f.reader;
  Require(f.ui.SetSceneOverviewCamera({100, 100, 256}), "overview all camera reset failed");
  f.Draw();
  home();
  const auto overview = f.ui.GetSceneOverviewCamera();
  Require(Near(overview.x, -5) && Near(overview.z, -1.5) && overview.pixels_per_unit < 256 &&
              f.scene.Selection().empty() && f.original == f.world.SaveScene(f.scene_id) &&
              !f.scene.Dirty(),
          "overview Home did not fit all origins without authoring changes");
  Require(f.ui.SetSceneOverviewCamera({100, 100, 256}), "overview button camera reset failed");
  f.ClickFrame(true);
  Require(f.ui.GetSceneOverviewCamera().x == overview.x &&
              f.ui.GetSceneOverviewCamera().z == overview.z &&
              f.ui.GetSceneOverviewCamera().pixels_per_unit == overview.pixels_per_unit,
          "overview Frame all and Home used different bounds or canvas dimensions");
  Require(f.scene.Redo() && f.scene.Name(f.redo_entity) == "Retained Redo",
          "Frame all consumed authoring Redo");
  Require(f.scene.NewScene(), "empty frame fixture failed");
  const auto empty_camera = f.ui.GetSceneOverviewCamera();
  home();
  f.ClickFrame(true);
  Require(f.ui.GetSceneOverviewCamera().x == empty_camera.x &&
              f.ui.GetSceneOverviewCamera().z == empty_camera.z &&
              f.ui.GetSceneOverviewCamera().pixels_per_unit == empty_camera.pixels_per_unit,
          "empty overview Frame all changed the camera");
  f.ui.SetNativeScenePreview(true);
  f.Draw();
  const auto empty_orbit = f.ui.GetNativeSceneOrbit();
  home();
  f.ClickFrame(true);
  Require(f.ui.GetNativeSceneOrbit().target_y == empty_orbit.target_y &&
              f.ui.GetNativeSceneOrbit().distance == empty_orbit.distance,
          "empty native Frame all changed the camera");
}
void Run(float dpi) {
  Fixture f(dpi);
  const double c = std::sqrt(0.5);
  // Independent closed form: root T * scale(-2,3,.5) * Rz(45) * T(1,0,2),
  // applied to local bounds [4,2,-1]..[8,6,3]. The lossy world TRS gives a different center.
  const double x = 10 - 6 * c, y = -3 + 33 * c, z = 5.5;
  const double radius = std::sqrt(105.0); // half-extents (8c,12c,1)
  f.Frame();
  f.Verify(x, y, z, radius);
  const auto first = f.ui.GetNativeSceneOrbit();
  Require(f.ui.SetNativeSceneOrbit({0.3, 0.6, 25, 0}), "frame camera reset failed");
  f.ClickFrame();
  f.Verify(x, y, z, radius);
  Require(Near(f.ui.GetNativeSceneOrbit().distance, first.distance),
          "button and F used different viewport dimensions");
  // Read-only projects permit view navigation and use the same immutable CPU catalog.
  f.active = &f.reader;
  Require(f.ui.SetNativeSceneOrbit({0.3, 0.6, 25, 0}), "readonly camera reset failed");
  f.Frame();
  f.Verify(x, y, z, radius);
  // Resolve the latest published geometry each action; retaining an old snapshot must not win.
  auto entry = f.assets.Entries()[0];
  const auto replacement = editor::ImportObjMesh("v 4 2 -1\nv 5 2 -1\nv 4 3 0\nf 1 2 3\n");
  Require(replacement.geometry.has_value(), "replacement bounds fixture failed");
  entry.mesh = std::make_shared<const editor::MeshGeometry>(*replacement.geometry);
  Require(f.meshes.Publish(std::array{entry}, 7), "replacement bounds publication failed");
  f.Frame();
  const auto wide_distance = f.ui.GetNativeSceneOrbit().distance;
  f.ui.SetDisplay(1600, 2000, dpi);
  f.Draw();
  f.Frame();
  // Center (4.5,2.5,-.5), half-extents (.5,.5,.5) before ancestry.
  f.Verify(10 - 6 * c, -3 + 24 * c, 4.75, std::sqrt(6.5625));
  Require(f.ui.NativeScenePreviewViewport()->width < f.ui.NativeScenePreviewViewport()->height,
          "portrait viewport fixture is not narrow");
  Require(f.ui.GetNativeSceneOrbit().distance > wide_distance * 1.5,
          "narrow canvas did not back the camera away from the same geometry");
  // A selected group includes descendants once, even when a descendant is also selected.
  Require(f.scene.Select(std::array{f.ancestor}), "frame group selection failed");
  f.Frame();
  const auto group_camera = f.ui.GetSceneOverviewCamera();
  const auto group_orbit = f.ui.GetNativeSceneOrbit();
  Require(group_orbit.target_y > 2, "frame group ignored its authored child");
  Require(f.scene.Select(std::array{f.ancestor, f.leaf}), "overlapping frame selection failed");
  f.Frame();
  Require(f.ui.GetSceneOverviewCamera().x == group_camera.x &&
              f.ui.GetNativeSceneOrbit().target_y == group_orbit.target_y &&
              f.ui.GetNativeSceneOrbit().distance == group_orbit.distance,
          "selected descendants weighted framing bounds twice");
  Require(f.scene.Select(std::array{f.leaf}), "fallback selection failed");
  editor::MeshAssetCatalog stale;
  Require(stale.Publish(std::array{entry}, 8), "stale frame catalog fixture failed");
  f.catalog = &stale;
  f.Frame();
  f.Verify(10 - 2 * c, -3 + 3 * c + 0.5, 5, std::sqrt(5.113125));
  const auto fallback = f.ui.GetNativeSceneOrbit();
  auto malformed = std::make_shared<editor::MeshGeometry>(*entry.mesh);
  malformed->minimum[0] = malformed->maximum[0] + 1;
  entry.mesh = malformed;
  Require(f.meshes.Publish(std::array{entry}, 7), "malformed bounds catalog fixture failed");
  f.catalog = &f.meshes;
  f.Frame();
  f.Verify(10 - 2 * c, -3 + 3 * c + 0.5, 5, std::sqrt(5.113125));
  Access::FocusHierarchy(f.ui);
  f.Draw();
  f.Key(Nexora::Window::Key::F);
  Require(f.ui.GetNativeSceneOrbit().target_y == fallback.target_y &&
              f.ui.GetNativeSceneOrbit().distance == fallback.distance,
          "F from another panel navigated the Scene camera");
  Require(f.ui.SetNativeSceneOrbit({0.3, 0.6, 25, 0}), "modal camera reset failed");
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Key(Nexora::Window::Key::F);
  Require(f.ui.GetNativeSceneOrbit().target_y == 0 && f.ui.GetNativeSceneOrbit().distance == 25,
          "F during close confirmation navigated the Scene camera");
  f.Key(Nexora::Window::Key::Escape);
  Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
          "close modal did not cancel");
  f.Frame();
  const auto center = f.ui.GetSceneOverviewCamera();
  runtime::WorldCommandBuffer outside;
  outside.SetTransform(f.leaf, {1000000, 0, 2});
  Require(outside.Apply(f.world), "out-of-range frame fixture failed");
  const auto unchanged_world = f.world.SaveScene(f.scene_id);
  f.Frame();
  Require(f.ui.GetSceneOverviewCamera().x == center.x &&
              f.ui.GetSceneOverviewCamera().z == center.z &&
              f.ui.GetNativeSceneOrbit().target_y == fallback.target_y &&
              f.ui.GetNativeSceneOrbit().distance == fallback.distance &&
              f.world.SaveScene(f.scene_id) == unchanged_world,
          "rejected bounds partially published camera state or modified the World");
  Require(f.scene.Redo() && f.scene.Name(f.redo_entity) == "Retained Redo",
          "framing consumed authoring Redo");
}
void RunBudget() {
  Fixture f(1);
  constexpr std::size_t count = editor::imgui::kMaximumNativeSceneFrameCandidates + 2;
  const auto path = f.root / "Content/Budget.scene";
  {
    std::ofstream source(path);
    source << "NEXORA_EDITOR_SCENE 2\n";
    for (std::size_t index = 0; index < count; ++index)
      source << "node " << index + 100 << " 0 Budget " << index << '\n';
    source << "world\nNEXORA_SCENE 3 \"Budget\" 0 " << count << '\n';
    for (std::size_t index = 0; index < count; ++index) {
      const double x = index == 0 ? 1000000 : index == count - 1 ? 10000 : 0;
      source << index + 100 << " 0 " << x << " 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
    }
  }
  Require(f.scene.Reload(path), "large native framing fixture failed");
  f.original = f.world.SaveScene(f.scene_id);
  const auto prepared = editor::preview::PrepareNativeSceneMeshes(f.scene, f.meshes, f.content, 7);
  const auto candidates = editor::preview::NativeSceneProxyCandidates(f.scene, &prepared);
  Require(candidates.size() == editor::imgui::kMaximumNativeSceneFrameCandidates &&
              candidates.front().entity == 101 && candidates.back().entity == 4099,
          "native candidate budget counted an excluded transform or included a hidden tail");
  Access::FocusScene(f.ui);
  f.Draw();
  f.Key(Nexora::Window::Key::Home);
  f.Verify(0, 0.5, 0, std::sqrt(0.6075));
  Require(f.scene.Selection().empty(), "bounded Frame all changed selection");
  f.consume_framing = false;
  auto oversized = candidates;
  oversized.push_back({4100, {9999.55, 0.05, -0.45}, {10000.45, 0.95, 0.45}});
  const auto camera = f.ui.GetSceneOverviewCamera();
  const auto orbit = f.ui.GetNativeSceneOrbit();
  Require(!f.ui.ApplyNativeSceneFrameAll(f.RequestAll(), oversized) &&
              f.ui.GetSceneOverviewCamera().x == camera.x &&
              f.ui.GetNativeSceneOrbit().target_y == orbit.target_y &&
              f.ui.GetNativeSceneOrbit().distance == orbit.distance,
          "oversized submission packet partially framed invisible tail geometry");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    RunAll(1);
    RunAll(2);
    RunBudget();
    std::cout << "Scene mesh frame selection contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
