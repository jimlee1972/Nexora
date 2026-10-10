#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"
#include <bit>
#include <chrono>
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
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-reflected-ui-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace writer, reader;
  std::string error;
  Require(writer.Create(root, "Reflected UI", &error) &&
              reader.Open(root, editor::ProjectAccess::ReadOnly, &error),
          "workspace failed");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Reflected UI"));
  const auto a = scene.Create("A"), b = scene.Create("B");
  const std::array keys{*scene.Key(a), *scene.Key(b)};
  editor::OpaqueComponent payload{91, "Plugin.Properties", std::vector<std::uint8_t>(24)};
  payload.data[23] = 255;
  Require(scene.SetOpaqueComponent(keys[0], payload), "first payload failed");
  payload.data[0] = 1;
  Require(scene.SetOpaqueComponent(keys[1], payload) && scene.Select(keys), "mixed payload failed");
  editor::ReflectedInspector metadata;
  Require(metadata.SetComponents({{91,
                                   "Plugin.Properties",
                                   24,
                                   {{"enabled", editor::ReflectedKind::Boolean, 0},
                                    {"nested.amount", editor::ReflectedKind::Number, 8}}}}),
          "metadata failed");
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  Require(ui.SetReflectedInspector(metadata), "graphical metadata rejected");
  ui.SetDisplay(1600, 1200, scale);
  Access::ConfigureSyntheticInput(ui);
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  auto *workspace = &writer;
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  Access::FocusInspector(ui);
  draw();
  draw();
  Require(Access::ReflectedPropertyMixed(ui, "enabled") == true, "actual mixed state absent");
  const auto click = [&](std::string_view path) {
    const auto point = Access::ReflectedPropertyPosition(ui, path);
    Require(point.has_value(), "actual property control absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    draw();
    draw();
  };
  click("enabled");
  Require(scene.OpaqueComponents(keys[0])->front().data[0] == 1 &&
              scene.OpaqueComponents(keys[1])->front().data[0] == 1 &&
              Access::ReflectedPropertyMixed(ui, "enabled") == false && scene.Undo() &&
              scene.OpaqueComponents(keys[0])->front().data[0] == 0 &&
              scene.OpaqueComponents(keys[1])->front().data[0] == 1 && scene.Redo(),
          "actual atomic Boolean edit/Undo failed");
  draw();
  const auto key = [&](Nexora::Window::Key code, Nexora::Window::KeyModifiers modifiers = {}) {
    Nexora::Window::WindowEvent e;
    e.type = Nexora::Window::WindowEventType::Key;
    e.value0 = static_cast<int>(code);
    e.value1 = 1;
    e.modifiers = modifiers;
    ui.ProcessEvents(std::array{e});
    draw();
    e.value1 = 0;
    e.modifiers = {};
    ui.ProcessEvents(std::array{e});
    draw();
  };
  const auto type = [&](std::string_view value) {
    key(Nexora::Window::Key::A, Nexora::Window::KeyModifiers::Control);
    for (unsigned char character : value) {
      Nexora::Window::WindowEvent e;
      e.type = Nexora::Window::WindowEventType::Text;
      e.value0 = character;
      ui.ProcessEvents(std::array{e});
      draw();
    }
  };
  click("nested.amount");
  type("7.25");
  Require(metadata.Inspect(scene, keys, 91)->properties[1].value == editor::ReflectedValue{0.0},
          "draft mutated bytes before Enter");
  key(Nexora::Window::Key::Enter);
  const auto result = metadata.Inspect(scene, keys, 91);
  Require(result && result->properties[1].value == editor::ReflectedValue{7.25} &&
              !result->properties[1].mixed &&
              scene.OpaqueComponents(keys[0])->front().data[23] == 255 && scene.Undo() &&
              metadata.Inspect(scene, keys, 91)->properties[1].value == editor::ReflectedValue{0.0},
          "actual number commit/Undo/padding failed");
  draw();
  workspace = &reader;
  draw();
  const auto before = scene.OpaqueComponents(keys[0]);
  click("enabled");
  Require(scene.OpaqueComponents(keys[0]) == before, "read-only control mutated bytes");
  workspace = &writer;
  draw();
  click("nested.amount");
  type("99");
  Require(ui.SetReflectedInspector(metadata), "replacement metadata failed");
  draw();
  key(Nexora::Window::Key::Enter);
  Require(scene.OpaqueComponents(keys[0]) == before, "metadata replacement revived an old draft");
  // Exercise every remaining actual adapter, rather than only decoding fixture bytes.
  const auto typed = scene.Create("Typed");
  const auto typed_key = *scene.Key(typed);
  Require(scene.Select(std::array{typed_key}), "typed selection failed");
  struct Case {
    editor::ReflectedKind kind;
    std::string text;
    editor::ReflectedValue expected;
  };
  const std::array cases{
      Case{editor::ReflectedKind::Integer, "-9223372036854775808",
           std::int64_t{std::numeric_limits<std::int64_t>::min()}},
      Case{editor::ReflectedKind::Unsigned, "18446744073709551615",
           std::uint64_t{std::numeric_limits<std::uint64_t>::max()}},
      Case{editor::ReflectedKind::EntityReference, "18446744073709551615",
           std::uint64_t{std::numeric_limits<std::uint64_t>::max()}},
      Case{editor::ReflectedKind::AssetReference, "12345678-1234-5678-1234-567812345678",
           foundation::Uuid::Parse("12345678-1234-5678-1234-567812345678").Value()},
      Case{editor::ReflectedKind::Vector2, "1.25, -2.5", std::array<double, 4>{1.25, -2.5, 0, 0}},
      Case{editor::ReflectedKind::Vector3, "1, 2, 3", std::array<double, 4>{1, 2, 3, 0}},
      Case{editor::ReflectedKind::Vector4, "1, 2, 3, 4", std::array<double, 4>{1, 2, 3, 4}},
      Case{editor::ReflectedKind::Color, "0.25, 0.5, 0.75, 1",
           std::array<double, 4>{.25, .5, .75, 1}},
      Case{editor::ReflectedKind::Number, "3.125", 3.125}};
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto &test = cases[index];
    const auto id = static_cast<runtime::TypeId>(200 + index);
    editor::OpaqueComponent component{id, "Typed.Component", std::vector<std::uint8_t>(256)};
    component.data[255] = 99;
    Require(scene.SetOpaqueComponent(typed_key, component), "typed component failed");
    const std::size_t elements = index + 1 == cases.size() ? 2 : 1;
    Require(metadata.SetComponents(
                {{id, "Typed.Component", 256, {{"nested.field", test.kind, 0, elements, {}}}}}) &&
                ui.SetReflectedInspector(metadata),
            "typed metadata failed");
    draw();
    draw();
    Access::FocusInspector(ui);
    draw();
    const std::string path = elements == 2 ? "nested.field[1]" : "nested.field";
    click(path);
    type(test.text);
    key(Nexora::Window::Key::Enter);
    const auto view = metadata.Inspect(scene, std::array{typed_key}, id);
    const std::size_t field = elements == 2 ? 1 : 0;
    Require(view && view->properties[field].value == test.expected &&
                scene.OpaqueComponents(typed_key)->back().data[255] == 99,
            "actual reflected typed adapter failed");
    Require(scene.Undo() &&
                metadata.Inspect(scene, std::array{typed_key}, id)->properties[field].value !=
                    test.expected &&
                scene.Redo(),
            "typed widget lost its independent Undo boundary");
  }
  for (const auto kind : {editor::ReflectedKind::Enum, editor::ReflectedKind::Flags}) {
    const runtime::TypeId id = kind == editor::ReflectedKind::Enum ? 301 : 302;
    editor::OpaqueComponent component{id, "Typed.Choice", std::vector<std::uint8_t>(16)};
    Require(scene.SetOpaqueComponent(typed_key, component), "choice component failed");
    const std::vector<editor::ReflectedChoice> choices =
        kind == editor::ReflectedKind::Enum
            ? std::vector<editor::ReflectedChoice>{{0, "Off"}, {1, "On"}}
            : std::vector<editor::ReflectedChoice>{{1, "Read"}, {2, "Write"}};
    Require(metadata.SetComponents({{id, "Typed.Choice", 16, {{"choice", kind, 0, 1, choices}}}}) &&
                ui.SetReflectedInspector(metadata),
            "choice metadata failed");
    draw();
    draw();
    click("choice");
    const auto point = Access::ReflectedChoicePosition(
        ui, "choice", kind == editor::ReflectedKind::Enum ? "On" : "Read");
    Require(point.has_value(), "actual popup choice absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    draw();
    key(Nexora::Window::Key::Escape);
    Require(metadata.Inspect(scene, std::array{typed_key}, id)->properties[0].value ==
                    editor::ReflectedValue{std::uint64_t{1}} &&
                scene.Undo() &&
                metadata.Inspect(scene, std::array{typed_key}, id)->properties[0].value ==
                    editor::ReflectedValue{std::uint64_t{0}},
            "actual enum/flags selection and Undo failed");
  }
  editor::OpaqueComponent flags_a{303, "Mixed.Flags", std::vector<std::uint8_t>(16)};
  flags_a.data[0] = 1;
  flags_a.data[14] = 255;
  auto flags_b = flags_a;
  flags_b.data[0] = 2;
  Require(scene.SetOpaqueComponent(keys[0], flags_a) &&
              scene.SetOpaqueComponent(keys[1], flags_b) && scene.Select(keys) &&
              metadata.SetComponents({{303,
                                       "Mixed.Flags",
                                       16,
                                       {{"permissions",
                                         editor::ReflectedKind::Flags,
                                         0,
                                         1,
                                         {{1, "Read"}, {2, "Write"}}}}}}) &&
              ui.SetReflectedInspector(metadata),
          "Mixed flags controls fixture failed");
  draw();
  draw();
  const auto choose_flag = [&](std::string_view label) {
    click("permissions");
    const auto point = Access::ReflectedChoicePosition(ui, "permissions", label);
    Require(point.has_value(), "Mixed flags popup choice absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    draw();
    key(Nexora::Window::Key::Escape);
  };
  const auto masks = [&] {
    return std::array{scene.OpaqueComponents(keys[0])->back().data[0],
                      scene.OpaqueComponents(keys[1])->back().data[0]};
  };
  choose_flag("Write");
  Require(masks() == std::array<std::uint8_t, 2>{3, 2} && scene.Undo() &&
              masks() == std::array<std::uint8_t, 2>{1, 2} && scene.Redo(),
          "Mixed flags click copied another entity's unrelated bits");
  draw();
  choose_flag("Read");
  Require(masks() == std::array<std::uint8_t, 2>{2, 2} && scene.Undo() &&
              masks() == std::array<std::uint8_t, 2>{3, 2} && scene.Undo() &&
              masks() == std::array<std::uint8_t, 2>{1, 2} &&
              scene.OpaqueComponents(keys[0])->back().data[14] == 255 &&
              scene.OpaqueComponents(keys[1])->back().data[14] == 255,
          "Mixed flags clear lost unrelated flags/padding or one-step Undo");
  Require(scene.Select(std::span<const runtime::Id>{}), "deselect failed");
  draw();
  Require(!Access::ReflectedPropertyPosition(ui, "enabled"), "deselection retained old controls");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Actual reflected Inspector controls passed at 1x/2x\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
