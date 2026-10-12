#include "EditorImGuiTestAccess.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Run(float scale) {
  editor::imgui::EditorImGuiHost ui;
  editor::TelemetryConsent diagnostics;
  const auto first = foundation::Uuid{1, 11};
  const auto second = foundation::Uuid{2, 22};
  auto scope = first;
  ui.SetDisplay(1200, 900, scale);
  Access::ConfigureSyntheticInput(ui);
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawDiagnosticPrivacy(diagnostics, scope);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  Require(!diagnostics.Enabled() && !diagnostics.Record("frame.presented"),
          "Default-off privacy admitted an event");
  Nexora::Window::WindowEvent key;
  key.type = Nexora::Window::WindowEventType::Key;
  key.value0 = static_cast<int>(Key::T);
  key.value1 = 1;
  key.modifiers =
      static_cast<Mods>(static_cast<unsigned>(Mods::Control) | static_cast<unsigned>(Mods::Alt));
  ui.ProcessEvents(std::array{key});
  draw();
  key.value1 = 0;
  key.modifiers = {};
  ui.ProcessEvents(std::array{key});
  for (int i = 0; i < 4; ++i)
    draw();
  const auto click = [&](std::size_t control) {
    const auto point = Access::DiagnosticPosition(ui, control);
    Require(point.has_value(), "Actual privacy control is absent");
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
  click(0);
  Require(diagnostics.Enabled() && diagnostics.Record("frame.presented") &&
              diagnostics.Record("unrecognized source text\nnot for display"),
          "Explicit opt-in did not operate on the real queue");
  draw();
  Require(Access::DiagnosticRows(ui) == std::array<std::size_t, 2>{1, 1},
          "Unknown text bypassed display allowlisting");
  while (diagnostics.Events().size() < editor::TelemetryConsent::kMaximumEvents)
    Require(diagnostics.Record("frame.presented"), "Bounded queue rejected valid capacity");
  Require(!diagnostics.Record("frame.presented"), "Queue capacity was not bounded");
  draw();
  Require(Access::DiagnosticRows(ui)[0] == 1023 && Access::DiagnosticRows(ui)[1] == 1,
          "Bounded inspection lost safe/excluded counts");
  click(1);
  Require(diagnostics.Enabled() && diagnostics.Events().empty(),
          "Clear retained events lost consent or retained old data");
  Require(diagnostics.Record("frame.presented"), "Clear prevented future authorized intake");
  click(0);
  Require(!diagnostics.Enabled() && diagnostics.Events().empty() &&
              !diagnostics.Record("frame.presented"),
          "Opt-out failed immediate deletion");
  click(0);
  Require(diagnostics.Enabled() && diagnostics.Events().empty(), "Re-enable resurrected old data");
  Require(diagnostics.Record("frame.presented"), "Re-enable did not accept a new event");
  scope = second;
  draw();
  Require(!diagnostics.Enabled() && diagnostics.Events().empty(),
          "Project switch retained consent/data");
  click(0);
  Require(diagnostics.Enabled(), "New project could not explicitly enable consent");
  scope = {};
  draw();
  click(0);
  Require(!diagnostics.Enabled() && diagnostics.Events().empty(),
          "Detached project allowed consent");
  editor::TelemetryConsent restarted;
  Require(!restarted.Enabled() && restarted.Events().empty(),
          "Restart model restored consent/data");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Actual 1x/2x session diagnostic privacy passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
