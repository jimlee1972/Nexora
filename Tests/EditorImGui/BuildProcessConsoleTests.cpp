#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Phase = editor::BuildProcessPhase;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Run(float scale, const std::filesystem::path &executable) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-build-console-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto marker = root / "authoring.txt";
  {
    std::ofstream source(marker);
    source << "last good authoring";
  }
  try {
    core::JobSystem jobs{1};
    jobs.Start();
    editor::BuildProcess process{jobs};
    editor::imgui::EditorImGuiHost ui;
    std::uint64_t scope = 3;
    bool allowed = true;
    ui.SetDisplay(1400, 1000, scale);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    const auto draw = [&] {
      ui.BeginFrame();
      ui.DrawBuildProcess(process, scope, root, allowed);
      static_cast<void>(ui.EndFrame());
    };
    for (int i = 0; i < 4; ++i)
      draw();
    Nexora::Window::WindowEvent key;
    key.type = Nexora::Window::WindowEventType::Key;
    key.value0 = static_cast<int>(Nexora::Window::Key::B);
    key.value1 = 1;
    key.modifiers = static_cast<Nexora::Window::KeyModifiers>(
        static_cast<unsigned>(Nexora::Window::KeyModifiers::Control) |
        static_cast<unsigned>(Nexora::Window::KeyModifiers::Alt));
    ui.ProcessEvents(std::array{key});
    draw();
    key.value1 = 0;
    key.modifiers = {};
    ui.ProcessEvents(std::array{key});
    for (int i = 0; i < 4; ++i)
      draw();
    const auto click = [&](std::size_t control) {
      const auto point = Access::BuildControlPosition(ui, control);
      Require(point.has_value(), "Actual build console control absent");
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
    const auto shortcut = [&](bool cancel) {
      Nexora::Window::WindowEvent enter;
      enter.type = Nexora::Window::WindowEventType::Key;
      enter.value0 = static_cast<int>(Nexora::Window::Key::Enter);
      enter.value1 = 1;
      enter.modifiers = static_cast<Nexora::Window::KeyModifiers>(
          static_cast<unsigned>(Nexora::Window::KeyModifiers::Control) |
          (cancel ? static_cast<unsigned>(Nexora::Window::KeyModifiers::Shift) : 0u));
      ui.ProcessEvents(std::array{enter});
      draw();
      enter.value1 = 0;
      enter.modifiers = {};
      ui.ProcessEvents(std::array{enter});
      draw();
      draw();
    };
    const auto executable_utf8 = executable.u8string(), cwd_utf8 = root.u8string();
    const std::string exe(executable_utf8.begin(), executable_utf8.end());
    const std::string cwd(cwd_utf8.begin(), cwd_utf8.end());
    const auto command = [&](std::vector<std::string> arguments) {
      Access::SetBuildCommand(ui, exe, cwd, arguments);
      draw();
    };
    const auto finish = [&] {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
      do {
        draw();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      } while (process.Busy() && std::chrono::steady_clock::now() < deadline);
      Require(!process.Busy(), "Actual UI process failed to finish");
      draw();
    };
    click(2);
    Require(Access::BuildControlPosition(ui, 6).has_value(), "Add argument did not create input");
    click(3);
    Require(!Access::BuildControlPosition(ui, 6), "Remove argument retained input");
    command({"--args", "", "quoted \"value\" ; $ literal", "line\n\x1b[31m", "\xff"});
    click(4);
    finish();
    // Invalid UTF-8 arguments are rejected before child creation.
    Require(process.Snapshot().operation == 0, "Malformed UTF-8 command launched a process");
    command({"--args", "", "quoted \"value\" ; $ literal", "line\n\x1b[31m"});
    click(4);
    finish();
    auto status = Access::BuildStatus(ui);
    Require(status.phase == Phase::Exited && status.exit_code == 0u &&
                status.output.find("0:\n") != std::string::npos &&
                status.output.find("STDERR_END") != std::string::npos,
            "Real argv/empty argument/stderr outcome lost");
    const auto displayed = Access::BuildOutput(ui);
    Require(displayed.find('\x1b') == std::string::npos &&
                displayed.find("\\x1b[31m") != std::string::npos &&
                foundation::IsValidUtf8(displayed),
            "Raw process control bytes reached display");
    command({"--binary-output"});
    shortcut(false);
    finish();
    status = Access::BuildStatus(ui);
    Require(status.phase == Phase::Exited &&
                status.output.find(std::string{"\0\xff", 2}) != std::string::npos &&
                Access::BuildOutput(ui).find("\\x00\\xff\\x1b") != std::string::npos &&
                foundation::IsValidUtf8(Access::BuildOutput(ui)),
            "Binary child output bypassed escaped display");
    command({"--fail"});
    click(4);
    finish();
    status = Access::BuildStatus(ui);
    Require(status.phase == Phase::Failed && status.exit_code == 7u,
            "Failure was presented as success");
    command({"--flood"});
    click(4);
    finish();
    status = Access::BuildStatus(ui);
    Require(status.output.size() <= 16 * 1024 && status.dropped_output_bytes > 1024 * 1024 &&
                status.output.ends_with("FLOOD_END\n"),
            "Flood bypassed bounded tail/dropped count");
    command({"--sleep"});
    click(4);
    const auto wait_ready = [&] {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (process.Snapshot().output.find("READY") == std::string::npos &&
             std::chrono::steady_clock::now() < deadline) {
        draw();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      Require(process.Snapshot().output.find("READY") != std::string::npos,
              "Real process never ran");
    };
    wait_ready();
    click(5);
    finish();
    Require(Access::BuildStatus(ui).phase == Phase::Cancelled,
            "UI cancellation lost actual terminal state");
    command({"--sleep"});
    shortcut(false);
    wait_ready();
    shortcut(true);
    finish();
    Require(Access::BuildStatus(ui).phase == Phase::Cancelled,
            "Focused keyboard cancellation failed");
    command({"--sleep"});
    click(4);
    wait_ready();
    ++scope;
    draw();
    finish();
    Require(Access::BuildStatus(ui).operation == 0 && Access::BuildOutput(ui).empty(),
            "Project replacement exposed old command output");
    command({"--args"});
    allowed = false;
    draw();
    const auto before = process.Snapshot().operation;
    click(4);
    Require(process.Snapshot().operation == before && !process.Busy(),
            "Read-only/blocked scope launched code");
    allowed = true;
    command({"--sleep"});
    click(4);
    wait_ready();
    allowed = false;
    draw();
    finish();
    Require(Access::BuildStatus(ui).phase == Phase::Cancelled,
            "Revoked project authority left process running");
    allowed = true;
    command({"--args"});
    const auto closed_operation = process.Snapshot().operation;
    ui.RequestCloseConfirmation();
    draw();
    click(4);
    Require(process.Snapshot().operation == closed_operation && !process.Busy(),
            "Close confirmation launched a new child process");
    process.Shutdown();
    jobs.Stop();
    std::ifstream input(marker);
    Require(std::string(std::istreambuf_iterator<char>(input), {}) == "last good authoring",
            "Process UI mutated authoring source");
  } catch (...) {
    std::filesystem::remove_all(root);
    throw;
  }
  std::filesystem::remove_all(root);
}
} // namespace
int main(int argc, char **argv) {
  try {
    Require(argc == 2, "Missing real executable fixture");
    Run(1, std::filesystem::absolute(argv[1]));
    Run(2, std::filesystem::absolute(argv[1]));
    std::cout << "Actual build console 1x/2x process/argv/tail/cancel/scope passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
