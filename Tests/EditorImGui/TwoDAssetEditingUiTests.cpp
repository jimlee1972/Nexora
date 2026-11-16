#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/ProjectBuildSettings.h"
#include "Nexora/Editor/TwoDAssetEditing.h"
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
void Fixture(const std::filesystem::path &root, runtime::AssetUuid atlas_id = defaultAtlasId,
             bool native = false) {
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Atlas Clip editing"), "workspace fixture");
  twod::Atlas atlas{atlas_id,
                    16,
                    16,
                    1,
                    false,
                    {{7, 3, 4, 7, 9}, {8, 12, 1, 1, 1}},
                    twod::Bytes(16 * 16 * 4, std::byte{255})};
  if (native) {
    atlas.width = atlas.height = 32;
    atlas.regions = {{7, 2, 2, 8, 8}, {8, 20, 2, 8, 8}};
    atlas.rgba.resize(32 * 32 * 4);
    for (std::uint32_t y = 0; y < 32; ++y)
      for (std::uint32_t x = 0; x < 32; ++x) {
        const auto offset = (std::size_t(y) * 32 + x) * 4;
        atlas.rgba[offset] = std::byte(x < 10 ? 240 : 20);
        atlas.rgba[offset + 1] = std::byte(x < 10 ? 20 : 240);
        atlas.rgba[offset + 2] = std::byte{20};
        atlas.rgba[offset + 3] = std::byte{255};
      }
    twod::SpriteAsset source{{202, 2}, atlas_id, 7, 8, 8, 0, 0, 8, 8, 4, 4, 2, {}};
    const auto sourceBytes = twod::EncodeSpriteAsset(source);
    std::ofstream sourceOut(root / "Content/Source.nx2sprite", std::ios::binary);
    sourceOut.write(reinterpret_cast<const char *>(sourceBytes.data()),
                    std::streamsize(sourceBytes.size()));
    sourceOut.close();
    Check(bool(sourceOut), "native Sprite source write");
  }
  if (!native) {
    twod::SpriteAsset source{{202, 2}, atlas_id, 7, 19, 23, 3, 5, 7, 9, 4.5F, 8.25F, 2, {}};
    const auto sourceBytes = twod::EncodeSpriteAsset(source);
    std::ofstream sourceOut(root / "Content/Source###same.nx2sprite", std::ios::binary);
    sourceOut.write(reinterpret_cast<const char *>(sourceBytes.data()),
                    std::streamsize(sourceBytes.size()));
    sourceOut.close();
    Check(bool(sourceOut), "Sprite source fixture");
    source.id = {203, 2};
    const auto otherBytes = twod::EncodeSpriteAsset(source);
    std::ofstream otherOut(root / "Content/ZSource###same.nx2sprite", std::ios::binary);
    otherOut.write(reinterpret_cast<const char *>(otherBytes.data()),
                   std::streamsize(otherBytes.size()));
    otherOut.close();
    Check(bool(otherOut), "second source fixture with shared ImGui label suffix");
  }
  const auto bytes = twod::EncodeAtlas(atlas);
  Check(!bytes.empty(), "Atlas fixture");
  std::ofstream output(root / "Content/Atlas.nx2atlas", std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  output.close();
  Check(bool(output), "Atlas write");
  twod::Clip clip{{303, 3}, atlas_id, true, {{7, .1, "first"}, {8, .2, "last"}}};
  const auto clipBytes = twod::EncodeClip(clip);
  std::ofstream clipOut(root / "Content/Animation.nx2clip", std::ios::binary);
  clipOut.write(reinterpret_cast<const char *>(clipBytes.data()),
                std::streamsize(clipBytes.size()));
  clipOut.close();
  Check(bool(clipOut), "Clip write");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Source creation"));
  const auto first = scene.Create("First");
  scene.Create("Second");
  if (native) {
    runtime::Transform transform;
    transform.y = 3;
    Check(scene.SetTransform(first, transform) &&
              scene.SetOpaqueComponent(*scene.Key(first), editor::SpriteSourceReference({202, 2})),
          "native source assignment");
  }
  std::filesystem::create_directories(root / ".nexora/scenes");
  Check(scene.Save(root / ".nexora/scenes/Main.scene"), "Scene fixture");
  if (native) {
    Check(scene.Save(root / "Authored.scene"), "Build Scene fixture");
    editor::AssetWorkspace assets;
    Check(
        assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "native identity index");
    editor::ProjectContentSession content;
    Check(content.Open(workspace, assets, 9), "native Content");
    editor::ProjectBuildSettingsSession settings(workspace);
    Check(settings.Reload(), "native Build settings");
    const auto enrolled = settings.Enroll("Authored.scene", content.Browser());
    Check(enrolled.has_value(), "enroll authored scene");
    editor::ProjectBuildSettings build{enrolled->id, {*enrolled}};
    build.animation_clips = {{303, 3}};
    Check(settings.Save(build, content.Browser()), "explicit edited Clip root");
  }
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
  ui.SetDisplay(1600 * dpi, (dpi == 1 ? 720 : 1200) * dpi, dpi);
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
    const auto rect = Access::TwoDAssetEditingRect(ui, index);
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
  const auto open = [&](runtime::AssetUuid assetId, bool atlas) {
    const auto asset = Access::ContentAssetPosition(ui, assetId);
    Check(bool(asset), "Content asset row");
    click(*asset, 1);
    const auto menu = Access::TwoDAssetEditingMenu(ui, atlas);
    Check(bool(menu), "asset editing menu");
    click(*menu);
  };
  const auto before = Files(root);
  open(atlasId, true);
  const auto *modal =
      ImGui::FindWindowByName("Atlas / Clip editor###editor.content.twod-asset-edit");
  Check(modal && modal->Pos.x >= 0 && modal->Pos.y >= 0 &&
            modal->Pos.x + modal->Size.x <= ImGui::GetIO().DisplaySize.x &&
            modal->Pos.y + modal->Size.y <= ImGui::GetIO().DisplaySize.y,
        "asset editor modal exceeds visible display");
  text(10, "3");
  key(Key::F5);
  Check(ui.TakePlayCommand() == editor::imgui::PlayCommand::None,
        "asset dialog admitted Play shortcut");
  click(point(13));
  auto request = ui.TakeTwoDAssetEditingRequest();
  Check(request && !ui.TakeTwoDAssetEditingRequest(), "one-shot Atlas request");
  auto edited = twod::DecodeAtlas(request->editing.proposed);
  Check(edited && edited->id == atlasId && edited->Find(7)->y == 3 &&
            request->editing.project == workspace.Project().id &&
            request->editing.root == workspace.Root() &&
            request->editing.path == "Content/Atlas.nx2atlas",
        "actual Atlas controls or owning identity");
  Check(Files(root) == before, "GUI performed IO");
  ui.SetTwoDAssetEditingResult(request->identity, false, "External change");
  settle();
  // Draft history is independent of Scene history and a rejected file publication.
  click(point(0));
  click(point(13));
  Check(!ui.TakeTwoDAssetEditingRequest(), "undo did not restore unchanged Atlas");
  click(point(1));
  click(point(13));
  request = ui.TakeTwoDAssetEditingRequest();
  Check(request.has_value(), "redo lost Atlas draft");
  const auto result = editor::SaveTwoDAssetFile(workspace, content, request->editing, true);
  Check(result.state == editor::TwoDAssetEditingState::Saved, "owner Atlas save");
  ui.SetTwoDAssetEditingResult(request->identity, true, "Saved");
  settle();
  open(atlasId, true);
  const auto cellRect = Access::TwoDAssetEditingRect(ui, 2);
  Check(cellRect.has_value(), "grid cell fields");
  click({(*cellRect)[0] + 12, ((*cellRect)[1] + (*cellRect)[3]) * .5F});
  key(Key::A, true);
  send(Type::Text, '4');
  key(Key::Tab);
  key(Key::A, true);
  send(Type::Text, '4');
  key(Key::Enter);
  settle();
  click(point(5));
  click(point(13));
  request = ui.TakeTwoDAssetEditingRequest();
  Check(request.has_value(), "actual Grid slicing request");
  const auto grid = twod::DecodeAtlas(request->editing.proposed);
  Check(grid && grid->regions ==
                    std::vector<twod::Region>{
                        {1, 1, 1, 4, 4}, {2, 7, 1, 4, 4}, {3, 1, 7, 4, 4}, {4, 7, 7, 4, 4}},
        "actual Grid cell/offset/gutter layout");
  const auto beforeGrid = Files(root);
  const auto gridResult = editor::SaveTwoDAssetFile(workspace, content, request->editing, true);
  Check(gridResult.state == editor::TwoDAssetEditingState::Rejected &&
            gridResult.error == editor::TwoDAssetEditingError::Closure && Files(root) == beforeGrid,
        "Grid save broke referenced source/Clip regions");
  ui.SetTwoDAssetEditingResult(request->identity, false, "Referenced regions must remain valid");
  settle();
  click(point(0));
  click(point(13));
  Check(!ui.TakeTwoDAssetEditingRequest(), "Grid Undo did not restore original regions");
  click(point(14));
  const auto savedAtlasFiles = Files(root);
  open({303, 3}, false);
  text(8, "0.375");
  text(9, "changed");
  click(point(12));
  click(point(13));
  request = ui.TakeTwoDAssetEditingRequest();
  Check(request.has_value(), "Clip request");
  const auto clip = twod::DecodeClip(request->editing.proposed);
  Check(clip && clip->id == runtime::AssetUuid{303, 3} && clip->frames[1].region == 7 &&
            clip->frames[1].seconds == .375 && clip->frames[1].event == "changed",
        "Clip time/event/reorder");
  Check(Files(root) == savedAtlasFiles, "Clip GUI performed IO");
  ui.SetTwoDAssetEditingResult(request->identity, false, "Retry");
  settle();
  click(point(13));
  Event blur{};
  blur.type = Type::FocusChanged;
  blur.value0 = 0;
  Event refocus = blur;
  refocus.value0 = 1;
  ui.ProcessEvents(std::array{blur, refocus});
  Check(!ui.TakeTwoDAssetEditingRequest(), "transient blur retained unconsumed save");
  settle();
  click(point(13));
  request = ui.TakeTwoDAssetEditingRequest();
  Check(request.has_value(), "usable draft lost on blur");
  const auto clipResult = editor::SaveTwoDAssetFile(workspace, content, request->editing, true);
  Check(clipResult.state == editor::TwoDAssetEditingState::Saved, "Clip owner save");
  ui.SetTwoDAssetEditingResult(request->identity, true, "Saved");
  settle();
  open({303, 3}, false);
  click(point(15));
  click(point(15));
  click(point(3));
  click(point(5));
  text(8, "0.3");
  text(9, "source");
  click(point(13));
  request = ui.TakeTwoDAssetEditingRequest();
  Check(request.has_value(), "source Clip request");
  const auto sourceClip = twod::DecodeClip(request->editing.proposed);
  Check(sourceClip && sourceClip->atlas == runtime::AssetUuid{} && sourceClip->frames.size() == 1 &&
            sourceClip->frames[0].source == runtime::AssetUuid{202, 2} &&
            sourceClip->frames[0].seconds == .3 && sourceClip->frames[0].event == "source",
        "actual source Clip append/time/event");
  ui.SetTwoDAssetEditingResult(request->identity, false, "Review source binding");
  settle();
  click(point(10));
  click(point(6));
  click(point(13));
  request = ui.TakeTwoDAssetEditingRequest();
  Check(request.has_value(), "source rebinding request");
  const auto rebound = twod::DecodeClip(request->editing.proposed);
  Check(rebound && rebound->frames.size() == 1 &&
            rebound->frames.front().source == runtime::AssetUuid{203, 2},
        "source picker confused shared ImGui label suffixes");
  Check(editor::SaveTwoDAssetFile(workspace, content, request->editing, true).state ==
            editor::TwoDAssetEditingState::Saved,
        "source Clip save/closure");
  Check(content.Dependencies().Forward({303, 3}) == std::vector<runtime::AssetUuid>{{203, 2}},
        "source rebinding did not replace dependency closure");
  ui.SetTwoDAssetEditingResult(request->identity, true, "Saved source Clip");
  settle();
  const auto savedFiles = Files(root);
  open({303, 3}, false);
  text(8, "-1");
  click(point(13));
  Check(!ui.TakeTwoDAssetEditingRequest(), "negative duration admitted");
  text(8, "0.6");
  // Revision changes invalidate this owning modal rather than silently rebasing its baseline.
  Check(content.Rename(atlasId, "Renamed.nx2atlas"), "rename revision");
  settle();
  click(point(13));
  Check(!ui.TakeTwoDAssetEditingRequest(), "stale modal save admitted");
  click(point(14));
  Check(content.Undo(), "rename Undo");
  settle();
  project = &reader;
  active = &readonly;
  settle();
  const auto row = Access::ContentAssetPosition(ui, atlasId);
  Check(row.has_value(), "reader row");
  click(*row, 1);
  // Read-only menu may be visible, but opening it cannot create a request.
  if (const auto menu = Access::TwoDAssetEditingMenu(ui, true))
    click(*menu);
  Check(!ui.TakeTwoDAssetEditingRequest() && Files(root) == savedFiles, "reader mutated asset");
  Check(scene.PrepareSave()->Bytes() == original && world.SaveScene(id) == runtime,
        "asset editor changed Scene or Runtime");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-native-fixture") {
      Fixture(argv[2], defaultAtlasId, true);
      return 0;
    }
    Run(1);
    Run(2);
    std::cout << "Atlas/Clip actual UI passed at 1x/2x\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
