#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/SpriteSourceCatalog.h"
#include "TemporaryDirectoryCleanup.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace nexora;
namespace {
using Access = editor::imgui::EditorImGuiTestAccess;
using Event = Nexora::Window::WindowEvent;
using EventType = Nexora::Window::WindowEventType;
constexpr runtime::AssetUuid atlasId{101, 1}, sourceId{102, 1}, secondId{103, 1};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
twod::SpriteAsset Source(runtime::AssetUuid id, float pivot = 2) {
  twod::SpriteAsset source;
  source.id = id;
  source.atlas = atlasId;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = 4;
  source.trimX = source.trimY = source.trimWidth = source.trimHeight = 1;
  source.pivotX = pivot;
  source.pivotY = 2;
  source.pixelsPerUnit = 1;
  return source;
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(output), "asset fixture write");
}
void Assets(const std::filesystem::path &root) {
  const twod::Image image{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
  Check(bool(atlas), "Atlas fixture");
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
  Write(root / "Content/First.nx2sprite", twod::EncodeSpriteAsset(Source(sourceId)));
  Write(root / "Content/Second.nx2sprite", twod::EncodeSpriteAsset(Source(secondId, 3)));
}
void DetailsAssets(const std::filesystem::path &root) {
  twod::Atlas atlas{
      atlasId, 16, 16, 1, false, {{1, 3, 4, 7, 9}}, twod::Bytes(16 * 16 * 4, std::byte{255})};
  auto source = Source(sourceId);
  source.sourceWidth = 19;
  source.sourceHeight = 23;
  source.trimX = 3;
  source.trimY = 5;
  source.trimWidth = 7;
  source.trimHeight = 9;
  source.pivotX = 4.5F;
  source.pivotY = 8.25F;
  source.pixelsPerUnit = 2;
  Check(twod::ValidateAtlas(atlas) && twod::ValidateSpriteAsset(source),
        "asymmetric source geometry fixture");
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(atlas));
  Write(root / "Content/First.nx2sprite", twod::EncodeSpriteAsset(source));
  source.id = secondId;
  source.pivotX = 6.5F;
  Write(root / "Content/Second.nx2sprite", twod::EncodeSpriteAsset(source));
}
void Populate(editor::SceneDocument &scene) {
  const auto first = scene.Create("First"), second = scene.Create("Second");
  Check(scene.SetTransform(first, {-1, 0, 0}) && scene.SetTransform(second, {1, 0, 0}) &&
            scene.SetOpaqueComponent(*scene.Key(first), {991, "unrelated.plugin", {7, 9, 11}}),
        "scene fixture");
}
void NativeFixture(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Native Sprite Inspector"), "native project fixture");
  Assets(root);
  DetailsAssets(root);
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Native Sprite Inspector"));
  Populate(scene);
  std::filesystem::create_directories(root / ".nexora/scenes");
  Check(scene.Save(root / ".nexora/scenes/Main.scene"), "native scene save");
}
void Run(float dpi) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-sprite-inspector-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup(root);
  editor::ProjectWorkspace writer, reader;
  Check(writer.Create(root, "Sprite Inspector"), "project fixture");
  Assets(root);
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "typed asset import");
  editor::ProjectContentSession content, readonly;
  Check(content.Open(writer, assets, 7) && reader.Open(root, editor::ProjectAccess::ReadOnly) &&
            readonly.Open(reader, assets, 7, false),
        "Content fixture");
  runtime::World world;
  const auto id = world.LoadScene("Sprite Inspector");
  Check(world.Activate(id), "scene activation");
  editor::SceneDocument scene(world, id);
  Populate(scene);
  auto keys = std::vector{*scene.Key(scene.Nodes()[0].id), *scene.Key(scene.Nodes()[1].id)};
  Check(scene.Select(keys), "selection fixture");
  const auto runtimeBefore = world.SaveScene(id);
  const auto initial = scene.PrepareSave()->Bytes();
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  ui.SetDisplay(1600, 1200, dpi);
  Access::ConfigureSyntheticInput(ui);
  auto *workspace = &writer;
  auto *current = &content;
  runtime::PlaySession play(world);
  const auto send = [&](EventType type, int value0, int value1 = 0) {
    Event e{};
    e.type = type;
    e.value0 = value0;
    e.value1 = value1;
    ui.ProcessEvents(std::array{e});
  };
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace, current, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  };
  const auto settle = [&] {
    for (int i = 0; i < 4; ++i)
      draw();
  };
  const auto point = [&](std::size_t control) {
    const auto value = Access::InspectorSpriteSourcePosition(ui, control);
    Check(bool(value), "actual visible Sprite source control missing");
    return *value;
  };
  const auto hover = [&](std::array<float, 2> p) {
    send(EventType::Pointer, int(p[0] * dpi), int(p[1] * dpi));
    draw();
  };
  const auto click = [&](std::array<float, 2> p) {
    hover(p);
    send(EventType::PointerButton, 0, 1);
    draw();
    send(EventType::PointerButton, 0, 0);
    settle();
  };
  const auto choose = [&](runtime::AssetUuid asset) {
    click(point(0));
    const auto p = Access::InspectorSpriteSourceChoicePosition(ui, asset);
    Check(bool(p), "actual typed Sprite choice missing");
    click(*p);
  };
  const auto refs = [&](std::optional<runtime::AssetUuid> first,
                        std::optional<runtime::AssetUuid> second) {
    Check(editor::ReadSpriteSourceReference(scene, keys[0]) == first &&
              editor::ReadSpriteSourceReference(scene, keys[1]) == second,
          "Sprite reference result differs from actual control input");
  };
  const auto detail = [&](std::string_view expected) {
    Check(Access::InspectorSpriteSourceDetails(ui).find(expected) != std::string_view::npos,
          "current Sprite source detail differs from independent geometry oracle");
  };
  const auto noDetails = [&] {
    Check(Access::InspectorSpriteSourceDetails(ui).empty(), "stale Sprite source details remain");
  };
  send(EventType::FocusChanged, 1);
  settle();
  Access::FocusInspector(ui);
  settle();
  Check(Access::InspectorSpriteSourceLabel(ui) == "None", "empty source label");
  choose(sourceId);
  refs(sourceId, sourceId);
  noDetails(); // Closed by default; inspection is an explicit user action.
  click(point(3));
  detail("Original: 4 x 4 px");
  detail("Pivot (bottom-left): 2, 2 px");
  detail("(-1, 0) to (0, 1)");
  Check(world.SaveScene(id) == runtimeBefore && scene.Undo() &&
            scene.PrepareSave()->Bytes() == initial && scene.Redo(),
        "one atomic metadata Undo");
  settle();
  const auto saved = root / "Saved.scene";
  Check(scene.Save(saved), "wrapped scene save");
  const auto baseline = scene.PrepareSave()->Bytes();
  choose(secondId);
  refs(secondId, secondId);
  detail("Pivot (bottom-left): 3, 2 px");
  Check(scene.Undo() && !scene.Dirty(), "saved baseline Undo");
  settle();
  detail("Pivot (bottom-left): 2, 2 px");
  choose(sourceId);
  Check(scene.Redo(), "equal assignment consumed pending Redo");
  refs(secondId, secondId);
  settle();
  click(point(2));
  refs({}, {});
  noDetails();
  Check(scene.Undo(), "clear batch Undo");
  refs(secondId, secondId);
  settle();

  Check(scene.SetOpaqueComponent(keys[0], editor::SpriteSourceReference(sourceId)), "mixed setup");
  settle();
  Check(Access::InspectorSpriteSourceLabel(ui) == "Mixed", "mixed source label");
  noDetails();
  click(point(2));
  refs({}, {});
  Check(scene.Undo(), "mixed batch Undo");
  Check(scene.SetOpaqueComponents(keys, editor::SpriteSourceReference({999, 1})), "missing setup");
  settle();
  Check(Access::InspectorSpriteSourceLabel(ui) == "Missing source or Atlas",
        "missing source label");
  noDetails();
  click(point(2));
  refs({}, {});
  Check(scene.Undo(), "missing clear Undo");
  auto unsupported = editor::SpriteSourceReference(sourceId);
  unsupported.data[0] = 2;
  Check(scene.SetOpaqueComponent(keys[0], unsupported), "unsupported setup");
  settle();
  const auto unsupportedBytes = scene.PrepareSave()->Bytes();
  Check(Access::InspectorSpriteSourceLabel(ui) == "Unsupported reference", "unsupported label");
  noDetails();
  click(point(2));
  click(point(0));
  Check(scene.PrepareSave()->Bytes() == unsupportedBytes &&
            !Access::InspectorSpriteSourcePosition(ui, 1),
        "unsupported bytes edited");
  Check(scene.SetOpaqueComponents(keys, editor::SpriteSourceReference(sourceId)), "restore setup");
  settle();
  const auto stable = scene.PrepareSave()->Bytes();
  const auto clear = point(2);
  workspace = &reader;
  current = &readonly;
  settle();
  detail("Pivot (bottom-left): 2, 2 px");
  click(point(2));
  click(point(0));
  Check(scene.PrepareSave()->Bytes() == stable, "read-only UI mutated document");
  workspace = &writer;
  current = &content;
  settle();
  Check(play.Start(.1, [](runtime::World &, double) { return true; }), "Play fixture start");
  settle();
  detail("Original: 4 x 4 px");
  click(clear);
  Check(scene.PrepareSave()->Bytes() == stable, "Play source edit admitted");
  play.Stop();
  settle();
  send(EventType::FocusChanged, 0);
  settle();
  click(clear);
  Check(scene.PrepareSave()->Bytes() == stable, "unfocused source edit admitted");
  send(EventType::FocusChanged, 1);
  settle();

  click(point(0));
  const auto staleChoice = *Access::InspectorSpriteSourceChoicePosition(ui, secondId);
  Write(root / "Content/First.nx2sprite", twod::EncodeSpriteAsset(Source(sourceId, 4)));
  Check(content.Reimport(sourceId), "source reimport");
  click(staleChoice);
  Check(scene.PrepareSave()->Bytes() == stable, "stale reimport popup assigned");
  detail("Pivot (bottom-left): 4, 2 px");
  choose(secondId);
  refs(secondId, secondId);
  Check(scene.Undo(), "fresh reimport assignment Undo");
  settle();
  click(point(0));
  const auto selectionChoice = *Access::InspectorSpriteSourceChoicePosition(ui, secondId);
  hover(selectionChoice);
  send(EventType::PointerButton, 0, 1);
  draw();
  Check(scene.Select(std::span(keys).first(1)), "selection replacement");
  send(EventType::PointerButton, 0, 0);
  settle();
  Check(scene.PrepareSave()->Bytes() == stable && scene.Redo(),
        "stale selection edit consumed Redo");
  Check(scene.Select(keys) && scene.Undo(), "selection restore");
  settle();
  auto invalid = Source(sourceId, 4);
  invalid.region = 99;
  Write(root / "Content/First.nx2sprite", twod::EncodeSpriteAsset(invalid));
  Check(content.Reimport(sourceId), "valid metadata with missing Atlas region import");
  settle();
  Check(Access::InspectorSpriteSourceLabel(ui) == "Missing source or Atlas",
        "missing Atlas closure label");
  noDetails();
  click(point(0));
  Check(!Access::InspectorSpriteSourcePosition(ui, 1) && scene.PrepareSave()->Bytes() == stable,
        "incomplete closure offered a source choice");
  send(EventType::Key, int(Nexora::Window::Key::Escape), 1);
  draw();
  send(EventType::Key, int(Nexora::Window::Key::Escape), 0);
  settle();
  click(point(2));
  refs({}, {});
  Check(scene.Undo(), "incomplete closure prevented clear Undo");
  Write(root / "Content/First.nx2sprite", twod::EncodeSpriteAsset(Source(sourceId, 4)));
  Check(content.Reimport(sourceId), "restored source closure import");
  settle();
  click(point(0));
  const auto projectChoice = *Access::InspectorSpriteSourceChoicePosition(ui, secondId);
  Check(content.Open(writer, assets, 8), "new project generation");
  click(projectChoice);
  Check(scene.PrepareSave()->Bytes() == stable, "stale project popup assigned");
  std::ofstream(root / ".nexora/workspace.recovery") << "schema=1\ndocument=Recovered.scene\n";
  settle();
  click(clear);
  Check(scene.PrepareSave()->Bytes() == stable, "recovery source edit admitted");
  std::filesystem::remove(root / ".nexora/workspace.recovery");
  Check(scene.Reload(saved), "wrapped source reopen");
  keys = {*scene.Key(keys[0].id), *scene.Key(keys[1].id)};
  Check(scene.Select(keys) && scene.PrepareSave()->Bytes() == baseline, "reopen identity or bytes");
  refs(sourceId, sourceId);
  DetailsAssets(root);
  Check(content.Reimport(atlasId) && content.Reimport(sourceId), "details geometry reimport");
  settle();
  detail("Original: 19 x 23 px");
  detail("Trim (top-left): 3, 5; 7 x 9 px");
  detail("Pivot (bottom-left): 4.5, 8.25 px");
  detail("Pixels per unit: 2");
  detail("Borders (L/R/T/B): 0 / 0 / 0 / 0 px");
  detail("Atlas: 16 x 16 px");
  detail("Region 1 (top-left): 3, 4; 7 x 9 px");
  detail("(-0.75, 0.375) to (2.75, 4.875)");
  const auto inspected = scene.PrepareSave()->Bytes();
  const auto sourceBytes =
      twod::EncodeSpriteAsset(*content.Browser().Find(sourceId)->twod->SpriteSource());
  click(point(3));
  noDetails();
  click(point(3));
  detail("(-0.75, 0.375) to (2.75, 4.875)");
  Check(scene.PrepareSave()->Bytes() == inspected && world.SaveScene(id) == runtimeBefore &&
            scene.Undo() == false && scene.Redo() == false,
        "read-only geometry inspection modified document/Runtime/history");
  std::ifstream sourceFile(root / "Content/First.nx2sprite", std::ios::binary);
  const std::string sourceRead{std::istreambuf_iterator<char>(sourceFile), {}};
  Check(sourceRead ==
            std::string(reinterpret_cast<const char *>(sourceBytes.data()), sourceBytes.size()),
        "geometry inspection modified source bytes");
  current = nullptr;
  settle();
  noDetails();
  current = &content;
  settle();
  detail("Original: 19 x 23 px");
  // A single source can be shared by many selected entities in a large unrelated catalog.
  // Exercise actual idle presentation without an O(selected * Content rows) lookup path.
  auto rows = std::vector<editor::ContentItem>(content.Browser().Items().begin(),
                                               content.Browser().Items().end());
  for (std::uint64_t index = 1; index <= 32768; ++index) {
    editor::ContentItem row;
    row.id = {0x7fffffff, index};
    row.path = "Content/000-Unrelated/" + std::to_string(index) + ".txt";
    row.type = ".txt";
    rows.push_back(std::move(row));
  }
  Check(content.Browser().Reset(rows, 8), "large unrelated Content fixture");
  for (int index = 0; index < 30; ++index)
    static_cast<void>(scene.Create("Shared source " + std::to_string(index)));
  std::vector<editor::SceneDocument::NodeKey> shared;
  for (const auto &node : scene.Nodes())
    shared.push_back(*scene.Key(node.id));
  Check(scene.Select(shared) &&
            scene.SetOpaqueComponents(shared, editor::SpriteSourceReference(sourceId)),
        "shared source selection fixture");
  const auto beforeIdle = scene.PrepareSave()->Bytes();
  const auto started = std::chrono::steady_clock::now();
  draw();
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  Check(Access::InspectorSpriteSourceLabel(ui) == "Content/First.nx2sprite" &&
            scene.PrepareSave()->Bytes() == beforeIdle,
        "large catalog idle presentation changed references");
  std::cout << "Sprite source presentation: " << rows.size() << " Content rows, " << shared.size()
            << " selected, observed frame " << elapsed << "ms (diagnostic only)\n";
  std::cout << "Sprite Inspector " << dpi
            << "x: actual choices, batch history, reopen, missing/unsupported, access and stale "
               "popup guards passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-native-fixture") {
      NativeFixture(std::filesystem::path(
          std::u8string(argv[2], argv[2] + std::char_traits<char>::length(argv[2]))));
      return 0;
    }
    Run(1);
    Run(2);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
