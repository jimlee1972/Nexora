#include "../../Engine/Editor/src/ReflectedArrayTestAccess.h"
#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/SceneFiles.h"
#include "TemporaryDirectoryCleanup.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Put(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i)
    bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint64_t Get(std::span<const std::uint8_t> bytes, std::size_t offset) {
  std::uint64_t value{};
  for (std::size_t i = 0; i < 8; ++i)
    value |= std::uint64_t{bytes[offset + i]} << (8 * i);
  return value;
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(bool(input), "Scene file read failed");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
struct Fixture final {
  static constexpr runtime::TypeId kType = 301;
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-array-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  const std::filesystem::path path = std::filesystem::path(u8"Content/陣列.scene");
  editor::ProjectWorkspace writer, reader;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Array UI");
  editor::SceneDocument scene{world, scene_id};
  std::array<editor::SceneDocument::NodeKey, 2> keys;
  std::unique_ptr<editor::SceneFileSession> files;
  editor::SceneFileSession *active_files{};
  editor::ProjectWorkspace *workspace = &writer;
  editor::ReflectedInspector metadata;
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Array UI") && reader.Open(root, editor::ProjectAccess::ReadOnly) &&
                world.Activate(scene_id),
            "Project/World fixture failed");
    keys = {*scene.Key(scene.Create("A")), *scene.Key(scene.Create("B"))};
    for (std::size_t t = 0; t < 2; ++t) {
      const auto count = 2 - t;
      editor::OpaqueComponent source{kType, "Plugin.Arrays",
                                     std::vector<std::uint8_t>(32 + count * 8 + 16)};
      Put(source.data, 0, count);
      Put(source.data, 8, 2);
      source.data[16] = static_cast<std::uint8_t>(t);
      source.data[31] = 255;
      for (std::size_t i = 0; i < count; ++i)
        Put(source.data, 32 + i * 8, std::bit_cast<std::uint64_t>(double(i + 1 + 2 * t)));
      Put(source.data, 32 + count * 8, 7);
      Put(source.data, 40 + count * 8, 9);
      Require(scene.SetOpaqueComponent(keys[t], source) &&
                  scene.SetOpaqueComponent(keys[t], {999, "Missing.Type", {0, 255, 1, 127, 42}}),
              "Array fixture bytes failed");
    }
    Require(scene.Select(keys), "Initial multi-selection failed");
    const auto catalog_path = root / ".nexora/inspector.reflection";
    {
      std::ofstream output(catalog_path, std::ios::binary);
      output << "NXEDITORREFLECTION 2\ntype 301 \"Plugin.Arrays\" 32 1 2\n"
                "field \"enabled\" bool 16 1 0\narray \"samples\" double 0 4 0\n"
                "array \"ids\" uint64 8 3 0\n";
      Require(bool(output), "Version2 fixture metadata write failed");
    }
    auto loaded = editor::ReflectedInspector::LoadProject(root);
    Require(loaded.has_value(), "Actual version2 project metadata load failed");
    metadata = std::move(*loaded);
    Require(ui.SetReflectedInspector(metadata), "UI catalog admission failed");
    files = std::make_unique<editor::SceneFileSession>(writer, scene);
    active_files = files.get();
    Require(files->SaveAs(files->Token(), path).Applied(),
            "Initial production Scene SaveAs failed");
    ui.SetDisplay(1600 * scale, 1200 * scale, scale);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusInspector(ui);
    Draw();
    Draw();
  }
  std::string Draw(bool capture = false) {
    ui.SetSceneFileContext(active_files->Token(), active_files->CurrentPath(),
                           active_files->SaveBlocked());
    ui.BeginFrame();
    if (capture)
      Access::BeginTextCapture(ui);
    ui.DrawProductShell(shell, &scene, workspace);
    std::string text;
    if (capture)
      text = Access::EndTextCapture(ui);
    Require(ui.EndFrame().vertices > 0, "Actual array UI produced no draw data");
    return text;
  }
  auto Snapshot() {
    return std::array{*scene.OpaqueComponents(keys[0]), *scene.OpaqueComponents(keys[1])};
  }
  editor::OpaqueComponent Bytes(std::size_t t) {
    const auto components = scene.OpaqueComponents(keys[t]);
    return *std::ranges::find(*components, kType, &editor::OpaqueComponent::type);
  }
  void Conserve() {
    for (std::size_t t = 0; t < 2; ++t) {
      const auto bytes = Bytes(t).data;
      const auto count = Get(bytes, 0);
      Require(bytes[31] == 255 && bytes[16] == t && Get(bytes, 32 + count * 8) == 7 &&
                  Get(bytes, 40 + count * 8) == 9,
              "Array edit changed prefix/other tail");
      const auto components = scene.OpaqueComponents(keys[t]);
      const auto missing = std::ranges::find(*components, 999, &editor::OpaqueComponent::type);
      Require(missing != components->end() &&
                  missing->data == std::vector<std::uint8_t>({0, 255, 1, 127, 42}),
              "Array edit lost unavailable component");
    }
  }
  void Event(Key key, bool down, Mods mods = Mods::None) {
    Nexora::Window::WindowEvent e;
    e.type = Nexora::Window::WindowEventType::Key;
    e.value0 = static_cast<int>(key);
    e.value1 = down;
    e.modifiers = mods;
    ui.ProcessEvents(std::array{e});
    Draw();
  }
  void Tap(Key key, Mods mods = Mods::None) {
    if (mods == Mods::Control)
      Event(Key::LeftControl, true, mods);
    Event(key, true, mods);
    Event(key, false, mods);
    if (mods == Mods::Control)
      Event(Key::LeftControl, false);
  }
  void Click(const std::optional<std::array<float, 2>> &point) {
    Require(point.has_value(), "Actual array control is absent/clipped");
    Nexora::Window::WindowEvent p, b;
    p.type = Nexora::Window::WindowEventType::Pointer;
    p.value0 = static_cast<int>((*point)[0] * scale);
    p.value1 = static_cast<int>((*point)[1] * scale);
    b.type = Nexora::Window::WindowEventType::PointerButton;
    b.value0 = 0;
    b.value1 = 1;
    ui.ProcessEvents(std::array{p, b});
    Draw();
    b.value1 = 0;
    ui.ProcessEvents(std::array{b});
    Draw();
    Draw();
  }
  void Control(std::string_view action, std::optional<std::size_t> element = {}) {
    Click(Access::ReflectedArrayControlPosition(ui, kType, "samples", action, element));
  }
  void Draft(std::string_view text) {
    Click(Access::ReflectedPropertyPosition(ui, "samples[0]"));
    Tap(Key::A, Mods::Control);
    const auto before = Snapshot();
    for (const unsigned char c : text) {
      Nexora::Window::WindowEvent e;
      e.type = Nexora::Window::WindowEventType::Text;
      e.value0 = c;
      ui.ProcessEvents(std::array{e});
      Draw();
      Require(Snapshot() == before, "Typing partially committed array bytes");
    }
  }
  template <typename Check>
  void Cycle(std::string_view action, std::optional<std::size_t> element, Check check) {
    const auto before = Snapshot();
    const auto dirty = scene.Dirty();
    Control(action, element);
    Require(Snapshot() != before, "Actual structural control published no change");
    Conserve();
    check();
    const auto after = Snapshot();
    Tap(Key::Z, Mods::Control);
    Require(Snapshot() == before && scene.Dirty() == dirty,
            "Actual Undo did not restore one complete array batch");
    Tap(Key::Y, Mods::Control);
    Require(Snapshot() == after, "Actual Redo lost array batch");
    Tap(Key::Z, Mods::Control);
    Require(Snapshot() == before, "Actual repeated Undo failed");
  }
};
void Run(float scale) {
  Fixture f(scale);
  const auto original = f.Snapshot();
  const auto original_file = Read(f.root / f.path);
  Require(f.Draw(true).find("Mixed lengths (1..2)") != std::string::npos &&
              Access::ReflectedPropertyMixed(f.ui, "samples[0]") == true,
          "Actual mixed array count/value missing");
  f.Control("Down", 0);
  f.Control("Up", 0);
  Require(f.Snapshot() == original, "Invalid reorder controls changed source");
  f.Cycle("Add", {}, [&] {
    Require(Get(f.Bytes(0).data, 0) == 3 && Get(f.Bytes(1).data, 0) == 2,
            "Actual Add did not use each target's length");
  });
  f.Cycle("Insert", 0, [&] {
    Require(Get(f.Bytes(0).data, 32) == 0 && Get(f.Bytes(1).data, 32) == 0,
            "Actual Insert did not add a valid typed default");
  });
  f.Cycle("Remove", 0, [&] {
    Require(Get(f.Bytes(0).data, 0) == 1 && Get(f.Bytes(1).data, 0) == 0 &&
                !Access::ReflectedPropertyPosition(f.ui, "samples[0]"),
            "Actual Remove did not retain complete empty/mixed lengths");
  });
  f.Cycle("Clear", {}, [&] {
    Require(Get(f.Bytes(0).data, 0) == 0 && Get(f.Bytes(1).data, 0) == 0,
            "Actual Clear did not remove complete tails");
  });
  f.Control("Add");
  f.Cycle("Down", 0, [&] {
    Require(Get(f.Bytes(0).data, 32) == std::bit_cast<std::uint64_t>(2.0) &&
                Get(f.Bytes(1).data, 32) == 0,
            "Actual Down changed a partial value/only one target");
  });
  f.Cycle("Up", 1, [&] {
    Require(Get(f.Bytes(0).data, 32) == std::bit_cast<std::uint64_t>(2.0) &&
                Get(f.Bytes(1).data, 32) == 0,
            "Actual Up changed a partial value/only one target");
  });
  f.Tap(Key::Z, Mods::Control);
  Require(f.Snapshot() == original && !f.scene.Dirty(), "Add setup was not one Undo");
  f.Control("Add");
  f.Control("Add");
  const auto at_capacity = f.Snapshot();
  f.Control("Add");
  Require(f.Snapshot() == at_capacity, "Over-capacity control partially added to shorter target");
  f.Tap(Key::Z, Mods::Control);
  f.Tap(Key::Z, Mods::Control);
  Require(f.Snapshot() == original, "Capacity Undo did not restore original arrays");
  editor::ReflectedArrayTestAccess::FailAfterReplacements(1);
  f.Control("Add");
  Require(f.Snapshot() == original && f.Draw(true).find("Array edit rejected") != std::string::npos,
          "Actual partial-allocation failure changed source or hid rejection");
  f.Draft("-12.5");
  f.Tap(Key::Enter);
  f.Conserve();
  Require(Get(f.Bytes(0).data, 32) == std::bit_cast<std::uint64_t>(-12.5) &&
              Get(f.Bytes(1).data, 32) == std::bit_cast<std::uint64_t>(-12.5) &&
              Access::ReflectedPropertyMixed(f.ui, "samples[0]") == false,
          "Actual mixed element commit failed");
  const auto edited = f.Snapshot();
  f.Tap(Key::Z, Mods::Control);
  Require(f.Snapshot() == original, "Element Undo failed");
  f.Tap(Key::Y, Mods::Control);
  Require(f.Snapshot() == edited, "Element Redo failed");
  Require(Read(f.root / f.path) == original_file, "Authoring controls wrote a file before Save");
  const auto token = f.files->Token();
  f.Tap(Key::S, Mods::Control);
  Require(f.ui.TakeSceneSaveRequest() && f.files->Save(token).Applied() && !f.scene.Dirty(),
          "Actual Save did not publish through the scoped production service");
  const auto saved = Read(f.root / f.path);
  f.Draft("99.25");
  f.Tap(Key::Escape);
  Require(f.Snapshot() == edited && Read(f.root / f.path) == saved,
          "Cancelled element draft changed bytes");
  f.Draft("nan");
  f.Tap(Key::Enter);
  Require(f.Snapshot() == edited, "Invalid typed value changed bytes");
  f.Draft("77");
  Require(f.ui.SetReflectedInspector(f.metadata), "Metadata replacement failed");
  f.Draw();
  f.Draw();
  f.Tap(Key::Enter);
  Require(f.Snapshot() == edited, "Replaced metadata revived an old array draft");
  f.Draft("88");
  {
    editor::SceneFileSession read_files(f.reader, f.scene);
    Require(read_files.BindCurrent(f.path), "Read-only binding failed");
    f.workspace = &f.reader;
    f.active_files = &read_files;
    f.Draw();
    f.Control("Add");
    f.Control("Clear");
    f.Control("Remove", 0);
    f.Tap(Key::S, Mods::Control);
    Require(f.Snapshot() == edited && !f.ui.TakeSceneSaveRequest() &&
                Read(f.root / f.path) == saved,
            "Read-only actual controls wrote source/file");
    f.workspace = &f.writer;
    f.active_files = f.files.get();
    f.Draw();
  }
  f.Tap(Key::Enter);
  Require(f.Snapshot() == edited, "Read-only handoff revived an old array draft");
  f.Draft("66");
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  f.Tap(Key::Enter);
  Require(f.Snapshot() == edited, "Focus handoff revived an old array draft");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Fresh arrays"));
  editor::SceneFileSession reopened_files(f.reader, reopened);
  Require(reopened_files.Open(reopened_files.Token(), f.path).Applied(),
          "Fresh World production Open failed");
  const std::array fresh_keys{*reopened.Key(f.keys[0].id), *reopened.Key(f.keys[1].id)};
  Require(reopened.Select(fresh_keys) && fresh_keys[0] != f.keys[0] &&
              *reopened.OpaqueComponents(fresh_keys[0]) == edited[0] &&
              *reopened.OpaqueComponents(fresh_keys[1]) == edited[1],
          "Fresh reopen lost exact unknown/prefix/array bytes or generation");
  editor::imgui::EditorImGuiHost fresh_ui;
  Require(fresh_ui.SetReflectedInspector(f.metadata), "Fresh UI catalog failed");
  fresh_ui.SetDisplay(1600 * scale, 1200 * scale, scale);
  Access::ConfigureSyntheticInput(fresh_ui);
  const auto draw = [&] {
    fresh_ui.SetSceneFileContext(reopened_files.Token(), reopened_files.CurrentPath(),
                                 reopened_files.SaveBlocked());
    fresh_ui.BeginFrame();
    fresh_ui.DrawProductShell(f.shell, &reopened, &f.reader);
    static_cast<void>(fresh_ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  Access::FocusInspector(fresh_ui);
  draw();
  draw();
  Require(Access::ReflectedPropertyMixed(fresh_ui, "samples[0]") == false &&
              Access::ReflectedArrayControlPosition(fresh_ui, Fixture::kType, "samples", "Add")
                  .has_value() &&
              Read(f.root / f.path) == saved,
          "Fresh UI lost persisted array interpretation");
}
void Defaults() {
  Fixture f(1);
  using K = editor::ReflectedKind;
  const std::array kinds{K::Boolean, K::Integer, K::Unsigned,        K::Number,
                         K::Enum,    K::Flags,   K::Vector2,         K::Vector3,
                         K::Vector4, K::Color,   K::EntityReference, K::AssetReference};
  for (const auto kind : kinds) {
    const auto type = static_cast<runtime::TypeId>(600 + static_cast<unsigned>(kind));
    editor::ReflectedComponent schema{type, "Typed.Default", 16, {}, {{"values", kind, 0, 2}}};
    editor::ReflectedValue expected;
    switch (kind) {
    case K::Boolean:
      expected = false;
      break;
    case K::Integer:
      expected = std::int64_t{};
      break;
    case K::Number:
      expected = 0.;
      break;
    case K::Enum:
      schema.arrays[0].choices = {{7, "Seven"}, {9, "Nine"}};
      expected = std::uint64_t{7};
      break;
    case K::Flags:
      schema.arrays[0].choices = {{8, "Write"}};
      expected = std::uint64_t{};
      break;
    case K::Color:
      expected = std::array<double, 4>{0, 0, 0, 1};
      break;
    case K::Vector2:
    case K::Vector3:
    case K::Vector4:
      expected = std::array<double, 4>{};
      break;
    case K::AssetReference:
      expected = foundation::Uuid{};
      break;
    default:
      expected = std::uint64_t{};
      break;
    }
    editor::OpaqueComponent source{type, schema.name, std::vector<std::uint8_t>(16)};
    source.data[15] = 255;
    Require(f.metadata.SetComponents({schema}) && f.ui.SetReflectedInspector(f.metadata) &&
                f.scene.SetOpaqueComponent(f.keys[0], source) &&
                f.scene.SetOpaqueComponent(f.keys[1], source),
            "Typed default fixture failed");
    f.Draw();
    f.Draw();
    Access::FocusInspector(f.ui);
    f.Draw();
    f.Click(Access::ReflectedArrayControlPosition(f.ui, type, "values", "Add"));
    const auto view = f.metadata.Inspect(f.scene, f.keys, type);
    Require(view && view->arrays[0].counts == std::vector<std::size_t>({1, 1}) &&
                view->properties.size() == 1 && view->properties[0].value == expected &&
                !view->properties[0].mixed,
            "Actual Add supplied an invalid/wrong typed default");
    for (const auto key : f.keys) {
      const auto components = f.scene.OpaqueComponents(key);
      Require(std::ranges::find(*components, type, &editor::OpaqueComponent::type)->data[15] == 255,
              "Typed default Add changed opaque prefix padding");
    }
  }
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    Defaults();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
