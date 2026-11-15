#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/SpriteSourceCreation.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace nexora;
namespace {
using Access = editor::imgui::EditorImGuiTestAccess;
using Event = Nexora::Window::WindowEvent;
using Type = Nexora::Window::WindowEventType;
using Key = Nexora::Window::Key;
constexpr runtime::AssetUuid defaultAtlasId{101, 1};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Fixture(const std::filesystem::path &root, runtime::AssetUuid atlas_id = defaultAtlasId) {
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Create Sprite source"), "workspace fixture");
  twod::Atlas atlas{atlas_id,
                    16,
                    16,
                    1,
                    false,
                    {{7, 3, 4, 7, 9}, {8, 12, 1, 1, 1}},
                    twod::Bytes(16 * 16 * 4, std::byte{255})};
  const auto bytes = twod::EncodeAtlas(atlas);
  Check(!bytes.empty(), "Atlas fixture");
  std::ofstream output(root / "Content/Atlas.nx2atlas", std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  output.close();
  Check(bool(output), "Atlas write");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Source creation"));
  scene.Create("First");
  scene.Create("Second");
  std::filesystem::create_directories(root / ".nexora/scenes");
  Check(scene.Save(root / ".nexora/scenes/Main.scene"), "Scene fixture");
}
auto Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file()) {
      std::ifstream input(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string(std::istreambuf_iterator<char>(input), {}));
    }
  return result;
}
void Run(float dpi) {
  const runtime::AssetUuid atlasId{dpi == 1 ? 101U : 1U, 1};
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-source-create-ui-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup(root);
  Fixture(root, atlasId);
  editor::ProjectWorkspace workspace, reader;
  Check(workspace.Open(root) && reader.Open(root, editor::ProjectAccess::ReadOnly), "workspace");
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "import");
  editor::ProjectContentSession content, readonly;
  Check(content.Open(workspace, assets, 5) && readonly.Open(reader, assets, 5, false), "Content");
  auto *active = &content;
  auto *project = &workspace;
  runtime::World world;
  const auto id = world.LoadScene("Unchanged");
  Check(world.Activate(id), "activate");
  editor::SceneDocument scene(world, id);
  scene.Create("Unchanged");
  const auto original = scene.PrepareSave()->Bytes();
  const auto runtime = world.SaveScene(id);
  editor::ProductShell shell;
  runtime::PlaySession play(world);
  editor::imgui::EditorImGuiHost ui;
  ui.SetDisplay(1600 * dpi, 1200 * dpi, dpi);
  Access::ConfigureSyntheticInput(ui);
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, project, active, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  };
  const auto settle = [&] {
    for (int i = 0; i < 4; ++i)
      draw();
  };
  const auto send = [&](Type type, int v0, int v1 = 0) {
    Event event{};
    event.type = type;
    event.value0 = v0;
    event.value1 = v1;
    ui.ProcessEvents(std::array{event});
    draw();
  };
  const auto click = [&](std::array<float, 2> p, int button = 0) {
    send(Type::Pointer, int(p[0] * dpi), int(p[1] * dpi));
    send(Type::PointerButton, button, 1);
    send(Type::PointerButton, button, 0);
    settle();
  };
  const auto point = [&](std::size_t index) {
    const auto rect = Access::SpriteSourceCreationRect(ui, index);
    Check(bool(rect), "visible creation control missing");
    return std::array{((*rect)[0] + (*rect)[2]) * .5F, ((*rect)[1] + (*rect)[3]) * .5F};
  };
  const auto key = [&](Key k, bool ctrl = false) {
    Event event{};
    event.type = Type::Key;
    event.value0 = int(k);
    event.value1 = 1;
    event.modifiers =
        ctrl ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    draw();
    event.value1 = 0;
    event.modifiers = {};
    ui.ProcessEvents(std::array{event});
    draw();
  };
  const auto text = [&](std::size_t index, std::string_view value) {
    click(point(index));
    key(Key::A, true);
    for (const auto c : value)
      send(Type::Text, int(c));
    key(Key::Enter);
    settle();
  };
  send(Type::FocusChanged, 1);
  settle();
  auto *window = ImGui::FindWindowByName("Content###nexora.content");
  Check(window, "Content window");
  ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
  ImGui::SetWindowPos(window, {40, 40});
  ImGui::SetWindowSize(window, {1000, 900});
  Access::FocusContent(ui);
  settle();
  const auto open = [&] {
    const auto asset = Access::ContentAssetPosition(ui, atlasId);
    Check(bool(asset), "Atlas row");
    click(*asset, 1);
    const auto menu = Access::SpriteSourceCreationMenu(ui);
    Check(bool(menu), "Atlas creation menu");
    click(*menu);
  };
  const auto before = Files(root);
  open();
  text(0, "Created");
  text(2, "19");
  text(3, "23");
  text(4, "3");
  text(5, "5");
  text(6, "4.5");
  text(7, "8.25");
  text(8, "2");
  key(Key::F5);
  Check(ui.TakePlayCommand() == editor::imgui::PlayCommand::None, "creator admitted Play shortcut");
  click(point(9));
  auto request = ui.TakeSpriteSourceCreationRequest();
  Check(request && !ui.TakeSpriteSourceCreationRequest(), "one-shot creation request");
  const auto &r = request->creation;
  Check(r.name == "Created" && r.project == workspace.Project().id && r.root == workspace.Root() &&
            r.folder == "Content" && r.source.id == runtime::AssetUuid{} &&
            r.source.atlas == atlasId && r.source.region == 7 && r.source.sourceWidth == 19 &&
            r.source.sourceHeight == 23 && r.source.trimX == 3 && r.source.trimY == 5 &&
            r.source.trimWidth == 7 && r.source.trimHeight == 9 && r.source.pivotX == 4.5F &&
            r.source.pivotY == 8.25F && r.source.pixelsPerUnit == 2,
        "owning actual-input metadata");
  ui.SetSpriteSourceCreationResult(request->identity, false, "Name occupied");
  settle();
  Check(Access::SpriteSourceCreationStatus(ui) == "Name occupied", "rejected request feedback");
  text(0, "../unsafe");
  click(point(9));
  Check(!ui.TakeSpriteSourceCreationRequest(), "unsafe name admitted");
  text(0, "Created");
  text(8, "0");
  click(point(9));
  Check(!ui.TakeSpriteSourceCreationRequest(), "invalid PPU admitted");
  text(8, "2");
  send(Type::FocusChanged, 0);
  click(point(9));
  Check(!ui.TakeSpriteSourceCreationRequest(), "unfocused creation admitted");
  send(Type::FocusChanged, 1);
  settle();
  Check(content.Rename(atlasId, "Renamed.nx2atlas"), "stale setup");
  settle();
  click(point(9));
  Check(!ui.TakeSpriteSourceCreationRequest(), "stale creation admitted");
  click(point(10));
  Check(content.Undo(), "restore Atlas path");
  settle();
  project = &reader;
  active = &readonly;
  settle();
  open();
  Check(!Access::SpriteSourceCreationRect(ui, 0) && !ui.TakeSpriteSourceCreationRequest(),
        "read-only creation menu admitted");
  key(Key::Escape);
  project = &workspace;
  active = &content;
  settle();
  ui.SetGameBuildStatus(true, "Building");
  settle();
  open();
  Check(!Access::SpriteSourceCreationRect(ui, 0), "build-busy creation admitted");
  key(Key::Escape);
  ui.SetGameBuildStatus(false, "Idle");
  settle();
  Check(play.Start(.1, [](runtime::World &, double) { return true; }), "Play fixture");
  settle();
  open();
  Check(!Access::SpriteSourceCreationRect(ui, 0), "running Play creation admitted");
  key(Key::Escape);
  play.Stop();
  settle();
  open();
  click(point(9));
  request = ui.TakeSpriteSourceCreationRequest();
  Check(bool(request), "fresh draft did not recover");
  ui.SetSpriteSourceCreationResult(request->identity, true, "Created and imported");
  settle();
  Check(!Access::SpriteSourceCreationRect(ui, 0), "successful draft remained open");
  Check(Files(root) == before && scene.PrepareSave()->Bytes() == original &&
            world.SaveScene(id) == runtime,
        "GUI rendering or request handling mutated files, Scene or Runtime");

  // The same real controls edit an existing imported source; no request injection.
  constexpr nexora::runtime::AssetUuid edited_id{303, 3};
  twod::SpriteAsset baseline{edited_id, atlasId, 7, 19, 23, 3, 5, 7, 9, 4.5F, 8.25F, 2, {}};
  const auto source_path = workspace.Root() / "Content/Existing.nx2sprite";
  {
    const auto bytes = twod::EncodeSpriteAsset(baseline);
    std::ofstream output(source_path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
    Check(bool(output), "existing source fixture");
  }
  Check(assets.ImportTree(workspace.Root() / "Content", {}, {},
                          editor::AssetIdentityMode::PersistentReadWrite) &&
            content.Open(workspace, assets, 6) && readonly.Open(reader, assets, 6, false),
        "existing source import");
  const auto editing_files = Files(root);
  const auto retained_source = content.Browser().Find(edited_id)->twod;
  settle();
  const auto open_edit = [&] {
    Access::FocusContent(ui);
    settle();
    const auto asset = Access::ContentAssetPosition(ui, edited_id);
    Check(bool(asset), "existing source row");
    click(*asset, 1);
    const auto menu = Access::SpriteSourceEditingMenu(ui);
    Check(bool(menu), "source editing menu");
    click(*menu);
  };
  open_edit();
  Check(!Access::SpriteSourceCreationRect(ui, 0) && Access::SpriteSourceCreationRect(ui, 11),
        "edit must preserve filename and expose border metadata");
  click(point(9));
  Check(!ui.TakeSpriteSourceEditingRequest(), "unchanged source edit admitted");
  for (const auto &[index, value] : std::array<std::pair<std::size_t, const char *>, 7>{
           {{2, "31"}, {3, "37"}, {4, "6"}, {5, "7"}, {6, "9.75"}, {7, "12.5"}, {8, "4"}}})
    text(index, value);
  key(Key::F5);
  Check(ui.TakePlayCommand() == editor::imgui::PlayCommand::None, "editor admitted Play shortcut");
  click(point(9));
  Event blur{}, refocus{};
  blur.type = refocus.type = Type::FocusChanged;
  refocus.value0 = 1;
  ui.ProcessEvents(std::array{blur, refocus});
  Check(!ui.TakeSpriteSourceEditingRequest(), "transient blur retained an unconsumed source save");
  settle();
  click(point(9));
  auto edit_request = ui.TakeSpriteSourceEditingRequest();
  Check(edit_request && !ui.TakeSpriteSourceEditingRequest() &&
            !ui.TakeSpriteSourceCreationRequest(),
        "one-shot existing-source edit request");
  Check(edit_request->editing.baseline == baseline &&
            edit_request->editing.source.id == edited_id &&
            edit_request->editing.source.atlas == atlasId &&
            edit_request->editing.path == "Content/Existing.nx2sprite" &&
            edit_request->editing.root == workspace.Root() &&
            edit_request->editing.source.sourceWidth == 31 &&
            edit_request->editing.source.sourceHeight == 37 &&
            edit_request->editing.source.trimX == 6 && edit_request->editing.source.trimY == 7 &&
            edit_request->editing.source.pivotX == 9.75F &&
            edit_request->editing.source.pivotY == 12.5F &&
            edit_request->editing.source.pixelsPerUnit == 4 && Files(root) == editing_files &&
            scene.PrepareSave()->Bytes() == original && world.SaveScene(id) == runtime,
        "owning edit draft changed identity or live state");
  ui.SetSpriteSourceEditingResult(edit_request->identity, false, "Source was changed externally");
  settle();
  Check(Access::SpriteSourceCreationStatus(ui) == "Source was changed externally" &&
            Access::SpriteSourceCreationRect(ui, 2),
        "edit rejection lost usable draft");
  text(8, "0");
  click(point(9));
  Check(!ui.TakeSpriteSourceEditingRequest(), "invalid edit PPU admitted");
  text(8, "4");
  text(11, "1");
  click(point(9));
  Check(!ui.TakeSpriteSourceEditingRequest(), "bordered trimmed source admitted");
  text(11, "0");
  send(Type::FocusChanged, 0);
  click(point(9));
  Check(!ui.TakeSpriteSourceEditingRequest(), "unfocused source edit admitted");
  send(Type::FocusChanged, 1);
  settle();
  Check(content.Rename(atlasId, "Renamed.nx2atlas"), "stale edit fixture");
  settle();
  click(point(9));
  Check(!ui.TakeSpriteSourceEditingRequest(), "stale source edit admitted");
  click(point(10));
  Check(content.Undo(), "restore edit Atlas");
  settle();
  project = &reader;
  active = &readonly;
  settle();
  open_edit();
  Check(!Access::SpriteSourceCreationRect(ui, 2), "read-only source editing admitted");
  key(Key::Escape);
  project = &workspace;
  active = &content;
  settle();
  ui.SetGameBuildStatus(true, "Building");
  settle();
  open_edit();
  Check(!Access::SpriteSourceCreationRect(ui, 2), "build-busy source editing admitted");
  key(Key::Escape);
  ui.SetGameBuildStatus(false, "Idle");
  settle();
  Check(play.Start(.1, [](nexora::runtime::World &, double) { return true; }), "edit Play fixture");
  settle();
  open_edit();
  Check(!Access::SpriteSourceCreationRect(ui, 2), "Play source editing admitted");
  key(Key::Escape);
  play.Stop();
  settle();
  open_edit();
  text(2, "31");
  text(3, "37");
  text(4, "6");
  text(5, "7");
  text(6, "9.75");
  text(7, "12.5");
  text(8, "4");
  click(point(9));
  edit_request = ui.TakeSpriteSourceEditingRequest();
  Check(bool(edit_request), "fresh source edit did not recover");
  const auto result = editor::SaveSpriteSourceFile(workspace, content, edit_request->editing, true);
  Check(result.state == editor::SpriteSourceEditingState::Saved &&
            *retained_source->SpriteSource() == baseline &&
            *content.Browser().Find(edited_id)->twod->SpriteSource() ==
                edit_request->editing.source,
        "actual edit request did not publish immutable source");
  ui.SetSpriteSourceEditingResult(edit_request->identity, true,
                                  std::string(editor::SpriteSourceEditingMessage(result.error)));
  settle();
  Check(!Access::SpriteSourceCreationRect(ui, 2) && scene.PrepareSave()->Bytes() == original &&
            world.SaveScene(id) == runtime,
        "edit dismissal changed Scene or Runtime");
  const auto saved_files = Files(root);
  open_edit();
  text(6, "999");
  click(point(10));
  Check(Files(root) == saved_files, "cancelled edit wrote source");
  open_edit();
  text(2, "7");
  text(3, "9");
  text(4, "0");
  text(5, "0");
  text(11, "1");
  text(12, "2");
  text(13, "3");
  text(14, "4");
  click(point(9));
  edit_request = ui.TakeSpriteSourceEditingRequest();
  Check(edit_request &&
            edit_request->editing.source.border == std::array<std::uint32_t, 4>{1, 2, 3, 4} &&
            editor::SaveSpriteSourceFile(workspace, content, edit_request->editing, true).state ==
                editor::SpriteSourceEditingState::Saved,
        "actual full-canvas border editing controls");
  ui.SetSpriteSourceEditingResult(edit_request->identity, true, "Saved full-canvas borders");
  settle();
  Check(scene.PrepareSave()->Bytes() == original && world.SaveScene(id) == runtime,
        "border metadata edit mutated Scene or Runtime");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-native-fixture")
      Fixture(argv[2]);
    else {
      Run(1);
      Run(2);
      std::cout << "Actual 1x/2x source creation/editing controls and guards passed\n";
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
