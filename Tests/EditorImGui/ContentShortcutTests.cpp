#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Key = Nexora::Window::Key;
using Mod = Nexora::Window::KeyModifiers;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-content-keys-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace, observer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::vector<runtime::AssetUuid> matching;
  float dpi;
  Mod command;
  Fixture(float scale, bool macos = false)
      : dpi(scale), command(macos ? Mod::Super : Mod::Control) {
    Require(workspace.Create(root, "Content shortcuts"), "project create failed");
    std::filesystem::create_directory(root / "Content/Child");
    for (int i = 0; i < 128; ++i)
      std::ofstream(root / "Content" / ((i % 2 ? "Other-" : "Match-") + std::to_string(i) + ".txt"))
          << i;
    std::ofstream(root / "Content/Child/Match-child.txt") << "child";
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite) &&
                content.Open(workspace, assets, 5, true) &&
                observer.Open(root, editor::ProjectAccess::ReadOnly),
            "content setup failed");
    for (const auto &item : content.Browser().Items())
      if (item.path.parent_path() == "Content" &&
          item.path.filename().string().starts_with("Match-"))
        matching.push_back(item.id);
    ui.SetDisplay(1280 * dpi, 900 * dpi, dpi);
    Access::ConfigureSyntheticInput(ui, macos);
    Focus(true);
    for (int i = 0; i < 4; ++i)
      Draw();
    FocusContent();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, nullptr, active, &content);
    static_cast<void>(ui.EndFrame());
  }
  void FocusContent() {
    Access::FocusContent(ui);
    Draw();
    Draw();
  }
  void Focus(bool focused) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = focused;
    ui.ProcessEvents(std::array{event});
  }
  void Press(Key key, Mod modifiers = Mod::None) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = Mod::None;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  bool AllMatching() const {
    const auto selected = content.Browser().Selection();
    return selected.size() == matching.size() &&
           std::ranges::all_of(matching, [&](auto id) { return content.Browser().IsSelected(id); });
  }
};
void Run(float dpi, bool macos) {
  Fixture f(dpi, macos);
  auto &browser = f.content.Browser();
  browser.SetFilter("Match", ".txt");
  f.Draw();
  f.Draw();
  const auto revision = browser.Revision();
  f.Press(Key::A, f.command);
  Require(f.AllMatching() && browser.Revision() == revision && !f.content.CanUndo(),
          "filtered select-all missed clipped rows, selected children or changed content history");
  f.Press(Key::Delete, Mod::Control);
  Require(browser.Items().size() == 129 && f.AllMatching(), "modified Delete admitted deletion");
  f.Press(Key::Delete);
  Require(browser.Items().size() == 65 && f.content.CanUndo(),
          "Delete did not commit the selected batch");
  for (const auto id : f.matching)
    Require(!browser.Find(id), "deleted asset retained in model");
  Require(std::filesystem::exists(f.root / "Content/Child/Match-child.txt") &&
              std::filesystem::exists(f.root / "Content/Other-1.txt"),
          "Delete affected unselected assets");
  Require(f.content.Undo() && f.AllMatching(),
          "one content Undo did not restore full batch selection");
  for (const auto id : f.matching) {
    const auto *item = browser.Find(id);
    Require(
        item && std::filesystem::exists(f.root / item->path) &&
            std::filesystem::exists(editor::AssetWorkspace::IdentitySidecar(f.root / item->path)),
        "Undo lost source, sidecar or stable UUID");
  }
  browser.SetFilter("no-such-asset");
  f.Draw();
  f.Press(Key::A, f.command);
  Require(browser.Selection().empty(), "empty filter retained stale selection");
  browser.SetFilter("Match");
  f.active = &f.observer;
  f.Draw();
  f.Press(Key::A, f.command);
  Require(f.AllMatching(), "read-only workspace blocked selection");
  const auto read_only_revision = browser.Revision();
  f.Press(Key::Delete);
  Require(browser.Revision() == read_only_revision && f.AllMatching(),
          "read-only Delete changed content");
  f.active = &f.workspace;
  f.Draw();
  Require(browser.SetFolder("Content/Child"), "folder navigation failed");
  f.Draw();
  f.Press(Key::A, f.command);
  Require(browser.Selection().size() == 1 &&
              browser.Find(browser.Selection().front())->path.parent_path() == "Content/Child",
          "select-all included assets outside the current folder");
  Require(browser.SetFolder("Content"), "root folder failed");
  browser.SetFilter("Match");
  Require(browser.Select(f.matching.front()), "selection setup failed");
  f.Draw();
  f.Focus(false);
  f.Draw();
  f.Press(Key::A, f.command);
  f.Press(Key::Delete);
  Require(browser.Selection().size() == 1 && browser.Items().size() == 129,
          "blur admitted shortcuts");
  f.Focus(true);
  Access::FocusHierarchy(f.ui);
  f.Draw();
  f.Draw();
  f.Press(Key::A, f.command);
  f.Press(Key::Delete);
  Require(browser.Selection().size() == 1 && browser.Items().size() == 129,
          "another panel's focus admitted Content shortcuts");
  f.FocusContent();
  // The focused search field owns Ctrl/Cmd+A and Delete.
  const auto point = Access::ContentSearchPosition(f.ui);
  Require(point.has_value(), "search field absent");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>((*point)[0] * dpi);
  pointer.value1 = static_cast<int>((*point)[1] * dpi);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer, button});
  f.Draw();
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  f.Press(Key::A, f.command);
  f.Press(Key::Delete);
  Require(browser.Selection().size() == 1 && browser.Items().size() == 129,
          "text input triggered asset selection/deletion");
  f.Press(Key::Escape);
  f.Draw();
  f.FocusContent();
  f.Press(Key::F2);
  f.Press(Key::A, f.command);
  f.Press(Key::Delete);
  Require(browser.Selection().size() == 1 && browser.Items().size() == 129,
          "asset Rename admitted selection/delete shortcuts");
  f.Press(Key::Escape);
  f.Draw();
  f.FocusContent();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Press(Key::A, f.command);
  f.Press(Key::Delete);
  Require(browser.Selection().size() == 1 && browser.Items().size() == 129,
          "close modal admitted shortcuts");
}
void ModelScale() {
  editor::ContentBrowserModel model;
  std::vector<editor::ContentItem> items;
  for (std::uint64_t i = 0; i < 100000; ++i) {
    editor::ContentItem item;
    item.id = {1, i + 1};
    item.path = "Content/Match-" + std::to_string(i) + (i % 2 ? ".bin" : ".txt");
    item.type = i % 2 ? ".bin" : ".txt";
    item.artifact_hash = "fixture";
    items.push_back(std::move(item));
  }
  Require(model.Reset(items, 9) && model.Select({1, 1}) && model.Rename({1, 1}, "Renamed.txt"),
          "model scale setup failed");
  const auto revision = model.Revision();
  model.SetFilter("match", ".TXT");
  model.SelectVisible();
  Require(model.Selection().size() == 49999 && model.Revision() == revision && model.Undo() &&
              model.Selection() == std::vector<runtime::AssetUuid>{{1, 1}},
          "100k-item filter selection missed rows or damaged Undo selection");
  model.SelectVisible();
  const auto selected = model.Selection();
  const auto before_delete = model.Revision();
  Require(!model.Delete(std::array{selected.front(), selected.front()}) &&
              !model.Delete(std::array{selected.front(), runtime::AssetUuid{9, 9}}) &&
              model.Revision() == before_delete && model.Selection() == selected &&
              model.Items().size() == 100000,
          "invalid batch partially deleted or changed selection/history");
  Require(model.Delete(selected) && model.Items().size() == 50000 && model.Selection().empty() &&
              model.Undo() && model.Items().size() == 100000 && model.Selection() == selected,
          "large batch delete/Undo failed");
}
} // namespace
int main() {
  try {
    ModelScale();
    Run(1, false);
    Run(2, false);
    Run(1, true);
    {
      Fixture drag(2);
      Require(drag.content.Browser().Select(drag.matching.front()), "drag selection failed");
      drag.Draw();
      const auto point = Access::ContentAssetPosition(drag.ui, drag.matching.front());
      Require(point.has_value(), "drag row absent");
      Nexora::Window::WindowEvent pointer, button;
      pointer.type = Nexora::Window::WindowEventType::Pointer;
      pointer.value0 = static_cast<int>((*point)[0] * drag.dpi);
      pointer.value1 = static_cast<int>((*point)[1] * drag.dpi);
      button.type = Nexora::Window::WindowEventType::PointerButton;
      button.value0 = 0;
      button.value1 = 1;
      drag.ui.ProcessEvents(std::array{pointer});
      drag.Draw();
      drag.ui.ProcessEvents(std::array{button});
      drag.Draw();
      pointer.value0 += 24;
      drag.ui.ProcessEvents(std::array{pointer});
      drag.Draw();
      drag.Draw();
      Require(Access::ContentDragActive(drag.ui), "drag fixture did not start");
      drag.Press(Key::A, drag.command);
      drag.Press(Key::Delete);
      Require(drag.content.Browser().Selection().size() == 1 &&
                  drag.content.Browser().Items().size() == 129,
              "active asset drag admitted selection/deletion");
      drag.Press(Key::Escape);
      button.value1 = 0;
      drag.ui.ProcessEvents(std::array{button});
      drag.Draw();
    }
    {
      Fixture rename(1);
      Require(rename.content.Browser().Select(rename.matching.front()), "Rename selection failed");
      rename.Draw();
      Nexora::Window::WindowEvent f2, remove;
      f2.type = remove.type = Nexora::Window::WindowEventType::Key;
      f2.value0 = static_cast<int>(Key::F2);
      remove.value0 = static_cast<int>(Key::Delete);
      f2.value1 = remove.value1 = 1;
      rename.ui.ProcessEvents(std::array{f2, remove});
      rename.Draw();
      Require(rename.content.Browser().Items().size() == 129 &&
                  rename.content.Browser().Find(rename.matching.front()),
              "same-frame Rename allowed pending Delete publication");
      f2.value1 = remove.value1 = 0;
      rename.ui.ProcessEvents(std::array{f2, remove});
      rename.Draw();
      rename.Press(Key::Escape);
    }
    // Recovery blocks shortcuts before normal content editing resumes.
    Fixture f(2);
    Require(f.content.Browser().Select(f.matching.front()), "recovery selection setup failed");
    std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
    f.Draw();
    f.Press(Key::A, f.command);
    f.Press(Key::Delete);
    Require(f.content.Browser().Selection().size() == 1 &&
                f.content.Browser().Items().size() == 129,
            "recovery admitted shortcuts");
    std::cout << "Content keyboard selection and deletion contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
