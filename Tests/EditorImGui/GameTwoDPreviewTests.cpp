#include "EditorImGuiTestAccess.h"
#include "GameTwoDTextures.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "PlayGameplayModule.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
constexpr runtime::AssetUuid atlasId{17, 23}, clipId{19, 29}, poolId{31, 37};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  Check(bool(output), "fixture write failed");
}
void Text(const std::filesystem::path &path, std::string_view text) {
  Write(path, std::as_bytes(std::span(text)));
}
void Sources(const std::filesystem::path &root) {
  const twod::Image red{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
  auto green = red;
  green.id = 2;
  green.rgba = {std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255}};
  auto blue = red;
  blue.rgba = {std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}};
  const std::array images{red, green};
  auto atlas = twod::PackAtlas(atlasId, images, 8, 8, true);
  auto pool = twod::PackAtlas(poolId, std::span(&blue, 1), 8, 8, true);
  Check(atlas && pool, "fixture Atlas pack");
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
  Write(root / "Content/Pool.nx2atlas", twod::EncodeAtlas(*pool));
  Write(root / "Content/Clip.nx2clip",
        twod::EncodeClip({clipId, atlasId, true, {{1, .1, "red"}, {2, .1, "green"}}}));
  Text(root / "Content/Triangle.obj", "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvn 0 1 1\nf 1//1 2//1 3//1\n");
  Text(root / "Content/Triangle.obj.meta",
       "schema=1\nuuid=12345678-9abc-def0-fedc-ba9876543210\ntype=.obj\n");
}
struct Fixture final {
  editor::ProjectWorkspace workspace;
  editor::AssetWorkspace index;
  editor::ProjectContentSession content;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("2D Game");
  editor::SceneDocument document{world, scene_id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::preview::PlayGameplayModule module;
  editor::preview::GameTwoDTextures textures{ui};
  explicit Fixture(const std::filesystem::path &root) {
    Check(workspace.Create(root, "Game 2D Preview"), "project create");
    Sources(root);
    Check(index.ImportTree(root / "Content", {}, {},
                           editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(workspace, index, 13) && world.Activate(scene_id),
          "fixture import");
    const auto camera = document.CreateCamera("Camera");
    Check(camera == 2 && document.SetTransform(camera, {0, 0, 5}), "source camera");
    const auto mesh =
        runtime::MeshResourceId(*runtime::AssetUuid::Parse("12345678-9abc-def0-fedc-ba9876543210"));
    Check(document.CreateMesh("Sprite", {mesh, {}}, {-1, 0, 0}) == 3 &&
              document.Save(root / ".nexora/scenes/Main.scene"),
          "source scene");
    ui.SetDisplay(1280, 900, 1);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
  }
  void Draw(bool submit = true) {
    if (textures.Sync(module.TwoDAssets()))
      ui.SetGameTwoDAvailable(false);
    ui.SetGameTwoDAvailable(bool(module.TwoDAssets()));
    ui.BeginFrame();
    ui.DrawProductShell(shell, &document, &workspace, &content, nullptr, nullptr, nullptr, &play);
    if (submit) {
      if (const auto viewport = ui.GameTwoDViewport()) {
        const auto frame =
            module.ExtractTwoDFrame(viewport->width, viewport->height,
                                    [&](const auto &atlas) { return textures.Resolve(atlas); });
        Check(frame && frame->statistics.visible == 2 && frame->retainedAtlases.size() == 2,
              "multi-Atlas extraction");
        const Nexora::Presentation::UiDrawData packet{
            frame->vertices, std::as_bytes(std::span(frame->indices)), frame->batches, {}, true};
        auto malformed = frame->batches;
        malformed.back().indexOffset = UINT32_MAX;
        auto rejected = packet;
        rejected.commands = malformed;
        const auto before = Access::GameTwoDVertices(ui);
        Check(!ui.DrawGameTwoDSprites(rejected) && Access::GameTwoDVertices(ui) == before,
              "invalid second batch appended partial geometry");
        malformed = frame->batches;
        malformed.back().textureId = 0;
        rejected.commands = malformed;
        Check(!ui.DrawGameTwoDSprites(rejected) && Access::GameTwoDVertices(ui) == before,
              "invalid texture appended partial geometry");
        Check(ui.DrawGameTwoDSprites(packet) && Access::GameTwoDVertices(ui) == before + 8 &&
                  !ui.DrawGameTwoDSprites(packet),
              "Game Sprite append or duplicate admission");
      }
    }
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::array<float, 2> point) {
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>(point[0]);
    pointer.value1 = static_cast<int>(point[1]);
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
  void View(bool two_d) {
    const auto combo = Access::GameViewPosition(ui, std::nullopt);
    Check(bool(combo), "view selector missing");
    Click(*combo);
    const auto option = Access::GameViewPosition(ui, two_d);
    Check(bool(option), "view option missing");
    Click(*option);
    Check(bool(ui.GameTwoDViewport()) == two_d, "actual view click not applied");
  }
};
void Run(const std::filesystem::path &root, const std::filesystem::path &library) {
  Fixture f(root);
  const auto baseline = f.document.CaptureRuntimeScene();
  std::string error;
  const auto binding =
      editor::preview::CapturePlayTwoD(f.workspace, f.content, f.document, true, error);
  Check(binding &&
            f.play.Start(1.0 / 60,
                         [&](auto &, double seconds) { return f.module.FixedUpdate(seconds); }) &&
            f.module.LoadRelative(*f.play.PlayWorld(), library.parent_path(),
                                  library.filename().string(), error, {}, *binding) &&
            f.play.Pause(),
        "fixture module load");
  for (unsigned frame = 0; frame < 5; ++frame)
    f.Draw();
  Check(!f.ui.GameTwoDViewport(), "3D default changed");
  f.View(true);
  Check(f.ui.GameTwoDViewport() && !f.ui.NativeGameViewport() && f.textures.Size() == 2,
        "2D view did not own its Atlas registrations");
  auto retained = f.module.ExtractTwoDFrame(
      600, 180, [&](const auto &atlas) { return f.textures.Resolve(atlas); });
  Check(retained && retained->vertices.size() == 8 && retained->batches.size() == 2,
        "ordered multi-Atlas/pool packet");
  Check(f.play.Step(), "paused 2D step");
  const auto advanced = f.module.ExtractTwoDFrame(
      600, 180, [&](const auto &atlas) { return f.textures.Resolve(atlas); });
  Check(advanced && advanced->vertices.front().uv[0] != retained->vertices.front().uv[0],
        "manual step did not advance frozen Clip");
  const Nexora::Presentation::UiDrawData expired_packet{
      retained->vertices, std::as_bytes(std::span(retained->indices)), retained->batches, {}, true};
  Check(!f.ui.DrawGameTwoDSprites(expired_packet), "packet admitted after EndFrame");
  auto foreign = *retained->retainedAtlases.front().atlas;
  Check(f.textures.Resolve(foreign) == 0 && f.textures.Size() == 2,
        "same UUID from a foreign owner reused a Play texture");
  const auto old_texture = retained->batches.front().textureId;
  f.textures.Clear();
  f.textures.Sync(f.module.TwoDAssets());
  const std::array pixels{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
  std::array<std::uint64_t, 64> occupied{};
  for (auto &id : occupied) {
    id = f.ui.RegisterNativeTexture(1, 1, pixels);
    Check(id != 0, "GUI texture pressure fixture");
  }
  const auto unavailable = f.module.ExtractTwoDFrame(
      600, 180, [&](const auto &atlas) { return f.textures.Resolve(atlas); });
  Check(unavailable && unavailable->statistics.unavailable == 2 && f.textures.Size() == 0,
        "shared GUI texture pressure was not reported");
  for (auto id : occupied)
    Check(f.ui.UnregisterTexture(id), "pressure registration release");
  const auto layout = [](bool overlap) {
    std::string value = "schema=1\n";
    for (const auto &[name, panel] :
         std::array<std::pair<const char *, const char *>, 9>{{{"Project", "project"},
                                                               {"Hierarchy", "hierarchy"},
                                                               {"Inspector", "inspector"},
                                                               {"Console", "console"},
                                                               {"Content", "content"},
                                                               {"Scene", "scene"},
                                                               {"Game", "game"},
                                                               {"Profiler", "profiler"},
                                                               {"Build", "build"}}}) {
      value += std::string("[Window][") + name + "###nexora." + panel + "]\n";
      value += std::string_view(panel) == "scene" ? "Pos=0,40\nSize=620,640\nCollapsed=0\n\n"
               : std::string_view(panel) == "game"
                   ? (overlap ? "Pos=0,40\nSize=620,640\nCollapsed=0\n\n"
                              : "Pos=640,40\nSize=620,640\nCollapsed=0\n\n")
                   : "Pos=0,700\nSize=100,20\nCollapsed=1\n\n";
    }
    return value;
  };
  f.ui.SetNativeScenePreview(true);
  f.ui.SetConcurrentNativeViews(false);
  Check(f.ui.LoadLayout(layout(false)), "disjoint layout");
  for (unsigned frame = 0; frame < 4; ++frame)
    f.Draw();
  Check(f.ui.GameTwoDViewport() && f.ui.NativeScenePreviewViewport(),
        "UI 2D canvas incorrectly required multiple native 3D views");
  Check(f.ui.LoadLayout(layout(true)), "overlapping layout");
  for (unsigned frame = 0; frame < 4; ++frame)
    f.Draw();
  Check(f.ui.GameTwoDViewport() && !f.ui.NativeScenePreviewViewport(),
        "native Scene could overwrite an overlapping UI 2D canvas");
  f.ui.SetNativeScenePreview(false);
  f.View(false);
  Check(f.ui.NativeGameViewport() && !f.ui.GameTwoDViewport(), "3D switch lost its viewport");
  f.View(true);
  f.ui.SetDisplay(1280, 900, 2);
  for (unsigned frame = 0; frame < 4; ++frame)
    f.Draw();
  Check(bool(f.ui.GameTwoDViewport()), "2x DPI lost 2D canvas");
  f.module.Unload();
  Check(f.play.Stop(), "2D Stop");
  f.Draw();
  Check(!f.ui.GameTwoDViewport() && f.textures.Size() == 0 &&
            !f.ui.UnregisterTexture(old_texture) && f.document.CaptureRuntimeScene() == baseline &&
            retained->retainedAtlases.front().atlas->Find(1),
        "Stop retained registrations, changed source or invalidated an owning packet");
  f.ui.SetDisplay(1280, 900, 1);
  Check(f.play.Start(1.0 / 60,
                     [&](auto &, double seconds) { return f.module.FixedUpdate(seconds); }) &&
            f.module.LoadRelative(*f.play.PlayWorld(), library.parent_path(),
                                  library.filename().string(), error, {}, *binding) &&
            f.play.Pause(),
        "second Play session");
  for (unsigned frame = 0; frame < 4; ++frame)
    f.Draw();
  Check(!f.ui.GameTwoDViewport() && f.ui.NativeGameViewport(),
        "new Play session retained the previous 2D selection");
  f.View(true);
  const auto replacement =
      editor::preview::CapturePlayTwoD(f.workspace, f.content, f.document, true, error);
  Check(replacement && replacement->assets->Assets() != binding->assets->Assets() &&
            f.module.LoadRelative(*f.play.PlayWorld(), library.parent_path(),
                                  library.filename().string(), error, {}, *replacement),
        "replacement Play assets");
  f.Draw();
  Check(!f.ui.GameTwoDViewport() && f.ui.NativeGameViewport() && f.textures.Size() == 0,
        "changed Play owner retained a 2D selection or texture registration");
  f.module.Unload();
  Check(f.play.Stop(), "second Play Stop");
  f.Draw();
  Check(f.document.CaptureRuntimeScene() == baseline, "second Play changed authoring source");
  std::cout << "Main Game 2D preview: multi-Atlas pool, actual view selection, Step, DPI and Stop "
               "passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-fixture") {
      Fixture fixture(argv[2]);
      return 0;
    }
    Check(argc == 2, "fixture module argument required");
    const auto root = std::filesystem::temp_directory_path() /
                      ("nexora-game-2d-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    editor::test::TemporaryDirectoryCleanup cleanup{root};
    Run(root, std::filesystem::absolute(argv[1]));
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
