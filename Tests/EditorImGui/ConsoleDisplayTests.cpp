#include "CoreConsoleIngress.h"
#include "EditorImGuiTestAccess.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float dpi) {
  using namespace nexora;
  using editor::imgui::EditorImGuiTestAccess;
  using Nexora::Window::WindowEvent;
  using Nexora::Window::WindowEventType;
  runtime::RuntimeConsole ingress{2};
  core::AsyncLogService producer{2};
  producer.Start();
  editor::preview::CoreConsoleIngress bridge{producer, ingress, "source"};
  const auto write = [&](core::LogLevel level, std::string category, std::string message) {
    producer.Write(level, std::move(category), std::move(message));
    producer.Flush(); // Test synchronization only; the graphical owner never flushes per frame.
    bridge.Poll();
  };
  write(core::LogLevel::Trace, "test", "trace event");
  write(core::LogLevel::Warning, "test", "warning event");
  editor::imgui::EditorImGuiHost host;
  host.SetDisplay(1280 * dpi, 900 * dpi, dpi);
  EditorImGuiTestAccess::ConfigureSyntheticInput(host);
  WindowEvent focus;
  focus.type = WindowEventType::FocusChanged;
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  editor::ProductShell shell;
  runtime::RuntimeConsole *active = &ingress;
  const auto draw = [&] {
    bridge.Poll();
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, nullptr, nullptr, nullptr, nullptr, active);
    static_cast<void>(host.EndFrame());
  };
  draw();
  draw();
  EditorImGuiTestAccess::FocusConsole(host);
  draw();
  draw();
  const auto expect = [&](std::size_t count, std::uint64_t first) {
    Require(EditorImGuiTestAccess::ConsoleVisibleCount(host) == count &&
                EditorImGuiTestAccess::ConsoleFirstVisibleSequence(host) ==
                    (count ? std::optional{first} : std::nullopt),
            "Console displayed the wrong filtered snapshot");
  };
  const auto click = [&](std::size_t control) {
    const auto point = EditorImGuiTestAccess::ConsoleControlPosition(host, control);
    Require(point.has_value(), "Console control was not visible");
    WindowEvent pointer;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    WindowEvent button;
    button.type = WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    host.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    host.ProcessEvents(std::array{button});
    draw();
  };
  expect(2, 1);
  click(0); // Pause display; ingress remains live.
  std::thread writer([&] {
    for (int index = 0; index < 50; ++index)
      producer.Write(core::LogLevel::Info, "producer", "new log");
  });
  writer.join();
  producer.Flush();
  draw();
  expect(2, 1);
  Require(ingress.Snapshot().front().sequence == 3 && ingress.DroppedCount() == 50,
          "pausing display changed producer admission or eviction");
  EditorImGuiTestAccess::SetConsoleFilter(host, "warning", 2);
  draw();
  expect(1, 2); // Filters continue to operate on the frozen owning records.
  click(1);     // Clear also hides live records received while the display was paused.
  expect(0, 0);
  Require(ingress.Snapshot().size() == 2 && ingress.Snapshot().back().sequence == 4 &&
              ingress.DroppedCount() == 50,
          "Clear view changed the original records or drop counter");
  write(core::LogLevel::Fatal, "new", "after clear");
  EditorImGuiTestAccess::SetConsoleFilter(host, "", 0);
  draw();
  expect(0, 0);
  click(0); // Resume includes only records newer than the clear watermark.
  expect(1, 5);
  click(1);
  expect(0, 0);
  draw();
  expect(0, 0);
  write(core::LogLevel::Info, "new", "visible again");
  draw();
  expect(1, 6);

  runtime::RuntimeConsole replacement{1};
  replacement.Push({0, runtime::RuntimeLogSeverity::Error, "other", 1, "source", "replacement"});
  click(0);
  active = &replacement;
  draw();
  expect(1, 1); // Pause and clear state cannot leak into a different ingress.
  active = nullptr;
  draw();
  expect(0, 0);
  Require(!EditorImGuiTestAccess::ConsoleControlPosition(host, 0),
          "unavailable Console retained a clickable control");
  active = &ingress;
  draw();
  expect(2, 5);
  Require(ingress.DroppedCount() == 52, "display controls reset cumulative drops");
  click(1);
  for (std::uint64_t index = 0; index < 32; ++index) {
    click(0);
    write(core::LogLevel::Info, "soak", "next record");
    draw();
    expect(0, 0);
    click(0);
    expect(1, 7 + index);
    click(1);
    expect(0, 0);
  }
  producer.Stop();
  bridge.Poll();
  Require(ingress.Snapshot().size() == 2 && ingress.DroppedCount() == 84,
          "repeated pause/resume/clear changed bounded ingress behavior");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Console pause/clear ownership and input contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
