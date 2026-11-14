#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "TemporaryDirectoryCleanup.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Event = Nexora::Window::WindowEvent;
using EventType = Nexora::Window::WindowEventType;
constexpr runtime::AssetUuid atlasId{101, 1}, firstId{105, 1}, secondId{106, 1}, missingId{999, 1},
    untypedId{998, 1};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(out), "fixture write");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-build-clips-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *workspace = &writer;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content, readonly;
  editor::ProjectContentSession *current = &content;
  runtime::World world;
  runtime::Id sceneId = world.LoadScene("Build animations");
  editor::SceneDocument scene{world, sceneId};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectBuildSettings settings;
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    Check(writer.Create(root, "Build animation picker"), "workspace");
    const twod::Image image{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
    const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
    Check(bool(atlas), "Atlas fixture");
    Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
    Write(root / "Content/First###Clip.nx2clip",
          twod::EncodeClip({firstId, atlasId, true, {{1, .25, "first"}}}));
    Write(root / "Content/Second###Clip.nx2clip",
          twod::EncodeClip({secondId, atlasId, true, {{1, .25, "second"}}}));
    Check(assets.ImportTree(root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(writer, assets, 7) &&
              reader.Open(root, editor::ProjectAccess::ReadOnly) &&
              readonly.Open(reader, assets, 7, false),
          "typed Content");
    // A filename/type/ready flag alone must never authorize a Clip choice.
    Check(content.Browser().Discover({untypedId, "Content/Unimported.nx2clip", ".nx2clip", "hash",
                                      editor::ThumbnailState::Ready}),
          "untyped row");
    scene.Create("Saved root");
    Check(scene.Save(root / "Title.scene") && world.Activate(sceneId), "scene");
    editor::ProjectBuildSettingsSession session(writer);
    Check(session.Reload(), "settings reload");
    const auto entry = session.Enroll("Title.scene", content.Browser());
    Check(bool(entry), "scene enrollment");
    settings = {entry->id, {*entry}};
    settings.animation_clips = {missingId};
    Check(session.Save(settings, content.Browser()), "initial settings");
    ui.SetProjectBuildSettings(writer, *session.Settings(), true);
    ui.SetDisplay(1600, 1600, dpi);
    Access::ConfigureSyntheticInput(ui);
    Send(EventType::FocusChanged, 1);
    Settle();
    Send(EventType::Key, int(Nexora::Window::Key::B), 1, 3);
    Draw();
    Send(EventType::Key, int(Nexora::Window::Key::B), 0);
    Settle();
  }
  void Send(EventType type, int v0, int v1 = 0, int modifiers = 0) {
    Event event;
    event.type = type;
    event.value0 = v0;
    event.value1 = v1;
    event.modifiers = static_cast<Nexora::Window::KeyModifiers>(modifiers);
    ui.ProcessEvents(std::array{event});
  }
  void Draw() {
    ui.SetSceneFileContext({workspace->Project().id, scene.Generation()}, "Title.scene");
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace, current, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Settle() {
    for (int i = 0; i < 4; ++i)
      Draw();
  }
  void Click(std::optional<std::array<float, 2>> p,
             const std::source_location &from = std::source_location::current()) {
    if (!p)
      throw std::runtime_error("Visible animation control missing at line " +
                               std::to_string(from.line()));
    Send(EventType::Pointer, int((*p)[0] * scale), int((*p)[1] * scale));
    Draw();
    Send(EventType::PointerButton, 0, 1);
    Draw();
    Send(EventType::PointerButton, 0, 0);
    Settle();
  }
  std::optional<std::array<float, 2>> Asset(runtime::AssetUuid id, bool remove = false) {
    for (const auto &[asset, point] : Access::BuildAnimationAssetPositions(ui, remove))
      if (asset == id)
        return point;
    return std::nullopt;
  }
  void Open(const std::source_location &from = std::source_location::current()) {
    Click(Access::BuildAnimationPosition(ui, 0), from);
  }
  void Close(const std::source_location &from = std::source_location::current()) {
    Click(Access::BuildAnimationPosition(ui, 1), from);
  }
  void Roots(std::vector<runtime::AssetUuid> expected) {
    Check(Access::BuildAnimationRoots(ui) == expected, "animation draft identity changed");
  }
};
void Run(float scale) {
  Fixture f(scale);
  const auto original = Read(f.root / "project.build.json");
  const auto buildPosition = Access::ProjectBuildSettingsPosition(f.ui, 13);
  f.Open();
  Check(f.Asset(firstId) && f.Asset(secondId) && !f.Asset(atlasId) && !f.Asset(untypedId) &&
            f.Asset(missingId, true),
        "typed choices or unresolved saved root missing");
  f.Click(f.Asset(firstId));
  f.Roots({missingId, firstId});
  f.Click(f.Asset(firstId)); // Already selected, disabled.
  f.Roots({missingId, firstId});
  f.Click(f.Asset(missingId, true));
  f.Roots({firstId});
  Check(Read(f.root / "project.build.json") == original, "picker wrote settings before Save");
  f.Close();
  Check(!Access::BuildAnimationPosition(f.ui, 1), "Close animation assets did not close the popup");
  Check(Access::ProjectBuildSettingsPosition(f.ui, 13) == buildPosition,
        "animation picker moved the existing Build control");
  f.Click(Access::ProjectBuildSettingsPosition(f.ui, 5));
  auto request = f.ui.TakeProjectBuildSettingsRequest();
  editor::ProjectBuildSettingsSession session(f.writer);
  Check(bool(request), "Save control did not emit a request");
  Check(request->settings.animation_clips == std::vector{firstId}, "Save lost animation roots");
  std::string error;
  if (!session.Reload(&error) || !session.Save(request->settings, f.content.Browser(), &error))
    throw std::runtime_error("UI Save request failed: " + error);
  editor::ProjectBuildSettingsSession reopened(f.writer);
  Check(reopened.Reload() && reopened.Settings()->animation_clips == std::vector{firstId},
        "saved roots did not reopen");
  f.settings = *session.Settings();
  f.ui.SetProjectBuildSettings(f.writer, f.settings, true);
  f.Settle();
  f.Open();
  const auto stale = f.Asset(secondId);
  Check(f.content.Rename(secondId, "Renamed###Clip.nx2clip"), "rename Clip");
  f.Click(stale);
  f.Roots({firstId});
  Check(!Access::BuildAnimationPosition(f.ui, 1), "stale Content popup retained");
  f.Open();
  f.Click(f.Asset(secondId));
  f.Roots({firstId, secondId});
  f.Click(f.Asset(secondId, true));
  f.Roots({firstId});
  for (int blocked = 0; blocked < 3; ++blocked) {
    f.ui.SetGameBuildStatus(blocked == 0, "Busy");
    f.ui.SetGameRunStatus(blocked == 1, "Running");
    if (blocked == 2)
      f.Send(EventType::FocusChanged, 0);
    f.Settle();
    f.Click(f.Asset(secondId));
    f.Click(f.Asset(firstId, true));
    f.Roots({firstId});
    f.Send(EventType::FocusChanged, 1);
  }
  f.ui.SetGameBuildStatus(false, "Ready");
  f.ui.SetGameRunStatus(false, "Stopped");
  f.Settle();
  const auto playingChoice = f.Asset(secondId);
  Check(f.play.Start(.1, [](runtime::World &, double) { return true; }), "Play start");
  f.Settle();
  f.Click(playingChoice); // Play may hide the popup; stale coordinates still grant no edit.
  f.Roots({firstId});
  f.play.Stop();
  f.Settle();
  if (!Access::BuildAnimationPosition(f.ui, 1))
    f.Open();
  const auto incoming = f.Asset(secondId);
  f.ui.SetProjectBuildSettings(f.writer, f.settings, true);
  f.Click(incoming);
  f.Roots({firstId});
  f.Open();
  f.Click(f.Asset(secondId));
  f.Roots({firstId, secondId});
  f.Close();
  auto external = f.settings;
  external.build.output = "Build/External";
  const auto bytes = editor::SerializeProjectBuildSettings(external);
  Check(bool(bytes), "external settings encode");
  Write(f.root / "project.build.json", std::as_bytes(std::span(bytes->data(), bytes->size())));
  f.Click(Access::ProjectBuildSettingsPosition(f.ui, 5));
  request = f.ui.TakeProjectBuildSettingsRequest();
  Check(request && !session.Save(request->settings, f.content.Browser()) &&
            Read(f.root / "project.build.json") == *bytes,
        "external settings baseline overwritten");
  f.Roots({firstId, secondId});
  f.Click(Access::ProjectBuildSettingsPosition(f.ui, 6));
  request = f.ui.TakeProjectBuildSettingsRequest();
  Check(request && request->action == editor::imgui::ProjectBuildSettingsAction::Reload &&
            session.Reload(),
        "actual Reload request");
  f.settings = *session.Settings();
  f.ui.SetProjectBuildSettings(f.writer, f.settings, true);
  f.Settle();
  f.Roots({firstId});
  auto full = f.settings;
  full.animation_clips.clear();
  for (std::uint64_t i = 0; i < 4096; ++i)
    full.animation_clips.push_back({777, i + 1});
  f.ui.SetProjectBuildSettings(f.writer, full, true);
  f.Settle();
  f.Open();
  Check(Access::BuildAnimationAssetPositions(f.ui, true).size() < 64,
        "animation roots were not virtualized");
  f.Click(f.Asset(firstId));
  f.Roots(full.animation_clips);
  auto oversized = full;
  oversized.animation_clips.push_back(secondId);
  f.ui.SetProjectBuildSettings(f.writer, oversized, false);
  f.Settle();
  f.Roots(full.animation_clips);
  f.Close();
  f.ui.SetProjectBuildSettings(f.writer, f.settings, true);
  f.Settle();
  f.Open();
  const auto detached = f.Asset(secondId);
  f.current = nullptr;
  f.Click(detached);
  f.Roots({firstId});
  f.current = &f.content;
  f.Settle();
  f.Open();
  const auto foreignChoice = f.Asset(secondId);
  editor::ProjectWorkspace foreign;
  editor::AssetWorkspace foreignAssets;
  editor::ProjectContentSession foreignContent;
  Check(foreign.Create(f.root / "Foreign", "Foreign Content") &&
            foreignAssets.ImportTree(foreign.Root() / "Content", {}, {},
                                     editor::AssetIdentityMode::PersistentReadWrite) &&
            foreignContent.Open(foreign, foreignAssets, 7),
        "foreign Content fixture");
  f.current = &foreignContent;
  f.Click(foreignChoice);
  f.Roots({firstId});
  Check(!Access::BuildAnimationPosition(f.ui, 1), "foreign Content popup retained");
  f.workspace = &f.reader;
  f.current = &f.readonly;
  f.ui.SetProjectBuildSettings(f.reader, f.settings, true);
  f.Settle();
  const auto readonlyBytes = Read(f.root / "project.build.json");
  f.Open();
  f.Click(f.Asset(secondId));
  f.Click(f.Asset(firstId, true));
  f.Roots({firstId});
  f.Close();
  f.Click(Access::ProjectBuildSettingsPosition(f.ui, 5));
  Check(!f.ui.TakeProjectBuildSettingsRequest() &&
            Read(f.root / "project.build.json") == readonlyBytes,
        "read-only inspection emitted a write");
  f.workspace = &f.writer;
  f.current = &f.content;
  f.ui.SetProjectBuildSettings(f.writer, f.settings, true);
  f.Settle();
  f.Open();
  const auto recoveryChoice = f.Asset(secondId);
  std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\ndocument=Recovered.scene\n";
  f.Settle();
  f.Click(recoveryChoice);
  f.Roots({firstId});
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout
        << "Build animation picker actual 1x/2x controls, scoped drafts and persistence passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
