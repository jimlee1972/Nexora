#include "EditorIcons.h"
#include "EditorImGuiTestAccess.h"
#include "Nexora/EditorImGui/EditorImGui.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
using nexora::editor::ProjectWorkspace;
using nexora::editor::imgui::EditorImGuiHost;
using nexora::editor::imgui::EditorImGuiTestAccess;
using nexora::editor::imgui::RecoveryChoice;
using Nexora::Window::Key;
using Nexora::Window::WindowEvent;
using Nexora::Window::WindowEventType;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  Require(bool(stream), "could not read recovery fixture");
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
void Write(const std::filesystem::path &path, const std::string &bytes) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << bytes;
  Require(bool(stream), "could not write recovery fixture");
}
struct TemporaryRoot {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("nexora-recovery-scope-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TemporaryRoot() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
struct Fixture {
  TemporaryRoot temporary;
  ProjectWorkspace a, b;
  EditorImGuiHost host;
  nexora::editor::ProductShell shell;
  float dpi;
  explicit Fixture(float scale = 1.0F) : dpi(scale) {
    std::string error;
    Require(a.Create(temporary.path / "a", "Recovery A", &error), "create A failed");
    Require(b.Create(temporary.path / "b", "Recovery B", &error), "create B failed");
    Write(Journal(a), "schema=1\ndocument=Recovered-A.scene\n");
    Write(Journal(b), "schema=1\ndocument=Recovered-B.scene\n");
    host.SetDisplay(1280.0F / dpi, 720.0F / dpi, dpi);
    EditorImGuiTestAccess::ConfigureSyntheticInput(host);
    Send(WindowEventType::FocusChanged, 1);
  }
  static std::filesystem::path Journal(const ProjectWorkspace &workspace) {
    return workspace.Root() / ".nexora/workspace.recovery";
  }
  void Send(WindowEventType type, int value0, int value1 = 0) {
    WindowEvent event{};
    event.type = type;
    event.value0 = value0;
    event.value1 = value1;
    host.ProcessEvents(std::array{event});
  }
  void Frame(ProjectWorkspace *workspace) {
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, workspace);
    static_cast<void>(host.EndFrame());
  }
  void Settle(ProjectWorkspace *workspace) {
    for (unsigned frame = 0; frame < 6; ++frame)
      Frame(workspace);
  }
  static ImGuiWindow *Modal() { return ImGui::GetTopMostPopupModal(); }
  void FocusRecover(ProjectWorkspace &workspace) {
    // Default NavId alone does not enable keyboard activation in ImGui. Enter navigation with
    // actual Tab transitions, retaining visible focus and the precise production button ID.
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
      const auto &context = *ImGui::GetCurrentContext();
      auto *modal = Modal();
      if (modal && context.NavCursorVisible && context.NavWindow == modal &&
          context.NavId == modal->GetID("Recover"))
        return;
      Send(WindowEventType::Key, static_cast<int>(Key::Tab), 1);
      Frame(&workspace);
      Send(WindowEventType::Key, static_cast<int>(Key::Tab), 0);
      Frame(&workspace);
      Frame(&workspace);
    }
    throw std::runtime_error("actual Tab navigation did not visibly focus Recover");
  }
  static ImGuiID DiscardId() {
    auto *window = Modal();
    Require(window != nullptr, "recovery modal is absent");
    return window->GetID("Discard recovery data");
  }
  void HoverDiscard(ProjectWorkspace &workspace) {
    auto *window = Modal();
    Require(window != nullptr, "recovery modal is absent");
    // CursorPosPrevLine is the actual last submitted button's end/top, retained in this
    // window's layout state. Do not assume popup placement, a fixed label width or screen DPI.
    const auto button_width = nexora::editor::imgui::detail::IconButtonExtent();
    const auto x = window->DC.CursorPosPrevLine.x - button_width * 0.5F;
    const auto y = window->DC.CursorPosPrevLine.y + button_width * 0.5F;
    Send(WindowEventType::Pointer, static_cast<int>(x * dpi), static_cast<int>(y * dpi));
    Frame(&workspace);
  }
  void PressDiscard(ProjectWorkspace &workspace) {
    HoverDiscard(workspace);
    const auto expected_id = DiscardId();
    Send(WindowEventType::PointerButton, 0, 1);
    Frame(&workspace);
    Require(ImGui::GetActiveID() == expected_id, "pointer did not press actual Discard button");
  }
  void Discard(ProjectWorkspace &workspace) {
    Settle(&workspace);
    PressDiscard(workspace);
    Send(WindowEventType::PointerButton, 0, 0);
    Frame(&workspace);
  }
};

void PointerReplacement(bool detach, float dpi) {
  Fixture f(dpi);
  const auto a_before = Read(Fixture::Journal(f.a));
  const auto b_before = Read(Fixture::Journal(f.b));
  f.Settle(&f.a);
  f.PressDiscard(f.a);
  f.Send(WindowEventType::PointerButton, 0, 0);
  f.Frame(detach ? nullptr : &f.b);
  Require(Read(Fixture::Journal(f.a)) == a_before && Read(Fixture::Journal(f.b)) == b_before,
          "old-scope release discarded a journal");
  Require(f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "old-scope release published a recovery choice");
  if (detach) {
    Require(Fixture::Modal() == nullptr, "detaching retained the recovery modal");
    f.Settle(&f.b);
  }
  f.Discard(f.b);
  // Same-scope Draw sees the removed journal but must not erase the unconsumed success.
  f.Frame(&f.b);
  Require(!f.b.HasRecoveryJournal() && Read(Fixture::Journal(f.a)) == a_before,
          "fresh B action did not preserve A");
  Require(Fixture::Modal() == nullptr, "successful discard retained the modal");
  Require(f.host.TakeRecoveryChoice() == RecoveryChoice::Discard &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "successful same-scope result was lost or delivered twice");
}

void CopiedIdentityBoundaries(bool root_only) {
  Fixture f;
  std::string error;
  if (root_only) {
    const auto root = f.b.Root();
    f.b = {};
    Write(root / "project.nexora", Read(f.a.Root() / "project.nexora"));
    Require(f.b.Open(root, &error), "same-UUID B reopen failed");
    Require(f.a.Project().id == f.b.Project().id && f.a.Root() != f.b.Root(),
            "root-only scope fixture is not distinct");
  }
  f.Settle(&f.a);
  const auto a_root = f.a.Root();
  const auto a_before = Read(Fixture::Journal(f.a));
  f.PressDiscard(f.a);
  ProjectWorkspace *replacement = &f.b;
  if (!root_only) {
    // Reuse the very same C++ object and canonical root with a different UUID.
    const auto previous_id = f.a.Project().id;
    f.a = {};
    Write(a_root / "project.nexora", Read(f.b.Root() / "project.nexora"));
    Require(f.a.Open(a_root, &error), "new-UUID A reopen failed");
    Require(f.a.Project().id != previous_id, "UUID-only fixture did not change identity");
    replacement = &f.a;
  }
  const auto replacement_before = Read(Fixture::Journal(*replacement));
  f.Send(WindowEventType::PointerButton, 0, 0);
  f.Frame(replacement);
  Require(Read(a_root / ".nexora/workspace.recovery") == a_before &&
              Read(Fixture::Journal(*replacement)) == replacement_before &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "root/UUID scope replacement reused an old gesture");
  f.Discard(*replacement);
  Require(!replacement->HasRecoveryJournal() &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::Discard,
          "new copied scope could not deliberately discard");
}

void KeyboardReplacement(Key key, ImGuiKey gui_key) {
  Fixture f;
  const auto a_before = Read(Fixture::Journal(f.a));
  const auto b_before = Read(Fixture::Journal(f.b));
  f.Settle(&f.a);
  f.FocusRecover(f.a);
  Require(ImGui::GetCurrentContext()->NavId == Fixture::Modal()->GetID("Recover"),
          "recovery default keyboard focus is absent");
  // NewFrame converts this old-scope key into actual navigation activation before Draw learns
  // that its caller replaced the project. It must not activate B's same-named Recover button.
  f.Send(WindowEventType::Key, static_cast<int>(key), 1);
  f.Frame(&f.b);
  for (unsigned frame = 0; frame < 45; ++frame)
    f.Frame(&f.b);
  Require(ImGui::GetKeyData(gui_key)->Down && Read(Fixture::Journal(f.a)) == a_before &&
              Read(Fixture::Journal(f.b)) == b_before &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "held old-scope navigation activated the replacement project");
  f.Send(WindowEventType::Key, static_cast<int>(key), 0);
  f.Frame(&f.b);
  f.Settle(&f.b);
  f.FocusRecover(f.b);
  Require(ImGui::GetCurrentContext()->NavId == Fixture::Modal()->GetID("Recover"),
          "replacement modal lost keyboard focus");
  f.Send(WindowEventType::Key, static_cast<int>(key), 1);
  f.Frame(&f.b);
  f.Send(WindowEventType::Key, static_cast<int>(key), 0);
  f.Frame(&f.b);
  if (f.b.HasRecoveryJournal()) {
    const auto &context = *ImGui::GetCurrentContext();
    std::cerr << "fresh recovery key=" << static_cast<int>(key) << " nav=" << context.NavId
              << " visible=" << context.NavCursorVisible << " active=" << context.ActiveId
              << " activate=" << context.NavActivateId << " error=" << f.host.RecoveryError()
              << '\n';
  }
  Require(!f.b.HasRecoveryJournal() && Read(Fixture::Journal(f.a)) == a_before &&
              Read(f.b.Root() / ".nexora/workspace").find("Recovered-B.scene") != std::string::npos,
          "fresh keyboard recovery did not apply only to B");
  Require(f.host.TakeRecoveryChoice() == RecoveryChoice::Recover &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "fresh keyboard recovery result was not exactly once");
}

void DeferredInputReplacement() {
  Fixture f;
  f.Settle(&f.a);
  f.HoverDiscard(f.a);
  const auto a_before = Read(Fixture::Journal(f.a));
  const auto b_before = Read(Fixture::Journal(f.b));
  ImGui::GetIO().ConfigInputTrickleEventQueue = true;
  // Both transitions were admitted against A. Trickling defers their release into B's frames.
  f.Send(WindowEventType::PointerButton, 0, 1);
  f.Send(WindowEventType::PointerButton, 0, 0);
  f.Settle(&f.b);
  Require(Read(Fixture::Journal(f.a)) == a_before && Read(Fixture::Journal(f.b)) == b_before &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "deferred old-scope mouse events activated B");
  f.Discard(f.b);
  Require(!f.b.HasRecoveryJournal() && f.host.TakeRecoveryChoice() == RecoveryChoice::Discard,
          "quarantine did not release after deferred input drained");
}

void NewlyDiscoveredJournal() {
  Fixture f;
  Require(f.a.DiscardRecovery(), "initial journal removal failed");
  f.Settle(&f.a);
  Require(Fixture::Modal() == nullptr, "journal-free scope unexpectedly has a modal");
  f.Send(WindowEventType::Key, static_cast<int>(Key::Enter), 1);
  f.Frame(&f.a);
  Write(Fixture::Journal(f.a), "schema=1\ndocument=Recovered-A.scene\n");
  const auto before = Read(Fixture::Journal(f.a));
  f.Settle(&f.a);
  Require(Fixture::Modal() != nullptr && Read(Fixture::Journal(f.a)) == before &&
              f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "held underlying input activated a newly discovered recovery prompt");
  f.Send(WindowEventType::Key, static_cast<int>(Key::Enter), 0);
  f.Settle(&f.a);
  f.FocusRecover(f.a);
  f.Send(WindowEventType::Key, static_cast<int>(Key::Enter), 1);
  f.Frame(&f.a);
  f.Send(WindowEventType::Key, static_cast<int>(Key::Enter), 0);
  f.Frame(&f.a);
  Require(!f.a.HasRecoveryJournal() && f.host.TakeRecoveryChoice() == RecoveryChoice::Recover,
          "newly discovered prompt rejected a fresh keyboard action");
}

void JournalRemovalAndDirectApi() {
  Fixture f;
  f.Settle(&f.a);
  f.PressDiscard(f.a);
  Require(f.a.DiscardRecovery(), "external discard failed");
  f.Send(WindowEventType::PointerButton, 0, 0);
  f.Frame(&f.a);
  Require(Fixture::Modal() == nullptr && f.host.TakeRecoveryChoice() == RecoveryChoice::None,
          "journal removal retained a stale modal/action");

  // Direct calls remain supported without any preceding Draw. Failure persists on same-scope
  // Draw, successful retries preserve their result, and a new scope clears old errors/results.
  EditorImGuiHost direct;
  direct.SetDisplay(1280, 720, 1);
  Write(Fixture::Journal(f.a), "schema=1\ninvalid record\n");
  Require(!direct.ApplyRecoveryChoice(f.a, RecoveryChoice::Recover) &&
              !direct.RecoveryError().empty(),
          "direct recover failure lost its error");
  const auto failure = std::string(direct.RecoveryError());
  direct.BeginFrame();
  direct.DrawProductShell(f.shell, nullptr, &f.a);
  static_cast<void>(direct.EndFrame());
  Require(direct.RecoveryError() == failure, "same-scope Draw cleared a recover failure");
  Require(direct.ApplyRecoveryChoice(f.a, RecoveryChoice::Discard), "direct retry failed");
  Require(!direct.ApplyRecoveryChoice(f.a, RecoveryChoice::Recover),
          "same-scope duplicate choice was accepted");
  direct.BeginFrame();
  direct.DrawProductShell(f.shell, nullptr, &f.a);
  static_cast<void>(direct.EndFrame());
  Require(direct.TakeRecoveryChoice() == RecoveryChoice::Discard &&
              direct.TakeRecoveryChoice() == RecoveryChoice::None,
          "same-scope direct success was lost or repeated");
  Write(Fixture::Journal(f.a), "schema=1\ninvalid record\n");
  Require(!direct.ApplyRecoveryChoice(f.a, RecoveryChoice::Recover), "invalid recovery succeeded");
  direct.BeginFrame();
  direct.DrawProductShell(f.shell, nullptr, &f.b);
  static_cast<void>(direct.EndFrame());
  Require(direct.RecoveryError().empty() && direct.TakeRecoveryChoice() == RecoveryChoice::None,
          "replacement scope retained the old error/result");
  Require(direct.ApplyRecoveryChoice(f.b, RecoveryChoice::Discard), "direct B discard failed");
  direct.BeginFrame();
  direct.DrawProductShell(f.shell, nullptr, nullptr);
  static_cast<void>(direct.EndFrame());
  Require(direct.TakeRecoveryChoice() == RecoveryChoice::None && Fixture::Modal() == nullptr,
          "detach retained an old pending result/modal");
  Write(Fixture::Journal(f.a), "schema=1\ndocument=Recovered-A.scene\n");
  Require(direct.ApplyRecoveryChoice(f.a, RecoveryChoice::Discard), "direct A discard failed");
  Write(Fixture::Journal(f.b), "schema=1\ndocument=Recovered-B.scene\n");
  Require(direct.ApplyRecoveryChoice(f.b, RecoveryChoice::Recover),
          "direct scope replacement retained the previous duplicate guard");
  Require(direct.TakeRecoveryChoice() == RecoveryChoice::Recover &&
              direct.TakeRecoveryChoice() == RecoveryChoice::None,
          "direct replacement did not publish exactly one current result");
}
} // namespace

int main() {
  try {
    for (const float dpi : {1.0F, 2.0F}) {
      PointerReplacement(false, dpi);
      PointerReplacement(true, dpi);
    }
    CopiedIdentityBoundaries(true);
    CopiedIdentityBoundaries(false);
    KeyboardReplacement(Key::Enter, ImGuiKey_Enter);
    KeyboardReplacement(Key::Space, ImGuiKey_Space);
    DeferredInputReplacement();
    NewlyDiscoveredJournal();
    JournalRemovalAndDirectApi();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
