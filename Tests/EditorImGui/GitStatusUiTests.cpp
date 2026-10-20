#include "../../Engine/Editor/src/GitStatusProviderTestAccess.h"
#include "../Editor/GitTestSupport.h"
#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Phase = editor::GitStatusPhase;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
constexpr Mods kChord =
    static_cast<Mods>(static_cast<unsigned>(Mods::Control) | static_cast<unsigned>(Mods::Alt));
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return {value.begin(), value.end()};
}
void Write(const std::filesystem::path &path, std::string_view text) {
  std::ofstream output(path, std::ios::binary);
  output << text;
  output.close();
  Require(!output.fail(), "Fixture write failed");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "Fixture read failed");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
struct Fixture final {
  std::filesystem::path temporary =
      std::filesystem::temp_directory_path() /
      ("nexora-git-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{temporary};
  editor::ProjectWorkspace workspace, reader, other;
  core::JobSystem jobs{1};
  editor::GitStatusProvider provider{jobs};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::uint64_t scope{11};
  bool allowed{true};
  float scale;
  std::filesystem::path git, fault, root;
  Fixture(float dpi, std::filesystem::path tool, std::filesystem::path binary)
      : scale(dpi), git(std::move(tool)), fault(std::move(binary)) {
    Require(workspace.Create(temporary / std::filesystem::path(u8"專案 µ"), "Git status UI"),
            "Project creation failed");
    root = workspace.Root();
    jobs.Start();
    std::filesystem::create_directories(temporary / "empty-template");
    Git({"init", "-q", "--template=" + Utf8(temporary / "empty-template")});
    Write(root / ".gitignore", ".nexora/\n.fixture-ready\n");
    Write(root / "Content/authoring.txt", "last good authoring\n");
    Write(root / "Content/before.txt", "rename payload\n");
    Git({"add", "--", "."});
    Git({"commit", "-qm", "initial"});
    ui.SetDisplay(1600 * scale, 1000 * scale, scale);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    Require(provider.Query().phase == Phase::Idle && !provider.Busy(),
            "Opening a project executed Git automatically");
    Event(Key::LeftControl, true, Mods::Control);
    Event(Key::LeftAlt, true, kChord);
    Event(Key::G, true, kChord);
    Event(Key::G, false, kChord);
    Event(Key::LeftAlt, false, Mods::Control);
    Event(Key::LeftControl, false);
    for (int i = 0; i < 3; ++i)
      Draw();
    Require(Access::GitControlPosition(ui, 0).has_value() && provider.Query().phase == Phase::Idle,
            "Actual source-control shortcut failed or executed Git on open");
  }
  void Git(std::vector<std::string> args) {
    static_cast<void>(editor::test::RunGit(jobs, git, root, std::move(args)));
  }
  std::string Draw(bool capture = false) {
    ui.BeginFrame();
    if (capture)
      Access::BeginTextCapture(ui);
    ui.DrawProductShell(shell, nullptr, active);
    ui.DrawGitStatus(provider, scope, active ? active->Root() : std::filesystem::path{}, allowed);
    std::string text;
    if (capture)
      text = Access::EndTextCapture(ui);
    Require(ui.EndFrame().vertices > 0, "Actual UI did not produce draw data");
    return text;
  }
  void Event(Key key, bool down, Mods modifiers = Mods::None) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = down;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Click(std::size_t control) {
    const auto point = Access::GitControlPosition(ui, control);
    Require(point.has_value(), "Actual Git control not visible");
    Nexora::Window::WindowEvent move, button;
    move.type = Nexora::Window::WindowEventType::Pointer;
    move.value0 = static_cast<int>((*point)[0] * scale);
    move.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{move, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Tool(const std::filesystem::path &path, bool commit = true) {
    Click(0);
    Event(Key::LeftControl, true, Mods::Control);
    Event(Key::A, true, Mods::Control);
    Event(Key::A, false, Mods::Control);
    Event(Key::LeftControl, false);
    const auto text = Utf8(path);
    for (std::size_t i = 0; i < text.size();) {
      const auto first = static_cast<unsigned char>(text[i++]);
      std::uint32_t point = first;
      int tails{};
      if (first >= 0xf0) {
        point = first & 7;
        tails = 3;
      } else if (first >= 0xe0) {
        point = first & 15;
        tails = 2;
      } else if (first >= 0xc0) {
        point = first & 31;
        tails = 1;
      }
      while (tails--) {
        Require(i < text.size(), "Fixture UTF-8 path truncated");
        point = (point << 6) | (static_cast<unsigned char>(text[i++]) & 63);
      }
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = static_cast<int>(point);
      ui.ProcessEvents(std::array{event});
      Draw();
    }
    if (commit) {
      Event(Key::Enter, true);
      Event(Key::Enter, false);
    }
  }
  void Finish() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    do {
      Draw();
      Require(std::chrono::steady_clock::now() < deadline, "Status did not drain");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (provider.Busy());
  }
  std::filesystem::path CopyFault(const char *name) {
    const auto directory = temporary / "faults";
    std::filesystem::create_directories(directory);
    auto output = directory / name;
    output += fault.extension();
    std::filesystem::copy_file(fault, output);
    return output;
  }
  void Marker() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!std::filesystem::exists(root / ".fixture-ready")) {
      Draw();
      Require(std::chrono::steady_clock::now() < deadline, "Negative native fixture did not start");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
};
void Run(float scale, const std::filesystem::path &git, const std::filesystem::path &fault) {
  Fixture f(scale, git, fault);
  auto index = Read(f.root / ".git/index");
  f.Tool(git);
  f.Click(1);
  f.Finish();
  Require(f.provider.Query().phase == Phase::Ready && f.provider.Capture(f.scope) &&
              f.provider.Capture(f.scope)->entries.empty() &&
              Read(f.root / ".git/index") == index &&
              f.Draw(true).find("Clean at last successful refresh.") != std::string::npos,
          "Actual clean refresh unavailable, mutated index or reported false status");
  Write(f.root / "Content/authoring.txt", "external authoring change\n");
  Write(f.root / std::filesystem::path(u8"Content/新增 %s.txt"), "new asset\n");
#if !defined(_WIN32)
  Write(f.root / "Content/line\nbreak.txt", "newline asset\n");
  Write(f.root / ("Content/raw-" + std::string(1, static_cast<char>(0xff))), "raw asset\n");
#endif
  f.Git({"mv", "--", "Content/before.txt", "Content/after %s.txt"});
  index = Read(f.root / ".git/index");
  f.Click(1);
  f.Finish();
  const auto good = f.provider.Capture(f.scope);
  const auto text = f.Draw(true);
  Require(good && good->entries.size() >= 3 && text.find("新增 %s.txt") != std::string::npos &&
              text.find("after %s.txt <- Content/before.txt") != std::string::npos &&
              Read(f.root / ".git/index") == index &&
              Read(f.root / "Content/authoring.txt") == "external authoring change\n",
          "Actual changed/Unicode/rename status or source/index conservation failed");
#if !defined(_WIN32)
  Require(text.find("line\\nbreak.txt") != std::string::npos &&
              text.find("raw-\\xff") != std::string::npos &&
              text.find("line\nbreak.txt") == std::string::npos,
          "Raw path bytes were lost or rendered as injected UI lines");
#endif
  f.Tool(f.temporary / "missing-git");
  f.Click(1);
  Require(f.provider.Capture(f.scope) == good &&
              f.Draw(true).find("Git executable is unavailable") != std::string::npos,
          "Missing tool erased last-good or looked clean");
  f.Tool(f.CopyFault("malformed"));
  f.Click(1);
  f.Finish();
  Require(f.provider.Query().error == editor::GitStatusError::MalformedOutput &&
              f.provider.Capture(f.scope) == good &&
              f.Draw(true).find("could not be validated") != std::string::npos,
          "Malformed native result replaced last-good or was hidden");
  f.Tool(git);
  editor::GitStatusProviderTestAccess::FailNext(f.provider,
                                                editor::GitStatusAllocationPoint::Capture);
  f.Click(1);
  f.Finish();
  Require(f.provider.Query().error == editor::GitStatusError::AllocationFailed &&
              f.provider.Capture(f.scope) == good &&
              f.Draw(true).find("could not allocate") != std::string::npos,
          "Failed capture allocation lost source/status or reported success");
  const auto sleep = f.CopyFault("sleep");
  f.Tool(sleep);
  f.Click(1);
  f.Marker();
  f.Click(2);
  f.Finish();
  Require(f.provider.Query().phase == Phase::Cancelled && f.provider.Capture(f.scope) == good,
          "Actual Cancel failed to drain/preserve last-good");
  std::filesystem::remove(f.root / ".fixture-ready");
  f.Click(1);
  f.Marker();
  f.allowed = false;
  f.Draw();
  f.Finish();
  Require(f.provider.Query().phase == Phase::Cancelled && f.provider.Capture(f.scope) == good,
          "Revoking refresh permission failed to cancel and retain last-good");
  f.allowed = true;
  std::filesystem::remove(f.root / ".fixture-ready");
  f.Click(1);
  f.Marker();
  Nexora::Window::WindowEvent blur;
  blur.type = Nexora::Window::WindowEventType::FocusChanged;
  blur.value0 = 0;
  f.ui.ProcessEvents(std::array{blur});
  f.Draw();
  f.Finish();
  Require(f.provider.Query().phase == Phase::Cancelled && f.provider.Capture(f.scope) == good,
          "Losing application focus failed to cancel and retain last-good");
  blur.value0 = 1;
  f.ui.ProcessEvents(std::array{blur});
  f.Draw();
  std::filesystem::remove(f.root / ".fixture-ready");
  f.Click(1);
  f.Marker();
  f.scope = 12;
  f.Draw();
  f.Finish();
  f.scope = 11;
  f.Draw();
  Require(!f.provider.Capture(11) && !f.provider.Capture(12),
          "Scope switch-back revived stale data");
  std::filesystem::remove(f.root / ".fixture-ready");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "Read-only observer failed");
  f.active = &f.reader;
  f.scope = 13;
  f.Draw();
  f.Tool(git);
  f.Click(1);
  f.Finish();
  Require(f.provider.Query().phase == Phase::Ready && f.provider.Capture(13) &&
              Read(f.root / ".git/index") == index &&
              Read(f.root / "Content/authoring.txt") == "external authoring change\n",
          "Read-only status inspection was blocked or wrote authoring/index bytes");
  for (std::size_t i = 0; i < 500; ++i)
    Write(f.root / "Content" / ("row-" + std::to_string(i) + ".txt"), "untracked\n");
  f.Click(1);
  f.Finish();
  f.Draw();
  Require(f.provider.Capture(13)->entries.size() >= 500 && Access::GitRenderedRows(f.ui) > 0 &&
              Access::GitRenderedRows(f.ui) < 100 && Read(f.root / ".git/index") == index,
          "Actual clipped view lost complete data or rendered the whole repository");
  f.Tool(git, false);
  f.scope = 0;
  f.active = nullptr;
  f.Draw();
  Require(!f.provider.Capture(13) && f.Draw(true).find("Open a project") != std::string::npos,
          "Project close kept an old status visible");
  Require(f.other.Create(f.temporary / "no-repository", "No repository"), "Other project failed");
  f.active = &f.other;
  f.scope = 14;
  f.Draw();
  f.Click(1);
  Require(!f.provider.Capture(14) &&
              f.Draw(true).find("Git executable is unavailable") != std::string::npos,
          "A previous project's active Git draft survived scope replacement");
  f.Tool(git);
  f.Click(1);
  Require(!f.provider.Capture(14) &&
              f.Draw(true).find("not an available Git repository") != std::string::npos,
          "Unavailable repository was represented as clean");
  f.active = &f.workspace;
  f.scope = 15;
  f.Draw();
  f.Tool(sleep);
  f.Click(1);
  f.Marker();
  f.provider.Shutdown();
  f.Draw();
  Require(!f.provider.Busy() && !f.provider.Capture(15) &&
              f.Draw(true).find("unavailable for this session") != std::string::npos &&
              good->entries.size() >= 3 && Read(f.root / ".git/index") == index,
          "Teardown retained a child/borrow or lost owning snapshots/source");
}
} // namespace
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    Require(argc == 4 && argv[3][1] == 0 && (argv[3][0] == '1' || argv[3][0] == '2'),
            "Expected actual Git executable, native negative fixture and1/2 scale");
    Run(argv[3][0] == '2' ? 2.F : 1.F, std::filesystem::absolute(argv[1]),
        std::filesystem::absolute(argv[2]));
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
