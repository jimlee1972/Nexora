#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <iostream>
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
      ("nexora-builtin-create-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  runtime::World world;
  runtime::Id id = world.LoadScene("Builtin creation UI");
  editor::SceneDocument scene{world, id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::Id parent{};
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Builtin creation") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(id),
            "Builtin UI workspace failed");
    parent = scene.Create("Parent");
    Require(scene.SetTransform(parent, {12, 3, -5, 0, 0, 0, 1, -2, 3, 4}) &&
                scene.Select(std::array{*scene.Key(parent)}) && scene.Save(Path()),
            "Builtin UI parent fixture failed");
    ui.SetDisplay(1600, 1000, dpi);
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusHierarchy(ui);
    Draw();
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  std::filesystem::path Path() const { return root / "Content/Main.scene"; }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::size_t control, bool apply = true) {
    const auto point = Access::HierarchyCreatePosition(ui, control);
    Require(point.has_value(), "Builtin creation UI control absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    if (apply)
      Draw();
  }
  void Choose(bool camera) {
    Click(0);
    Click(camera ? 2 : 3);
  }
  void Key(Nexora::Window::Key value, Nexora::Window::KeyModifiers modifiers = {}) {
    Nexora::Window::WindowEvent key;
    key.type = Nexora::Window::WindowEventType::Key;
    key.value0 = static_cast<int>(value);
    key.value1 = 1;
    key.modifiers = modifiers;
    ui.ProcessEvents(std::array{key});
    Draw();
    key.value1 = 0;
    key.modifiers = {};
    ui.ProcessEvents(std::array{key});
    Draw();
  }
  void CreateKey() {
    Key(Nexora::Window::Key::N, static_cast<Nexora::Window::KeyModifiers>(
                                    static_cast<unsigned>(Nexora::Window::KeyModifiers::Control) |
                                    static_cast<unsigned>(Nexora::Window::KeyModifiers::Shift)));
  }
  void CustomName(std::string_view name) {
    Click(6);
    Key(Nexora::Window::Key::A, Nexora::Window::KeyModifiers::Control);
    for (char character : name) {
      Nexora::Window::WindowEvent text;
      text.type = Nexora::Window::WindowEventType::Text;
      text.value0 = character;
      ui.ProcessEvents(std::array{text});
      Draw();
    }
  }
  runtime::Id Verify(bool camera, bool child, std::string_view name = {}) {
    Require(scene.Selection().size() == 1, "Builtin creation did not select the created node");
    const auto created = scene.Selection().front();
    const auto key = *scene.Key(created);
    Require(created != parent && scene.Parent(created) == (child ? parent : 0) &&
                scene.Transform(created) == runtime::Transform{} &&
                scene.Name(created) == (name.empty() ? (camera ? "Camera" : "Light") : name),
            "Builtin creation lost local pose, name or parent");
    Require(camera ? scene.Camera(key) && scene.Camera(key)->vertical_field_of_view == 60 &&
                         scene.Camera(key)->near_plane == 0.1 &&
                         scene.Camera(key)->far_plane == 1000 && !scene.Light(key)
                   : scene.Light(key) && scene.Light(key)->intensity == 1 && !scene.Camera(key),
            "Builtin creation did not initialize the chosen component");
    return created;
  }
};
void Run(float dpi, bool camera, bool child, bool keyboard = false) {
  Fixture f(dpi);
  const auto before = f.world.SaveScene(f.id);
  f.Choose(camera);
  Require(f.world.SaveScene(f.id) == before && !f.scene.Dirty(),
          "Choosing an entity type changed World or history");
  if (keyboard)
    f.CreateKey();
  else
    f.Click(child ? 5 : 4);
  const auto created = f.Verify(camera, child);
  const auto initialized = f.world.SaveScene(f.id);
  Require(f.scene.Undo() && f.world.SaveScene(f.id) == before && !f.scene.Key(created) &&
              !f.scene.Dirty() && f.scene.Redo() && f.world.SaveScene(f.id) == initialized &&
              f.scene.Name(created) == (camera ? "Camera" : "Light"),
          "Builtin UI creation was not one complete stable-ID Undo step");
  Require(f.scene.Save(f.Path()) && f.scene.Reload(f.Path()) &&
              f.scene.Select(std::array{*f.scene.Key(created)}),
          "Builtin UI persistence failed");
  f.Draw();
  f.Verify(camera, child);
}
void RunCustomName(float dpi, bool camera, std::string_view name) {
  Fixture f(dpi);
  const auto before = f.world.SaveScene(f.id);
  f.CustomName(name);
  f.Choose(!camera);
  f.Choose(camera);
  Require(f.world.SaveScene(f.id) == before && !f.scene.Dirty(),
          "Name/type authoring preferences changed World or history");
  f.Click(4);
  f.Verify(camera, false, name);
}
void RunAccess(float dpi, bool camera) {
  Fixture f(dpi);
  f.Choose(camera);
  const auto before = f.world.SaveScene(f.id);
  f.active = &f.reader;
  f.Draw();
  f.Click(4);
  f.Click(5);
  f.CreateKey();
  Require(f.world.SaveScene(f.id) == before && !f.scene.Dirty(),
          "Read-only builtin controls admitted creation");
  f.active = &f.writer;
  f.Draw();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Click(4);
  f.Click(5);
  f.CreateKey();
  Require(f.world.SaveScene(f.id) == before && !f.scene.Dirty(),
          "Close modal admitted builtin creation");
}
void RunStaleDocument(float dpi, bool camera) {
  Fixture f(dpi);
  f.Choose(camera);
  f.Click(4, false);
  const auto before = f.world.SaveScene(f.id);
  Require(f.scene.Reload(f.Path()), "Stale root document fixture failed");
  f.Draw();
  Require(f.world.SaveScene(f.id) == before && f.scene.Nodes().size() == 1 && !f.scene.Dirty() &&
              !f.scene.Undo(),
          "Queued root creation survived reload and mutated the replacement document");
}
void RunStaleParent(float dpi, bool camera) {
  Fixture f(dpi);
  f.Choose(camera);
  f.Click(5, false); // Request is captured; the next frame revalidates its parent generation.
  Require(f.scene.DeleteSelection(), "Stale builtin parent fixture failed");
  const auto deleted = f.world.SaveScene(f.id);
  f.Draw();
  Require(f.world.SaveScene(f.id) == deleted && f.scene.Nodes().empty() && f.scene.Undo() &&
              f.scene.Key(f.parent),
          "Builtin request admitted a deleted parent or consumed Undo");
}
} // namespace
int main() {
  try {
    for (const auto dpi : {1.0F, 2.0F})
      for (const bool camera : {true, false}) {
        Run(dpi, camera, false);
        Run(dpi, camera, true);
        Run(dpi, camera, false, true);
        RunCustomName(dpi, camera, "Authored Camera");
        RunCustomName(dpi, camera, "Camera");
        RunAccess(dpi, camera);
        RunStaleParent(dpi, camera);
        RunStaleDocument(dpi, camera);
      }
    std::cout << "Camera/Light Hierarchy creation input contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
