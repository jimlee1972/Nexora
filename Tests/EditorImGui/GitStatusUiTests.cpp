#include "../../Engine/Editor/src/GitDiffProviderTestAccess.h"
#include "../../Engine/Editor/src/GitStatusProviderTestAccess.h"
#include "../Editor/GitTestSupport.h"
#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <cerrno>
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
  const auto open_error = errno;
  if (!output)
    throw std::runtime_error("Fixture open failed (errno=" + std::to_string(open_error) + ")");
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
  editor::GitDiffProvider diff{jobs};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::uint64_t scope{11};
  bool allowed{true};
  float scale;
  bool diff_mode{};
  std::filesystem::path git, fault, root;
  Fixture(float dpi, std::filesystem::path tool, std::filesystem::path binary, bool diffs = false)
      : scale(dpi), diff_mode(diffs), git(std::move(tool)), fault(std::move(binary)) {
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
    if (diff_mode)
      ui.DrawGitStatus(provider, diff, scope, active ? active->Root() : std::filesystem::path{},
                       allowed);
    else
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
  void Click(std::size_t control) { ClickPoint(Access::GitControlPosition(ui, control)); }
  void ClickDiff(std::size_t control) { ClickPoint(Access::GitDiffControlPosition(ui, control)); }
  void Select(std::string_view path) { ClickPoint(Row(path)); }
  std::optional<std::array<float, 2>> Row(std::string_view path) {
    const auto capture = provider.Capture(scope);
    Require(static_cast<bool>(capture), "No status selection source");
    for (std::size_t index = 0; index < capture->entries.size(); ++index)
      if (capture->entries[index].path == path) {
        return Access::GitRowPosition(ui, index);
      }
    throw std::runtime_error("Selected status path missing");
  }
  void ClickPoint(const std::optional<std::array<float, 2>> &point) {
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
    } while (provider.Busy() || diff.Busy());
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
  Require(Access::GitPathDisplay("raw-" + std::string(1, static_cast<char>(0xff))) == "raw-\\xff" &&
              Access::GitPathDisplay("%s\\\n\r\t\x01\x7f") == "%s\\\\\\n\\r\\t\\x01\\x7f" &&
              Access::GitPathDisplay("新增 %s.txt") == "新增 %s.txt",
          "Cross-platform byte escaping lost raw/Unicode/control data");
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
#endif
#if defined(__linux__)
  // Darwin filesystems require valid Unicode filenames; Linux also exercises actual raw bytes.
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
              text.find("line\nbreak.txt") == std::string::npos,
          "Control path bytes were lost or rendered as injected UI lines");
#endif
#if defined(__linux__)
  Require(text.find("raw-\\xff") != std::string::npos,
          "Actual Linux raw filename was lost or decoded incorrectly");
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
void RunDiff(float scale, const std::filesystem::path &git, const std::filesystem::path &fault) {
  using DiffPhase = editor::GitDiffPhase;
  Fixture f(scale, git, fault, true);
  f.Tool(git);
  Require(f.diff.Query().phase == DiffPhase::Idle, "Opening/selecting Git executed a diff");
#if defined(__linux__)
  const auto raw_name = "Content/raw-" + std::string(1, static_cast<char>(0xff)) + ".txt";
  Write(f.root / raw_name, "raw tracked source\n");
  f.Git({"add", "--", "."});
#endif
  f.Git({"mv", "--", "Content/before.txt", "Content/after %s##.txt"});
  Write(f.root / "Content/authoring.txt", "staged value\n");
  f.Git({"add", "--", "Content/authoring.txt"});
  Write(f.root / "Content/authoring.txt", "working value\n");
  const auto index = Read(f.root / ".git/index");
  f.Click(1);
  f.Finish();
  f.Select("Content/authoring.txt");
  Require(f.diff.Query().phase == DiffPhase::Idle, "Selecting a path automatically executed Git");
  f.ClickDiff(0);
  f.Finish();
  auto text = f.Draw(true);
  const auto working = f.diff.Capture(f.scope);
  Require(working && working->output.find("+working value") != std::string::npos &&
              text.find("Working-tree Git output at last successful query.") != std::string::npos &&
              text.find("+working value") != std::string::npos &&
              Read(f.root / ".git/index") == index,
          "Actual working-tree diff widgets lost output/labels or wrote index");
  // A held press belongs to the old status generation. Its release must not select a row
  // in a replacement capture, even when the same path happens to occupy that ordinal.
  const auto point = f.Row("Content/authoring.txt");
  Require(point.has_value(), "Held status row unavailable");
  Nexora::Window::WindowEvent move, button;
  move.type = Nexora::Window::WindowEventType::Pointer;
  move.value0 = static_cast<int>((*point)[0] * scale);
  move.value1 = static_cast<int>((*point)[1] * scale);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{move, button});
  f.Draw();
  Require(f.provider.Refresh(git, f.root, f.scope), "Replacement real status not admitted");
  f.Finish();
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  const auto old_operation = f.diff.Query().operation;
  f.ClickDiff(0);
  Require(f.diff.Query().operation == old_operation && !Access::GitDiffLineCount(f.ui) &&
              f.Draw(true).find("Select a tracked path") != std::string::npos,
          "Held row release retargeted a replacement status generation");
  f.Select("Content/authoring.txt");
  f.ClickDiff(1);
  f.Finish();
  text = f.Draw(true);
  const auto staged = f.diff.Capture(f.scope);
  Require(staged && staged->request.target == editor::GitDiffTarget::Index &&
              text.find("Staged Git output at last successful query.") != std::string::npos &&
              text.find("+staged value") != std::string::npos &&
              text.find("+working value") == std::string::npos,
          "Actual staged widgets displayed working-tree data");
  editor::GitDiffProviderTestAccess::FailNext(f.diff, editor::GitDiffAllocationPoint::Capture);
  f.ClickDiff(0);
  f.Finish();
  text = f.Draw(true);
  Require(f.diff.Query().error == editor::GitDiffError::AllocationFailed &&
              f.diff.Capture(f.scope) == staged &&
              text.find("could not allocate") != std::string::npos &&
              text.find("Staged Git output at last successful query.") != std::string::npos &&
              text.find("Working-tree Git output at last successful query.") == std::string::npos,
          "Backend allocation failure mislabeled/replaced last-good UI output");
  Access::FailNextGitDiffDisplay(f.ui);
  f.ClickDiff(0);
  f.Finish();
  text = f.Draw(true);
  Require(f.diff.Query().phase == DiffPhase::Ready && f.diff.Capture(f.scope) != staged &&
              text.find("could not allocate") != std::string::npos &&
              text.find("Staged Git output at last successful query.") != std::string::npos &&
              text.find("+working value") == std::string::npos,
          "Display-stage allocation published partial/newly mislabeled data");
  f.ClickDiff(0);
  f.Finish();
  Require(f.Draw(true).find("+working value") != std::string::npos,
          "Display staging failure did not recover on explicit retry");
  const auto head = Read(f.root / ".git/HEAD");
  Write(f.root / ".git/HEAD", "broken HEAD\n");
  f.ClickDiff(1);
  f.Finish();
  text = f.Draw(true);
  Require(f.diff.Query().phase == DiffPhase::Failed &&
              text.find("could not complete") != std::string::npos &&
              text.find("Working-tree Git output at last successful query.") != std::string::npos &&
              text.find("Staged Git output at last successful query.") == std::string::npos,
          "Native failure replaced/mislabeled last-good diff");
  Write(f.root / ".git/HEAD", head);
  f.Select("Content/after %s##.txt");
  Require(Access::GitDiffLineCount(f.ui) == 0, "Selection change kept another path's output");
  f.ClickDiff(1);
  f.Finish();
  text = f.Draw(true);
  const auto renamed = f.diff.Capture(f.scope);
  Require(renamed &&
              renamed->request.paths ==
                  std::vector<std::string>{"Content/after %s##.txt", "Content/before.txt"} &&
              text.find("Content/after %s##.txt") != std::string::npos &&
              text.find("Content/before.txt") != std::string::npos &&
              renamed->output.find("+rename payload") != std::string::npos,
          "Actual rename selection lost paths or interpreted UI format/ID characters");
  f.Select("Content/authoring.txt");
  std::string large;
  for (int i = 0; i < 1000; ++i)
    large += "large row " + std::to_string(i) + "\n";
  Write(f.root / "Content/authoring.txt", large);
  f.ClickDiff(0);
  f.Finish();
  f.Draw();
  Require(f.diff.Capture(f.scope)->output.find("+large row 999") != std::string::npos &&
              Access::GitDiffLineCount(f.ui) > 1000 && Access::GitDiffRenderedLines(f.ui) > 0 &&
              Access::GitDiffRenderedLines(f.ui) < 100 && Read(f.root / ".git/index") == index &&
              Read(f.root / "Content/authoring.txt") == large,
          "Large diff omitted retained data, rendered every row or wrote sources");
  f.Click(1);
  f.Finish();
  Require(Access::GitDiffLineCount(f.ui) == 0 &&
              f.Draw(true).find("Select a tracked path") != std::string::npos,
          "New status generation retained old selection/output");
  Write(f.root / "Content/untracked.txt", "untracked\n");
  f.Click(1);
  f.Finish();
  f.Select("Content/untracked.txt");
  const auto before_denied = f.diff.Query().operation;
  f.ClickDiff(0);
  Require(f.diff.Query().operation == before_denied &&
              f.Draw(true).find("require tracked UTF-8") != std::string::npos,
          "Untracked selection fabricated a normal diff");
#if defined(__linux__)
  f.Select(raw_name);
  f.ClickDiff(1);
  Require(f.diff.Query().operation == before_denied &&
              f.Draw(true).find("require tracked UTF-8") != std::string::npos &&
              Read(f.root / raw_name) == "raw tracked source\n",
          "Non-UTF8 tracked path crossed the process argv boundary or lost source bytes");
#endif
  f.Select("Content/authoring.txt");
  const auto raw = f.CopyFault("raw");
  const auto overflow = f.CopyFault("overflow");
  const auto maximum = f.CopyFault("maximum");
  const auto sleeper = f.CopyFault("sleep");
  f.Tool(raw);
  f.Select("Content/authoring.txt");
  f.ClickDiff(0);
  f.Finish();
  text = f.Draw(true);
  Require(text.find("raw\\x00bytes") != std::string::npos && text.find('\0') == std::string::npos,
          "Raw diagnostic NUL bytes escaped into native text rendering");
  f.Tool(maximum);
  f.Select("Content/authoring.txt");
  f.ClickDiff(0);
  f.Finish();
  f.Draw();
  Require(f.diff.Capture(f.scope)->output.size() == editor::GitDiffProvider::kMaximumOutputBytes &&
              Access::GitDiffLineCount(f.ui) == 1,
          "Complete maximum-sized single-line diff output was lost");
  f.Tool(overflow);
  f.Select("Content/authoring.txt");
  f.ClickDiff(0);
  f.Finish();
  Require(f.diff.Query().error == editor::GitDiffError::OutputBudgetExceeded &&
              Access::GitDiffLineCount(f.ui) == 0 &&
              f.Draw(true).find("exceeds the supported diff budget") != std::string::npos,
          "Overflow displayed a partial patch or an old tool's result");
  f.Tool(sleeper);
  f.Select("Content/authoring.txt");
  f.ClickDiff(0);
  f.Marker();
  f.ClickDiff(2);
  f.Finish();
  Require(f.diff.Query().phase == DiffPhase::Cancelled && !f.diff.Busy(),
          "Actual diff Cancel failed to drain");
  std::filesystem::remove(f.root / ".fixture-ready");
  f.ClickDiff(0);
  f.Marker();
  f.allowed = false;
  f.Draw();
  f.Finish();
  Require(f.diff.Query().phase == DiffPhase::Cancelled && !Access::GitDiffLineCount(f.ui),
          "Revoked interaction published an in-flight diff");
  f.allowed = true;
  std::filesystem::remove(f.root / ".fixture-ready");
  f.ClickDiff(0);
  f.Marker();
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  f.Finish();
  Require(f.diff.Query().phase == DiffPhase::Cancelled, "Focus loss failed to cancel diff");
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  std::filesystem::remove(f.root / ".fixture-ready");
  f.ClickDiff(0);
  f.Marker();
  f.scope = 12;
  f.Draw();
  f.Finish();
  f.scope = 11;
  f.Draw();
  Require(!f.diff.Capture(11) && !f.diff.Capture(12) && !Access::GitDiffLineCount(f.ui),
          "Scope switch-back revived old diff output");
  std::filesystem::remove(f.root / ".fixture-ready");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "Read-only diff observer failed");
  f.active = &f.reader;
  f.scope = 13;
  f.Draw();
  f.Tool(git);
  f.Click(1);
  f.Finish();
  f.Select("Content/authoring.txt");
  f.ClickDiff(0);
  f.Finish();
  Require(f.diff.Query().phase == DiffPhase::Ready && Read(f.root / ".git/index") == index &&
              Read(f.root / "Content/authoring.txt") == large,
          "Read-only diff inspection failed or changed index/source");
  f.active = nullptr;
  f.scope = 0;
  f.Draw();
  Require(!f.diff.Capture(13) && !Access::GitDiffLineCount(f.ui),
          "Project close retained old diff display");
  f.active = &f.workspace;
  f.scope = 14;
  f.Draw();
  f.Tool(git);
  f.Click(1);
  f.Finish();
  f.Tool(sleeper);
  f.Select("Content/authoring.txt");
  f.ClickDiff(0);
  f.Marker();
  f.diff.Shutdown();
  f.Draw();
  f.ClickDiff(0);
  Require(!f.diff.Busy() && f.diff.Query().phase == DiffPhase::Stopped &&
              f.Draw(true).find("Diff queries are unavailable") != std::string::npos &&
              Read(f.root / ".git/index") == index &&
              Read(f.root / "Content/authoring.txt") == large,
          "Stopped provider accepted work or teardown changed source data");
}
} // namespace
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    Require((argc == 4 || argc == 5) && argv[3][1] == 0 && (argv[3][0] == '1' || argv[3][0] == '2'),
            "Expected actual Git executable, native negative fixture and1/2 scale");
    if (argc == 5)
      RunDiff(argv[3][0] == '2' ? 2.F : 1.F, std::filesystem::absolute(argv[1]),
              std::filesystem::absolute(argv[2]));
    else
      Run(argv[3][0] == '2' ? 2.F : 1.F, std::filesystem::absolute(argv[1]),
          std::filesystem::absolute(argv[2]));
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
