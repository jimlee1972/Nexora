#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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
  editor::ProjectWorkspace *workspace_override{};
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
    auto *project = workspace_override    ? workspace_override
                    : active == &readonly ? &observer
                                          : &workspace;
    ui.SetSceneFileContext({project->Project().id, scene.Generation()}, "Content/Untitled.scene");
    ui.DrawProductShell(shell, &scene, project, active);
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
void FolderClipping(float dpi) {
  using Key = Nexora::Window::Key;
  Fixture f(dpi);
  const auto files = f.Files();
  std::vector<editor::ContentItem> items;
  std::vector<std::filesystem::path> folders;
  for (std::uint64_t i = 0; i < 2048; ++i) {
    const auto number = std::to_string(10000 + i);
    const auto folder = std::filesystem::path("Content") / ("Folder" + number + "###same");
    folders.push_back(folder);
    items.push_back(
        {{300, i + 1}, folder / "RootNested.txt", ".txt", "v1", editor::ThumbnailState::Ready});
  }
  const runtime::AssetUuid first{301, 1}, last{301, 2};
  items.push_back({first, "Content/RootA.txt", ".txt", "v1", editor::ThumbnailState::Ready});
  items.push_back({last, "Content/RootZ.txt", ".txt", "v1", editor::ThumbnailState::Ready});
  const auto key = [&](Key value) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(value);
    for (int pressed : {1, 0}) {
      event.value1 = pressed;
      f.ui.ProcessEvents(std::array{event});
      f.Draw();
    }
  };
  auto *window = ImGui::FindWindowByName("Content###nexora.content");
  std::uint32_t maximum_submitted = 0;
  const auto bounded = [&] {
    const auto state = Access::Inspect(f.ui);
    maximum_submitted = std::max(maximum_submitted, state.content_rendered_folders);
    Require(state.content_visible_folders == folders.size() && state.content_rendered_folders < 128,
            "Content submitted all folder rows instead of a clipped range");
  };
  for (bool readonly : {false, true}) {
    f.active = readonly ? &f.readonly : &f.content;
    auto &browser = f.active->Browser();
    Require(browser.Reset(items, readonly ? 6 : 5) && browser.Select(first) &&
                browser.Rename(first, "RootRenamed.txt") && browser.SetFolder("Content"),
            "clipped folder model setup failed");
    browser.SetFilter("Root", ".txt");
    const auto revision = browser.Revision();
    Access::FocusContent(f.ui);
    ImGui::SetScrollY(window, 0);
    f.Draw();
    f.Draw();
    bounded();
    Require(Access::ContentPathPosition(f.ui, folders.front()) &&
                !Access::ContentPathPosition(f.ui, folders.back()) && window->ScrollMax.y > 20000,
            "folder clipping lost total scroll extent or exposed the far row");
    ImGui::SetScrollY(window, window->ScrollMax.y * 0.5F);
    f.Draw();
    f.Draw();
    bounded();
    std::size_t edge = 0;
    for (std::size_t i = 0; i + 1 < folders.size(); ++i) {
      if (const auto point = Access::ContentPathPosition(f.ui, folders[i]);
          point && window->InnerClipRect.Contains(ImVec2((*point)[0], (*point)[1])))
        edge = i;
    }
    Require(edge > 900 && edge < 1200, "middle scrolling did not reach actual folder controls");
    f.Click(Access::ContentPathPosition(f.ui, folders[edge]));
    // The first Tab after mouse input reveals the current navigation cursor.
    key(Key::Tab);
    Require(Access::ContentFocusedFolder(f.ui) == folders[edge],
            "first Tab lost the clicked folder focus");
    key(Key::Tab);
    f.Draw();
    f.Draw();
    if (Access::ContentFocusedFolder(f.ui) != folders[edge + 1])
      throw std::runtime_error(
          "Tab clipped boundary expected=" + folders[edge + 1].string() +
          " actual=" + Access::ContentFocusedFolder(f.ui).value_or("none").string() +
          " scroll=" + std::to_string(window->Scroll.y) +
          " nav=" + std::to_string(ImGui::GetCurrentContext()->NavId));
    bounded();
    key(Key::Enter);
    f.Draw();
    f.Draw();
    Require(browser.Breadcrumbs().back().path == folders[edge + 1] &&
                browser.Selection() == std::vector{first} && browser.VisibleCount() == 1,
            "clipped folder keyboard entry changed selection/filter or opened the wrong folder");
    f.Click(Access::ContentPathPosition(f.ui, "Content", true));
    f.Draw();
    f.Draw();
    key(Key::End);
    f.Draw();
    f.Draw();
    bounded();
    const auto asset = Access::ContentAssetPosition(f.ui, last);
    Require(browser.Selection() == std::vector{last} && asset &&
                window->InnerClipRect.Contains(ImVec2((*asset)[0], (*asset)[1])),
            "asset End reveal failed below clipped folders");
    f.Click(Access::ContentPathPosition(f.ui, folders.back()));
    f.Click(Access::ContentPathPosition(f.ui, folders.back()));
    f.Draw();
    f.Draw();
    Require(browser.Breadcrumbs().back().path == folders.back() &&
                browser.Selection() == std::vector{last} && browser.Revision() == revision &&
                browser.Undo() && browser.Find(first)->path == "Content/RootA.txt" &&
                f.Files() == files,
            "far-folder pointer navigation lost model Undo or changed source/sidecar bytes");
  }
  std::cout << "Folder clipping dpi=" << dpi << " total=" << folders.size()
            << " maximum submitted=" << maximum_submitted << '\n';
}
// Verify the emitted glyph mesh, rather than a copy of the label supplied to a widget.
// Normalizing positions permits searching the full literal sequence in the actual draw list.
void RequireLiteral(std::string_view text) {
  ImDrawList expected(ImGui::GetDrawListSharedData());
  expected._ResetForNewFrame();
  expected.PushClipRectFullScreen();
  expected.PushTextureID(ImGui::GetFont()->ContainerAtlas->TexID);
  expected.AddText(ImVec2(0, 0), IM_COL32_WHITE, text.data(), text.data() + text.size());
  Require(!expected.VtxBuffer.empty(), "literal expectation has no glyphs");
  const auto *window = ImGui::FindWindowByName("Content###nexora.content");
  const auto &actual = window->DrawList->VtxBuffer;
  for (int start = 0; start + expected.VtxBuffer.Size <= actual.Size; ++start) {
    bool match = true;
    const auto offset = ImVec2(actual[start].pos.x - expected.VtxBuffer[0].pos.x,
                               actual[start].pos.y - expected.VtxBuffer[0].pos.y);
    for (int i = 0; i < expected.VtxBuffer.Size && match; ++i) {
      const auto &a = actual[start + i];
      const auto &e = expected.VtxBuffer[i];
      match = a.uv.x == e.uv.x && a.uv.y == e.uv.y &&
              std::abs(a.pos.x - offset.x - e.pos.x) < 0.01F &&
              std::abs(a.pos.y - offset.y - e.pos.y) < 0.01F;
    }
    if (match)
      return;
  }
  throw std::runtime_error("full literal Content glyph sequence missing: " + std::string(text));
}
void MarkerLabels(float dpi) {
  Fixture f(dpi);
  using Key = Nexora::Window::Key;
  const auto name = std::filesystem::path(u8"同名##%###same.scene");
  const std::filesystem::path foo = "Content/Foo###same", bar = "Content/Bar###same";
  const auto nested = foo / "Nested##%###same";
  for (const auto &folder : {foo, bar, nested}) {
    std::filesystem::create_directories(f.root / folder);
    std::ofstream(f.root / folder / name) << "unchanged scene source";
  }
  std::ofstream(f.root / "Content" / name) << "root source";
  Require(f.assets.ImportTree(f.root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite) &&
              f.content.Open(f.workspace, f.assets, 7, true) &&
              f.readonly.Open(f.observer, f.assets, 8, false),
          "marker content reopen failed");
  const auto files = f.Files();
  const auto key = [&](Key value) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(value);
    event.value1 = 1;
    f.ui.ProcessEvents(std::array{event});
    f.Draw();
    event.value1 = 0;
    f.ui.ProcessEvents(std::array{event});
    f.Draw();
  };
  const auto current = [&] { return f.active->Browser().Breadcrumbs().back().path; };
  const auto asset_at = [&](const std::filesystem::path &path) {
    for (const auto &item : f.active->Browser().Items())
      if (item.path == path)
        return item.id;
    throw std::runtime_error("marker asset missing");
  };
  const auto focus_folder = [&](const std::filesystem::path &path) {
    for (int i = 0; i < 180 && Access::ContentFocusedFolder(f.ui) != path; ++i) {
      key(Key::Tab);
    }
    if (Access::ContentFocusedFolder(f.ui) != path) {
      auto *g = ImGui::GetCurrentContext();
      throw std::runtime_error(
          "Tab marker folder: " + path.string() + " current=" + current().string() +
          " focused=" + Access::ContentFocusedFolder(f.ui).value_or("<none>").string() +
          " nav=" + (g->NavWindow ? std::string(g->NavWindow->Name) : "<none>") +
          " text=" + std::to_string(g->IO.WantTextInput));
    }
  };
  for (bool readonly : {false, true}) {
    f.active = readonly ? &f.readonly : &f.content;
    auto &browser = f.active->Browser();
    Require(browser.SetFolder("Content"), "marker root failed");
    Access::FocusContent(f.ui);
    f.Draw();
    f.Draw();
    const auto revision = browser.Revision();
    // The two ###same suffixes collided before this fix. Pointer double-click and
    // real Tab/Enter must independently open each sibling without opening a scene.
    RequireLiteral("[Folder] Foo###same");
    RequireLiteral("[Folder] Bar###same");
    for (const auto &folder : {foo, bar}) {
      const auto point = Access::ContentPathPosition(f.ui, folder);
      f.Click(point);
      f.Click(point);
      Require(current() == folder, "pointer opened the wrong marker sibling");
      f.Draw();
      f.Draw();
      Require(browser.ChildFolders().size() == (folder == foo ? 1U : 0U) &&
                  !Access::ContentPathPosition(f.ui, "Content") &&
                  Access::ContentPathPosition(f.ui, "Content", true).has_value(),
              "nested view rendered a parent row or lost the explicit parent breadcrumb");
      RequireLiteral(folder == foo ? "Foo###same" : "Bar###same");
      const auto id = asset_at(folder / name);
      Require(browser.Select(id), "marker asset selection failed");
      f.Draw();
      f.Draw();
      RequireLiteral(std::string("[Ready] ") +
                     reinterpret_cast<const char *>(u8"同名##%###same.scene"));
      for (int i = 0; i < 180 && Access::ContentFocusedAsset(f.ui) != id; ++i)
        key(Key::Tab);
      Require(Access::ContentFocusedAsset(f.ui) == id, "Tab could not focus marker scene row");
      key(Key::Enter);
      const auto request = f.ui.TakeSceneFileRequest();
      Require(request && request->path == folder / name,
              "focused marker asset Enter did not request the owning scene");
      f.Click(Access::ContentPathPosition(f.ui, "Content", true));
      f.Draw();
      f.Draw();
      Require(current() == "Content", "root breadcrumb click failed");
      focus_folder(folder);
      key(Key::Enter);
      Require(current() == folder && !f.ui.TakeSceneFileRequest(),
              "folder Enter opened wrong sibling or selected scene");
      f.Click(Access::ContentPathPosition(f.ui, "Content", true));
      f.Draw();
      f.Draw();
    }
    focus_folder(foo);
    key(Key::Enter);
    f.Draw();
    f.Draw();
    focus_folder(nested);
    key(Key::Enter);
    f.Draw();
    f.Draw();
    Require(current() == nested && browser.ChildFolders().empty() &&
                !Access::ContentPathPosition(f.ui, foo) &&
                Access::ContentPathPosition(f.ui, foo, true).has_value(),
            "nested marker leaf retained an unrelated parent row or lost its breadcrumb");
    RequireLiteral("Nested##%###same");
    f.Click(Access::ContentPathPosition(f.ui, foo, true));
    f.Draw();
    f.Draw();
    Require(current() == foo, "literal marker breadcrumb did not navigate");
    Require(browser.Revision() == revision && !f.active->CanUndo() && f.Files() == files,
            "marker navigation mutated source, sidecars or history");
  }
  // Scope changes must produce distinct live row IDs even for copied paths and UUIDs.
  f.active = &f.content;
  Require(f.content.Browser().SetFolder(foo), "scope folder setup failed");
  f.Draw();
  f.Draw();
  const auto id = asset_at(foo / name);
  f.Click(Access::ContentAssetPosition(f.ui, id));
  const auto old_nav = ImGui::GetCurrentContext()->NavId;
  Require(f.content.Open(f.workspace, f.assets, 9, true) && f.content.Browser().SetFolder(foo),
          "generation replacement failed");
  f.Draw();
  f.Draw();
  f.Click(Access::ContentAssetPosition(f.ui, id));
  Require(ImGui::GetCurrentContext()->NavId != old_nav, "generation reused marker control ID");
  // Inspection with a pending file transaction must preserve exactly that Undo.
  Require(f.content.Rename(f.source, "Retained.scene"), "pending marker Undo setup failed");
  const auto pending_files = f.Files();
  const auto pending_revision = f.content.Browser().Revision();
  Require(f.content.Browser().SetFolder("Content"), "pending root failed");
  f.Draw();
  f.Draw();
  focus_folder(bar);
  key(Key::Enter);
  f.Draw();
  f.Draw();
  f.Click(Access::ContentPathPosition(f.ui, "Content", true));
  Require(f.content.CanUndo() && f.content.Browser().Revision() == pending_revision &&
              f.Files() == pending_files && f.content.Undo() && f.Files() == files,
          "marker inspection consumed or changed pending Undo");

  editor::ProjectWorkspace other;
  editor::AssetWorkspace copied_assets;
  editor::ProjectContentSession copied_content;
  Require(other.Create(f.root / "OtherProject", "Copied marker project"), "other project failed");
  std::filesystem::copy(f.root / "Content", other.Root() / "Content",
                        std::filesystem::copy_options::recursive |
                            std::filesystem::copy_options::overwrite_existing);
  Require(copied_assets.ImportTree(other.Root() / "Content", {}, {},
                                   editor::AssetIdentityMode::PersistentReadWrite) &&
              copied_content.Open(other, copied_assets, 9, true) &&
              copied_content.Browser().SetFolder(foo),
          "copied scope setup failed");
  Require(f.content.Browser().SetFolder(foo), "original scope folder failed");
  f.Draw();
  f.Draw();
  f.Click(Access::ContentAssetPosition(f.ui, id));
  const auto original_root_nav = ImGui::GetCurrentContext()->NavId;
  f.active = &copied_content;
  f.workspace_override = &other;
  f.Draw();
  f.Draw();
  Require(copied_content.Browser().Find(id), "copied source did not preserve UUID");
  f.Click(Access::ContentAssetPosition(f.ui, id));
  Require(ImGui::GetCurrentContext()->NavId != original_root_nav,
          "copied root reused marker control ID at same generation");
  Require(f.Files() == files, "scope replacement wrote source files");
}
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
void FolderDropSnapshot(float dpi) {
  Fixture f(dpi);
  const auto files = f.Files();
  const auto retained = f.content.Browser().ChildFolderSnapshot();
  Require(f.content.Browser().Select(f.source), "folder drop selection failed");
  f.Draw();
  const auto source = Access::ContentAssetPosition(f.ui, f.source);
  const auto target = Access::ContentPathPosition(f.ui, "Content/Nested");
  Require(source && target, "folder drop controls absent");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>((*source)[0] * dpi);
  pointer.value1 = static_cast<int>((*source)[1] * dpi);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  pointer.value0 += static_cast<int>(24 * dpi);
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  f.Draw();
  Require(Access::ContentDragActive(f.ui), "folder drop drag did not start");
  pointer.value0 = static_cast<int>((*target)[0] * dpi);
  pointer.value1 = static_cast<int>((*target)[1] * dpi);
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  f.Draw();
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  f.Draw();
  Require(f.content.Browser().Find(f.source)->path == "Content/Nested/Source.scene" &&
              retained->front().path == "Content/Nested" &&
              f.content.Browser().ChildFolderSnapshot() != retained && f.content.Undo(),
          "real folder drop failed publication or invalidated retained projection");
  f.Draw();
  Require(f.Files() == files && f.content.Browser().Find(f.source)->path == "Content/Source.scene",
          "folder drop Undo did not preserve source and sidecar bytes");
}
} // namespace
int main() {
  try {
    FolderDropSnapshot(1);
    FolderDropSnapshot(2);
    FolderClipping(1);
    FolderClipping(2);
    MarkerLabels(1);
    MarkerLabels(2);
    Run(1);
    Run(2);
    std::cout << "Literal Content labels and forward/reverse dependency Reveal contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
