#include "EditorImGuiTestAccess.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
using Event = Nexora::Window::WindowEvent;
using Access = editor::imgui::EditorImGuiTestAccess;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

struct Fixture {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-modifier-chords-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *workspace{};
  runtime::World world;
  runtime::Id id = world.LoadScene("Modifier chords");
  editor::SceneDocument scene{world, id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  bool previous_open{};
  unsigned opens{}, closes{};
  bool text_owner{}, focus_text{};
  std::array<char, 64> text{};

  Fixture(float dpi, bool readonly) {
    Require(writer.Create(root, "Modifier chords") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(id),
            "Modifier chord workspace failed");
    workspace = readonly ? &reader : &writer;
    Require(scene.CreateCamera("Camera") != 0, "Modifier chord camera failed");
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    // Exercise production input trickling, including queued native press/release batches.
    ImGui::GetIO().ConfigInputTrickleEventQueue = true;
    ui.SetNativeScenePreview(true);
    ui.SetNativeScenePreviewAvailable(true);
    editor::imgui::ExtensionManagerObservation view;
    view.project = writer.Project().id;
    view.root = writer.Root();
    view.scope = 1;
    Require(ui.SetExtensionManagerObservation(std::move(view)), "Extension view fixture failed");
    Event focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Frame();
    ImGui::SetWindowFocus("Scene###nexora.scene");
    for (int i = 0; i < 4; ++i)
      Frame();
    Require(!Open() && ui.GetNativeSceneTool() == editor::imgui::NativeSceneTool::Move,
            "Initial Scene tool/Extensions state differs");
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  bool Open() const {
    const auto *window = ImGui::FindWindowByName("Extensions");
    return window && window->Active;
  }
  void Frame() {
    ui.BeginFrame(1.0F / 60.0F);
    ui.DrawProductShell(shell, &scene, workspace, nullptr, nullptr, nullptr, nullptr, &play);
    ui.DrawExtensionManager(workspace->Writable(), workspace->Writable());
    if (text_owner) {
      ImGui::Begin("Chord text owner");
      if (focus_text)
        ImGui::SetKeyboardFocusHere();
      ImGui::InputText("Text", text.data(), text.size());
      ImGui::End();
      focus_text = false;
    }
    static_cast<void>(ui.EndFrame());
    const bool open = Open();
    opens += open && !previous_open;
    closes += !open && previous_open;
    previous_open = open;
  }
  void Drain() {
    for (int i = 0; i < 16; ++i)
      Frame();
  }
};

Event Keyboard(Key key, bool down, Mods mods) {
  Event event;
  event.type = Nexora::Window::WindowEventType::Key;
  event.value0 = static_cast<int>(key);
  event.value1 = down;
  event.modifiers = mods;
  return event;
}

constexpr auto both =
    static_cast<Mods>(static_cast<unsigned>(Mods::Control) | static_cast<unsigned>(Mods::Alt));

bool RunPartition(float dpi, bool readonly, unsigned boundaries) {
  Fixture f(dpi, readonly);
  const auto before = f.world.SaveScene(f.id);
  // The real failing X11 stream releases modifiers before E. Every partition is a legal
  // native poll/frame schedule; a single E press must retain its press-time modifiers.
  const std::array events{Keyboard(Key::LeftControl, true, Mods::Control),
                          Keyboard(Key::LeftAlt, true, both),
                          Keyboard(Key::E, true, both),
                          Keyboard(Key::LeftControl, false, Mods::Alt),
                          Keyboard(Key::LeftAlt, false, {}),
                          Keyboard(Key::E, false, {})};
  std::vector<Event> batch;
  for (std::size_t i = 0; i < events.size(); ++i) {
    batch.push_back(events[i]);
    if (i + 1 == events.size() || (boundaries & (1U << i))) {
      f.ui.ProcessEvents(batch);
      f.Frame();
      batch.clear();
    }
  }
  f.Drain();
  const auto &io = ImGui::GetIO();
  const bool passed = f.Open() && f.opens == 1 && f.closes == 0 &&
                      f.ui.GetNativeSceneTool() == editor::imgui::NativeSceneTool::Move &&
                      !io.KeyCtrl && !io.KeyAlt && !ImGui::GetKeyData(ImGuiKey_E)->Down &&
                      f.world.SaveScene(f.id) == before;
  if (!passed)
    std::cerr << "Chord partition failed: dpi=" << dpi << " readonly=" << readonly
              << " boundaries=" << boundaries << " opens=" << f.opens << " closes=" << f.closes
              << " tool=" << static_cast<int>(f.ui.GetNativeSceneTool()) << '\n';
  return passed;
}

void RunPlainPress(float dpi) {
  Fixture f(dpi, false);
  const auto before = f.world.SaveScene(f.id);
  const std::array events{Keyboard(Key::E, true, {}),
                          Keyboard(Key::LeftControl, true, Mods::Control),
                          Keyboard(Key::LeftAlt, true, both),
                          Keyboard(Key::E, false, both),
                          Keyboard(Key::LeftControl, false, Mods::Alt),
                          Keyboard(Key::LeftAlt, false, {})};
  f.ui.ProcessEvents(events);
  f.Drain();
  Require(!f.Open() && f.opens == 0 &&
              f.ui.GetNativeSceneTool() == editor::imgui::NativeSceneTool::Rotate &&
              f.world.SaveScene(f.id) == before,
          "Later modifier presses retroactively changed an ordinary E press");
}

void RunFocusLoss(float dpi) {
  Fixture f(dpi, false);
  const auto before = f.world.SaveScene(f.id);
  Event lost;
  lost.type = Nexora::Window::WindowEventType::FocusChanged;
  lost.value0 = 0;
  const std::array events{Keyboard(Key::LeftControl, true, Mods::Control),
                          Keyboard(Key::LeftAlt, true, both), Keyboard(Key::E, true, both), lost};
  f.ui.ProcessEvents(events);
  f.Drain();
  lost.value0 = 1;
  f.ui.ProcessEvents(std::array{lost});
  f.Drain();
  Require(!f.Open() && f.opens == 0 && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt &&
              !ImGui::GetKeyData(ImGuiKey_E)->Down && f.world.SaveScene(f.id) == before,
          "Focus loss retained a chord or emitted a deferred command on regain");
}

void RunTextOwner(float dpi) {
  Fixture f(dpi, false);
  const auto before = f.world.SaveScene(f.id);
  f.text_owner = f.focus_text = true;
  f.Frame();
  f.Frame();
  ImGui::SetWindowFocus("Chord text owner");
  f.focus_text = true;
  f.Frame();
  f.Frame();
  Require(ImGui::GetIO().WantTextInput, "Text ownership fixture was not active");
  const std::array events{Keyboard(Key::LeftControl, true, Mods::Control),
                          Keyboard(Key::LeftAlt, true, both),
                          Keyboard(Key::E, true, both),
                          Keyboard(Key::LeftControl, false, Mods::Alt),
                          Keyboard(Key::LeftAlt, false, {}),
                          Keyboard(Key::E, false, {})};
  f.ui.ProcessEvents(events);
  f.Drain();
  Require(!f.Open() && f.opens == 0 &&
              f.ui.GetNativeSceneTool() == editor::imgui::NativeSceneTool::Move &&
              f.world.SaveScene(f.id) == before,
          "Global chord stole a text edit or changed the concurrent Scene");
}
} // namespace

int main() {
  try {
    unsigned scenarios{}, failed{};
    for (const float dpi : {1.0F, 2.0F}) {
      for (const bool readonly : {false, true})
        for (unsigned boundaries = 0; boundaries < 32; ++boundaries) {
          ++scenarios;
          failed += !RunPartition(dpi, readonly, boundaries);
        }
      RunPlainPress(dpi);
      RunFocusLoss(dpi);
      RunTextOwner(dpi);
    }
    std::cout << "Modifier chord frame partitions: scenarios=" << scenarios << " failed=" << failed
              << '\n';
    return failed ? 1 : 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
