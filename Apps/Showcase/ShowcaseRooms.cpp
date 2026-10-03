#include "ShowcaseRooms.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Math/Math.h"
#include "Nexora/Renderer/SceneFrame.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Runtime/GameplaySimulation.h"
#include "Nexora/Runtime/InputUi.h"
#include "Nexora/Runtime/LargeWorld.h"
#include "Nexora/Runtime/Platform.h"
#include "Nexora/Runtime/Presentation.h"
#include "Nexora/Runtime/Runtime.h"
#include "Nexora/Runtime/Shipping.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nexora::showcase {
namespace {
using Key = Nexora::Window::Key;
using namespace runtime;
constexpr std::array<std::string_view, 8> rooms{
    "hub", "rendering", "scene", "input", "gameplay", "presentation", "streaming", "shipping"};
constexpr std::array<std::string_view, 7> tourRooms{
    "hub", "rendering", "scene", "gameplay", "presentation", "streaming", "shipping"};
std::string Number(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << value;
  return out.str();
}
std::string Escape(std::string_view value) {
  std::string out;
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n')
      out += "\\n";
    else if (c >= 32)
      out += c;
  }
  return out;
}
// Original compact 5x7 glyphs; authored here, no external font or redistributable asset required.
constexpr std::string_view characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.:/()_=+!? ";
constexpr std::array<std::array<std::uint8_t, 7>, 48> glyphs{
    {{14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
     {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
     {14, 17, 16, 23, 17, 17, 14}, {17, 17, 17, 31, 17, 17, 17}, {31, 4, 4, 4, 4, 4, 31},
     {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
     {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
     {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
     {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
     {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
     {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
     {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
     {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
     {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
     {0, 0, 0, 31, 0, 0, 0},       {0, 0, 0, 0, 0, 12, 12},      {0, 12, 12, 0, 12, 12, 0},
     {1, 2, 2, 4, 8, 8, 16},       {2, 4, 8, 8, 8, 4, 2},        {8, 4, 2, 2, 2, 4, 8},
     {0, 0, 0, 0, 0, 0, 31},       {0, 0, 31, 0, 31, 0, 0},      {0, 4, 4, 31, 4, 4, 0},
     {4, 4, 4, 4, 4, 0, 4},        {14, 17, 1, 2, 4, 0, 4},      {0, 0, 0, 0, 0, 0, 0}}};
} // namespace

struct RoomSession::State final {
  std::string selected{"hub"};
  bool minimal{}, tour{}, paused{}, overview{true}, profiler{true}, matrix{}, french{},
      textureUploaded{};
  double clock{}, tourSeconds{};
  std::size_t tourStep{}, selectedProbe{}, reloads{}, probeRuns{}, droppedParticles{},
      cameraMoves{};
  std::uint64_t ticks{}, inputSequence{};
  std::set<Key> held;
  std::set<std::string> visited;
  std::set<std::string> visualized;
  float yaw{0.6F}, pitch{0.45F}, radius{14.0F};
  int pointerX{}, pointerY{};
  bool dragging{};
  std::uint64_t atlasGeneration{~std::uint64_t{0}};
  std::string lastAction{"Ready"}, pluginLibrary;
  ErrorInjection injection{ErrorInjection::None};
  std::size_t metricOffset{}, primitive{};
  std::vector<ProbeResult> probes{ProbeRegistry::CreateV1Registry().RunAll()};
  std::vector<Nexora::Presentation::SceneVertex> vertices;
  std::vector<Nexora::Presentation::SceneInstance> instances;
  std::array<std::byte, 8 * 8 * 4> checker{};
  std::vector<Nexora::Presentation::UiTextureUpload> sceneUploads;
  std::vector<std::uint16_t> indices;
  std::vector<Nexora::Presentation::UiVertex> uiVertices;
  std::vector<std::uint32_t> uiIndices;
  std::vector<Nexora::Presentation::UiDrawCommand> commands;
  std::vector<Nexora::Presentation::UiTextureUpload> uploads;
  std::array<std::byte, 128 * 64 * 4> atlas{};
  LocalizationTable localization;
  InputSystem input;
  ActionMap actions;
  UIDocument document{1280, 720};
  World editorWorld;
  Id editorScene{}, editorEntity{};
#if NEXORA_EDITOR_SDK_ENABLED
  SceneEditor editor{editorWorld};
  PlaySession play{editorWorld};
  ReflectionRegistry reflection;
  std::unique_ptr<PrefabInstance> prefab;
#endif
#if NEXORA_ASSET_PIPELINE_ENABLED
  AssetGenerationStore assets;
  std::string assetHash;
  renderer::Mesh assetMesh;
  bool assetRejected{}, cycleRejected{}, rolledBack{};
#endif
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  PhysicsWorld physics;
  CharacterController controller;
  StandardCharacterMotor motor;
  CharacterState character;
  CharacterMoveResult motion;
  NavigationWorld navigation;
  std::optional<NavigationPath> path;
  Blackboard blackboard{{true}};
  BehaviorProgram behavior{{{BehaviorOp::BlackboardBool, 0, 0, 0}}};
  BehaviorTrace trace;
  PerceptionSystem perception;
  std::vector<Stimulus> stimuli;
  std::optional<PhysicsHit> hit;
#endif
#if NEXORA_PRESENTATION_ENABLED
  presentation::AnimationGraph animation;
  presentation::AnimationPose pose;
  presentation::SkinningPalette skin;
  presentation::ParticleSystem particles{32, presentation::ParticleRenderer::Sprite};
  presentation::ResidencyTracker residency;
  presentation::AudioEngine audio{2, residency};
  presentation::VideoPlayer video{2};
  std::uint64_t videoSequence{};
  bool backPressure{};
#endif
#if NEXORA_LARGE_WORLD_ENABLED
  large_world::StreamingManager streaming{{4096, 2048}, 1};
  large_world::TerrainClipmap terrain;
  large_world::VegetationField vegetation;
  large_world::OfflineHlodSet hlod;
  const std::vector<large_world::VegetationInstance> trees{
      {{-4, 0, -4}, 1}, {{0, 0, -4}, 1}, {{4, 0, 4}, 1}};
#endif
#if NEXORA_PLATFORM_ENABLED
  platform::Runtime platform;
#endif
#if NEXORA_SHIPPING_ENABLED
  shipping::BundleUpdater updater{{1, "showcase-v1"}};
  shipping::CrashReporter crash{8};
  std::vector<shipping::PackageResult> profiles;
  bool updateRolledBack{};
#endif

  State() {
    for (std::size_t y = 0; y < 8; ++y)
      for (std::size_t x = 0; x < 8; ++x) {
        const auto offset = (y * 8 + x) * 4;
        const auto shade = ((x / 2 + y / 2) % 2) ? std::byte{70} : std::byte{255};
        checker[offset] = checker[offset + 1] = checker[offset + 2] = shade;
        checker[offset + 3] = std::byte{255};
      }
    for (std::size_t glyph = 0; glyph < glyphs.size(); ++glyph)
      for (std::size_t y = 0; y < 7; ++y)
        for (std::size_t x = 0; x < 5; ++x) {
          const auto offset = (((glyph / 16) * 8 + y) * 128 + (glyph % 16) * 8 + x) * 4;
          atlas[offset] = atlas[offset + 1] = atlas[offset + 2] = std::byte{255};
          atlas[offset + 3] = (glyphs[glyph][y] & (1U << (4 - x))) ? std::byte{255} : std::byte{0};
        }
    for (std::size_t c = 0; c < 4; ++c)
      atlas[(63 * 128 + 127) * 4 + c] = std::byte{255};
    localization.Set("en", "title", "Nexora Visual Showcase");
    localization.Set("fr", "title", "Nexora Vitrine Visuelle");
    localization.Set("en", "fallback", "Missing translation uses English fallback");
    input.Assign(0, InputDeviceKind::Keyboard, 0);
    input.Assign(0, InputDeviceKind::Mouse, 0);
    input.Assign(0, InputDeviceKind::Gamepad,
                 0); // Extensible public input route; no device is fabricated.
    actions.Bind("move-x", {"D", 1});
    actions.Bind("move-x", {"A", -1});
    actions.Bind("move-z", {"S", 1});
    actions.Bind("move-z", {"W", -1});
    for (std::size_t i = 0; i < rooms.size(); ++i) {
      const auto id = document.Create(UIElementKind::Button);
      auto *element = document.Find(id);
      element->bounds = {18 + static_cast<float>(i) * 154, 75, 146, 28};
      element->on_pointer = [this, i](const UIPointerEvent &event, PointerPhase phase) {
        if (phase != PointerPhase::Target || !event.pressed)
          return false;
        SelectRoom(rooms[i]);
        return true;
      };
    }
    editorScene = editorWorld.LoadScene("Showcase Editor");
    editorWorld.Activate(editorScene);
#if NEXORA_EDITOR_SDK_ENABLED
    editorEntity = editor.CreateEntity(editorScene);
    reflection.Register({"Transform",
                         HashTypeName("Transform"),
                         {{"x", HashTypeName("float"), offsetof(Transform, x), sizeof(float)}}});
    auto prefabSource = std::make_shared<Prefab>(PrefabNode{"Cube", {{"color", "blue"}}, {}});
    prefab = std::make_unique<PrefabInstance>(prefabSource);
    prefab->SetOverride("", "color", "cyan");
    prefab->Rebase(std::make_shared<Prefab>(PrefabNode{"Cube", {{"color", "orange"}}, {}}));
#else
    editorEntity = editorWorld.CreateEntity(editorScene).id;
#endif
#if NEXORA_ASSET_PIPELINE_ENABLED
    ImporterRegistry importer;
    importer.Register(".showcase", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      if (source.bytes.empty())
        return {};
      return CanonicalAsset{source.id, source.type, {}, source.bytes};
    });
    const auto authoredMesh = renderer::MakeProceduralRenderingRoom().mesh;
    std::ostringstream sourceText;
    sourceText << "nexora.showcase.mesh.v1 " << authoredMesh.vertices.size() << ' '
               << authoredMesh.indices.size() << '\n';
    for (const auto &vertex : authoredMesh.vertices) {
      for (const auto value : vertex.position)
        sourceText << value << ' ';
      for (const auto value : vertex.normal)
        sourceText << value << ' ';
      for (const auto value : vertex.uv)
        sourceText << value << ' ';
      sourceText << '\n';
    }
    for (const auto index : authoredMesh.indices)
      sourceText << index << ' ';
    const auto text = sourceText.str();
    const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
    const SourceAsset source{
        {0x4e58, 1}, "procedural-mesh-v1", "cube.showcase", ByteBuffer{bytes.begin(), bytes.end()}};
    const auto canonical = importer.Import(source);
    DerivedDataCache cache;
    const auto cooked =
        canonical ? AssetCooker{}.Cook(*canonical, "portable", "showcase-v1", cache) : std::nullopt;
    if (!cooked)
      throw std::runtime_error("Showcase procedural asset cook failed");
    assetHash = cooked->content_hash;
    for (std::uint64_t generation = 1; generation <= 2; ++generation) {
      const auto bundle = BundleBuilder::Build("showcase", generation, {}, {*cooked});
      if (!bundle || !assets.Stage({*bundle}) || !assets.ActivateStaged())
        throw std::runtime_error("Showcase bundle activation failed");
    }
    rolledBack = assets.Rollback();
    const auto *loadedAsset = assets.Load(source.id);
    if (!loadedAsset)
      throw std::runtime_error("Showcase asset load failed");
    const std::string loadedText(reinterpret_cast<const char *>(loadedAsset->payload.data()),
                                 loadedAsset->payload.size());
    std::istringstream reader(loadedText);
    std::string schema;
    std::size_t vertexCount{}, indexCount{};
    if (!(reader >> schema >> vertexCount >> indexCount) || schema != "nexora.showcase.mesh.v1" ||
        vertexCount > 65535 || indexCount > 1048576)
      throw std::runtime_error("Unsupported Showcase mesh schema");
    assetMesh.vertices.resize(vertexCount);
    assetMesh.indices.resize(indexCount);
    for (auto &vertex : assetMesh.vertices) {
      for (auto &value : vertex.position)
        reader >> value;
      for (auto &value : vertex.normal)
        reader >> value;
      for (auto &value : vertex.uv)
        reader >> value;
    }
    for (auto &index : assetMesh.indices)
      reader >> index;
    if (!reader)
      throw std::runtime_error("Invalid cooked Showcase mesh");
    assetRejected = !importer.Import({source.id, source.type, source.source_path, {}});
    cycleRejected =
        !BundleBuilder::ValidateDependencyDag({{"a", 1, {"b"}, {}}, {"b", 1, {"a"}, {}}});
#endif
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
    physics.AddBody({1, {-6, -1, -6}, {6, 0, 6}, false, false, {}});
    physics.AddBody({2, {1, 0, 1}, {2, 1.5, 2}, false, false, {}});
    physics.AddBody({3, {-2, 0, 1}, {-1, 0.25, 2}, false, false, {}});
    for (std::uint64_t step = 0; step < 5; ++step) {
      const double x = -4 + static_cast<double>(step) * 0.4;
      const double height = 0.2 + static_cast<double>(step) * 0.2;
      physics.AddBody({10 + step, {x, 0, 1}, {x + 0.4, height, 2}, false, false, {}});
    }
    character.position = {-2, 0, 0};
    navigation.LoadTile(
        1, {{1, 1, {-3, 0, -3}, {2}}, {2, 1, {0, 0, -3}, {1, 3}}, {3, 1, {3, 0, -3}, {2}}});
    path = navigation.FindPath(1, 3);
    perception.Publish({2, StimulusKind::Sight, {1, 1, 1}, 1});
#endif
#if NEXORA_PRESENTATION_ENABLED
    presentation::Skeleton skeleton;
    skeleton.Build({{-1, "root"}, {0, "arm"}, {1, "hand"}});
    animation.SetSkeleton(std::move(skeleton));
    animation.AddClip({1,
                       2,
                       true,
                       {{0, {{0, {0, 0, 0}}, {2, {1, 0, 0}}}},
                        {1, {{0, {0, 1, 0}}, {1, {0.6F, 1.5F, 0}}, {2, {0, 1, 0}}}},
                        {2, {{0, {0, 2, 0}}, {2, {0, 2.5F, 0}}}}}});
    animation.AddClip({2,
                       2,
                       true,
                       {{0, {{0, {0, 0, 0}}, {2, {-1, 0, 0}}}},
                        {1, {{0, {0, 1, 0}}, {2, {0.4F, 1.2F, 0}}}},
                        {2, {{0, {0, 2, 0}}, {2, {-0.4F, 2.8F, 0}}}}}});
    animation.Play(1);
    residency.Acquire(1);
    audio.SetBusGain("Master", 0.6F);
    if (!audio.Play({1, "Master", 0.8F, false}))
      throw std::runtime_error("Showcase audio voice failed");
    video.AddSubtitle({0, 10000, "Contract-only queue / no decoder SDK"});
    video.SubmitDecoded({1, 0, 1});
    video.SubmitDecoded({2, 1, 2});
    backPressure = !video.SubmitDecoded({3, 2, 3});
    video.Seek(0); // The demonstration really invalidates queued frames.
#endif
#if NEXORA_LARGE_WORLD_ENABLED
    for (std::uint64_t cell = 1; cell <= 9; ++cell) {
      const double x = static_cast<double>((cell - 1) % 3) * 4 - 6;
      const double z = static_cast<double>((cell - 1) / 3) * 4 - 6;
      const large_world::Bounds bounds{{x, -1, z}, {x + 4, 1, z + 4}};
      streaming.AddCell({cell, 100 + cell, 200 + cell, bounds, 512, 256, 64, 32});
      hlod.Add({200 + cell, cell, 300 + cell, 400 + cell, bounds});
    }
    streaming.AddPortal(5, 9);
    streaming.SetOccupied(5, true);
    terrain.Build(3, 4, 3);
    vegetation.SetSpecies(1, trees);
#endif
#if NEXORA_SHIPPING_ENABLED
    for (const auto profile :
         {shipping::Profile::Minimal, shipping::Profile::Full, shipping::Profile::Dedicated}) {
      shipping::PackageRequest request{
          profile,
          {{"cube", "Content/cube", "abc", shipping::ArtifactKind::Asset, 64, false},
           {"room", "Content/room", "def", shipping::ArtifactKind::Presentation, 128, false}},
          {},
          {},
          false};
      profiles.push_back(shipping::Packager{}.Build(request));
    }
    updateRolledBack =
        updater.Stage({2, "showcase-v2"}) && updater.Activate() && updater.Rollback();
#endif
  }
  void SelectRoom(std::string_view room) {
    if (room == "math")
      room = "rendering";
    if (room == "world")
      room = "streaming";
    if (room == "platform")
      room = "shipping";
    if (room == "input-ui-localization")
      room = "input";
    if (room == "tour")
      room = "hub";
    if (std::find(rooms.begin(), rooms.end(), room) == rooms.end())
      throw std::invalid_argument("Unknown showcase room");
    selected = room;
    visited.insert(selected);
    lastAction = "Entered " + selected;
  }
  void Probe(std::size_t m, ErrorInjection error = ErrorInjection::None) {
    if (m >= probes.size())
      return;
    auto &probe = probes[m];
    bool available = true, success = true;
    probe.issues.clear();
    probe.metrics = {{"scope", "runtime_integration"},
                     {"contract_gate", "NOT_RUN"},
                     {"sample_tick", std::to_string(ticks)},
                     {"input.milestone", std::to_string(m)},
                     {"input.error_case", std::to_string(static_cast<unsigned>(error))}};
    // Integration probes compose public APIs in the running app. CTest remains the contract
    // authority.
    switch (m) {
    case 0:
      success = foundation::GetBuildInfo().build_configuration[0] != 0;
      break;
    case 1:
      success = ticks > 0;
      probe.metrics.push_back({"fixed_ticks", std::to_string(ticks)});
      break;
    case 2:
      success = renderer::ValidateSceneFrame(renderer::MakeProceduralRenderingRoom());
      break;
    case 3:
      available = false;
      probe.summary = "Native counters are reported separately in windowed_evidence";
      break;
    case 4: {
      const auto snapshot = editorWorld.SaveScene(editorScene);
      World restored;
      success = snapshot && restored.LoadSceneSnapshot(*snapshot).has_value();
      break;
    }
    case 5:
#if NEXORA_ASSET_PIPELINE_ENABLED
      available = !minimal;
      success = assets.ActiveGeneration() == 1 && assetRejected && cycleRejected && rolledBack;
      probe.metrics.push_back({"hash", assetHash});
      probe.metrics.push_back({"resident_bytes", std::to_string(assets.ResidentBytes())});
      probe.metrics.push_back({"generation", std::to_string(assets.ActiveGeneration())});
      break;
#else
      available = false;
      break;
#endif
    case 6:
#if NEXORA_EDITOR_SDK_ENABLED
      available = !minimal;
      success = reflection.Size() == 1 && prefab->Resolve("", "color") == "cyan";
      probe.metrics.push_back({"editor_sdk", "available"});
      probe.metrics.push_back({"graphical_editor", "separate application"});
      break;
#else
      available = false;
      break;
#endif
    case 7:
      success = localization.Resolve("fallback") == "Missing translation uses English fallback";
      break;
    case 8:
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
      available = !minimal;
      success = path.has_value() && trace.succeeded && hit.has_value();
      probe.metrics.push_back({"adapter", "portable CPU / no third-party physics SDK"});
      probe.metrics.push_back({"character_x", Number(character.position.x)});
      break;
#else
      available = false;
      break;
#endif
    case 9:
#if NEXORA_PRESENTATION_ENABLED
      available = !minimal;
      success = pose.translations.size() == 3 && skin.MatrixCount() == 3 &&
                audio.ActiveVoices() == 1 && backPressure;
      probe.metrics.push_back({"native_audio_video", "adapter unavailable / contract only"});
      probe.metrics.push_back({"particles", std::to_string(particles.Count())});
      break;
#else
      available = false;
      break;
#endif
    case 10:
#if NEXORA_LARGE_WORLD_ENABLED
      available = !minimal;
      success = streaming.Metrics().pinned_cells == 1 && terrain.PatchCount() == 9 &&
                hlod.Clusters().size() == 9;
      probe.metrics.push_back({"full_cells", std::to_string(streaming.Metrics().full_cells)});
      break;
#else
      available = false;
      break;
#endif
    case 11:
#if NEXORA_PLATFORM_ENABLED
      available = !minimal;
      success = platform.SetSafeArea({4, 12, 4, 4});
      probe.metrics.push_back({"webview", "adapter unavailable"});
      break;
#else
      available = false;
      break;
#endif
    case 12:
#if NEXORA_SHIPPING_ENABLED
      available = !minimal;
      success = updateRolledBack && std::all_of(profiles.begin(), profiles.end(),
                                                [](const auto &p) { return static_cast<bool>(p); });
      probe.metrics.push_back(
          {"rollback_generation", std::to_string(updater.Current().generation)});
      break;
#else
      available = false;
      break;
#endif
    }
    if (error != ErrorInjection::None) {
      // Each run uses fresh, Showcase-owned state; failure injection never corrupts the room.
      available = !minimal;
      success = false;
      std::string observed;
      if (m == 5) {
#if NEXORA_ASSET_PIPELINE_ENABLED
        if (error == ErrorInjection::InvalidAsset) {
          ImporterRegistry importer;
          importer.Register(".showcase",
                            [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
                              if (source.bytes.empty())
                                return {};
                              return CanonicalAsset{source.id, source.type, {}, source.bytes};
                            });
          success = !importer.Import({{1, 1}, "mesh", "empty.showcase", {}});
          observed = "Empty source rejected by ImporterRegistry";
        } else if (error == ErrorInjection::DependencyCycle) {
          success =
              !BundleBuilder::ValidateDependencyDag({{"a", 1, {"b"}, {}}, {"b", 1, {"a"}, {}}});
          observed = "Cyclic bundle dependencies rejected";
        } else if (error == ErrorInjection::Rollback) {
          AssetGenerationStore store;
          DerivedDataCache cache;
          const auto blob =
              AssetCooker{}.Cook({{1, 1}, "mesh", {}, {std::byte{1}}}, "portable", "lab", cache);
          const auto first = blob ? BundleBuilder::Build("lab", 1, {}, {*blob}) : std::nullopt;
          const auto second = blob ? BundleBuilder::Build("lab", 2, {}, {*blob}) : std::nullopt;
          success = first && second && store.Stage({*first}) && store.ActivateStaged() &&
                    store.Stage({*second}) && store.ActivateStaged() && store.Rollback() &&
                    store.ActiveGeneration() == 1;
          observed = "Asset generation 2 rolled back to 1";
        } else
          available = false;
#else
        available = false;
#endif
      } else if (m == 6 && error == ErrorInjection::PluginAbiMismatch) {
#if NEXORA_EDITOR_SDK_ENABLED
        if (pluginLibrary.empty())
          available = false;
        else {
          ServiceRegistry services;
          PluginHost host(foundation::kEngineAbiVersion + 1);
          const auto result = host.Load(pluginLibrary, &services);
          success = !result.loaded && result.error == PluginLoadError::AbiMismatch &&
                    result.reported_abi == foundation::kEngineAbiVersion && !result.registered &&
                    host.LoadedCount() == 0 && services.Size() == 0;
          probe.metrics.push_back(
              {"input.host_abi", std::to_string(foundation::kEngineAbiVersion + 1)});
          probe.metrics.push_back({"input.plugin_library", pluginLibrary});
          probe.metrics.push_back({"output.reported_abi", std::to_string(result.reported_abi)});
          probe.metrics.push_back({"output.loaded", result.loaded ? "true" : "false"});
          probe.metrics.push_back({"output.registered", result.registered ? "true" : "false"});
          observed = result.error == PluginLoadError::OpenFailed
                         ? "Plugin file could not be opened"
                         : "Plugin ABI mismatch rejected before registration";
        }
#else
        available = false;
#endif
      } else if (m == 12 && error == ErrorInjection::Rollback) {
#if NEXORA_SHIPPING_ENABLED
        shipping::BundleUpdater manager{{1, "lab-v1"}};
        success = manager.Stage({2, "lab-v2"}) && manager.Activate() && manager.Rollback() &&
                  manager.Current().generation == 1;
        observed = "Shipping update rolled back to generation 1";
#else
        available = false;
#endif
      } else
        available = false;
      probe.metrics.push_back(
          {"output.error_observation",
           observed.empty() ? "No matching error case / dependency unavailable" : observed});
      if (available && !success)
        probe.issues.push_back({"INJECTION_FAILED", observed});
    }
    probe.metrics.push_back({"output.accepted", available && success ? "true" : "false"});
    probe.status =
        available ? (success ? ProbeStatus::Pass : ProbeStatus::Fail) : ProbeStatus::Unsupported;
    if (m != 3)
      probe.summary =
          available
              ? "Live public Runtime integration; contract/visual acceptance tracked separately"
              : "Feature disabled or native adapter unavailable";
    const auto registry = ProbeRegistry::CreateV1Registry();
    probe.metrics.push_back({"contract_test", registry.Find(probe.id)->contract_test});
    ++probeRuns;
  }
  void Cube(float x, float y, float z, float sx, float sy, float sz) {
    constexpr std::array<std::array<float, 3>, 8> points{{{-1, -1, 1},
                                                          {1, -1, 1},
                                                          {1, 1, 1},
                                                          {-1, 1, 1},
                                                          {-1, -1, -1},
                                                          {1, -1, -1},
                                                          {1, 1, -1},
                                                          {-1, 1, -1}}};
    constexpr std::array<std::array<std::uint16_t, 4>, 6> faces{
        {{0, 1, 2, 3}, {1, 5, 6, 2}, {5, 4, 7, 6}, {4, 0, 3, 7}, {3, 2, 6, 7}, {4, 5, 1, 0}}};
    constexpr std::array<std::array<float, 3>, 6> normals{
        {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
    constexpr std::array<std::array<float, 2>, 4> uv{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    for (std::size_t face = 0; face < faces.size(); ++face) {
      const auto offset = static_cast<std::uint16_t>(vertices.size());
      for (std::size_t corner = 0; corner < 4; ++corner) {
        const auto &point = points[faces[face][corner]];
        const auto &normal = normals[face];
        vertices.push_back({{x + point[0] * sx, y + point[1] * sy, z + point[2] * sz},
                            {normal[0], normal[1], normal[2]},
                            {uv[corner][0], uv[corner][1]}});
      }
      for (const auto index : {0, 1, 2, 2, 3, 0})
        indices.push_back(static_cast<std::uint16_t>(offset + index));
    }
  }
  void Capsule(math::Vector3 center, float capsuleRadius, float height) {
    constexpr std::size_t sides = 16, hemisphereRings = 5;
    const auto base = static_cast<std::uint16_t>(vertices.size());
    for (std::size_t ring = 0; ring < hemisphereRings * 2; ++ring) {
      const bool upper = ring >= hemisphereRings;
      const float latitude =
          upper ? static_cast<float>(ring - hemisphereRings) / (hemisphereRings - 1) * math::kPi / 2
                : -math::kPi / 2 + static_cast<float>(ring) / (hemisphereRings - 1) * math::kPi / 2;
      const float offset = (upper ? 1 : -1) * std::max(0.0F, height * 0.5F - capsuleRadius);
      for (std::size_t side = 0; side < sides; ++side) {
        const float angle = static_cast<float>(side) / sides * math::kPi * 2;
        const math::Vector3 normal{std::cos(latitude) * std::cos(angle), std::sin(latitude),
                                   std::cos(latitude) * std::sin(angle)};
        vertices.push_back(
            {{center.x + normal.x * capsuleRadius, center.y + offset + normal.y * capsuleRadius,
              center.z + normal.z * capsuleRadius},
             {normal.x, normal.y, normal.z}});
        if (ring + 1 < hemisphereRings * 2) {
          const auto a = static_cast<std::uint16_t>(base + ring * sides + side);
          const auto b = static_cast<std::uint16_t>(base + ring * sides + (side + 1) % sides);
          for (const auto index :
               {a, b, static_cast<std::uint16_t>(a + sides), b,
                static_cast<std::uint16_t>(b + sides), static_cast<std::uint16_t>(a + sides)})
            indices.push_back(index);
        }
      }
    }
  }
  void Segment(math::Vector3 start, math::Vector3 end, float tubeRadius) {
    const auto axis = math::NormalizeSafe(end - start);
    if (math::Length(end - start) < 0.0001F)
      return;
    const auto tangent = math::NormalizeSafe(math::Cross(
        axis, std::abs(axis.y) < 0.9F ? math::Vector3{0, 1, 0} : math::Vector3{1, 0, 0}));
    const auto bitangent = math::Cross(axis, tangent);
    const auto base = static_cast<std::uint16_t>(vertices.size());
    constexpr std::size_t sides = 8;
    for (const auto center : {start, end})
      for (std::size_t side = 0; side < sides; ++side) {
        const float angle = static_cast<float>(side) / sides * math::kPi * 2;
        const auto normal = tangent * std::cos(angle) + bitangent * std::sin(angle);
        const auto point = center + normal * tubeRadius;
        vertices.push_back({{point.x, point.y, point.z}, {normal.x, normal.y, normal.z}});
      }
    for (std::size_t side = 0; side < sides; ++side) {
      const auto a = static_cast<std::uint16_t>(base + side),
                 b = static_cast<std::uint16_t>(base + (side + 1) % sides);
      for (const auto index :
           {a, b, static_cast<std::uint16_t>(a + sides), b, static_cast<std::uint16_t>(b + sides),
            static_cast<std::uint16_t>(a + sides)})
        indices.push_back(index);
    }
  }
#if NEXORA_PRESENTATION_ENABLED
  void SkinnedColumn() {
    const auto palette = skin.Matrices();
    if (palette.size() != 3)
      return;
    const auto base = static_cast<std::uint16_t>(vertices.size());
    constexpr std::size_t rows = 9, sides = 8;
    for (std::size_t row = 0; row < rows; ++row) {
      const float y = static_cast<float>(row) / (rows - 1) * 2;
      const auto joint = std::min<std::size_t>(1, static_cast<std::size_t>(y));
      const float weight = y - static_cast<float>(joint);
      for (std::size_t side = 0; side < sides; ++side) {
        const float angle = static_cast<float>(side) / sides * math::kPi * 2;
        const math::Vector3 normal{std::cos(angle), 0, std::sin(angle)};
        const math::Vector3 rest{normal.x * 0.35F, y, normal.z * 0.35F};
        // Translation-only demo rig: inverse bind subtracts each joint's rest height.
        auto deform = [&](std::size_t id) {
          const auto &m = palette[id];
          return math::Vector3{m[0] * rest.x + m[1] * (rest.y - id) + m[2] * rest.z + m[3],
                               m[4] * rest.x + m[5] * (rest.y - id) + m[6] * rest.z + m[7],
                               m[8] * rest.x + m[9] * (rest.y - id) + m[10] * rest.z + m[11]};
        };
        const auto point = deform(joint) * (1 - weight) + deform(joint + 1) * weight;
        vertices.push_back({{point.x - 2, point.y, point.z}, {normal.x, normal.y, normal.z}});
        if (row + 1 < rows) {
          const auto a = static_cast<std::uint16_t>(base + row * sides + side),
                     b = static_cast<std::uint16_t>(base + row * sides + (side + 1) % sides);
          for (const auto index :
               {a, b, static_cast<std::uint16_t>(a + sides), b,
                static_cast<std::uint16_t>(b + sides), static_cast<std::uint16_t>(a + sides)})
            indices.push_back(index);
        }
      }
    }
  }
#endif
  void Terrain(float x, float z, float size, std::size_t subdivisions) {
    const auto base = static_cast<std::uint16_t>(vertices.size());
    for (std::size_t row = 0; row <= subdivisions; ++row)
      for (std::size_t column = 0; column <= subdivisions; ++column) {
        const float px = x + size * static_cast<float>(column) / subdivisions;
        const float pz = z + size * static_cast<float>(row) / subdivisions;
        const float y = 0.2F + 0.15F * std::sin(px) * std::cos(pz);
        const auto normal = math::NormalizeSafe(math::Vector3{
            -0.15F * std::cos(px) * std::cos(pz), 1, 0.15F * std::sin(px) * std::sin(pz)});
        vertices.push_back({{px, y, pz}, {normal.x, normal.y, normal.z}});
        if (row < subdivisions && column < subdivisions) {
          const auto a = static_cast<std::uint16_t>(base + row * (subdivisions + 1) + column);
          const auto b = static_cast<std::uint16_t>(a + 1),
                     c = static_cast<std::uint16_t>(a + subdivisions + 1),
                     d = static_cast<std::uint16_t>(c + 1);
          for (const auto index : {a, c, b, b, c, d})
            indices.push_back(index);
        }
      }
  }
  void Quad(float x, float y, float w, float h, std::uint32_t color, float u0, float v0, float u1,
            float v1) {
    const auto base = static_cast<std::uint32_t>(uiVertices.size());
    uiVertices.push_back({{x, y}, {u0, v0}, color});
    uiVertices.push_back({{x + w, y}, {u1, v0}, color});
    uiVertices.push_back({{x + w, y + h}, {u1, v1}, color});
    uiVertices.push_back({{x, y + h}, {u0, v1}, color});
    for (auto i : {0U, 1U, 2U, 2U, 3U, 0U})
      uiIndices.push_back(base + i);
  }
  void Rect(float x, float y, float w, float h, std::uint32_t color) {
    Quad(x, y, w, h, color, 127.5F / 128, 63.5F / 64, 127.5F / 128, 63.5F / 64);
  }
  void Text(float x, float y, std::string_view text, std::uint32_t color = 0xffe9ded4,
            float scale = 2) {
    for (auto c : text) {
      if (c >= 'a' && c <= 'z')
        c = static_cast<char>(c - 'a' + 'A');
      const auto glyph = characters.find(c);
      if (glyph != std::string_view::npos)
        Quad(x, y, 5 * scale, 7 * scale, color, static_cast<float>((glyph % 16) * 8) / 128,
             static_cast<float>((glyph / 16) * 8) / 64,
             static_cast<float>((glyph % 16) * 8 + 5) / 128,
             static_cast<float>((glyph / 16) * 8 + 7) / 64);
      x += 6 * scale;
    }
  }
  std::vector<std::string> Lines() const {
    std::vector<std::string> lines{"Public Runtime state sampled at tick " + std::to_string(ticks)};
    if (selected == "hub")
      lines.insert(lines.end(),
                   {"Eight rooms / central 3D display stand", "Choose a portal above or press 1-8",
                    "Green probe = integration only. F3 shows contract mapping",
                    "T starts a 3.5-minute tour / Space pauses / R replays"});
    if (selected == "rendering")
      lines.insert(lines.end(),
                   {"Camera / indexed cube / material / directional light / depth",
                    "Native RenderGraph: Offscreen / Main / UI / Present",
                    "Offscreen depth scene / GPU copy to acquired image / UI",
                    "Original checker texture / hardware instances / shadow placeholder",
                    "P cycles instanced cubes / quad / triangle",
                    "Drag mouse to orbit / wheel zoom / WASD orbit"});
    if (selected == "scene") {
      lines.push_back("Editor World / Play World / snapshots / deferred unload");
#if NEXORA_EDITOR_SDK_ENABLED
      const auto *entity = editorWorld.FindEntity(editorEntity);
      lines.push_back("Inspector Transform.x = " + Number(entity ? entity->transform.x : 0) +
                      " / Undo depth " + std::to_string(editor.UndoDepth()));
      lines.push_back("E modify / U undo / P toggle isolated Play World");
      lines.push_back("Prefab override = " + prefab->Resolve("", "color").value_or("missing") +
                      " after rebase");
      lines.push_back("Editor SDK available / graphical Editor is separate");
#else
      lines.push_back("Editor SDK unavailable");
#endif
#if NEXORA_ASSET_PIPELINE_ENABLED
      lines.push_back("Import / cook / bundle / load: generation " +
                      std::to_string(assets.ActiveGeneration()));
      lines.push_back("Resident bytes " + std::to_string(assets.ResidentBytes()) +
                      " / rollback verified");
      lines.push_back("Hash " + assetHash.substr(0, 32));
      lines.push_back("Invalid asset and dependency cycle rejected");
#endif
    }
    if (selected == "input")
      lines.insert(lines.end(), {"Normalized keyboard / mouse / extensible gamepad route",
                                 "L switches EN/FR / absent key uses EN fallback",
                                 localization.Resolve("fallback"),
                                 "UI buttons use Runtime UIDocument hit testing"});
    if (selected == "gameplay") {
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
      lines.push_back("WASD character / C crouch / Space jump / G teleport");
      lines.push_back("Character " + Number(character.position.x) + " / " +
                      Number(character.position.y) + " / " + Number(character.position.z));
      lines.push_back("Ground state " + std::to_string(static_cast<int>(character.ground)) +
                      " / crouch " + std::to_string(character.crouched));
      lines.push_back("Collision query hit " + std::to_string(hit ? hit->body : 0) +
                      " / desired velocity " + Number(motion.desired_velocity.x));
      lines.push_back("Nav tile generation " + std::to_string(navigation.Generation()) +
                      " / path points " + std::to_string(path ? path->points.size() : 0));
      lines.push_back("AI trace " + std::to_string(trace.visited.size()) +
                      " nodes / perception budget 4 / observed " + std::to_string(stimuli.size()));
      lines.push_back("Capsule / AABB step ramp / collision ray / nav path");
#else
      lines.push_back("Gameplay adapter unavailable");
#endif
    }
    if (selected == "presentation") {
#if NEXORA_PRESENTATION_ENABLED
      lines.push_back("Animation joints " + std::to_string(pose.translations.size()) +
                      " / skin matrices " + std::to_string(skin.MatrixCount()));
      lines.push_back("J clip blend / CPU skin deformation / live particles");
      lines.push_back("Root motion " + Number(pose.root_motion.x) + " / particle capacity 32");
      lines.push_back("Live particles " + std::to_string(particles.Count()) + " / dropped " +
                      std::to_string(droppedParticles));
      lines.push_back("Audio Master gain 0.60 / voices " + std::to_string(audio.ActiveVoices()) +
                      " / limit 2");
      lines.push_back("Video queue " + std::to_string(video.QueuedFrames()) + " / texture " +
                      std::to_string(video.VideoTexture()));
      lines.push_back("V seek / queue back-pressure verified");
#endif
      lines.push_back("Audio/video CONTRACT ONLY / native decoder and output unavailable");
    }
    if (selected == "streaming") {
#if NEXORA_LARGE_WORLD_ENABLED
      const auto m = streaming.Metrics();
      lines.push_back("9 cells / occupied center pin / portal 5 to 9");
      lines.push_back("Full " + std::to_string(m.full_cells) + " / HLOD " +
                      std::to_string(m.hlod_cells) + " / pinned " + std::to_string(m.pinned_cells));
      lines.push_back("RAM " + std::to_string(m.usage.ram) + "/4096 / VRAM " +
                      std::to_string(m.usage.vram) + "/2048");
      lines.push_back("Procedural terrain mesh / 9 authored HLOD proxies");
      lines.push_back("Terrain patches " + std::to_string(terrain.PatchCount()) + " / vegetation " +
                      std::to_string(vegetation.InstanceCount()));
      for (std::uint64_t id = 1; id <= 9; ++id) {
        const auto cell = streaming.Status(id);
        lines.push_back("Cell " + std::to_string(id) + " bundle " + std::to_string(100 + id) +
                        " residency " + std::to_string(static_cast<int>(cell->residency)) +
                        (cell->occupied     ? " occupied pin"
                         : cell->prefetched ? " portal prefetch"
                                            : " distance / budget"));
      }
#else
      lines.push_back("Large world feature unavailable");
#endif
    }
    if (selected == "shipping") {
#if NEXORA_PLATFORM_ENABLED
      const auto p = platform.CurrentPolicy();
      lines.push_back("B lifecycle / H thermal-memory pressure / safe area 4-12-4-4");
      lines.push_back("App state " + std::to_string(static_cast<int>(platform.State())) +
                      " / reduce quality " + std::to_string(p.reduce_quality) +
                      " / release caches " + std::to_string(p.release_caches));
#endif
      lines.push_back("WebView adapter unavailable / host owns native overlays");
#if NEXORA_SHIPPING_ENABLED
      for (std::size_t i = 0; i < profiles.size(); ++i)
        lines.push_back(std::string(i == 0   ? "Minimal"
                                    : i == 1 ? "Full"
                                             : "Dedicated") +
                        " manifest artifacts " +
                        std::to_string(profiles[i].manifest.artifacts.size()) + " / bytes " +
                        std::to_string(profiles[i].manifest.total_bytes));
      lines.push_back("Update staged / activated / rollback generation " +
                      std::to_string(updater.Current().generation));
      const auto report = crash.Capture({std::string(foundation::GetBuildId()),
                                         "showcase",
                                         "demonstration capture",
                                         {"startup", "room-switch", lastAction}});
      lines.push_back("Crash breadcrumb snapshot " +
                      std::to_string(report ? report->breadcrumbs.size() : 0));
#endif
      lines.push_back("V1 final acceptance pending / Vulkan and Metal parity open");
    }
    if (minimal)
      lines.push_back("Minimal capability override / optional rooms unavailable");
    lines.push_back("Last action: " + lastAction);
    return lines;
  }
};

RoomSession::RoomSession(std::string scene, bool tour, bool minimal, std::string pluginLibrary)
    : state_(std::make_unique<State>()) {
  state_->pluginLibrary = std::move(pluginLibrary);
  state_->minimal = minimal;
  state_->tour = tour;
  state_->SelectRoom(scene);
}
RoomSession::~RoomSession() = default;
void RoomSession::Select(std::string_view room) { state_->SelectRoom(room); }
std::string_view RoomSession::Selected() const { return state_->selected; }
void RoomSession::ReplayTour() {
  state_->tour = true;
  state_->paused = false;
  state_->tourSeconds = 0;
  state_->tourStep = 0;
  Select("hub");
}
void RoomSession::RerunProbe(std::size_t milestone, ErrorInjection injection) {
  state_->Probe(milestone, injection);
}
std::vector<ProbeResult> RoomSession::Probes() const { return state_->probes; }
bool RoomSession::Healthy() const {
  return std::none_of(state_->probes.begin(), state_->probes.end(),
                      [](const auto &p) { return p.status == ProbeStatus::Fail; });
}

void RoomSession::Event(const Nexora::Window::WindowEvent &event, std::uint32_t width,
                        std::uint32_t height) {
  auto &s = *state_;
  using Type = Nexora::Window::WindowEventType;
  if (event.type == Type::FocusChanged && !event.value0) {
    s.held.clear();
    s.dragging = false;
    return;
  }
  if (event.type == Type::Wheel) {
    s.radius = std::clamp(s.radius - static_cast<float>(event.value1) / 120, 5.0F, 30.0F);
    return;
  }
  if (event.type == Type::Pointer) {
    if (s.dragging) {
      s.yaw += (event.value0 - s.pointerX) * 0.005F;
      s.pitch = std::clamp(s.pitch + (event.value1 - s.pointerY) * 0.005F, 0.1F, 1.2F);
      ++s.cameraMoves;
    }
    s.pointerX = event.value0;
    s.pointerY = event.value1;
    return;
  }
  if (event.type == Type::PointerButton) {
    if (event.value0 == 0) {
      s.dragging = event.value1 != 0;
      if (event.value1 && width && height)
        static_cast<void>(s.document.Dispatch(
            {0, s.pointerX * 1280.0F / width, s.pointerY * 720.0F / height, true}));
    }
    return;
  }
  if (event.type != Type::Key)
    return;
  const auto key = static_cast<Key>(event.value0);
  if (!event.value1) {
    s.held.erase(key);
    return;
  }
  if (!s.held.insert(key).second)
    return;
  if (key >= Key::Digit1 && key <= Key::Digit8) {
    s.tour = false;
    Select(rooms[static_cast<std::size_t>(key) - static_cast<std::size_t>(Key::Digit1)]);
  }
  if (key == Key::F1)
    s.overview = !s.overview;
  if (key == Key::F2)
    s.profiler = !s.profiler;
  if (key == Key::F3)
    s.matrix = !s.matrix;
  if (key == Key::F5) {
    ++s.reloads;
#if NEXORA_EDITOR_SDK_ENABLED
    if (s.play.State() != PlayState::Stopped)
      s.play.Stop();
    s.editor.ClearUndo();
#endif
    s.lastAction = "Scene snapshot reload";
    const auto snapshot = s.editorWorld.SaveScene(s.editorScene);
    World restored;
    if (!snapshot || !restored.LoadSceneSnapshot(*snapshot))
      s.lastAction = "Scene snapshot reload failed";
    else {
      s.editorWorld = std::move(restored);
      s.editorScene = s.editorWorld.FindEntity(s.editorEntity) ? s.editorScene : 0;
    }
  }
  if (key == Key::L) {
    s.french = !s.french;
    s.localization.SetLocale(s.french ? "fr" : "en");
  }
  if (key == Key::T)
    ReplayTour();
  if (key == Key::R) {
    if (s.tour)
      ReplayTour();
    else
      RerunProbe(s.selectedProbe, s.injection);
  }
  if (key == Key::Space && s.tour)
    s.paused = !s.paused;
  if (key == Key::Tab) {
    s.selectedProbe = (s.selectedProbe + 1) % 13;
    s.metricOffset = 0;
  }
  if (s.matrix && key == Key::I) {
    s.injection = static_cast<ErrorInjection>((static_cast<unsigned>(s.injection) + 1) % 5);
    s.metricOffset = 0;
  }
  if (s.matrix && key == Key::PageDown)
    ++s.metricOffset;
  if (s.matrix && key == Key::PageUp && s.metricOffset > 0)
    --s.metricOffset;
  if (s.matrix && key == Key::X) {
    try {
      std::ofstream json("showcase-lab.json"), markdown("showcase-lab.md");
      json << Report();
      markdown << Markdown();
      json.flush();
      markdown.flush();
      s.lastAction =
          json && markdown ? "Exported showcase-lab.json / showcase-lab.md" : "Lab export failed";
    } catch (const std::exception &) {
      s.lastAction = "Lab export failed";
    }
  }
  if (s.selected == "rendering" && key == Key::P) {
    s.primitive = (s.primitive + 1) % 3;
    s.lastAction = "Changed native primitive";
  }
#if NEXORA_EDITOR_SDK_ENABLED
  if (s.selected == "scene" && key == Key::E) {
    auto transform = s.editorWorld.FindEntity(s.editorEntity)->transform;
    transform.x += 0.5F;
    s.editor.SetTransform(s.editorEntity, transform);
    s.lastAction = "Modified Transform.x";
  }
  if (s.selected == "scene" && key == Key::U) {
    s.editor.Undo();
    s.lastAction = "Undo";
  }
  if (s.selected == "scene" && key == Key::P) {
    if (s.play.State() != PlayState::Stopped)
      s.play.Stop();
    else
      s.play.Start(1.0 / 60, [id = s.editorEntity](World &world, double dt) {
        WorldCommandBuffer commands;
        auto transform = world.FindEntity(id)->transform;
        transform.x += static_cast<float>(dt);
        commands.SetTransform(id, transform);
        return commands.Apply(world);
      });
    s.lastAction = "Toggled isolated Play World";
  }
#endif
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  if (s.selected == "gameplay" && key == Key::C)
    s.controller.SetCrouched(s.character, !s.character.crouched, s.physics);
  if (s.selected == "gameplay" && key == Key::G)
    s.controller.Teleport(s.character, {-2, 2, 0});
#endif
#if NEXORA_PRESENTATION_ENABLED
  if (s.selected == "presentation" && key == Key::J) {
    s.animation.Play(s.animation.ActiveClip() == 1 ? 2 : 1, 0.5F);
    s.lastAction = "Animation clip blend / 0.5 s";
  }
  if (key == Key::V) {
    s.video.Seek(s.clock);
    s.lastAction = "Video seek invalidated queue";
  }
#endif
#if NEXORA_PLATFORM_ENABLED
  if (key == Key::B) {
    s.platform.Transition(s.platform.State() == platform::AppState::Foreground
                              ? platform::AppState::Background
                              : platform::AppState::Foreground);
    s.lastAction = "Lifecycle transition";
  }
  if (key == Key::H) {
    const auto pressure = s.platform.CurrentPolicy().reduce_quality;
    s.platform.SetPressure(
        pressure ? platform::ThermalState::Nominal : platform::ThermalState::Serious,
        pressure ? platform::MemoryPressure::Normal : platform::MemoryPressure::Critical);
    s.lastAction = "Memory and thermal pressure simulation";
  }
#endif
}

void RoomSession::Tick(double seconds) {
  auto &s = *state_;
  if (!std::isfinite(seconds) || seconds < 0 || seconds > 1)
    throw std::invalid_argument("Invalid showcase delta");
  ++s.ticks;
  s.clock += seconds;
  if (s.tour && !s.paused) {
    s.tourSeconds += seconds;
    s.tourStep = std::min<std::size_t>(6, static_cast<std::size_t>(s.tourSeconds / 30));
    s.SelectRoom(tourRooms[s.tourStep]);
    if (s.tourSeconds >= 210)
      s.paused = true;
  }
  for (const auto key : {Key::W, Key::A, Key::S, Key::D})
    if (s.held.contains(key)) {
      const char c = key == Key::W ? 'W' : key == Key::A ? 'A' : key == Key::S ? 'S' : 'D';
      s.input.Push({++s.inputSequence,
                    InputDeviceKind::Keyboard,
                    0,
                    InputEventKind::Button,
                    std::string(1, c),
                    1,
                    0,
                    0,
                    0,
                    {}});
    }
  const auto controls = s.actions.Evaluate(s.input.Consume(0));
  s.input.EndFrame();
  const auto axis = [&](const char *name) {
    const auto found = controls.find(name);
    return found == controls.end() ? 0.0F : found->second;
  };
  if (s.selected != "gameplay") {
    s.yaw += axis("move-x") * static_cast<float>(seconds);
    s.radius = std::clamp(s.radius + axis("move-z") * static_cast<float>(seconds) * 4, 5.0F, 30.0F);
  }
#if NEXORA_EDITOR_SDK_ENABLED
  if (s.play.State() == PlayState::Playing)
    s.play.Tick();
#endif
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  CharacterInput intent{};
  if (s.selected == "gameplay") {
    intent.move_x = axis("move-x");
    intent.move_z = axis("move-z");
    if (s.held.contains(Key::Space))
      intent.flags = CharacterJump;
  }
  s.motion = s.motor.Tick(s.character, intent, seconds, s.physics, s.controller);
  s.hit = s.physics.Raycast({{s.character.position.x, 5, s.character.position.z}, {0, -1, 0}, 10});
  s.trace = s.behavior.Tick(s.blackboard);
  s.stimuli = s.perception.Query(s.character.position, 8, 4);
#endif
#if NEXORA_PRESENTATION_ENABLED
  s.pose = s.animation.Update(static_cast<float>(seconds));
  std::vector<std::array<float, 16>> palette;
  for (const auto &translation : s.pose.translations) {
    math::Matrix4 matrix{};
    matrix(0, 3) = translation.x;
    matrix(1, 3) = translation.y;
    matrix(2, 3) = translation.z;
    palette.push_back(matrix.values);
  }
  s.skin.Upload(palette);
  if (!s.particles.Spawn({{2, 0.1F, 0},
                          {static_cast<float>(std::sin(s.clock * 3)), 1,
                           static_cast<float>(std::cos(s.clock * 3))},
                          2}))
    ++s.droppedParticles;
  s.particles.Update(static_cast<float>(seconds));
  s.video.SubmitDecoded({++s.videoSequence, s.clock, 100 + s.videoSequence});
  static_cast<void>(s.video.Tick(s.clock));
#endif
#if NEXORA_LARGE_WORLD_ENABLED
  s.streaming.AddOrUpdateSource({1, {std::sin(s.clock) * 6, 0, std::cos(s.clock) * 6}, 2, 6, 1});
  s.streaming.Update();
#endif
  if (s.ticks == 1)
    for (std::size_t m = 0; m < 13; ++m)
      s.Probe(m);
}

Nexora::Presentation::SceneDrawData RoomSession::Scene(std::uint32_t width, std::uint32_t height) {
  auto &s = *state_;
  s.visualized.insert(s.selected);
  s.vertices.clear();
  s.indices.clear();
  s.instances.clear();
  s.sceneUploads.clear();
  s.Cube(0, -0.3F, 0, 6, 0.3F, 6);
  if (s.selected == "hub") {
    s.Cube(0, 0.5F, 0, 1.5F, 0.5F, 1.5F);
    s.Cube(0, 2, 0, 0.7F, 1, 0.7F);
    for (std::size_t i = 0; i < 8; ++i) {
      float angle = static_cast<float>(i) * 0.7854F;
      s.Cube(std::sin(angle) * 4, 1, std::cos(angle) * 4, 0.4F, 1, 0.4F);
    }
  } else if (s.selected == "rendering") {
    if (s.primitive == 0) {
      s.vertices.clear();
      s.indices.clear();
      s.Cube(0, 0, 0, 1, 1, 1);
      s.instances = {{{0, -0.3F, 0}, {6, 0.3F, 6}, {0.5F, 0.5F, 0.5F, 1}},
                     {{0, 1.5F, 0}, {1, 1, 1}, {1, 1, 1, 1}},
                     {{-3, 0.5F, 0}, {0.5F, 0.5F, 0.5F}, {1, 0.4F, 0.4F, 1}},
                     {{3, 0.75F, 0}, {0.75F, 0.75F, 0.75F}, {0.4F, 1, 0.4F, 1}}};
    } else {
      const auto base = static_cast<std::uint16_t>(s.vertices.size());
      s.vertices.push_back({{-2, 0.25F, 0}, {0, 0, 1}, {0, 0}});
      s.vertices.push_back({{2, 0.25F, 0}, {0, 0, 1}, {1, 0}});
      s.vertices.push_back({{s.primitive == 1 ? 2.0F : 0.0F, 3.75F, 0},
                            {0, 0, 1},
                            {s.primitive == 1 ? 1.0F : 0.5F, 1}});
      for (const auto index : {0, 1, 2})
        s.indices.push_back(static_cast<std::uint16_t>(base + index));
      if (s.primitive == 1) {
        s.vertices.push_back({{-2, 3.75F, 0}, {0, 0, 1}, {0, 1}});
        for (const auto index : {2, 3, 0})
          s.indices.push_back(static_cast<std::uint16_t>(base + index));
      }
    }
  } else if (s.selected == "scene") {
    const auto *entity = s.editorWorld.FindEntity(s.editorEntity);
    s.Cube(entity ? static_cast<float>(entity->transform.x) : 0.0F, 1, 0, 0.6F, 1, 0.6F);
#if NEXORA_EDITOR_SDK_ENABLED
    const auto inspection = s.play.Inspect();
    for (const auto &e : inspection.entities)
      s.Cube(static_cast<float>(e.transform.x), 1, 3, 0.5F, 0.8F, 0.5F);
#endif
#if NEXORA_ASSET_PIPELINE_ENABLED
    const auto offset = static_cast<std::uint16_t>(s.vertices.size());
    for (const auto &vertex : s.assetMesh.vertices)
      s.vertices.push_back({{vertex.position[0] * 0.5F - 3, vertex.position[1] * 0.5F + 0.5F,
                             vertex.position[2] * 0.5F},
                            {vertex.normal[0], vertex.normal[1], vertex.normal[2]}});
    for (const auto index : s.assetMesh.indices)
      s.indices.push_back(static_cast<std::uint16_t>(offset + index));
#endif
  } else if (s.selected == "gameplay") {
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
    s.Capsule({static_cast<float>(s.character.position.x),
               static_cast<float>(s.character.position.y) + (s.character.crouched ? 0.5F : 0.9F),
               static_cast<float>(s.character.position.z)},
              0.4F, s.character.crouched ? 1.0F : 1.8F);
    for (std::size_t step = 0; step < 5; ++step) {
      const float h = 0.2F + static_cast<float>(step) * 0.2F;
      s.Cube(-3.8F + static_cast<float>(step) * 0.4F, h * 0.5F, 1.5F, 0.2F, h * 0.5F, 0.5F);
    }
    s.Cube(1.5F, 0.75F, 1.5F, 0.5F, 0.75F, 0.5F);
    s.Cube(-1.5F, 0.125F, 1.5F, 0.5F, 0.125F, 0.5F);
    if (s.path) {
      for (const auto &p : s.path->points)
        s.Cube(static_cast<float>(p.x), 0.1F, static_cast<float>(p.z), 0.15F, 0.1F, 0.15F);
      for (std::size_t i = 1; i < s.path->points.size(); ++i) {
        const auto &a = s.path->points[i - 1], &b = s.path->points[i];
        s.Segment({static_cast<float>(a.x), 0.1F, static_cast<float>(a.z)},
                  {static_cast<float>(b.x), 0.1F, static_cast<float>(b.z)}, 0.04F);
      }
    }
    if (s.hit) {
      const math::Vector3 contact{static_cast<float>(s.hit->point.x),
                                  static_cast<float>(s.hit->point.y),
                                  static_cast<float>(s.hit->point.z)};
      s.Cube(contact.x, contact.y + 0.05F, contact.z, 0.15F, 0.05F, 0.15F);
      s.Segment({static_cast<float>(s.character.position.x), 5,
                 static_cast<float>(s.character.position.z)},
                contact, 0.025F);
    }
#endif
  } else if (s.selected == "presentation") {
#if NEXORA_PRESENTATION_ENABLED
    for (const auto &p : s.pose.translations)
      s.Cube(p.x, p.y + 1, p.z, 0.2F, 0.3F, 0.2F);
    s.SkinnedColumn();
    for (const auto &p : s.particles.PositionSnapshot())
      s.Cube(p.x, p.y, p.z, 0.06F, 0.06F, 0.06F);
#endif
  } else if (s.selected == "streaming") {
#if NEXORA_LARGE_WORLD_ENABLED
    for (const auto &patch : s.terrain.Cull({{-8, -1, -8}, {8, 1, 8}}, {0, 0, 0})) {
      const auto id =
          static_cast<std::uint64_t>((patch.coordinate.z + 1) * 3 + patch.coordinate.x + 2);
      const auto status = s.streaming.Status(id);
      const auto *cell = s.streaming.FindCell(id);
      if (!status || !cell)
        continue;
      const float x = static_cast<float>(cell->bounds.minimum.x),
                  z = static_cast<float>(cell->bounds.minimum.z);
      if (status->residency == large_world::Residency::Full)
        s.Terrain(x, z, 4, std::max<std::size_t>(1, 4U >> patch.lod));
      else if (status->residency == large_world::Residency::Unloaded)
        s.Cube(x + 2, 0.01F, z + 2, 1.95F, 0.01F, 1.95F); // Explicit unloaded map tile.
    }
    for (const auto &proxy : s.hlod.Clusters()) {
      const auto status = s.streaming.Status(proxy.source_cell);
      if (status && status->residency == large_world::Residency::Hlod)
        s.Terrain(static_cast<float>(proxy.bounds.minimum.x),
                  static_cast<float>(proxy.bounds.minimum.z), 4, 1);
    }
    for (const auto &tree : s.trees) {
      const float x = static_cast<float>(tree.position.x), z = static_cast<float>(tree.position.z);
      const auto id = static_cast<std::uint64_t>(static_cast<int>((z + 6) / 4) * 3 +
                                                 static_cast<int>((x + 6) / 4) + 1);
      const auto status = s.streaming.Status(id);
      if (!status || status->residency == large_world::Residency::Unloaded)
        continue;
      const float ground = 0.2F + 0.15F * std::sin(x) * std::cos(z);
      s.Cube(x, ground + 0.5F, z, 0.1F, 0.5F, 0.1F);
      if (status->residency == large_world::Residency::Full)
        s.Capsule({x, ground + 1.2F, z}, 0.45F, 1.2F);
      else
        s.Cube(x, ground + 1.2F, z, 0.4F, 0.5F, 0.4F);
    }
#endif
  } else {
    s.Cube(-2, 1, 0, 0.5F, 1, 0.5F);
    s.Cube(0, 2, 0, 0.5F, 2, 0.5F);
    s.Cube(2, 0.6F, 0, 0.5F, 0.6F, 0.5F);
  }
  const math::Vector3 eye{s.radius * std::sin(s.yaw) * std::cos(s.pitch),
                          s.radius * std::sin(s.pitch),
                          s.radius * std::cos(s.yaw) * std::cos(s.pitch)};
  const auto mvp =
      math::PerspectiveRadians(0.85F, height ? static_cast<float>(width) / height : 1, 0.1F, 100) *
      math::LookAt(eye, {0, 0.8F, 0});
  Nexora::Presentation::SceneDrawData data{};
  data.vertices = s.vertices;
  data.instances = s.instances;
  if (s.selected == "rendering" || s.selected == "hub") {
    s.sceneUploads.push_back({1, 8, 8, 32, s.checker});
    data.textureId = 1;
    data.textureUploads = s.sceneUploads;
  }
  data.indices = s.indices;
  std::memcpy(data.model_view_projection, mvp.values.data(), sizeof(data.model_view_projection));
  data.base_color[0] = 0.15F;
  data.base_color[1] = 0.65F;
  data.base_color[2] = 0.9F;
  return data;
}

Nexora::Presentation::UiDrawData
RoomSession::Overlay(std::uint32_t width, std::uint32_t height, std::string_view backend,
                     const Nexora::Presentation::SurfaceDiagnostics &d, double frameMs) {
  auto &s = *state_;
  s.uiVertices.clear();
  s.uiIndices.clear();
  s.commands.clear();
  s.uploads.clear();
  s.Rect(0, 0, 1280, 112, 0xf0271a10);
  s.Text(18, 14, s.localization.Resolve("title"), 0xffefdc80, 3);
  s.Text(18, 45, "F1 overview / F2 profiler / F3 V1 matrix / F5 reload / L locale");
  for (std::size_t i = 0; i < rooms.size(); ++i) {
    const float x = 18 + static_cast<float>(i) * 154;
    s.Rect(x, 75, 146, 28, s.selected == rooms[i] ? 0xff996828 : 0xff453123);
    s.Text(x + 8, 79, std::to_string(i + 1) + " " + std::string(rooms[i]), 0xffffffff, 1.2F);
    s.Text(x + 8, 93,
           s.minimal && i >= 4 ? "UNAVAILABLE" : (i == 5 || i == 7 ? "CONTRACT ONLY" : "PARTIAL"),
           0xff8fd4ef, 0.9F);
  }
  if (s.overview) {
    const auto lines = s.Lines();
    s.Rect(18, 126, 740, static_cast<float>(lines.size()) * 22 + 22, 0xde241a10);
    float y = 139;
    for (const auto &line : lines) {
      s.Text(30, y, line, 0xffe9ded4, 1.5F);
      y += 22;
    }
  }
  if (s.profiler) {
    s.Rect(930, 126, 328, 242, 0xef241a10);
    s.Text(945, 140, "Live frame / " + std::string(backend));
    s.Text(945, 165, d.softwareRasterizer ? "Rasterizer software" : "Rasterizer hardware");
    s.Text(945, 190, "Frame ms " + Number(frameMs));
    s.Text(945, 215, "Acquire " + std::to_string(d.acquiredFrames));
    s.Text(945, 240, "Present " + std::to_string(d.presentedFrames));
    s.Text(945, 265, "Scene draws " + std::to_string(d.sceneDrawCalls));
    s.Text(945, 290, "Instances " + std::to_string(d.sceneInstances));
    s.Text(945, 315, "UI draws " + std::to_string(d.nativeUiDrawCalls));
    s.Text(945, 340, "Build " + std::string(foundation::GetBuildId()).substr(0, 12), 0xffa5cedd,
           1.5F);
  }
  if (s.matrix) {
    s.Rect(18, 126, 1240, 475, 0xf8241a10);
    s.Text(30, 139, "Lab / Tab select / R run / I error case / PgUp-PgDn details / X export");
    float y = 166;
    for (std::size_t m = 0; m < s.probes.size(); ++m) {
      const auto &p = s.probes[m];
      const auto color = p.status == ProbeStatus::Pass   ? 0xff92d786
                         : p.status == ProbeStatus::Fail ? 0xff7777ee
                                                         : 0xff8fd4ef;
      s.Text(30, y,
             (m == s.selectedProbe ? "> " : "  ") + p.milestone + " " +
                 std::string(ToString(p.status)) + " / CTest NOT_RUN / PARTIAL",
             color, 1.5F);
      y += 23;
    }
    s.Text(30, y + 12, "Contract authority: CTest / green shows runtime integration only",
           0xff8fd4ef, 1.5F);
    const auto &selected = s.probes[s.selectedProbe];
    s.Text(30, y + 36, selected.summary.substr(0, 85), 0xffe9ded4, 1.5F);
    constexpr std::array<std::string_view, 5> errorNames{"None", "Empty asset (M5)", "Cycle (M5)",
                                                         "Plugin ABI (M6)", "Rollback (M5/M12)"};
    s.Text(620, 169, "Input: " + std::string(errorNames[static_cast<unsigned>(s.injection)]),
           0xffefdc80, 1.5F);
    s.Text(620, 192, "Run uses fresh owned state / output sampled at run", 0xffa5cedd, 1.5F);
    const auto offset = std::min(s.metricOffset, selected.metrics.size());
    float detailY = 221;
    for (std::size_t i = offset; i < selected.metrics.size() && i < offset + 10; ++i) {
      const auto &metric = selected.metrics[i];
      s.Text(620, detailY, (metric.name + " = " + metric.value).substr(0, 65), 0xffe9ded4, 1.5F);
      detailY += 23;
    }
    for (const auto &issue : selected.issues) {
      if (detailY > 519)
        break;
      s.Text(620, detailY, (issue.code + ": " + issue.message).substr(0, 65), 0xff7777ee, 1.5F);
      detailY += 23;
    }
    s.Text(620, 552, s.lastAction.substr(0, 65), 0xffa5cedd, 1.5F);
  }
  if (s.tour) {
    s.Rect(18, 651, 1100, 50, 0xef241a10);
    s.Text(30, 665,
           "Guided tour " + Number(s.tourSeconds) + "/210 s / step " +
               std::to_string(s.tourStep + 1) + "/7 / " + (s.paused ? "paused" : "running") +
               " / Space pause / R replay",
           0xffefdc80, 1.5F);
  }
  for (auto &v : s.uiVertices) {
    v.position[0] *= static_cast<float>(width) / 1280;
    v.position[1] *= static_cast<float>(height) / 720;
  }
  s.commands.push_back(
      {0, 0, width, height, 0x4e580001, static_cast<std::uint32_t>(s.uiIndices.size()), 0, 0});
  if (!s.textureUploaded || s.atlasGeneration != d.resizeGenerations) {
    s.atlasGeneration = d.resizeGenerations;
    s.uploads.push_back({0x4e580001, 128, 64, 128 * 4, s.atlas});
    s.textureUploaded = true;
  }
  return {s.uiVertices, std::as_bytes(std::span{s.uiIndices}), s.commands, s.uploads, true};
}
std::string RoomSession::Markdown() const { return SerializeMarkdown(state_->probes); }
std::string RoomSession::Report() const {
  const auto &s = *state_;
  std::ostringstream out;
  out << std::boolalpha;
  out << "{\"schema\":\"nexora.showcase.rooms.v1\",\"selected\":\"" << s.selected
      << "\",\"ticks\":" << s.ticks << ",\"reloads\":" << s.reloads
      << ",\"probe_runs\":" << s.probeRuns << ",\"healthy\":" << Healthy()
      << ",\"tour\":{\"enabled\":" << s.tour << ",\"paused\":" << s.paused
      << ",\"seconds\":" << s.tourSeconds << ",\"step\":" << s.tourStep << "},\"visited\":[";
  bool first = true;
  for (const auto &room : s.visited) {
    if (!first)
      out << ",";
    first = false;
    out << "\"" << room << "\"";
  }
  out << "],\"visualized\":[";
  first = true;
  for (const auto &room : s.visualized) {
    if (!first)
      out << ",";
    first = false;
    out << "\"" << room << "\"";
  }
  out << "],\"sample_source\":\"public Runtime "
         "APIs\",\"native_media\":\"CONTRACT_ONLY\",\"native_webview\":\"UNAVAILABLE\",\"visual_"
         "acceptance\":\"pending target-host evidence\",\"lines\":[";
  first = true;
  for (const auto &line : s.Lines()) {
    if (!first)
      out << ",";
    first = false;
    out << "\"" << Escape(line) << "\"";
  }
  out << "],\"integration_probes\":" << SerializeJson(s.probes) << "}";
  return out.str();
}
} // namespace nexora::showcase
