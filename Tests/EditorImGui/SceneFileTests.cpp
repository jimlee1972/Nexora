#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/ProjectContent.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Action = editor::imgui::SceneFileAction;
using Request = editor::imgui::SceneFileRequest;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-scene-files-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  editor::ProjectContentSession *content = nullptr;
  runtime::World world;
  runtime::Id id = world.LoadScene("Scene files UI");
  editor::SceneDocument scene{world, id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  std::unique_ptr<editor::SceneFileSession> files;
  runtime::Id entity{};
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Scene files") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(id),
            "Scene file UI workspace failed");
    entity = scene.CreateCamera("Original");
    Require(entity && scene.Select(std::array{*scene.Key(entity)}), "UI scene fixture failed");
    files = std::make_unique<editor::SceneFileSession>(writer, scene);
    Require(files->SaveAs(files->Token(), "Content/Original.scene").Applied(), "UI save failed");
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusHierarchy(ui);
    Draw();
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  void Draw() {
    ui.SetSceneFileContext(files->Token(), files->CurrentPath(), files->SaveBlocked());
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, content, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::size_t control) {
    const auto point = Access::SceneFilePosition(ui, control);
    Require(point.has_value(), "Scene file control absent");
    ClickAt(*point);
  }
  void ClickAt(std::array<float, 2> point) {
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>(point[0] * scale);
    pointer.value1 = static_cast<int>(point[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    Draw();
  }
  void Tap(Key key, Mods modifiers = {}) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = {};
    ui.ProcessEvents(std::array{event});
    Draw();
    Draw();
  }
  void Shortcut(Key key, bool shift = false) {
    Tap(key, static_cast<Mods>(static_cast<unsigned>(Mods::Control) |
                               (shift ? static_cast<unsigned>(Mods::Shift) : 0)));
  }
  void Path(std::string_view text) {
    Click(4);
    Shortcut(Key::A);
    for (const unsigned char c : text) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = c;
      ui.ProcessEvents(std::array{event});
      Draw();
    }
    if (Access::SceneFileText(ui) != text)
      throw std::runtime_error("Path input retained " + std::string(Access::SceneFileText(ui)));
  }
  Request Take(Action action) {
    auto request = ui.TakeSceneFileRequest();
    Require(request && request->action == action && request->token == files->Token(),
            "Scene file action/token missing");
    return std::move(*request);
  }
};
void Run(float dpi) {
  Fixture f(dpi);
  const auto original = f.world.SaveScene(f.id);
  // Opening File cancels uncommitted Inspector input instead of saving its draft.
  Access::FocusInspector(f.ui);
  f.Draw();
  Access::FocusInspectorTransformField(f.ui, 0);
  f.Draw();
  f.Shortcut(Key::A);
  for (const char c : std::string_view("999")) {
    Nexora::Window::WindowEvent text;
    text.type = Nexora::Window::WindowEventType::Text;
    text.value0 = c;
    f.ui.ProcessEvents(std::array{text});
    f.Draw();
  }
  f.Shortcut(Key::S, true);
  Require(!Access::SceneFilePosition(f.ui, 4) && !f.ui.TakeSceneFileRequest(),
          "File shortcut stole an active text input");
  // Drive menu Save As with real pointer events, and submit its real path text via Enter.
  f.Click(0);
  f.Click(3);
  f.Path("Content/Copy.scene");
  f.Tap(Key::Enter);
  auto request = f.Take(Action::SaveAs);
  Require(request.path == "Content/Copy.scene" && !request.close_after_save &&
              f.world.SaveScene(f.id) == original && !f.scene.Dirty() &&
              f.files->SaveAs(request.token, request.path).Applied(),
          "Save As dialog mutated World or lost typed destination");
  f.Draw();
  f.Shortcut(Key::N);
  request = f.Take(Action::New);
  Require(f.files->New(request.token).Applied() && f.scene.Dirty() && f.scene.Nodes().empty(),
          "Clean New request failed");
  f.Draw();
  f.Shortcut(Key::O);
  f.Path("Content/Original.scene");
  f.Click(5);
  Require(!f.ui.TakeSceneFileRequest() && f.scene.Nodes().empty(),
          "Open bypassed dirty-document choice");
  f.Click(6);
  Require(!f.ui.TakeSceneFileRequest() && f.scene.Dirty(), "Cancel discarded the new document");
  f.Shortcut(Key::O);
  f.Path("Content/Original.scene");
  f.Tap(Key::Enter);
  f.Click(8);
  request = f.Take(Action::Open);
  Require(request.discard_unsaved && request.path == "Content/Original.scene" &&
              f.files->Open(request.token, request.path, request.discard_unsaved).Applied() &&
              f.world.SaveScene(f.id) == original && !f.scene.Dirty(),
          "Discard/Open did not restore the authored scene");
  f.Draw();
  Require(f.scene.Rename(*f.scene.Key(f.entity), "Unsaved"), "Dirty UI fixture failed");
  f.Shortcut(Key::N);
  f.Click(7);
  request = f.Take(Action::New);
  Require(request.save_current && !request.save_path && !request.discard_unsaved &&
              f.files->Save(request.token).Applied() && f.files->New(request.token).Applied(),
          "Save before New did not retain the current-file save request");
  f.Draw();
  f.Shortcut(Key::N);
  f.Click(7);
  f.Draw();
  f.Path("Content/Empty.scene");
  f.Click(5);
  request = f.Take(Action::New);
  Require(request.save_current && request.save_path == "Content/Empty.scene" &&
              f.files->SaveAs(request.token, *request.save_path).Applied() &&
              f.files->New(request.token).Applied(),
          "Unsaved New lost its nested Save As destination");
  f.Draw();
  f.Shortcut(Key::S, true);
  Require(!f.ui.TakeSceneSaveRequest(), "Save As shortcut also requested ordinary Save");
  f.Path("Content/Original.scene");
  f.Click(5);
  request = f.Take(Action::SaveAs);
  Require(f.files->SaveAs(request.token, request.path).status ==
              editor::SceneFileStatus::NeedsOverwrite,
          "Overwrite fixture failed");
  f.ui.RequestSceneOverwrite(request);
  f.Draw();
  f.Draw();
  f.Click(9);
  request = f.Take(Action::SaveAs);
  Require(request.replace_existing && f.files->SaveAs(request.token, request.path, true).Applied(),
          "Overwrite dialog lost confirmed request");
  f.Draw();
  f.Shortcut(Key::S);
  Require(f.ui.TakeSceneSaveRequest(), "Ordinary Save shortcut failed");
}
void RunGates(float dpi) {
  Fixture f(dpi);
  const auto original = f.world.SaveScene(f.id);
  f.active = &f.reader;
  f.Draw();
  f.Shortcut(Key::N);
  f.Shortcut(Key::S, true);
  Require(!f.ui.TakeSceneFileRequest() && !Access::SceneFilePosition(f.ui, 4),
          "Read-only shortcuts opened a write operation");
  f.Shortcut(Key::O);
  f.Path("Content/Original.scene");
  f.Click(5);
  Require(f.Take(Action::Open).path == "Content/Original.scene", "Read-only Open blocked");
  f.active = &f.writer;
  f.Draw();
  Require(f.play.Start(1.0 / 60, [](runtime::World &, double) { return true; }),
          "Play fixture failed");
  f.Draw();
  f.Shortcut(Key::N);
  f.Shortcut(Key::O);
  Require(!f.ui.TakeSceneFileRequest() && !Access::SceneFilePosition(f.ui, 4),
          "Play allowed document replacement");
  Require(f.play.Stop(), "Play stop failed");
  f.Draw();
  // A document generation change cancels an outstanding dialog before it can emit a request.
  f.Shortcut(Key::S, true);
  Require(f.scene.Reload(f.root / "Content/Original.scene"), "Generation fixture failed");
  f.files = std::make_unique<editor::SceneFileSession>(f.writer, f.scene);
  Require(f.files->BindCurrent("Content/Original.scene"), "Rebind failed");
  f.Draw();
  f.Draw();
  Require(!Access::SceneFilePosition(f.ui, 4) && !f.ui.TakeSceneFileRequest() &&
              f.world.SaveScene(f.id) == original,
          "Stale dialog survived document replacement");
  f.Shortcut(Key::N);
  Require(f.scene.Reload(f.root / "Content/Original.scene"), "Pending request reload failed");
  f.ui.SetSceneFileContext({f.writer.Project().id, f.scene.Generation()}, "Content/Original.scene");
  Require(!f.ui.TakeSceneFileRequest(), "Stale emitted request survived context replacement");
  f.files = std::make_unique<editor::SceneFileSession>(f.writer, f.scene);
  Require(f.files->BindCurrent("Content/Original.scene"), "Pending request rebind failed");
  f.Draw();
  f.Draw();
  f.Shortcut(Key::S, true);
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Shortcut(Key::N);
  Require(!f.ui.TakeSceneFileRequest(), "Close modal admitted file operation");
  f.Tap(Key::Escape);
  f.Draw();
  // Untitled Save and Exit opens a fresh Save As modal and carries the close intent.
  Require(f.files->New(f.files->Token(), true).Applied(), "Untitled fixture failed");
  f.Draw();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.ui.RequestSceneSaveAs(true);
  f.Draw();
  f.Draw();
  const auto before_retry = f.world.SaveScene(f.id);
  f.Path("../Rejected.scene");
  f.Click(5);
  auto rejected_request = f.Take(Action::SaveAs);
  const auto rejected = f.files->SaveAs(rejected_request.token, rejected_request.path);
  Require(rejected_request.close_after_save &&
              rejected.status == editor::SceneFileStatus::Rejected && f.scene.Dirty() &&
              !f.files->CurrentPath() && f.world.SaveScene(f.id) == before_retry,
          "Failed Save and Exit changed the current document");
  f.ui.SetSceneSaveResult(rejected.message, false);
  f.ui.RequestSceneSaveAs(true, rejected_request.path);
  f.Draw();
  f.Draw();
  f.Path("Content/Exit.scene");
  f.Click(5);
  const auto request = f.Take(Action::SaveAs);
  if (!request.close_after_save || request.path != "Content/Exit.scene")
    throw std::runtime_error("Save and Exit retry emitted path=" + request.path.generic_string() +
                             ", close=" + std::to_string(request.close_after_save));
  Require(f.files->SaveAs(request.token, request.path).Applied() && !f.scene.Dirty(),
          "Save and Exit retry did not save successfully");
}
void RunContentScene(float dpi) {
  Fixture f(dpi);
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  const auto target = std::filesystem::path(u8"Content/場景.scene");
  Require(f.scene.CreateLight("Target light") && f.scene.Save(f.root / target) &&
              f.files->Open(f.files->Token(), "Content/Original.scene").Applied() &&
              assets.ImportTree(f.root / "Content", {}, {},
                                editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(f.writer, assets, 1, true),
          "Content scene fixture failed");
  runtime::AssetUuid asset;
  for (const auto &item : content.Browser().Items())
    if (item.path == target)
      asset = item.id;
  Require(asset != runtime::AssetUuid{} && content.Browser().Select(asset), "Scene asset missing");
  f.content = &content;
  Access::FocusContent(f.ui);
  f.Draw();
  f.Draw();
  const auto open = [&] {
    const auto point = Access::ContentOpenScenePosition(f.ui);
    Require(point.has_value(), "Content Open control absent");
    f.ClickAt(*point);
  };
  const auto original = f.world.SaveScene(f.id);
  const runtime::AssetUuid text_asset{99, 1};
  Require(content.Browser().Discover({text_asset, "Content/Readme.txt", ".txt", "text", {}, {}}) &&
              content.Browser().Select(text_asset),
          "Non-scene asset fixture failed");
  f.Draw();
  open();
  f.Tap(Key::Enter);
  Require(!f.ui.TakeSceneFileRequest(), "Non-scene selection emitted Open");
  Require(content.Browser().Select(asset, true), "Multiple asset fixture failed");
  f.Draw();
  open();
  Require(!f.ui.TakeSceneFileRequest(), "Multiple asset selection emitted Open");
  Require(content.Browser().Select(asset), "Restore scene asset selection failed");
  f.Draw();
  open();
  auto request = f.Take(Action::Open);
  Require(request.path == target && !request.discard_unsaved && !request.save_current &&
              f.world.SaveScene(f.id) == original && !Access::SceneFilePosition(f.ui, 4) &&
              f.files->Open(request.token, request.path).Applied() && f.scene.Nodes().size() == 2,
          "Content Open lost the Unicode path or bypassed deferred file loading");
  f.Draw();
  const auto row = Access::ContentAssetPosition(f.ui, asset);
  const auto before_double_click = f.world.SaveScene(f.id);
  Require(row.has_value(), "Scene content row absent");
  f.ClickAt(*row);
  Require(!f.ui.TakeSceneFileRequest(), "Single scene click opened a file");
  f.ClickAt(*row);
  request = f.Take(Action::Open);
  Require(request.path == target && !request.discard_unsaved &&
              f.world.SaveScene(f.id) == before_double_click,
          "Double-click mutated the scene or lost its owning path");
  // The application must reject a copied request after a new document boundary.
  Require(f.files->New(f.files->Token(), true).Applied() &&
              f.files->Open(request.token, request.path, true).status ==
                  editor::SceneFileStatus::Rejected &&
              f.files->Open(f.files->Token(), target, true).Applied(),
          "Stale Content request replaced a new document");
  f.Draw();
  Require(f.scene.Create("Unsaved") != 0, "Dirty content scene fixture failed");
  f.Draw();
  open();
  Require(!f.ui.TakeSceneFileRequest() && Access::SceneFilePosition(f.ui, 8),
          "Content Open skipped the unsaved decision");
  f.Click(6);
  Require(f.scene.Dirty() && f.scene.Nodes().size() == 3 && !f.ui.TakeSceneFileRequest(),
          "Cancel discarded content scene edits");
  Access::FocusContent(f.ui);
  f.Draw();
  f.Tap(Key::Enter);
  f.Click(8);
  request = f.Take(Action::Open);
  Require(request.path == target && request.discard_unsaved &&
              f.files->Open(request.token, request.path, true).Applied() &&
              f.scene.Nodes().size() == 2,
          "Content Enter/Discard failed");
  f.Draw();
  Require(f.play.Start(1.0 / 60, [](runtime::World &, double) { return true; }),
          "Content Play failed");
  f.Draw();
  open();
  f.Tap(Key::Enter);
  Require(!f.ui.TakeSceneFileRequest(), "Content scene replacement was allowed during Play");
  Require(f.play.Stop(), "Content Play stop failed");
  f.active = &f.reader;
  f.files = std::make_unique<editor::SceneFileSession>(f.reader, f.scene);
  Require(f.files->BindCurrent(target), "Read-only content bind failed");
  f.Draw();
  open();
  request = f.Take(Action::Open);
  Require(request.path == target && f.files->Open(request.token, request.path).Applied(),
          "Read-only Content Open was blocked");
  f.Draw();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Tap(Key::Enter);
  Require(!f.ui.TakeSceneFileRequest(), "Close modal allowed Content Open");
}
void RunFolderSceneConflict(float dpi) {
  Fixture f(dpi);
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  Require(assets.ImportTree(f.root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(f.writer, assets, 1, true),
          "Folder/scene fixture failed");
  auto &browser = content.Browser();
  const auto scene_asset = browser.Items().front().id;
  Require(browser.Discover({{99, 3}, "Content/Child/Readme.txt", ".txt", "text", {}, {}}) &&
              browser.Select(scene_asset),
          "Folder/scene selection fixture failed");
  f.content = &content;
  Access::FocusContent(f.ui);
  f.Draw();
  f.Draw();
  const auto original = f.world.SaveScene(f.id);
  for (int i = 0; i < 40 && Access::ContentFocusedFolder(f.ui) != "Content/Child"; ++i)
    f.Tap(Key::Tab);
  Require(Access::ContentFocusedFolder(f.ui) == "Content/Child", "Folder Tab focus failed");
  f.Tap(Key::Enter);
  Require(browser.Breadcrumbs().back().path == "Content/Child" && !f.ui.TakeSceneFileRequest() &&
              f.world.SaveScene(f.id) == original && browser.IsSelected(scene_asset),
          "Folder Enter also opened the selected scene or mutated selection/document");
  f.Tap(Key::UpArrow, Mods::Alt);
  Require(browser.Breadcrumbs().back().path == "Content", "Folder parent return failed");
  const auto row = Access::ContentAssetPosition(f.ui, scene_asset);
  Require(row.has_value(), "Scene row absent after parent return");
  f.ClickAt(*row);
  f.Tap(Key::Enter);
  Require(f.Take(Action::Open).path == "Content/Original.scene" &&
              f.world.SaveScene(f.id) == original,
          "Folder traversal broke subsequent scene Enter");
}
void RunContentRename(float dpi) {
  Fixture f(dpi);
  std::filesystem::copy_file(f.root / "Content/Original.scene", f.root / "Content/Second.scene");
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  Require(assets.ImportTree(f.root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(f.writer, assets, 1),
          "Content rename fixture failed");
  const auto asset = content.Browser().Items().front().id;
  const auto second = content.Browser().Items().back().id;
  Require(content.Browser().Select(asset), "Content rename selection failed");
  f.content = &content;
  Access::FocusContent(f.ui);
  f.Draw();
  f.Draw();
  const auto type = [&](std::u32string_view text) {
    for (const auto c : text) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = static_cast<int>(c);
      f.ui.ProcessEvents(std::array{event});
      f.Draw();
    }
  };
  int rename_attempt = 0;
  const auto begin = [&] {
    ++rename_attempt;
    Access::FocusContent(f.ui);
    f.Draw();
    f.Tap(Key::F2);
    if (!Access::ContentRenamePosition(f.ui, 0))
      throw std::runtime_error("Focused Content F2 did not open Rename at attempt " +
                               std::to_string(rename_attempt) + " scale " + std::to_string(dpi));
  };
  Require(f.scene.CreateLight("Retained redo") && f.scene.Undo() && f.scene.CopySelection(),
          "Content rename history fixture failed");
  const auto original = f.world.SaveScene(f.id);
  begin();
  Require(Access::ContentRenameText(f.ui) == "Original.scene",
          "F2 did not preload the full filename");
  type(U"場景🙂.scene");
  const auto unicode_name = std::u8string(u8"場景🙂.scene");
  const std::string encoded(unicode_name.begin(), unicode_name.end());
  const auto unicode = std::filesystem::path("Content") / std::filesystem::path(unicode_name);
  Require(Access::ContentRenameText(f.ui) == encoded,
          "Focused/select-all UTF-32 rename input failed");
  f.Tap(Key::Enter);
  Require(content.Browser().Find(asset)->path == unicode &&
              std::filesystem::exists(f.root / unicode) &&
              !std::filesystem::exists(f.root / "Content/Original.scene") &&
              f.world.SaveScene(f.id) == original && !f.ui.TakeSceneFileRequest() &&
              f.scene.Redo() && f.scene.Undo(),
          "Enter rename lost source identity or consumed document history");
  begin();
  f.Shortcut(Key::A);
  f.Tap(Key::Backspace);
  f.Tap(Key::Enter);
  Require(!content.LastError().empty(), "Invalid rename did not report its error");
  type(U"場景🙂.scene");
  f.Tap(Key::Enter);
  Require(content.LastError().empty() && content.Undo() &&
              content.Browser().Find(asset)->path == "Content/Original.scene" &&
              content.Rename(asset, encoded),
          "Unchanged rename retained an error or consumed Content Undo");
  begin();
  type(U"Canceled.scene");
  f.Tap(Key::Escape);
  Require(content.Browser().Find(asset)->path == unicode &&
              !std::filesystem::exists(f.root / "Content/Canceled.scene"),
          "Escape committed a Content rename");
  begin();
  f.Shortcut(Key::A);
  f.Tap(Key::Backspace);
  f.Tap(Key::Enter);
  Require(Access::ContentRenamePosition(f.ui, 0).has_value() &&
              content.Browser().Find(asset)->path == unicode,
          "Invalid rename closed or mutated the asset");
  type(U"Retry.scene");
  f.Tap(Key::Enter);
  Require(content.Browser().Find(asset)->path == "Content/Retry.scene",
          "Rejected Content rename was not retryable");
  Require(content.Undo() && content.Browser().Find(asset)->path == unicode,
          "Content Undo did not retain the keyboard rename transaction");
  // Rename blocks authoring commands even when its field has lost active text input.
  begin();
  const auto apply = Access::ContentRenamePosition(f.ui, 1);
  Require(apply.has_value(), "Content Apply button missing");
  f.ClickAt(*apply); // unchanged name completes; reopen for the independent gates.
  begin();
  const auto input = Access::ContentRenamePosition(f.ui, 0);
  Require(input.has_value(), "Content filename input missing");
  f.ClickAt({(*input)[0] + 140.0F, (*input)[1]}); // Filename label deactivates the text field.
  f.Shortcut(Key::N);
  f.Shortcut(Key::S, true);
  f.Shortcut(Key::N, true);
  f.Shortcut(Key::Z);
  Require(!f.ui.TakeSceneFileRequest() && f.world.SaveScene(f.id) == original,
          "Content rename allowed File commands");
  f.active = &f.reader;
  f.Draw();
  f.Draw();
  Require(!Access::ContentRenamePosition(f.ui, 0), "Write loss retained the Rename modal");
  f.active = &f.writer;
  f.Draw();
  begin();
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  f.Draw();
  Require(!Access::ContentRenamePosition(f.ui, 0), "Focus loss retained Content Rename");
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  begin();
  type(U"FocusRestored.scene");
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus}); // No renderable frame occurred while unfocused.
  f.Tap(Key::Enter);
  f.Draw();
  Require(!Access::ContentRenamePosition(f.ui, 0) &&
              content.Browser().Find(asset)->path == unicode &&
              !std::filesystem::exists(f.root / "Content/FocusRestored.scene"),
          "Focus restoration before drawing revived the Content rename draft");
  begin();
  const std::vector<editor::ContentItem> snapshot(content.Browser().Items().begin(),
                                                  content.Browser().Items().end());
  Require(content.Browser().Reset(snapshot, 2), "Content generation fixture failed");
  f.Draw();
  f.Draw();
  Require(!Access::ContentRenamePosition(f.ui, 0) && content.Browser().Find(asset)->path == unicode,
          "Stale Content generation renamed an asset");
  Require(content.Browser().Select(asset), "Restore selection after Content reset failed");
  begin();
  Require(content.Browser().Rename(asset, "ExternallyMoved.scene"), "Stale path fixture failed");
  f.Draw();
  f.Draw();
  Require(!Access::ContentRenamePosition(f.ui, 0) && std::filesystem::exists(f.root / unicode) &&
              content.Browser().Undo(),
          "A stale captured asset path committed Rename");
  Require(content.Browser().Select(asset) && content.Browser().Select(second, true),
          "Multiple Content rename selection failed");
  Access::FocusContent(f.ui);
  f.Draw();
  f.Tap(Key::F2);
  Require(!Access::ContentRenamePosition(f.ui, 0), "Multiple asset selection opened Rename");
  Require(content.Browser().Select(asset), "Restore Content rename selection failed");
  begin();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Draw();
  Require(!Access::ContentRenamePosition(f.ui, 0), "Close modal retained Content Rename");
}
void RunUnicodeContent(float dpi) {
  Fixture f(dpi);
  const auto relative = std::filesystem::path(u8"Content/子資料夾/場景.scene");
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  Require(f.scene.Save(f.root / relative) &&
              assets.ImportTree(f.root / "Content", {}, {},
                                editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(f.writer, assets, 1, true),
          "Unicode content UI fixture failed");
  const auto draw = [&] {
    f.ui.BeginFrame();
    f.ui.DrawProductShell(f.shell, &f.scene, &f.writer, &content);
    static_cast<void>(f.ui.EndFrame());
  };
  Access::FocusContent(f.ui);
  draw(); // Draw UTF-8 child-folder label before entering it.
  runtime::AssetUuid asset;
  for (const auto &item : content.Browser().Items())
    if (item.path == relative)
      asset = item.id;
  Require(asset != runtime::AssetUuid{} && content.Browser().SetFolder(relative.parent_path()) &&
              content.Browser().Select(asset),
          "Unicode content selection failed");
  draw(); // Draw UTF-8 filename, breadcrumbs and selected path.
  Require(Access::ContentAssetPosition(f.ui, asset).has_value(),
          "Unicode scene was not submitted to the Content panel");
}
} // namespace
int main() {
  try {
    for (const float dpi : {1.0F, 2.0F}) {
      Run(dpi);
      RunGates(dpi);
      RunUnicodeContent(dpi);
      RunContentScene(dpi);
      RunFolderSceneConflict(dpi);
      RunContentRename(dpi);
    }
    std::cout << "Scene file menu, shortcuts and modal lifecycle passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
