#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-content-reveal-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace, observer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content, readonly;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Unchanged scene");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectContentSession *active = &content;
  runtime::AssetUuid source, target, duplicate, missing{99, 99};
  float dpi;
  Fixture(float scale) : dpi(scale) {
    Require(workspace.Create(root, "Dependency reveal"), "workspace create failed");
    std::filesystem::create_directories(root / "Content/Nested");
    std::ofstream(root / "Content/Source.scene") << "source";
    std::ofstream(root / std::filesystem::path(u8"Content/同名##%.txt")) << "unrelated";
    for (int i = 0; i < 128; ++i)
      std::ofstream(root / "Content/Nested" / ("A-" + std::to_string(i) + ".txt")) << i;
    std::ofstream(root / std::filesystem::path(u8"Content/Nested/同名##%.txt")) << "target";
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite) &&
                content.Open(workspace, assets, 5, true) &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) &&
                readonly.Open(observer, assets, 6, false),
            "content setup failed");
    for (const auto &item : content.Browser().Items()) {
      if (item.path == "Content/Source.scene")
        source = item.id;
      if (item.path == std::filesystem::path(u8"Content/同名##%.txt"))
        duplicate = item.id;
      if (item.path == std::filesystem::path(u8"Content/Nested/同名##%.txt"))
        target = item.id;
    }
    Require(source != runtime::AssetUuid{} && target != runtime::AssetUuid{} &&
                duplicate != runtime::AssetUuid{},
            "fixture identities missing");
    for (auto *session : {&content, &readonly})
      Require(session->Dependencies().Set(source, std::array{target, missing}),
              "dependency setup failed");
    ui.SetDisplay(1600, 1200, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    auto *content_window = ImGui::FindWindowByName("Content###nexora.content");
    Require(content_window, "Content window missing");
    ImGui::SetWindowDock(content_window, 0, ImGuiCond_Always);
    ImGui::SetWindowPos(content_window, ImVec2(40, 40));
    ImGui::SetWindowSize(content_window, ImVec2(1000, 900));
    Access::FocusContent(ui);
    Draw();
    Draw();
  }
  std::map<std::filesystem::path, std::string> Files() const {
    std::map<std::filesystem::path, std::string> result;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root / "Content"))
      if (entry.is_regular_file()) {
        std::ifstream input(entry.path(), std::ios::binary);
        result[entry.path().lexically_relative(root)] =
            std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
      }
    return result;
  }
  void Draw() {
    ui.BeginFrame();
    ui.SetSceneFileContext({workspace.Project().id, scene.Generation()}, "Content/Untitled.scene");
    ui.DrawProductShell(shell, &scene, active == &readonly ? &observer : &workspace, active);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::optional<std::array<float, 2>> point) {
    Require(point.has_value(), "expected control is not rendered");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer});
    Draw();
    ui.ProcessEvents(std::array{button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Enter() {
    Nexora::Window::WindowEvent key;
    key.type = Nexora::Window::WindowEventType::Key;
    key.value0 = static_cast<int>(Nexora::Window::Key::Enter);
    key.value1 = 1;
    ui.ProcessEvents(std::array{key});
    Draw();
    key.value1 = 0;
    ui.ProcessEvents(std::array{key});
    Draw();
  }
  void SelectSource() {
    Require(active->Browser().RevealAsset(source), "source select failed");
    // Bring details into view without changing selection or filters.
    auto *window = ImGui::FindWindowByName("Content###nexora.content");
    Require(window, "Content window missing");
    ImGui::SetScrollY(window, 0);
    Draw();
    Draw();
    if (!Access::ContentDependencyPosition(ui, target))
      Click(Access::ContentDependencyHeaderPosition(ui));
    Require(Access::ContentDependencyPosition(ui, target).has_value(), "forward link missing");
  }
  void CheckTarget() {
    Draw();
    Draw();
    const auto &browser = active->Browser();
    Require(browser.Selection() == std::vector{target} &&
                browser.Breadcrumbs().back().path == "Content/Nested" &&
                browser.VisibleCount() == 129,
            "Reveal did not navigate and clear both filters");
    const auto point = Access::ContentAssetPosition(ui, target);
    auto *window = ImGui::FindWindowByName("Content###nexora.content");
    Require(point && window && window->Scroll.y > 0 &&
                window->InnerClipRect.Contains(ImVec2((*point)[0], (*point)[1])),
            "far target was selected but not scrolled into the real clip region");
  }
};
void Run(float dpi) {
  Fixture f(dpi);
  const auto original_files = f.Files();
  f.SelectSource();
  f.active->Browser().SetFilter("Source", ".scene");
  f.Draw();
  f.Draw();
  const auto revision = f.content.Browser().Revision();
  const auto selection = f.content.Browser().Selection();
  f.Click(Access::ContentDependencyPosition(f.ui, f.missing));
  Require(f.content.Browser().Selection() == selection &&
              f.content.Browser().Revision() == revision && f.content.Browser().VisibleCount() == 1,
          "missing reference admitted Reveal or changed filters");
  f.Click(Access::ContentDependencyPosition(f.ui, f.target));
  f.CheckTarget();
  Require(f.content.Browser().Revision() == revision, "Reveal changed content revision");

  // Reverse references remain actionable while the target details are scrolled into view.
  auto *window = ImGui::FindWindowByName("Content###nexora.content");
  ImGui::SetScrollY(window, window->ScrollMax.y);
  f.Draw();
  f.Draw();
  if (!Access::ContentDependencyPosition(f.ui, f.source, true))
    f.Click(Access::ContentDependencyHeaderPosition(f.ui, true));
  f.Draw();
  f.Draw();
  ImGui::SetScrollY(window, window->ScrollMax.y);
  f.Draw();
  f.Draw();
  f.Click(Access::ContentDependencyPosition(f.ui, f.source, true));
  f.Draw();
  f.Draw();
  Require(f.content.Browser().Selection() == std::vector{f.source} &&
              f.content.Browser().Breadcrumbs().back().path == "Content",
          "reverse Reveal did not return to the owning source");

  // Only scrolling is deferred: a pending writer reveal must not retarget another generation.
  Require(f.readonly.Browser().RevealAsset(f.source), "observer source setup failed");
  f.SelectSource();
  f.Click(Access::ContentDependencyPosition(f.ui, f.target));
  f.active = &f.readonly;
  f.Draw();
  Require(f.readonly.Browser().Selection() == std::vector{f.source} &&
              f.readonly.Browser().Breadcrumbs().back().path == "Content",
          "writer Reveal retargeted a replacement session");

  // Read-only navigation and a real keyboard activation use the same model-only path.
  f.active = &f.readonly;
  f.SelectSource();
  f.readonly.Browser().SetFilter("Source", ".txt");
  Access::FocusContentDependency(f.ui, f.target);
  Require(!f.ui.TakeSceneFileRequest(), "pointer inspection requested a scene Open");
  f.Enter();
  Require(!f.ui.TakeSceneFileRequest(), "Reveal Enter also requested the selected scene Open");
  f.CheckTarget();
  Require(!f.readonly.Writable() && !f.readonly.CanUndo(), "read-only Reveal created history");

  f.active = &f.content;
  Require(f.content.Rename(f.source, "Renamed.scene"), "rename setup failed");
  f.SelectSource();
  f.Click(Access::ContentDependencyPosition(f.ui, f.target));
  f.CheckTarget();
  Require(f.content.CanUndo() && f.content.Undo() &&
              std::filesystem::is_regular_file(f.root / "Content/Source.scene") &&
              !std::filesystem::exists(f.root / "Content/Renamed.scene"),
          "Reveal consumed or replaced pending source/sidecar Undo");
  Require(f.Files() == original_files, "inspection changed source or identity sidecar bytes");
  f.SelectSource();
  f.Click(Access::ContentAssetPosition(f.ui, f.source));
  Require(!f.ui.TakeSceneFileRequest(), "single asset click requested a scene Open");
  f.Enter();
  const auto scene_open = f.ui.TakeSceneFileRequest();
  Require(scene_open && scene_open->action == editor::imgui::SceneFileAction::Open &&
              scene_open->path == "Content/Source.scene",
          "dependency Enter routing broke ordinary selected-scene Enter");
  f.Draw();
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  f.Click(Access::ContentDependencyPosition(f.ui, f.target));
  Require(f.content.Browser().Selection() == std::vector{f.source},
          "unfocused Reveal changed selection");
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  Require(f.content.Conflicts().Detect(f.source, "editor", "disk", true), "conflict setup failed");
  f.Draw();
  f.Draw();
  f.Click(Access::ContentDependencyPosition(f.ui, f.target));
  Require(f.content.Browser().Selection() == std::vector{f.source},
          "blocking conflict modal admitted dependency Reveal");
  Access::QueueContentConflictChoice(f.ui, f.source, editor::DirtyConflictChoice::Keep);
  f.Draw();
  f.Draw();
  f.SelectSource();
  f.Click(Access::ContentDependencyPosition(f.ui, f.target));
  f.active = nullptr;
  f.Draw();
  Require(!Access::ContentDependencyPosition(f.ui, f.target) &&
              !Access::ContentDependencyHeaderPosition(f.ui),
          "closed content retained controls");
  window = ImGui::FindWindowByName("Content###nexora.content");
  ImGui::SetScrollY(window, 0);
  f.active = &f.content;
  f.Draw();
  f.Draw();
  Require(window->Scroll.y == 0, "closed Content retained deferred reveal scrolling");
  Require(f.Files() == original_files, "input/close guards changed source or sidecar bytes");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Graphical forward/reverse dependency Reveal contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
