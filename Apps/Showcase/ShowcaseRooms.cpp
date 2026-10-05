#include "ShowcaseRooms.h"
#include "CourtyardAssets.h"
#include "CourtyardEnvironment.h"
#include "CourtyardHero.h"
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
#if NEXORA_ASSET_PIPELINE_ENABLED
renderer::Mesh ReadShowcaseMesh(std::span<const std::byte> payload) {
  if (payload.empty() || payload.size() > 4 * 1024 * 1024)
    throw std::runtime_error("Showcase mesh payload exceeds the bounded content contract");
  const std::string text(reinterpret_cast<const char *>(payload.data()), payload.size());
  std::istringstream reader(text);
  std::string schema;
  std::size_t vertexCount{}, indexCount{};
  if (!(reader >> schema >> vertexCount >> indexCount) || schema != "nexora.showcase.mesh.v1" ||
      vertexCount == 0 || indexCount == 0 || vertexCount > 65535 || indexCount > 1048576 ||
      indexCount % 3 != 0)
    throw std::runtime_error("Unsupported Showcase mesh schema");
  renderer::Mesh mesh;
  mesh.vertices.resize(vertexCount);
  mesh.indices.resize(indexCount);
  for (auto &vertex : mesh.vertices) {
    for (auto &value : vertex.position)
      if (!(reader >> value) || !std::isfinite(value))
        throw std::runtime_error("Invalid cooked Showcase position");
    for (auto &value : vertex.normal)
      if (!(reader >> value) || !std::isfinite(value))
        throw std::runtime_error("Invalid cooked Showcase normal");
    for (auto &value : vertex.uv)
      if (!(reader >> value) || !std::isfinite(value))
        throw std::runtime_error("Invalid cooked Showcase UV");
  }
  for (auto &index : mesh.indices)
    if (!(reader >> index) || index >= vertexCount)
      throw std::runtime_error("Invalid cooked Showcase index");
  reader >> std::ws;
  if (!reader.eof())
    throw std::runtime_error("Unexpected cooked Showcase mesh suffix");
  return mesh;
}
#endif
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
  bool dragging{}, screenshotMode{};
  std::size_t courtyardShot{};
  bool courtyardPbr{true}, courtyardIbl{true};
  float courtyardExposure = 1.0F;
  bool courtyardShadows{true}, courtyardStyled{true}, courtyardBloom{true}, courtyardFocus{true};
  bool courtyardReflections{true}, courtyardAtmosphere{true};
  bool courtyardPaused{}, courtyardActive{}, courtyardWind{true}, courtyardTransmission{true};
  bool courtyardTransparency{true}, courtyardRefraction{true}, courtyardCrystalLight{true};
  bool visualTour{}, courtyardFreeCamera{}, courtyardCompare{};
  unsigned courtyardQuality{1}; // Basic / Standard / High, shared by all native adapters.
  math::Vector3 freeEye{};
  double courtyardSeconds{};
  std::size_t courtyardParticleCount{}, courtyardFoliageQuadCount{};
  float courtyardShadowBias = 0.0008F;
  std::uint64_t atlasGeneration{~std::uint64_t{0}};
  std::string lastAction{"Ready"}, pluginLibrary;
  ErrorInjection injection{ErrorInjection::None};
  std::size_t metricOffset{}, primitive{};
  std::vector<ProbeResult> probes{ProbeRegistry::CreateV1Registry().RunAll()};
  std::vector<Nexora::Presentation::SceneVertex> vertices;
  std::vector<Nexora::Presentation::SceneInstance> instances;
  std::vector<Nexora::Presentation::SceneMaterial> materials;
  std::vector<Nexora::Presentation::SceneMeshBatch> batches;
  std::vector<Nexora::Presentation::SceneVertex> courtyardVertices;
  std::vector<Nexora::Presentation::SceneInstance> courtyardInstances;
  std::size_t courtyardCrystalFirst{}, courtyardCrystalEnd{};
  std::size_t courtyardReflectionDeviceIndexEnd{};
  std::vector<std::uint16_t> courtyardIndices;
  std::vector<Nexora::Presentation::SceneMeshBatch> courtyardBatches;
  std::array<std::byte, 8 * 8 * 4> checker{};
  std::vector<Nexora::Presentation::UiTextureUpload> sceneUploads;
  std::vector<Nexora::Presentation::SceneLinearTextureUpload> linearSceneUploads;
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
  AssetGenerationStore courtyardAssets;
  std::array<renderer::Mesh, 3> courtyardMeshes;
  std::array<std::byte, 64 * 64 * 4> courtyardAtlas{};
  std::array<std::string, 4> courtyardHashes;
  std::array<ByteBuffer, 3> courtyardEnvironment;
  std::array<std::string, 3> environmentHashes;
  std::string environmentMetadataHash;
  renderer::Mesh courtyardCrystal;
  std::array<ByteBuffer, 9> courtyardDetail;
  std::array<std::string, 10> courtyardHeroHashes;
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
    assetMesh = ReadShowcaseMesh(loadedAsset->payload);
    importer.Register(".rgba", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      if (source.bytes.size() != 64 * 64 * 4 && source.bytes.size() != 256 * 256 * 4 &&
          source.bytes.size() != 384 * 256 * 4)
        return {};
      return CanonicalAsset{source.id, source.type, {}, source.bytes};
    });
    std::vector<RuntimeBlob> courtyardBlobs;
    for (std::size_t i = 0; i < courtyard_content::meshes.size(); ++i) {
      std::string meshSource;
      for (const auto row : courtyard_content::meshes[i])
        meshSource += row;
      const auto meshBytes = std::as_bytes(std::span{meshSource.data(), meshSource.size()});
      const SourceAsset adopted{{0x4e58, 100 + i},
                                "kaykit-courtyard-mesh-v1",
                                "courtyard.showcase",
                                ByteBuffer{meshBytes.begin(), meshBytes.end()}};
      const auto imported = importer.Import(adopted);
      const auto blob = imported ? AssetCooker{}.Cook(*imported, "portable", "courtyard-v1", cache)
                                 : std::nullopt;
      if (!blob)
        throw std::runtime_error("Courtyard adopted mesh cook failed");
      courtyardHashes[i] = blob->content_hash;
      courtyardBlobs.push_back(*blob);
    }
    const auto atlasBytes = std::as_bytes(std::span{courtyard_content::atlas});
    const auto importedAtlas = importer.Import({{0x4e58, 103},
                                                "kaykit-gradient-rgba8-v1",
                                                "atlas.rgba",
                                                ByteBuffer{atlasBytes.begin(), atlasBytes.end()}});
    const auto atlasBlob =
        importedAtlas ? AssetCooker{}.Cook(*importedAtlas, "portable", "courtyard-v1", cache)
                      : std::nullopt;
    if (!atlasBlob)
      throw std::runtime_error("Courtyard atlas cook failed");
    courtyardHashes[3] = atlasBlob->content_hash;
    courtyardBlobs.push_back(*atlasBlob);
    const std::array<std::span<const std::byte>, 3> environmentSources{
        std::as_bytes(std::span{courtyard_environment::diffuse}),
        std::as_bytes(std::span{courtyard_environment::specular}),
        std::as_bytes(std::span{courtyard_environment::brdf})};
    const auto metadata = std::as_bytes(
        std::span{courtyard_environment::metadata, sizeof(courtyard_environment::metadata) - 1});
    importer.Register(".iblmeta", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      if (source.bytes.empty() || source.bytes.size() > 16384)
        return {};
      return CanonicalAsset{source.id, source.type, {}, source.bytes};
    });
    const auto importedMetadata = importer.Import({{0x4e58, 107},
                                                   "courtyard-ibl-metadata-v1",
                                                   "environment.iblmeta",
                                                   ByteBuffer{metadata.begin(), metadata.end()}});
    const auto metadataBlob = importedMetadata ? AssetCooker{}.Cook(*importedMetadata, "portable",
                                                                    "courtyard-ibl-v1", cache)
                                               : std::nullopt;
    if (!metadataBlob)
      throw std::runtime_error("Courtyard IBL metadata cook failed");
    courtyardBlobs.push_back(*metadataBlob);
    environmentMetadataHash = metadataBlob->content_hash;
    importer.Register(".rgba16f", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      using namespace Nexora::Presentation;
      if (source.id.high != 0x4e58 || source.id.low < 104 || source.id.low > 106)
        return {};
      const std::array widths{16U, 64U, 32U}, heights{8U, 32U, 32U}, levels{1U, 7U, 1U};
      const auto slot = static_cast<std::size_t>(source.id.low - 104);
      if (!ValidateSceneLinearTexture(
              {source.id.low, widths[slot], heights[slot], levels[slot], source.bytes}))
        return {};
      return CanonicalAsset{source.id, source.type, {{0x4e58, 107}}, source.bytes};
    });
    for (std::size_t i = 0; i < environmentSources.size(); ++i) {
      const auto sourceBytes = environmentSources[i];
      const auto imported = importer.Import({{0x4e58, 104 + i},
                                             "courtyard-ibl-rgba16f-v1",
                                             "environment.rgba16f",
                                             ByteBuffer{sourceBytes.begin(), sourceBytes.end()}});
      const auto blob = imported
                            ? AssetCooker{}.Cook(*imported, "portable", "courtyard-ibl-v1", cache)
                            : std::nullopt;
      if (!blob)
        throw std::runtime_error("Courtyard IBL resource cook failed");
      environmentHashes[i] = blob->content_hash;
      courtyardBlobs.push_back(*blob);
    }
    importer.Register(".artmeta", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      if (source.bytes.empty() || source.bytes.size() > 16384)
        return {};
      return CanonicalAsset{source.id, source.type, {}, source.bytes};
    });
    importer.Register(".heromesh", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      static_cast<void>(ReadShowcaseMesh(source.bytes));
      return CanonicalAsset{source.id, source.type, {{0x4e58, 119}}, source.bytes};
    });
    importer.Register(".surface", [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
      if (source.bytes.size() != 64 * 64 * 4 && source.bytes.size() != 256 * 256 * 4 &&
          source.bytes.size() != 384 * 256 * 4)
        return {};
      return CanonicalAsset{source.id, source.type, {{0x4e58, 119}}, source.bytes};
    });
    const std::array<std::span<const std::byte>, 11> heroSources{
        std::as_bytes(std::span{courtyard_hero::metadata, sizeof(courtyard_hero::metadata) - 1}),
        std::as_bytes(std::span{courtyard_hero::mesh, sizeof(courtyard_hero::mesh) - 1}),
        std::as_bytes(std::span{courtyard_hero::stone_color}),
        std::as_bytes(std::span{courtyard_hero::stone_normal}),
        std::as_bytes(std::span{courtyard_hero::stone_orm}),
        std::as_bytes(std::span{courtyard_hero::bronze_color}),
        std::as_bytes(std::span{courtyard_hero::bronze_normal}),
        std::as_bytes(std::span{courtyard_hero::bronze_orm}),
        std::as_bytes(std::span{courtyard_hero::leaf}),
        std::as_bytes(std::span{courtyard_hero::mote}),
        std::as_bytes(std::span{courtyard_hero::sky})};
    for (std::size_t i = 0; i < heroSources.size(); ++i) {
      const auto payload = heroSources[i];
      const auto imported = importer.Import({{0x4e58, 119 + i},
                                             "original-courtyard-art-v1",
                                             i == 0   ? "hero.artmeta"
                                             : i == 1 ? "crystal.heromesh"
                                                      : "detail.surface",
                                             ByteBuffer{payload.begin(), payload.end()}});
      const auto blob = imported
                            ? AssetCooker{}.Cook(*imported, "portable", "courtyard-art-v1", cache)
                            : std::nullopt;
      if (!blob)
        throw std::runtime_error("Courtyard original hero cook failed");
      if (i)
        courtyardHeroHashes[i - 1] = blob->content_hash;
      courtyardBlobs.push_back(*blob);
    }
    const auto adoptedBundle = BundleBuilder::Build("courtyard", 1, {}, courtyardBlobs);
    if (!adoptedBundle || !courtyardAssets.Stage({*adoptedBundle}) ||
        !courtyardAssets.ActivateStaged())
      throw std::runtime_error("Courtyard bundle activation failed");
    for (std::size_t i = 0; i < courtyardMeshes.size(); ++i) {
      const auto *loaded = courtyardAssets.Load({0x4e58, 100 + i});
      if (!loaded)
        throw std::runtime_error("Courtyard adopted mesh load failed");
      courtyardMeshes[i] = ReadShowcaseMesh(loaded->payload);
    }
    const auto *loadedAtlas = courtyardAssets.Load({0x4e58, 103});
    if (!loadedAtlas || loadedAtlas->payload.size() != courtyardAtlas.size())
      throw std::runtime_error("Courtyard adopted atlas load failed");
    std::copy(loadedAtlas->payload.begin(), loadedAtlas->payload.end(), courtyardAtlas.begin());
    for (std::size_t i = 0; i < courtyardEnvironment.size(); ++i) {
      const auto *loaded = courtyardAssets.Load({0x4e58, 104 + i});
      if (!loaded || loaded->payload.size() != environmentSources[i].size() ||
          loaded->dependencies != std::vector<AssetUuid>{{0x4e58, 107}})
        throw std::runtime_error("Courtyard cooked IBL resource load failed");
      courtyardEnvironment[i] = loaded->payload;
    }
    if (!courtyardAssets.Load({0x4e58, 107}))
      throw std::runtime_error("Courtyard cooked IBL metadata load failed");
    for (std::size_t i = 0; i < 10; ++i) {
      const auto *loaded = courtyardAssets.Load({0x4e58, 120 + i});
      if (!loaded || loaded->dependencies != std::vector<AssetUuid>{{0x4e58, 119}})
        throw std::runtime_error("Courtyard hero generation dependency failed");
      if (!i)
        courtyardCrystal = ReadShowcaseMesh(loaded->payload);
      else {
        if (loaded->payload.size() != (i == 9   ? 384 * 256 * 4
                                       : i <= 7 ? 256 * 256 * 4
                                                : 64 * 64 * 4))
          throw std::runtime_error("Courtyard detail payload invalid");
        courtyardDetail[i - 1] = loaded->payload;
      }
    }
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
    if (room != "courtyard" && std::find(rooms.begin(), rooms.end(), room) == rooms.end())
      throw std::invalid_argument("Unknown showcase room");
    const bool enteringCourtyard = room == "courtyard" && selected != room;
    selected = room;
    if (enteringCourtyard)
      CourtyardCamera(0);
    visited.insert(selected);
    lastAction = "Entered " + selected;
  }
  void CourtyardCamera(std::size_t shot) {
    courtyardFreeCamera = false;
    courtyardShot = shot % 3;
    constexpr std::array<float, 3> yaws{0.12F, -0.35F, 0.12F};
    constexpr std::array<float, 3> pitches{0.20F, 0.20F, 0.13F};
    constexpr std::array<float, 3> radii{9.0F, 9.0F, 11.0F};
    yaw = yaws[courtyardShot];
    pitch = pitches[courtyardShot];
    radius = radii[courtyardShot];
  }
#if NEXORA_ASSET_PIPELINE_ENABLED
  void AdoptedMesh(std::size_t meshIndex, math::Vector3 position, math::Vector3 scale) {
    const auto &mesh = courtyardMeshes.at(meshIndex);
    if (vertices.size() + mesh.vertices.size() > 65535)
      throw std::runtime_error("Courtyard geometry exceeds the native vertex budget");
    const auto base = static_cast<std::uint16_t>(vertices.size());
    for (const auto &v : mesh.vertices) {
      const auto normal = math::NormalizeSafe(
          math::Vector3{v.normal[0] / scale.x, v.normal[1] / scale.y, v.normal[2] / scale.z});
      vertices.push_back(
          {{position.x + v.position[0] * scale.x, position.y + v.position[1] * scale.y,
            position.z + v.position[2] * scale.z},
           {normal.x, normal.y, normal.z},
           {v.uv[0], v.uv[1]}});
    }
    for (const auto index : mesh.indices)
      indices.push_back(static_cast<std::uint16_t>(base + index));
  }
#endif
  void Lathe(math::Vector3 center, std::span<const std::array<float, 2>> profile,
             bool fluted = false) {
    const unsigned sides = fluted ? 64 : 48;
    for (std::size_t layer = 0; layer + 1 < profile.size(); ++layer)
      for (unsigned side = 0; side < sides; ++side) {
        const auto base = static_cast<std::uint16_t>(vertices.size());
        const std::array<std::array<unsigned, 2>, 4> corners{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
        for (const auto corner : corners) {
          const auto row = layer + corner[1];
          const float angle = 2 * math::kPi * (side + corner[0]) / sides;
          const auto previous = profile[row ? row - 1 : row];
          const auto next = profile[std::min(row + 1, profile.size() - 1)];
          const float dr = next[0] - previous[0], dy = next[1] - previous[1];
          const auto normal =
              math::NormalizeSafe(math::Vector3{dy * std::cos(angle), -dr, dy * std::sin(angle)});
          const float columnRadius =
              profile[row][0] * (fluted ? 0.97F + 0.03F * std::cos(angle * 12) : 1);
          vertices.push_back(
              {{center.x + columnRadius * std::cos(angle), center.y + profile[row][1],
                center.z + columnRadius * std::sin(angle)},
               {normal.x, normal.y, normal.z},
               {std::abs(normal.y) > 0.8F ? columnRadius * std::cos(angle) * 0.8F
                                          : angle * std::max(columnRadius, 0.3F) * 0.8F,
                std::abs(normal.y) > 0.8F ? columnRadius * std::sin(angle) * 0.8F
                                          : profile[row][1] * 0.8F}});
        }
        if (profile[layer][0] > 0)
          for (const auto index : {0, 1, 2})
            indices.push_back(static_cast<std::uint16_t>(base + index));
        if (profile[layer + 1][0] > 0)
          for (const auto index : {0, 2, 3})
            indices.push_back(static_cast<std::uint16_t>(base + index));
      }
  }
  void RingStone(float a, float b, float radialHalfWidth = 0.24F, float depthHalfWidth = 0.25F) {
    constexpr float middleRadius = 1.66F;
    const float middleAngle = (a + b) * 0.5F;
    const auto firstVertex = vertices.size();
    // Bend an original bevelled block into an annular wedge. Each chamfer retains a
    // correct transformed normal; the curve's differential is not a rigid rotation.
    CourtyardBlock(0, 0, 0, (b - a) * middleRadius * 0.5F, radialHalfWidth, depthHalfWidth);
    for (std::size_t i = firstVertex; i < vertices.size(); ++i) {
      auto &vertex = vertices[i];
      const float radiusAtVertex = middleRadius + vertex.position[1];
      const float angle = middleAngle + vertex.position[0] / middleRadius;
      const float sine = std::sin(angle), cosine = std::cos(angle);
      const auto normal = math::NormalizeSafe(math::Vector3{
          -sine * vertex.normal[0] * middleRadius / radiusAtVertex + cosine * vertex.normal[1],
          cosine * vertex.normal[0] * middleRadius / radiusAtVertex + sine * vertex.normal[1],
          vertex.normal[2]});
      vertex.position[0] = radiusAtVertex * cosine;
      vertex.position[1] = 2.9F + radiusAtVertex * sine;
      vertex.normal[0] = normal.x;
      vertex.normal[1] = normal.y;
      vertex.normal[2] = normal.z;
    }
  }
  void VisualTourCamera() {
    constexpr std::array<std::array<float, 4>, 6> route{{{0, .12F, .20F, 9.0F},
                                                         {25, .05F, .35F, 15},
                                                         {45, -.35F, .26F, 9},
                                                         {65, .12F, .20F, 11},
                                                         {80, -.45F, .32F, 13},
                                                         {100, .12F, .20F, 9.0F}}};
    std::size_t segment = 0;
    while (segment + 2 < route.size() && tourSeconds >= route[segment + 1][0])
      ++segment;
    const auto &a = route[segment], &b = route[segment + 1];
    float t = std::clamp((static_cast<float>(tourSeconds) - a[0]) / (b[0] - a[0]), 0.0F, 1.0F);
    t = t * t * (3 - 2 * t);
    yaw = a[1] + (b[1] - a[1]) * t;
    pitch = a[2] + (b[2] - a[2]) * t;
    radius = a[3] + (b[3] - a[3]) * t;
    tourStep = segment;
    courtyardActive = tourSeconds >= 75;
  }
  void LeafQuad(math::Vector3 center, float halfWidth, float height, float angle) {
    const auto base = static_cast<std::uint16_t>(vertices.size());
    const math::Vector3 right{std::cos(angle), 0, std::sin(angle)};
    const math::Vector3 normal{-std::sin(angle), 0, std::cos(angle)};
    for (const auto uv : {std::array{0.0F, 0.0F}, std::array{1.0F, 0.0F}, std::array{1.0F, 1.0F},
                          std::array{0.0F, 1.0F}}) {
      const auto point =
          center + right * ((uv[0] * 2 - 1) * halfWidth) + math::Vector3{0, uv[1] * height, 0};
      vertices.push_back({{point.x, point.y, point.z},
                          {normal.x, normal.y, normal.z},
                          {uv[0], uv[1]},
                          {right.x, right.y, right.z, 1}});
    }
    for (const auto i : {0, 1, 2, 0, 2, 3})
      indices.push_back(static_cast<std::uint16_t>(base + i));
  }
  void CourtyardParticles() {
    courtyardParticleCount = courtyardActive ? (24U << courtyardQuality) : 0;
    const auto first = static_cast<std::uint32_t>(indices.size());
    for (std::size_t i = 0; i < courtyardParticleCount; ++i) {
      const float phase =
          static_cast<float>(courtyardSeconds) * 0.35F + static_cast<float>(i) * 0.618F;
      const float y =
          0.8F + std::fmod(static_cast<float>(courtyardSeconds) * 0.6F + i * 0.113F, 3.5F);
      const float r = 0.75F + static_cast<float>(i % 7) * 0.08F;
      LeafQuad({std::cos(phase) * r, y, std::sin(phase) * r}, 0.025F, 0.05F, -yaw);
    }
    if (indices.size() > first)
      batches.push_back({first, static_cast<std::uint32_t>(indices.size() - first), 0, 1, 7});
  }
  Nexora::Presentation::ScenePlanarReflection CourtyardReflectionSettings() const {
    Nexora::Presentation::ScenePlanarReflection planar;
    planar.planeHeight = 0.16F + 0.003F * std::sin(static_cast<float>(courtyardSeconds) * 0.8F);
    planar.regions[0] = {0.6F, 5.1F, 0.9F, 0.8F};
    planar.shorelineVariation = 0.18F;
    planar.regions[1] = {3.1F, 3.0F, 0.85F, 0.65F};
    planar.regionCount = 2;
    return planar;
  }
  void CourtyardReflectionGeometry() {
    using Nexora::Presentation::SceneReflectionRole;
    const auto planar = CourtyardReflectionSettings();
    const auto materialCount = materials.size();
    materials[0].reflectionRole = materials[8].reflectionRole = SceneReflectionRole::Receiver;
    for (std::size_t i = 0; i < materialCount; ++i) {
      auto reflected = materials[i];
      reflected.reflectionRole = SceneReflectionRole::ReflectedGeometry;
      reflected.castsShadow = false;
      materials.push_back(reflected);
    }
    // Reuse the original vertices with an affine mirror instance. The public instance
    // upload already supplies the inverse-transpose normal rows and determinant sign.
    if (instances.empty())
      instances.push_back({});
    const auto mirrorIndex = static_cast<std::uint32_t>(instances.size());
    instances.push_back({});
    math::Matrix4 mirror;
    mirror(1, 1) = -1;
    mirror(1, 3) = 2 * planar.planeHeight;
    instances[mirrorIndex].model_transform = mirror.values;
    const auto sourceBatches = batches;
    for (const auto &batch : sourceBatches) {
      // Bound planar work to the focal device, vessels, foliage, pennants and sky.
      // Distant ruins/terrain and the water surface never participate recursively.
      if (batch.materialIndex < 18 &&
          batch.firstIndex + batch.indexCount > courtyardReflectionDeviceIndexEnd &&
          batch.materialIndex != 2 && batch.materialIndex != 3 && batch.materialIndex != 5 &&
          batch.materialIndex != 6 && batch.materialIndex != 7 && batch.materialIndex != 11 &&
          batch.materialIndex != 12 && batch.materialIndex != 15 && batch.materialIndex != 16 &&
          batch.materialIndex != 17)
        continue;
      batches.push_back({batch.firstIndex, batch.indexCount, mirrorIndex, 1,
                         batch.materialIndex + static_cast<std::uint32_t>(materialCount)});
    }
  }
  void CourtyardSplinters() {
#if NEXORA_ASSET_PIPELINE_ENABLED
    if (!courtyardActive)
      return;
    const auto first = static_cast<std::uint32_t>(indices.size());
    for (unsigned shard = 0; shard < 3; ++shard) {
      const float phase = static_cast<float>(courtyardSeconds) * 0.45F + shard * 2 * math::kPi / 3;
      const math::Vector3 center{1.1F * std::cos(phase), 3.1F + 0.25F * std::sin(phase * 2),
                                 1.1F * std::sin(phase)};
      const auto base = static_cast<std::uint16_t>(vertices.size());
      for (const auto &v : courtyardCrystal.vertices) {
        const math::Vector3 n{v.normal[0], v.normal[1], v.normal[2]};
        const auto tangent = math::NormalizeSafe(
            math::Cross(n, std::abs(n.y) < 0.9F ? math::Vector3{0, 1, 0} : math::Vector3{1, 0, 0}));
        Nexora::Presentation::SceneVertex vertex{{center.x + v.position[0] * 0.2F,
                                                  center.y + v.position[1] * 0.2F,
                                                  center.z + v.position[2] * 0.2F},
                                                 {n.x, n.y, n.z},
                                                 {v.uv[0], v.uv[1]}};
        vertex.tangent[0] = tangent.x;
        vertex.tangent[1] = tangent.y;
        vertex.tangent[2] = tangent.z;
        vertex.tangent[3] = 1;
        vertices.push_back(vertex);
      }
      for (const auto index : courtyardCrystal.indices)
        indices.push_back(static_cast<std::uint16_t>(base + index));
    }
    batches.push_back({first, static_cast<std::uint32_t>(indices.size()) - first, 0, 1, 12});
#endif
  }
  // Shared authoring sites keep the falling ribbons in front of their supporting cliffs.
  static constexpr std::array courtyardFallSites{std::array{-17.0F, -19.0F, 6.2F},
                                                 std::array{7.0F, -21.0F, 5.8F}};
  void CourtyardWaterfalls() {
    const auto first = static_cast<std::uint32_t>(indices.size());
    const float time = static_cast<float>(courtyardSeconds);
    for (const auto location : courtyardFallSites)
      for (unsigned ribbon = 0; ribbon < 4; ++ribbon)
        for (unsigned row = 0; row < 24; ++row) {
          const auto base = static_cast<std::uint16_t>(vertices.size());
          for (const auto corner :
               {std::array{0, 0}, std::array{1, 0}, std::array{1, 1}, std::array{0, 1}}) {
            const float y = location[2] - (row + corner[1]) * ((location[2] - 0.3F) / 24);
            const float pulse = std::sin(y * 4 + time * 3 + ribbon);
            const float px =
                location[0] + (ribbon - 1.5F) * 0.3F + corner[0] * 0.22F + pulse * 0.04F;
            Nexora::Presentation::SceneVertex vertex{{px, y, location[1] + pulse * 0.025F},
                                                     {0, 0, 1},
                                                     {corner[0] * 1.0F, (row + corner[1]) / 24.0F}};
            vertex.tangent[0] = 1;
            vertex.tangent[3] = 1;
            vertices.push_back(vertex);
          }
          for (const auto index : {0, 1, 2, 0, 2, 3})
            indices.push_back(static_cast<std::uint16_t>(base + index));
        }
    batches.push_back({first, static_cast<std::uint32_t>(indices.size()) - first, 0, 1, 14});
  }
  void CourtyardWater() {
    const auto first = static_cast<std::uint32_t>(indices.size());
    constexpr unsigned rings = 8, sides = 32;
    const float time = static_cast<float>(courtyardSeconds);
    for (const auto location : {std::array{-2.8F, 4.4F, 1.1F}, std::array{2.3F, 3.8F, 0.75F}}) {
      const auto base = static_cast<std::uint16_t>(vertices.size());
      for (unsigned ring = 0; ring <= rings; ++ring)
        for (unsigned side = 0; side <= sides; ++side) {
          const float angle = 2 * math::kPi * side / sides;
          const float rippleRadius = location[2] * ring / rings;
          const float x = location[0] + std::cos(angle) * rippleRadius;
          const float z = location[1] + std::sin(angle) * rippleRadius * 0.65F;
          const float phase = 4 * x + 3 * z + time * 1.3F;
          const float ripple = 0.004F * std::sin(phase);
          const auto normal = math::NormalizeSafe(
              math::Vector3{-0.016F * std::cos(phase), 1, -0.012F * std::cos(phase)});
          const auto tangent = math::NormalizeSafe(math::Vector3{1, 0.016F * std::cos(phase), 0});
          Nexora::Presentation::SceneVertex vertex{
              {x, 0.23F + ripple, z}, {normal.x, normal.y, normal.z}, {x, z}};
          vertex.tangent[0] = tangent.x;
          vertex.tangent[1] = tangent.y;
          vertex.tangent[2] = tangent.z;
          vertex.tangent[3] = -1;
          vertices.push_back(vertex);
        }
      for (unsigned ring = 0; ring < rings; ++ring)
        for (unsigned side = 0; side < sides; ++side) {
          const auto a = base + ring * (sides + 1) + side, b = a + sides + 1;
          for (const auto index : {a, b, a + 1, a + 1, b, b + 1})
            indices.push_back(static_cast<std::uint16_t>(index));
        }
    }
    batches.push_back({first, static_cast<std::uint32_t>(indices.size()) - first, 0, 1, 13});
  }
  void CourtyardSkybox(math::Vector3 eye) {
    // Six inward-facing faces, camera-centred so translation never reveals an edge.
    const std::array<math::Vector3, 6> normals{
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    const std::array<math::Vector3, 6> rights{
        {{0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {-1, 0, 0}}};
    const std::array<math::Vector3, 6> ups{
        {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}}};
    const auto first = static_cast<std::uint32_t>(indices.size());
    for (unsigned face = 0; face < 6; ++face) {
      const auto base = static_cast<std::uint16_t>(vertices.size());
      const auto n = normals[face], u = rights[face], v = ups[face];
      for (const auto corner :
           {std::array{0, 0}, std::array{1, 0}, std::array{1, 1}, std::array{0, 1}}) {
        const auto point =
            eye + (n + u * (2.0F * corner[0] - 1) + v * (2.0F * corner[1] - 1)) * 120;
        Nexora::Presentation::SceneVertex vertex{{point.x, point.y, point.z},
                                                 {-n.x, -n.y, -n.z},
                                                 {(face % 3 * 128 + 0.5F + corner[0] * 127) / 384,
                                                  (face / 3 * 128 + 0.5F + corner[1] * 127) / 256}};
        vertex.tangent[0] = u.x;
        vertex.tangent[1] = u.y;
        vertex.tangent[2] = u.z;
        vertex.tangent[3] = -1;
        vertices.push_back(vertex);
      }
      for (const auto index : {0, 2, 1, 0, 3, 2})
        indices.push_back(static_cast<std::uint16_t>(base + index));
    }
    batches.push_back({first, 36, 0, 1, 6});
    const auto sunDirection = math::NormalizeSafe(math::Vector3{courtyard_hero::sun_direction[0],
                                                                courtyard_hero::sun_direction[1],
                                                                courtyard_hero::sun_direction[2]});
    const auto right = math::NormalizeSafe(math::Cross(sunDirection, math::Vector3{0, 1, 0}));
    const auto up = math::Cross(right, sunDirection);
    const auto center = eye + sunDirection * 104;
    const auto sunFirst = static_cast<std::uint32_t>(indices.size());
    for (unsigned i = 0; i < 32; ++i) {
      const auto base = static_cast<std::uint16_t>(vertices.size());
      const float a = 2 * math::kPi * i / 32, b = 2 * math::kPi * (i + 1) / 32;
      for (const auto point : {center, center + (right * std::cos(a) + up * std::sin(a)) * 1.64F,
                               center + (right * std::cos(b) + up * std::sin(b)) * 1.64F}) {
        Nexora::Presentation::SceneVertex vertex{
            {point.x, point.y, point.z},
            {-sunDirection.x, -sunDirection.y, -sunDirection.z},
            {0, 0}};
        vertex.tangent[0] = right.x;
        vertex.tangent[1] = right.y;
        vertex.tangent[2] = right.z;
        vertex.tangent[3] = 1;
        vertices.push_back(vertex);
      }
      for (unsigned corner = 0; corner < 3; ++corner)
        indices.push_back(static_cast<std::uint16_t>(base + corner));
    }
    batches.push_back({sunFirst, 96, 0, 1, 11});
  }
  // Authored stone/bronze device; native visual acceptance is tracked separately.
  void CourtyardGeometry() {
    if (!courtyardVertices.empty()) {
      vertices = courtyardVertices;
      indices = courtyardIndices;
      batches = courtyardBatches;
      instances = courtyardInstances;
      return;
    }
    instances = {{}}; // Identity instance for world-authored geometry.
    // Close each consecutive geometry range with an explicit ephemeral material slot.
    std::size_t firstIndex = 0;
    const auto finish = [&](std::uint32_t material) {
      if (indices.size() > firstIndex)
        batches.push_back({static_cast<std::uint32_t>(firstIndex),
                           static_cast<std::uint32_t>(indices.size() - firstIndex), 0, 1,
                           material});
      firstIndex = indices.size();
    };
    for (const auto tier : {std::array{2.65F, 0.0F, 0.24F}, std::array{1.7F, 0.24F, 0.60F},
                            std::array{1.05F, 0.60F, 1.05F}}) {
      const float lip = std::min(0.09F, (tier[2] - tier[1]) * 0.2F);
      const std::array<std::array<float, 2>, 8> profile{{{0, tier[1]},
                                                         {tier[0] - 0.05F, tier[1]},
                                                         {tier[0], tier[1] + lip},
                                                         {tier[0] - 0.025F, tier[1] + lip * 1.8F},
                                                         {tier[0] - 0.025F, tier[2] - lip * 1.8F},
                                                         {tier[0], tier[2] - lip},
                                                         {tier[0] - 0.05F, tier[2]},
                                                         {0, tier[2]}}};
      Lathe({0, 0, 0}, profile);
    }
    finish(0);
    // Original sculpted basin and radial buttresses support the floating hero crystal.
    const std::array<std::array<float, 2>, 8> basin{{{0, 1.04F},
                                                     {0.48F, 1.04F},
                                                     {0.34F, 1.22F},
                                                     {0.42F, 1.6F},
                                                     {0.78F, 1.75F},
                                                     {0.8F, 1.87F},
                                                     {0.62F, 1.87F},
                                                     {0, 1.76F}}};
    Lathe({0, 0, 0.45F}, basin);
    for (unsigned rib = 0; rib < 12; ++rib) {
      const float angle = rib * 2 * math::kPi / 12;
      const auto point = [&](float r, float y) {
        return math::Vector3{r * std::cos(angle), y, 0.45F + r * std::sin(angle)};
      };
      Segment(point(0.54F, 1.08F), point(0.39F, 1.3F), 0.045F);
      Segment(point(0.39F, 1.3F), point(0.47F, 1.57F), 0.04F);
      Segment(point(0.47F, 1.57F), point(0.73F, 1.76F), 0.045F);
    }
    finish(0);
    // Original shallow stone relief around the middle pedestal tier.
    for (unsigned ornament = 0; ornament < 16; ++ornament) {
      const float angle = 2 * math::kPi * ornament / 16;
      const auto center = math::Vector3{1.71F * std::cos(angle), 0.42F, 1.71F * std::sin(angle)};
      const auto tangent = math::Vector3{-std::sin(angle), 0, std::cos(angle)};
      const std::array<math::Vector3, 4> corners{
          center + math::Vector3{0, 0.12F, 0}, center + tangent * 0.14F,
          center - math::Vector3{0, 0.12F, 0}, center - tangent * 0.14F};
      for (unsigned edge = 0; edge < corners.size(); ++edge)
        Segment(corners[edge], corners[(edge + 1) % corners.size()], 0.012F);
    }
    finish(8);
    constexpr std::size_t sides = 20;
    for (std::size_t i = 0; i < sides; ++i) {
      const float a = static_cast<float>(i) / sides * math::kPi * 2;
      const float b = static_cast<float>(i + 1) / sides * math::kPi * 2;
      if (i == 6 || i == 7 || i == 8)
        continue; // Broken upper-left stone silhouette.
      RingStone(a + 0.008F, b - 0.008F);
      finish(0);
      if (i % 4 == 0) {
        RingStone(a - 0.035F, a + 0.035F, 0.265F, 0.275F);
        finish(1);
      }
      const float mid = (a + b) * 0.5F;
      const math::Vector3 center{1.65F * std::cos(mid), 2.9F + 1.65F * std::sin(mid), 0.255F};
      const math::Vector3 radial{std::cos(mid) * 0.1F, std::sin(mid) * 0.1F, 0};
      const math::Vector3 tangent{-std::sin(mid) * 0.06F, std::cos(mid) * 0.06F, 0};
      const auto base = static_cast<std::uint16_t>(vertices.size());
      for (const auto point :
           {center + radial, center + tangent, center - radial, center - tangent})
        vertices.push_back({{point.x, point.y, point.z}, {0, 0, 1}, {0.5F, 0.5F}});
      for (const auto index : {0, 1, 2, 0, 2, 3})
        indices.push_back(static_cast<std::uint16_t>(base + index));
      finish(2);
      Segment({1.43F * std::cos(a), 2.9F + 1.43F * std::sin(a), 0.27F},
              {1.43F * std::cos(b), 2.9F + 1.43F * std::sin(b), 0.27F}, 0.018F);
      finish(2);
      Segment({1.94F * std::cos(a), 2.9F + 1.94F * std::sin(a), 0.05F},
              {1.94F * std::cos(b), 2.9F + 1.94F * std::sin(b), 0.05F}, 0.04F);
      finish(1);
    }
    courtyardReflectionDeviceIndexEnd = indices.size();
    const std::array<std::array<float, 2>, 10> column{{{0, 0},
                                                       {0.7F, 0},
                                                       {0.7F, 0.22F},
                                                       {0.5F, 0.35F},
                                                       {0.43F, 0.55F},
                                                       {0.4F, 2.8F},
                                                       {0.55F, 3},
                                                       {0.7F, 3.12F},
                                                       {0.7F, 3.35F},
                                                       {0, 3.35F}}};
    Lathe({0, 0, 0}, column, true);
    finish(0);
    batches.back().firstInstance = 1;
    batches.back().instanceCount = 4;
    for (const float x : {-4.5F, 4.5F})
      for (const float z : {-4.0F, 1.5F}) {
        Nexora::Presentation::SceneInstance instance{};
        instance.translation[0] = x + (z > 0 ? (x < 0 ? -1.0F : 1.0F) : 0.0F);
        instance.translation[2] = z;
        instance.scale[1] = z > 0 ? 0.7F : 1.0F;
        instances.push_back(instance);
      }
    // Original shallow relief uses the same column instance range.
    for (const float y : {0.9F, 1.6F, 2.3F}) {
      const std::array<math::Vector3, 4> motif{
          {{0, y + 0.19F, 0.43F}, {0.16F, y, 0.43F}, {0, y - 0.19F, 0.43F}, {-0.16F, y, 0.43F}}};
      for (unsigned edge = 0; edge < motif.size(); ++edge)
        Segment(motif[edge], motif[(edge + 1) % motif.size()], 0.012F);
      Segment({-0.25F, y - 0.23F, 0.43F}, {0.25F, y - 0.23F, 0.43F}, 0.01F);
    }
    finish(8);
    batches.back().firstInstance = 1;
    batches.back().instanceCount = 4;
    // Eight original chipped tile meshes share native instances. World-space maps and
    // inverse-transpose normals retain consistent scale and lighting on every stone.
    constexpr unsigned pavingVariants = 8;
    for (unsigned variant = 0; variant < pavingVariants; ++variant) {
      const auto tileVertex = vertices.size();
      CourtyardPaving(static_cast<int>(variant), 0);
      for (std::size_t v = tileVertex; v < vertices.size(); ++v)
        vertices[v].position[0] -= variant * 1.2F;
      const auto prototypeSeed = (variant + 11U) * 73856093U ^ 11U * 19349663U;
      const float prototypeX = 0.575F - static_cast<float>(prototypeSeed % 5) * 0.005F;
      const float prototypeZ = 0.575F - static_cast<float>((prototypeSeed >> 4) % 5) * 0.005F;
      const float prototypeTop = 0.13F + static_cast<float>(prototypeSeed % 7) * 0.003F;
      const auto pavingFirst = static_cast<std::uint32_t>(instances.size());
      for (int z = -10; z <= 10; ++z)
        for (int x = -10; x <= 10; ++x) {
          if (std::abs(x) <= 1 && std::abs(z) <= 1)
            continue;
          const auto seed = static_cast<std::uint32_t>(x + 11) * 73856093U ^
                            static_cast<std::uint32_t>(z + 11) * 19349663U;
          if (seed % pavingVariants != variant)
            continue;
          Nexora::Presentation::SceneInstance tile{};
          tile.translation[0] = x * 1.2F;
          tile.translation[2] = z * 1.2F;
          tile.scale[0] = (0.575F - static_cast<float>(seed % 5) * 0.005F) / prototypeX;
          tile.scale[1] = (0.13F + static_cast<float>(seed % 7) * 0.003F) / prototypeTop;
          tile.scale[2] = (0.575F - static_cast<float>((seed >> 4) % 5) * 0.005F) / prototypeZ;
          instances.push_back(tile);
        }
      finish(0);
      batches.back().firstInstance = pavingFirst;
      batches.back().instanceCount = static_cast<std::uint32_t>(instances.size()) - pavingFirst;
    }
#if NEXORA_ASSET_PIPELINE_ENABLED
    AdoptedMesh(2, {-3.8F, 0, -2.5F}, {0.22F, 0.22F, 0.22F});
    AdoptedMesh(2, {3.8F, 0, -2.5F}, {0.22F, 0.22F, 0.22F});
#endif
    finish(4);
    // Broken rear arch and low side walls keep the focal device visible.
    Cube(-2.8F, 2.95F, -4, 1.7F, 0.3F, 0.45F);
    Cube(3.8F, 2.95F, -4, 0.7F, 0.3F, 0.45F);
    Cube(-5.3F, 0.55F, -1, 0.3F, 0.55F, 3.0F);
    Cube(5.3F, 0.55F, -1, 0.3F, 0.55F, 3.0F);
    for (const float archX : {-3.6F, 3.6F}) {
      for (unsigned i = 0; i < 12; ++i) {
        if (archX > 0 && i == 5)
          continue;
        const float a = static_cast<float>(i) / 12 * math::kPi;
        const float b = static_cast<float>(i + 1) / 12 * math::kPi;
        Segment({archX + 1.2F * std::cos(a), 2.6F + 1.2F * std::sin(a), -4},
                {archX + 1.2F * std::cos(b), 2.6F + 1.2F * std::sin(b), -4}, 0.24F);
      }
      for (const float x : {archX - 1.2F, archX + 1.2F})
        Cube(x, 1.3F, -4, 0.25F, 1.3F, 0.3F);
    }
    finish(0);
    for (const float x : {-3.5F, 3.5F}) {
      const std::array<std::array<float, 2>, 10> vessel{{{0, 0},
                                                         {0.22F, 0},
                                                         {0.36F, 0.15F},
                                                         {0.43F, 0.5F},
                                                         {0.32F, 0.8F},
                                                         {0.22F, 0.95F},
                                                         {0.29F, 1.05F},
                                                         {0.24F, 1.05F},
                                                         {0.18F, 0.85F},
                                                         {0, 0.85F}}};
      Lathe({x, 0, 2.5F}, vessel); // Original hollow ceramic profile, not a downloaded prop.
      for (const float sign : {-1.0F, 1.0F})
        for (unsigned i = 0; i < 12; ++i) {
          const float a = -math::kPi * 0.5F + i * math::kPi / 12;
          const float b = -math::kPi * 0.5F + (i + 1) * math::kPi / 12;
          Segment({x + sign * (0.34F + std::cos(a) * 0.22F), 0.65F + std::sin(a) * 0.27F, 2.5F},
                  {x + sign * (0.34F + std::cos(b) * 0.22F), 0.65F + std::sin(b) * 0.27F, 2.5F},
                  0.035F);
        }
      finish(3);
      for (const float y : {0.2F, 0.72F, 0.99F}) {
        const float r = y < 0.3F ? 0.375F : y > 0.9F ? 0.253F : 0.355F;
        const std::array<std::array<float, 2>, 4> band{
            {{r, y - 0.015F}, {r + 0.012F, y - 0.015F}, {r + 0.012F, y + 0.015F}, {r, y + 0.015F}}};
        Lathe({x, 0, 2.5F}, band);
      }
      for (unsigned motif = 0; motif < 10; ++motif) {
        const float angle = motif * 2 * math::kPi / 10;
        const auto surfacePoint = [&](float a, float y) {
          const float radius =
              y < 0.5F ? 0.36F + (y - 0.15F) * 0.2F : 0.43F - (y - 0.5F) * (0.11F / 0.3F);
          return math::Vector3{x + (radius + 0.008F) * std::cos(a), y,
                               2.5F + (radius + 0.008F) * std::sin(a)};
        };
        const std::array points{surfacePoint(angle, 0.68F), surfacePoint(angle + 0.2F, 0.47F),
                                surfacePoint(angle, 0.29F), surfacePoint(angle - 0.2F, 0.47F)};
        for (std::size_t edge = 0; edge < points.size(); ++edge)
          Segment(points[edge], points[(edge + 1) % points.size()], 0.008F);
      }
      finish(17);
      for (unsigned i = 0; i < (8U << courtyardQuality); ++i) {
        const float z = -3.0F + i * (2.88F / (8U << courtyardQuality));
        LeafQuad({x + static_cast<float>(i % 3) * 0.15F, 0, z}, 0.35F, 0.85F + (i % 4) * 0.09F,
                 i * 0.73F);
        LeafQuad({x - 0.15F, 0, z}, 0.32F, 0.9F, i * 0.73F + 1.57F);
      }
      finish(5);
    }
    courtyardCrystalFirst = vertices.size();
#if NEXORA_ASSET_PIPELINE_ENABLED
    // The original faceted hero crystal is read from the active cooked/bundled generation.
    const auto crystalBase = static_cast<std::uint16_t>(vertices.size());
    for (const auto &v : courtyardCrystal.vertices)
      vertices.push_back({{v.position[0], v.position[1] + 2.9F, v.position[2]},
                          {v.normal[0], v.normal[1], v.normal[2]},
                          {v.uv[0], v.uv[1]}});
    for (const auto index : courtyardCrystal.indices)
      indices.push_back(static_cast<std::uint16_t>(crystalBase + index));
#else
    Cube(0, 2.9F, 0, 0.35F, 0.55F, 0.35F);
#endif
    finish(12);
    // Original inner mineral facets occupy real geometry behind the refractive shell.
    // Three restrained HDR materials give the interior a faceted light response.
#if NEXORA_ASSET_PIPELINE_ENABLED
    for (unsigned shade = 0; shade < 3; ++shade) {
      for (std::size_t face = 0; face < courtyardCrystal.indices.size() / 3; ++face) {
        if ((face * 7 + face / 8) % 3 != shade)
          continue;
        const auto base = static_cast<std::uint16_t>(vertices.size());
        for (unsigned corner = 0; corner < 3; ++corner) {
          const auto &v = courtyardCrystal.vertices[courtyardCrystal.indices[face * 3 + corner]];
          const auto n = math::NormalizeSafe(
              math::Vector3{v.normal[0] / 0.68F, v.normal[1] / 0.78F, v.normal[2] / 0.68F});
          vertices.push_back(
              {{v.position[0] * 0.68F, 2.9F + v.position[1] * 0.78F, v.position[2] * 0.68F},
               {n.x, n.y, n.z},
               {v.uv[0], v.uv[1]}});
          indices.push_back(static_cast<std::uint16_t>(base + corner));
        }
      }
      finish(18 + shade);
    }
#endif
    courtyardCrystalEnd = vertices.size();
    // Preserve each authored block's exact bevel profile while reusing repeated extents.
    struct MasonryPrototype {
      std::uint32_t material;
      std::array<float, 3> extent;
      std::vector<Nexora::Presentation::SceneInstance> placements;
    };
    std::vector<MasonryPrototype> masonry;
    const auto placeMasonry = [&](std::uint32_t material, float x, float y, float z, float sx,
                                  float sy, float sz) {
      const std::array extent{sx, sy, sz};
      auto prototype = std::find_if(masonry.begin(), masonry.end(), [&](const auto &entry) {
        return entry.material == material && entry.extent == extent;
      });
      if (prototype == masonry.end()) {
        masonry.push_back({material, extent, {}});
        prototype = std::prev(masonry.end());
      }
      Nexora::Presentation::SceneInstance placement{};
      placement.translation[0] = x;
      placement.translation[1] = y;
      placement.translation[2] = z;
      prototype->placements.push_back(placement);
    };
    // Layered original environment: terrain, distant ridge, ruined towers and cypress.
    Cube(0, -0.24F, 0, 38, 0.2F, 38);
    finish(8);
    // Continuous ridges with irregular peaks and foothills, rather than isolated cones.
    const auto ridgeHeight = [](float x, float z) {
      const float envelope = std::sin(math::kPi * (z + 50) / 26);
      return -0.3F + std::max(0.0F, envelope) * (8 + 3 * std::sin(x * 0.19F) +
                                                 2 * std::sin(x * 0.47F) + std::cos(x * 0.81F));
    };
    for (unsigned row = 0; row < 24; ++row)
      for (unsigned col = 0; col < 96; ++col) {
        const float x = -45 + col * (90.0F / 96), z = -50 + row * (26.0F / 24);
        const auto base = static_cast<std::uint16_t>(vertices.size());
        for (const auto corner :
             {std::array{0, 0}, std::array{0, 1}, std::array{1, 1}, std::array{1, 0}}) {
          const float px = x + corner[0] * (90.0F / 96), pz = z + corner[1] * (26.0F / 24);
          const float dx = (ridgeHeight(px + 0.1F, pz) - ridgeHeight(px - 0.1F, pz)) / 0.2F;
          const float dz = (ridgeHeight(px, pz + 0.1F) - ridgeHeight(px, pz - 0.1F)) / 0.2F;
          const auto n = math::NormalizeSafe(math::Vector3{-dx, 1, -dz});
          vertices.push_back({{px, ridgeHeight(px, pz), pz}, {n.x, n.y, n.z}, {px / 10, pz / 10}});
        }
        for (const auto index : {0, 1, 2, 0, 2, 3})
          indices.push_back(static_cast<std::uint16_t>(base + index));
      }
    finish(9);
    for (const float x : {-18.0F, -10.0F, 10.0F, 18.0F}) {
      for (unsigned layer = 0; layer < 10; ++layer)
        placeMasonry(8, x + (layer % 2) * 0.03F, 0.3F + layer * 0.6F, -16, 1.2F, 0.29F, 1.2F);
      placeMasonry(8, x, 6.2F, -16, 1.5F, 0.3F, 1.5F);
      for (const float dx : {-0.9F, 0.9F})
        placeMasonry(8, x + dx, 7.3F, -16, 0.3F, 0.8F, 0.5F);
      for (unsigned i = 0; i < 10; ++i) {
        const float a = math::kPi * i / 10, b = math::kPi * (i + 1) / 10;
        Segment({x + 2.8F + 1.6F * std::cos(a), 4 + 1.6F * std::sin(a), -16},
                {x + 2.8F + 1.6F * std::cos(b), 4 + 1.6F * std::sin(b), -16}, 0.3F);
      }
      placeMasonry(8, x + 4.4F, 2, -16, 0.35F, 2, 0.4F);
    }
    for (unsigned tower = 0; tower < 5; ++tower) {
      const float x = -4 + tower * 5.0F, height = 7.0F + static_cast<float>(tower * 7 % 6);
      const unsigned courses = static_cast<unsigned>(std::ceil(height / 0.72F));
      const float courseHeight = height / courses;
      for (unsigned course = 0; course < courses; ++course)
        placeMasonry(8, x + (course % 2) * 0.015F, 2 + (course + 0.5F) * courseHeight, -23, 1.0F,
                     courseHeight * 0.5F - 0.012F, 1.0F);
      for (const float y : {4.3F, height + 0.3F}) {
        const std::array<math::Vector3, 4> emblem{{{x, y + 0.42F, -21.95F},
                                                   {x + 0.3F, y, -21.95F},
                                                   {x, y - 0.42F, -21.95F},
                                                   {x - 0.3F, y, -21.95F}}};
        for (unsigned edge = 0; edge < emblem.size(); ++edge)
          Segment(emblem[edge], emblem[(edge + 1) % emblem.size()], 0.025F);
      }
      for (unsigned groove = 0; groove < 6; ++groove) {
        const float gx = x - 0.75F + groove * 0.3F;
        Segment({gx, 3.1F, -21.98F}, {gx, height + 1.6F, -21.98F}, 0.035F);
      }
      placeMasonry(8, x, height + 2.2F, -23, 1.3F, 0.3F, 1.3F);
      for (const float dx : {-0.9F, 0.9F})
        placeMasonry(8, x + dx, height + 3, -23, 0.25F, 0.5F, 0.45F);
      for (unsigned i = 0; i < 12; ++i) {
        const float a = math::kPi * i / 12, b = math::kPi * (i + 1) / 12;
        Segment({x + 2.5F + 1.5F * std::cos(a), 6 + 1.5F * std::sin(a), -23},
                {x + 2.5F + 1.5F * std::cos(b), 6 + 1.5F * std::sin(b), -23}, 0.28F);
      }
      placeMasonry(8, x + 4, 4, -23, 0.25F, 2, 0.3F);
    }
    for (unsigned step = 0; step < 6; ++step)
      placeMasonry(8, 6, step * 0.15F, -10 - step * 0.7F, 2.5F, step * 0.15F + 0.1F, 0.4F);
    // Cliff ledges support the distant falls and upper ruins.
    placeMasonry(8, 0, 1, -25, 20, 1, 3);
    for (const auto location : courtyardFallSites)
      placeMasonry(8, location[0], location[2] * 0.5F, location[1] - 2.2F, 2.6F, location[2] * 0.5F,
                   2);
    finish(8);
    // Side arcades frame the device, with hanging leaves driven by the shared wind shader.
    for (const float x : {-7.5F, 7.5F}) {
      for (const float z : {-7.0F, -1.0F, 5.0F}) {
        for (unsigned course = 0; course < 7; ++course) {
          const float stagger = static_cast<float>((course * 13) % 5) * 0.012F;
          placeMasonry(0, x + stagger - 0.024F, 0.4F + course * 0.8F, z, 0.55F - stagger * 0.25F,
                       0.39F, 0.55F);
        }
        placeMasonry(0, x, 0.18F, z, 0.85F, 0.18F, 0.85F);
        placeMasonry(0, x, 0.43F, z, 0.7F, 0.08F, 0.7F);
        placeMasonry(0, x, 5.53F, z, 0.68F, 0.12F, 0.68F);
        placeMasonry(0, x, 5.75F, z, 0.85F, 0.10F, 0.85F);
        placeMasonry(0, x, 5.92F, z, 0.74F, 0.06F, 0.74F);
        // Original raised geometric relief on the front face, with carved side rails.
        const auto point = [&](float dx, float y) { return math::Vector3{x + dx, y, z + 0.565F}; };
        for (const float dx : {-0.34F, 0.34F})
          Segment(point(dx, 0.9F), point(dx, 5.1F), 0.025F);
        const std::array<math::Vector3, 4> emblem{
            {point(0, 4.8F), point(0.23F, 4.45F), point(0, 4.1F), point(-0.23F, 4.45F)}};
        for (unsigned edge = 0; edge < emblem.size(); ++edge)
          Segment(emblem[edge], emblem[(edge + 1) % emblem.size()], 0.03F);
        Segment(point(0, 1.0F), point(0, 3.95F), 0.022F);
        for (const float sign : {-1.0F, 1.0F})
          for (const float y : {1.1F, 2.1F, 3.1F}) {
            Segment(point(0, y), point(sign * 0.2F, y + 0.35F), 0.022F);
            Segment(point(sign * 0.2F, y + 0.35F), point(sign * 0.2F, y + 0.75F), 0.022F);
          }
      }
      for (const float z : {-4.0F, 2.0F})
        for (unsigned i = 0; i < 20; ++i) {
          const float a = math::kPi * i / 20, b = math::kPi * (i + 1) / 20;
          Segment({x, 4.0F + 3 * std::sin(a), z + 3 * std::cos(a)},
                  {x, 4.0F + 3 * std::sin(b), z + 3 * std::cos(b)}, 0.45F);
        }
    }
    finish(0);
    // Subdivided pennants hang from rigid top anchors; UV.y drives the existing
    // bounded vegetation bend, so the same wind toggle and pause clock govern the cloth.
    for (const auto location : {std::array{-4.8F, -3.62F}, std::array{6.85F, -0.8F}}) {
      constexpr unsigned columns = 8, rows = 16;
      const auto bannerBase = static_cast<std::uint16_t>(vertices.size());
      for (unsigned row = 0; row <= rows; ++row)
        for (unsigned col = 0; col <= columns; ++col) {
          const float u = static_cast<float>(col) / columns, v = static_cast<float>(row) / rows;
          const float taper = 1 - std::max(0.0F, (v - 0.75F) * 4);
          vertices.push_back({{location[0] + (u - 0.5F) * 1.0F * taper, 4.65F - v * 2.5F,
                               location[1] + 0.025F * std::sin(u * 4 * math::kPi)},
                              {0, 0, 1},
                              {u, v},
                              {1, 0, 0, -1}});
        }
      for (unsigned row = 0; row < rows; ++row)
        for (unsigned col = 0; col < columns; ++col) {
          const auto a = bannerBase + row * (columns + 1) + col, b = a + columns + 1;
          for (const auto index : {a, b, a + 1, a + 1, b, b + 1})
            indices.push_back(static_cast<std::uint16_t>(index));
        }
    }
    finish(15);
    for (const auto location : {std::array{-4.8F, -3.62F}, std::array{6.85F, -0.8F}}) {
      const auto stripe = [&](float u0, float v0, float u1, float v1, float halfWidth) {
        const float du = u1 - u0, dv = v1 - v0;
        const float length = std::sqrt(du * du + dv * dv);
        const float su = -dv / length * halfWidth, sv = du / length * halfWidth;
        const auto first = static_cast<std::uint16_t>(vertices.size());
        for (const auto uv : {std::array{u0 + su, v0 + sv}, std::array{u0 - su, v0 - sv},
                              std::array{u1 - su, v1 - sv}, std::array{u1 + su, v1 + sv}})
          vertices.push_back({{location[0] + uv[0] - 0.5F, 4.65F - uv[1] * 2.5F,
                               location[1] + 0.025F * std::sin(uv[0] * 4 * math::kPi) + 0.012F},
                              {0, 0, 1},
                              {uv[0], uv[1]},
                              {1, 0, 0, -1}});
        for (const auto index : {0, 2, 1, 0, 3, 2})
          indices.push_back(static_cast<std::uint16_t>(first + index));
      };
      for (const float scale : {1.0F, 0.6F}) {
        const std::array points{
            std::array{0.5F, 0.46F - 0.14F * scale}, std::array{0.5F + 0.22F * scale, 0.46F},
            std::array{0.5F, 0.46F + 0.14F * scale}, std::array{0.5F - 0.22F * scale, 0.46F}};
        for (std::size_t edge = 0; edge < points.size(); ++edge) {
          const auto a = points[edge], b = points[(edge + 1) % points.size()];
          stripe(a[0], a[1], b[0], b[1], 0.007F);
        }
      }
      stripe(0.5F, 0.25F, 0.5F, 0.67F, 0.008F);
      stripe(0.1F, 0.06F, 0.1F, 0.73F, 0.005F);
      stripe(0.9F, 0.06F, 0.9F, 0.73F, 0.005F);
    }
    finish(16);
    for (const float x : {-7.5F, 7.5F})
      for (unsigned i = 0; i < 32; ++i) {
        const float z = -7 + i * 0.4F;
        const float arc = std::fmod(z + 7, 6.0F) - 3;
        const float root = 3.7F + std::sqrt(std::max(0.0F, 9 - arc * arc));
        Segment({x, root, z}, {x, root - 1.4F, z}, 0.018F);
      }
    finish(10);
    for (const float x : {-7.5F, 7.5F})
      for (unsigned i = 0; i < 32; ++i) {
        const float z = -7 + i * 0.4F;
        const float arc = std::fmod(z + 7, 6.0F) - 3;
        const float root = 3.7F + std::sqrt(std::max(0.0F, 9 - arc * arc));
        for (unsigned leaf = 0; leaf < 3; ++leaf) {
          const float height = 0.65F + (i % 4) * 0.1F;
          LeafQuad({x + (leaf % 2) * 0.1F, root - leaf * 0.42F - height, z}, 0.3F, height,
                   i * 0.61F + leaf);
          for (std::size_t v = vertices.size() - 4; v < vertices.size(); ++v)
            vertices[v].uv[1] = 1 - vertices[v].uv[1];
        }
      }
    finish(5);
    // Place the left cypress in the wide camera's arch opening, retaining its shared wind.
    const auto treeLocation = [](float x, float z) {
      return x < 0 && z == -10 ? std::array{-15.0F, -6.0F} : std::array{x, z};
    };
    for (const float x : {-12.0F, 12.0F})
      for (const float z : {-22.0F, -10.0F, 2.0F, 14.0F}) {
        const auto location = treeLocation(x, z);
        Cube(location[0], 2, location[1], 0.12F, 2, 0.12F);
      }
    finish(10);
    // Layered cutout foliage replaces smooth cones; it shares leaf lighting and wind.
    for (const float x : {-12.0F, 12.0F})
      for (const float z : {-22.0F, -10.0F, 2.0F, 14.0F})
        for (unsigned layer = 0; layer < 18; ++layer) {
          const float y = 0.7F + layer * 0.29F;
          const auto location = treeLocation(x, z);
          const float crownRadius =
              (x < 0 && z == -10 ? 0.6F : 0.85F) * (1 - std::pow(layer / 18.0F, 1.35F));
          for (unsigned branch = 0; branch < 3; ++branch) {
            const float angle = layer * 2.399963F + branch * 2 * math::kPi / 3;
            LeafQuad({location[0] + std::cos(angle) * crownRadius * 0.22F, y,
                      location[1] + std::sin(angle) * crownRadius * 0.22F},
                     crownRadius, 0.7F, angle);
          }
        }
    finish(5);
    // Ground-cover patches and climbing ivy use the existing original leaf mask and GPU wind.
    for (unsigned patch = 0; patch < 180; ++patch) {
      const auto seed = patch * 747796405U + 2891336453U;
      const float x = static_cast<float>((seed >> 3) % 1500) * 0.01F - 7.5F;
      const float z = static_cast<float>((seed >> 15) % 1100) * 0.01F - 4.0F;
      if (x * x + z * z < 7.3F)
        continue; // Keep the device's stone tiers readable.
      for (unsigned leaf = 0; leaf < 6; ++leaf) {
        const float angle = patch * 2.399963F + leaf * 1.047198F;
        const float offset = 0.04F + leaf * 0.028F;
        LeafQuad({x + std::cos(angle) * offset, 0.15F, z + std::sin(angle) * offset},
                 0.12F + (patch % 4) * 0.015F, 0.2F + ((patch + leaf) % 5) * 0.045F, angle);
      }
    }
    for (const float x : {-5.5F, 5.5F})
      for (unsigned vine = 0; vine < 10; ++vine)
        for (unsigned leaf = 0; leaf < 4; ++leaf) {
          const float height = 0.3F + ((vine + leaf) % 3) * 0.05F;
          LeafQuad({x + (vine % 2 ? -0.42F : 0.42F), 2.3F - leaf * 0.32F - height,
                    1.5F + static_cast<float>(vine) * 0.07F},
                   0.2F, height, vine * 0.57F);
          for (std::size_t v = vertices.size() - 4; v < vertices.size(); ++v)
            vertices[v].uv[1] = 1 - vertices[v].uv[1];
        }
    finish(5);
    // Ivy follows the outer right-hand device rim; root anchors share the scene wind clock.
    for (unsigned vine = 0; vine < 9; ++vine) {
      const float angle = -0.45F + vine * 0.15F;
      const float rootX = 1.94F * std::cos(angle);
      const float rootY = 2.9F + 1.94F * std::sin(angle);
      for (unsigned leaf = 0; leaf < 8; ++leaf) {
        const float height = 0.32F + ((vine + leaf) % 3) * 0.035F;
        LeafQuad({rootX + 0.08F * std::sin(leaf * 1.7F), rootY - leaf * 0.17F - height,
                  0.33F + 0.04F * std::cos(leaf * 1.3F)},
                 0.18F, height, vine * 0.57F);
        for (std::size_t v = vertices.size() - 4; v < vertices.size(); ++v)
          vertices[v].uv[1] = 1 - vertices[v].uv[1];
      }
    }
    finish(5);
    // Dense foreground banks retain the original alpha mask and the same GPU wind.
    for (const auto bank :
         {std::array{-5.0F, 5.3F}, std::array{5.4F, 5.9F}, std::array{-7.8F, -1.8F}})
      for (unsigned sprig = 0; sprig < 96; ++sprig) {
        const float angle = sprig * 2.399963F;
        const float radius = 1.3F * std::sqrt((sprig + 0.5F) / 96);
        LeafQuad({bank[0] + radius * std::cos(angle), 0.15F + 0.25F * (1 - radius / 1.3F),
                  bank[1] + radius * std::sin(angle)},
                 0.22F + (sprig % 4) * 0.035F, 0.45F + (sprig % 5) * 0.08F, angle);
      }
    finish(5);
    for (const auto &prototype : masonry) {
      Cube(0, 0, 0, prototype.extent[0], prototype.extent[1], prototype.extent[2]);
      finish(prototype.material);
      batches.back().firstInstance = static_cast<std::uint32_t>(instances.size());
      batches.back().instanceCount = static_cast<std::uint32_t>(prototype.placements.size());
      instances.insert(instances.end(), prototype.placements.begin(), prototype.placements.end());
    }
    courtyardFoliageQuadCount = 0;
    for (const auto &batch : batches)
      if (batch.materialIndex == 5)
        courtyardFoliageQuadCount += batch.indexCount / 6;
    renderer::Mesh tangentSource;
    tangentSource.indices = indices;
    tangentSource.vertices.reserve(vertices.size());
    for (const auto &vertex : vertices)
      tangentSource.vertices.push_back(
          {{vertex.position[0], vertex.position[1], vertex.position[2]},
           {vertex.normal[0], vertex.normal[1], vertex.normal[2]},
           {vertex.uv[0], vertex.uv[1]}});
    const auto tangents = renderer::GenerateMeshTangents(tangentSource);
    if (!tangents)
      throw std::runtime_error("Courtyard tangent generation failed");
    for (std::size_t i = 0; i < vertices.size(); ++i)
      std::copy((*tangents)[i].begin(), (*tangents)[i].end(), vertices[i].tangent);
    courtyardInstances = instances;
    courtyardVertices = vertices;
    courtyardIndices = indices;
    courtyardBatches = batches;
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
  void CourtyardPaving(int cellX, int cellZ) {
    const auto seed = static_cast<std::uint32_t>(cellX + 11) * 73856093U ^
                      static_cast<std::uint32_t>(cellZ + 11) * 19349663U;
    const float x = cellX * 1.2F, z = cellZ * 1.2F;
    const float top = 0.13F + static_cast<float>(seed % 7) * 0.003F;
    const float halfX = 0.575F - static_cast<float>(seed % 5) * 0.005F;
    const float halfZ = 0.575F - static_cast<float>((seed >> 4) % 5) * 0.005F;
    constexpr float bevel = 0.035F;
    std::array<math::Vector3, 4> inner{}, outer{}, bottom{};
    const std::array corners{std::array{-1.0F, -1.0F}, std::array{-1.0F, 1.0F},
                             std::array{1.0F, 1.0F}, std::array{1.0F, -1.0F}};
    for (std::size_t i = 0; i < corners.size(); ++i) {
      const float chip = static_cast<float>((seed >> (i * 3)) % 5) * 0.006F;
      inner[i] = {x + corners[i][0] * (halfX - bevel - chip), top,
                  z + corners[i][1] * (halfZ - bevel - chip)};
      outer[i] = {x + corners[i][0] * halfX, top - bevel, z + corners[i][1] * halfZ};
      bottom[i] = {outer[i].x, 0, outer[i].z};
    }
    const float offsetU = static_cast<float>(seed % 13) / 13;
    const float offsetV = static_cast<float>((seed >> 8) % 13) / 13;
    const auto emit = [&](const std::array<math::Vector3, 4> &points) {
      const auto normal =
          math::NormalizeSafe(math::Cross(points[1] - points[0], points[2] - points[0]));
      const auto base = static_cast<std::uint16_t>(vertices.size());
      for (const auto &p : points)
        vertices.push_back({{p.x, p.y, p.z},
                            {normal.x, normal.y, normal.z},
                            {(p.x - x) / 1.2F + offsetU, (p.z - z) / 1.2F + offsetV}});
      for (const auto index : {0, 1, 2, 0, 2, 3})
        indices.push_back(static_cast<std::uint16_t>(base + index));
    };
    emit(inner);
    for (std::size_t i = 0; i < corners.size(); ++i) {
      const auto next = (i + 1) % corners.size();
      emit({inner[i], outer[i], outer[next], inner[next]});
      emit({outer[i], bottom[i], bottom[next], outer[next]});
    }
  }
  void CourtyardBlock(float x, float y, float z, float sx, float sy, float sz) {
    // A bevelled stone block has six faces, twelve edge strips and eight corner caps.
    const std::array<float, 3> extent{sx, sy, sz};
    const float bevel = std::min({sx, sy, sz, 0.6F}) * 0.18F;
    const auto emit = [&](std::vector<math::Vector3> points, math::Vector3 normal) {
      normal = math::NormalizeSafe(normal);
      if (math::Dot(math::Cross(points[1] - points[0], points[2] - points[0]), normal) < 0)
        std::reverse(points.begin(), points.end());
      const auto base = static_cast<std::uint16_t>(vertices.size());
      const unsigned axis =
          std::abs(normal.x) >= std::abs(normal.y) && std::abs(normal.x) >= std::abs(normal.z) ? 0
          : std::abs(normal.y) >= std::abs(normal.z)                                           ? 1
                                                                                               : 2;
      for (const auto point : points) {
        const std::array<float, 3> p{point.x, point.y, point.z};
        const unsigned u = (axis + 1) % 3, v = (axis + 2) % 3;
        vertices.push_back({{x + point.x, y + point.y, z + point.z},
                            {normal.x, normal.y, normal.z},
                            {0.5F + p[u] / (2 * extent[u]), 0.5F + p[v] / (2 * extent[v])}});
      }
      for (unsigned i = 1; i + 1 < points.size(); ++i)
        for (const auto index : {0U, i, i + 1})
          indices.push_back(static_cast<std::uint16_t>(base + index));
    };
    for (unsigned axis = 0; axis < 3; ++axis)
      for (const float sign : {-1.0F, 1.0F}) {
        std::vector<math::Vector3> points;
        for (const auto corner :
             {std::array{-1, -1}, std::array{1, -1}, std::array{1, 1}, std::array{-1, 1}}) {
          std::array<float, 3> p{};
          p[axis] = sign * extent[axis];
          p[(axis + 1) % 3] = corner[0] * (extent[(axis + 1) % 3] - bevel);
          p[(axis + 2) % 3] = corner[1] * (extent[(axis + 2) % 3] - bevel);
          points.push_back({p[0], p[1], p[2]});
        }
        math::Vector3 normal{};
        if (axis == 0)
          normal.x = sign;
        else if (axis == 1)
          normal.y = sign;
        else
          normal.z = sign;
        emit(points, normal);
      }
    for (unsigned axis = 0; axis < 3; ++axis)
      for (const float a : {-1.0F, 1.0F})
        for (const float b : {-1.0F, 1.0F}) {
          const unsigned u = (axis + 1) % 3, v = (axis + 2) % 3;
          std::vector<math::Vector3> points;
          for (const auto corner :
               {std::array{0, -1}, std::array{1, -1}, std::array{1, 1}, std::array{0, 1}}) {
            std::array<float, 3> p{};
            p[axis] = corner[1] * (extent[axis] - bevel);
            p[u] = a * (extent[u] - (corner[0] ? bevel : 0));
            p[v] = b * (extent[v] - (corner[0] ? 0 : bevel));
            points.push_back({p[0], p[1], p[2]});
          }
          std::array<float, 3> n{};
          n[u] = a;
          n[v] = b;
          emit(points, {n[0], n[1], n[2]});
        }
    for (const float a : {-1.0F, 1.0F})
      for (const float b : {-1.0F, 1.0F})
        for (const float c : {-1.0F, 1.0F}) {
          const std::array<float, 3> sign{a, b, c};
          std::vector<math::Vector3> points;
          for (unsigned axis = 0; axis < 3; ++axis) {
            std::array<float, 3> p{};
            for (unsigned i = 0; i < 3; ++i)
              p[i] = sign[i] * (extent[i] - (i == axis ? 0 : bevel));
            points.push_back({p[0], p[1], p[2]});
          }
          emit(points, {a, b, c});
        }
  }
  void Cube(float x, float y, float z, float sx, float sy, float sz) {
    if (selected == "courtyard") {
      CourtyardBlock(x, y, z, sx, sy, sz);
      return;
    }
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
        vertices.push_back(
            {{point.x, point.y, point.z},
             {normal.x, normal.y, normal.z},
             {static_cast<float>(side) / sides,
              center.x == start.x && center.y == start.y && center.z == start.z ? 0.0F : 1.0F}});
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
    if (selected == "courtyard")
      lines.insert(lines.end(),
                   {"Ruins courtyard", "B cycles wide / material / motion framing; F4 hides UI",
                    "P compares Lambert; O toggles environment lighting",
                    "Material / light / wind comparisons available"});
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
  state_->visualTour = tour && scene == "courtyard";
  if (scene == "courtyard") {
    state_->overview = state_->profiler = false;
    if (tour)
      ReplayTour();
  }
}
RoomSession::~RoomSession() = default;
void RoomSession::SetScreenshotMode(bool enabled) { state_->screenshotMode = enabled; }
void RoomSession::Select(std::string_view room) { state_->SelectRoom(room); }
std::string_view RoomSession::Selected() const { return state_->selected; }
void RoomSession::ReplayTour() {
  state_->tour = true;
  state_->paused = false;
  state_->tourSeconds = 0;
  state_->tourStep = 0;
  if (state_->selected == "courtyard" || state_->visualTour) {
    state_->visualTour = true;
    state_->courtyardPaused = false;
    state_->courtyardSeconds = 0;
    state_->courtyardActive = false;
    state_->courtyardPbr = state_->courtyardIbl = state_->courtyardShadows = true;
    state_->courtyardBloom = state_->courtyardStyled = state_->courtyardFocus =
        state_->courtyardReflections = state_->courtyardAtmosphere = state_->courtyardRefraction =
            state_->courtyardCrystalLight = true;
    state_->courtyardWind = state_->courtyardTransmission = state_->courtyardTransparency = true;
    state_->courtyardExposure = 1;
    state_->courtyardShadowBias = 0.0008F;
    Select("courtyard");
    state_->CourtyardCamera(0);
  } else
    Select("hub");
}
bool RoomSession::TourComplete() const noexcept {
  return state_->tour && state_->paused && state_->tourSeconds >= (state_->visualTour ? 100 : 210);
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
      s.pitch = std::clamp(s.pitch + (event.value1 - s.pointerY) * 0.005F,
                           s.courtyardFreeCamera ? -1.2F : 0.1F, 1.2F);
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
    s.tour = s.visualTour = false;
    Select(rooms[static_cast<std::size_t>(key) - static_cast<std::size_t>(Key::Digit1)]);
  }
  if (key == Key::Digit9) {
    s.tour = s.visualTour = false;
    Select("courtyard");
  }
  if (s.selected == "courtyard" && key == Key::C) {
    s.tour = false;
    s.visualTour = false;
    s.courtyardFreeCamera = !s.courtyardFreeCamera;
    if (s.courtyardFreeCamera)
      s.freeEye = {s.radius * std::sin(s.yaw) * std::cos(s.pitch), s.radius * std::sin(s.pitch),
                   s.radius * std::cos(s.yaw) * std::cos(s.pitch)};
  }
  if (s.selected == "courtyard" && key == Key::Q)
    SetQuality(
        std::array<std::string_view, 3>{"basic", "standard", "high"}[(s.courtyardQuality + 1) % 3]);
  if (s.selected == "courtyard" && key == Key::H)
    s.courtyardCompare = !s.courtyardCompare;
  if (s.selected == "courtyard" && !s.tour && key == Key::Space)
    s.courtyardPaused = !s.courtyardPaused;
  if (s.selected == "courtyard" && key == Key::Enter)
    s.courtyardActive = !s.courtyardActive;
  if (s.selected == "courtyard" && key == Key::N)
    s.courtyardWind = !s.courtyardWind;
  if (s.selected == "courtyard" && key == Key::U)
    s.courtyardTransparency = !s.courtyardTransparency;
  if (s.selected == "courtyard" && key == Key::M)
    s.courtyardTransmission = !s.courtyardTransmission;
  if (s.selected == "courtyard" && key == Key::B) {
    s.tour = s.visualTour = false;
    s.CourtyardCamera(s.courtyardShot + 1);
  }
  if (s.selected == "courtyard" && key == Key::P) {
    s.courtyardPbr = !s.courtyardPbr;
    s.lastAction = s.courtyardPbr ? "PBR materials" : "Lambert material comparison";
  }
  if (s.selected == "courtyard" && key == Key::J)
    s.courtyardFocus = !s.courtyardFocus;
  if (s.selected == "courtyard" && key == Key::V)
    s.courtyardReflections = !s.courtyardReflections;
  if (s.selected == "courtyard" && key == Key::F9)
    s.courtyardCrystalLight = !s.courtyardCrystalLight;
  if (s.selected == "courtyard" && key == Key::F8)
    s.courtyardRefraction = !s.courtyardRefraction;
  if (s.selected == "courtyard" && key == Key::F7)
    s.courtyardAtmosphere = !s.courtyardAtmosphere;
  if (s.selected == "courtyard" && key == Key::K)
    s.courtyardBloom = !s.courtyardBloom;
  if (s.selected == "courtyard" && key == Key::F6) {
    s.courtyardShadows = !s.courtyardShadows;
    s.lastAction = s.courtyardShadows ? "Directional shadows on" : "Shadows off";
  }
  if (s.selected == "courtyard" && key == Key::G) {
    s.courtyardStyled = !s.courtyardStyled;
    s.lastAction = s.courtyardStyled ? "Stylized light on" : "Neutral light comparison";
  }
  if (s.selected == "courtyard" && key == Key::LeftBracket)
    s.courtyardShadowBias = std::max(0.0001F, s.courtyardShadowBias * 0.5F);
  if (s.selected == "courtyard" && key == Key::RightBracket)
    s.courtyardShadowBias = std::min(0.0128F, s.courtyardShadowBias * 2.0F);
  if (s.selected == "courtyard" && key == Key::E) {
    s.courtyardExposure = s.courtyardExposure == 1.0F ? 0.25F : 1.0F;
    s.lastAction = s.courtyardExposure == 1.0F ? "Exposure normal" : "Exposure highlight detail";
  }
  if (s.selected == "courtyard" && key == Key::O) {
    s.courtyardIbl = !s.courtyardIbl;
    s.lastAction = s.courtyardIbl ? "Environment lighting on" : "Direct-light comparison";
  }
  if (key == Key::F4)
    s.screenshotMode = !s.screenshotMode;
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
    else if (s.selected == "courtyard" && !s.matrix) {
      s.courtyardSeconds = 0;
      s.CourtyardCamera(0);
    } else
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
  if (key == Key::V && s.selected != "courtyard") {
    s.video.Seek(s.clock);
    s.lastAction = "Video seek invalidated queue";
  }
#endif
#if NEXORA_PLATFORM_ENABLED
  if (key == Key::B && s.selected != "courtyard") {
    s.platform.Transition(s.platform.State() == platform::AppState::Foreground
                              ? platform::AppState::Background
                              : platform::AppState::Foreground);
    s.lastAction = "Lifecycle transition";
  }
  if (key == Key::H && s.selected != "courtyard") {
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
  if (s.selected == "courtyard" && !s.courtyardPaused && !s.visualTour)
    s.courtyardSeconds = std::fmod(s.courtyardSeconds + seconds, 3600.0);
  if (s.tour && !s.paused) {
    if (s.visualTour) {
      s.tourSeconds = std::min(100.0, s.tourSeconds + seconds);
      s.courtyardSeconds = s.tourSeconds;
      s.VisualTourCamera();
      if (s.tourSeconds >= 100)
        s.paused = true;
    } else {
      s.tourSeconds += seconds;
      s.tourStep = std::min<std::size_t>(6, static_cast<std::size_t>(s.tourSeconds / 30));
      s.SelectRoom(tourRooms[s.tourStep]);
      if (s.tourSeconds >= 210)
        s.paused = true;
    }
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
  if (s.selected == "courtyard" && s.courtyardFreeCamera) {
    const math::Vector3 forward{-std::sin(s.yaw) * std::cos(s.pitch), -std::sin(s.pitch),
                                -std::cos(s.yaw) * std::cos(s.pitch)};
    const math::Vector3 right{std::cos(s.yaw), 0, -std::sin(s.yaw)};
    s.freeEye = s.freeEye + (right * axis("move-x") - forward * axis("move-z")) *
                                static_cast<float>(seconds) * 3;
    if (s.held.contains(Key::UpArrow))
      s.freeEye.y += static_cast<float>(seconds) * 3;
    if (s.held.contains(Key::DownArrow))
      s.freeEye.y -= static_cast<float>(seconds) * 3;
    s.freeEye.x = std::clamp(s.freeEye.x, -50.0F, 50.0F);
    s.freeEye.y = std::clamp(s.freeEye.y, 0.2F, 50.0F);
    s.freeEye.z = std::clamp(s.freeEye.z, -50.0F, 50.0F);
  } else if (s.selected != "gameplay" && !(s.tour && s.visualTour)) {
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

void RoomSession::SetQuality(std::string_view quality) {
  const auto tier = quality == "basic"      ? 0U
                    : quality == "standard" ? 1U
                    : quality == "high"     ? 2U
                                            : 3U;
  if (tier == 3)
    throw std::invalid_argument("Unknown showcase quality");
  if (tier == state_->courtyardQuality)
    return;
  state_->courtyardQuality = tier;
  // Scene spans are borrowed only for a draw; invalidate geometry between draws.
  state_->courtyardVertices.clear();
  state_->courtyardInstances.clear();
  state_->courtyardIndices.clear();
  state_->courtyardBatches.clear();
}
std::string_view RoomSession::QualityName() const noexcept {
  return std::array<std::string_view, 3>{"basic", "standard", "high"}[state_->courtyardQuality];
}
void RoomSession::SetAnimationPaused(bool paused) {
  state_->courtyardPaused = paused;
  if (state_->visualTour)
    state_->paused = paused;
}
void RoomSession::SetDeviceActive(bool active) { state_->courtyardActive = active; }

Nexora::Presentation::SceneDrawData RoomSession::Scene(std::uint32_t width, std::uint32_t height) {
  auto &s = *state_;
  s.visualized.insert(s.selected);
  s.vertices.clear();
  s.indices.clear();
  s.instances.clear();
  s.sceneUploads.clear();
  s.linearSceneUploads.clear();
  s.materials.clear();
  s.batches.clear();
  s.Cube(0, -0.3F, 0, 6, 0.3F, 6);
  if (s.selected == "courtyard") {
    s.materials = {{{0.95F, 0.95F, 0.95F, 1}, 0},
                   {{0.78F, 0.44F, 0.12F, 1}, 0},
                   {{0.08F, 0.8F, 0.95F, 1}, 0},
                   {{0.3F, 0.21F, 0.13F, 1}, 0},
                   {{1, 1, 1, 1}, 0},
                   {{0.7F, 0.8F, 0.65F, 1}, 0}};
#if NEXORA_ASSET_PIPELINE_ENABLED
    s.materials[4].textureId = 2;
#endif
    s.materials[0].roughness = 0.85F;
    s.materials[0].normalScale = 0.2F;
    s.materials[0].worldTextureScale = s.courtyardPbr ? 1.1F : 0;
#if NEXORA_ASSET_PIPELINE_ENABLED
    s.materials[0].textureId = 10;
    s.materials[0].normalTextureId = 11;
    s.materials[0].ormTextureId = 12;
    s.materials[1].textureId = 13;
    s.materials[1].normalTextureId = 14;
    s.materials[1].ormTextureId = 15;
#endif
    s.materials[4].baseColor = {0.52F, 0.44F, 0.31F, 1};
    s.materials[1].metallic = 1;
    s.materials[1].roughness = 0.28F;
    s.materials[1].baseColor = {0.95F, 0.85F, 0.65F, 1};
    s.materials[2].roughness = 0.15F;
    s.materials[2].emission = {0.05F, 2.2F, 3.0F};
    s.materials[3].baseColor = {0.58F, 0.4F, 0.24F, 1};
    s.materials[3].roughness = 0.6F;
    s.materials[4].roughness = 0.8F;
    s.materials[5].twoSidedLighting = s.courtyardPbr;
    s.materials[5].roughness = 0.7F;
    s.materials[5].emission = {0, 0, 0};
#if NEXORA_ASSET_PIPELINE_ENABLED
    s.materials[5].emissionTextureId = 16;
#endif
    if (s.courtyardPbr) {
      s.materials[5].alphaCutoff = 0.5F;
      s.materials[5].windAmplitude = s.courtyardWind ? 0.22F : 0;
      s.materials[5].transmissionThickness = s.courtyardTransmission ? 0.12F : 0;
      s.materials[5].transmissionColor = {0.25F, 0.45F, 0.12F};
    }
#if NEXORA_ASSET_PIPELINE_ENABLED
    s.materials[5].textureId = 16;
#endif
    const float pulse =
        s.courtyardActive ? 1.0F + 0.35F * std::sin(static_cast<float>(s.courtyardSeconds) * 2) : 1;
    for (auto &value : s.materials[2].emission)
      value *= pulse;
    Nexora::Presentation::SceneMaterial sky{};
    sky.baseColor = {0, 0, 0, 1};
    sky.metallic = sky.roughness = 1;
    sky.emission = {0.35F, 0.5F, 0.65F};
    sky.unlit = s.courtyardPbr;
    sky.castsShadow = false;
#if NEXORA_ASSET_PIPELINE_ENABLED
    sky.emission = {1, 1, 1};
    sky.emissionTextureId = 18;
#endif
    s.materials.push_back(sky);
    Nexora::Presentation::SceneMaterial motes{};
    motes.baseColor = {0, 0, 0, 1};
    motes.emission = {0.1F, 3, 4};
    motes.unlit = s.courtyardPbr;
    motes.castsShadow = false;
    if (s.courtyardPbr)
      motes.alphaCutoff = 0.5F;
#if NEXORA_ASSET_PIPELINE_ENABLED
    motes.textureId = 17;
#endif
    s.materials.push_back(motes);
    for (const auto color :
         {std::array{0.42F, 0.38F, 0.25F, 1.0F}, std::array{0.36F, 0.36F, 0.32F, 1.0F},
          std::array{0.09F, 0.18F, 0.07F, 1.0F}}) {
      Nexora::Presentation::SceneMaterial background{};
      background.baseColor = color;
      background.roughness = 0.95F;
      background.castsShadow = false;
      s.materials.push_back(background);
    }
#if NEXORA_ASSET_PIPELINE_ENABLED
    s.materials[8].worldTextureScale = s.courtyardPbr ? 1.1F : 0;
    s.materials[8].normalScale = 0.14F;
    s.materials[8].textureId = 10;
    s.materials[8].normalTextureId = 11;
    s.materials[8].ormTextureId = 12;
    s.materials[9].textureId = 10;
    s.materials[9].normalTextureId = 11;
    s.materials[9].ormTextureId = 12;
    s.materials[9].worldTextureScale = s.courtyardPbr ? 0.05F : 0;
    s.materials[9].normalScale = 0.08F;
#endif
    Nexora::Presentation::SceneMaterial sun{};
    sun.baseColor = {0, 0, 0, 1};
    sun.emission = courtyard_hero::sun_radiance;
    sun.unlit = s.courtyardPbr;
    sun.castsShadow = false;
    s.materials.push_back(sun);
    auto crystal = s.materials[2];
    crystal.baseColor = {0.012F, 0.09F, 0.11F, 1};
    crystal.emission = {0.003F, 0.025F * pulse, 0.04F * pulse};
    // Bounded thin-surface back lighting keeps the crystalline facets readable in the sunset.
    if (s.courtyardPbr) {
      crystal.transmissionThickness = s.courtyardTransmission ? 0.12F : 0;
      crystal.transmissionColor = {0.02F, 0.35F, 0.45F};
    }
    crystal.castsShadow = false;
    crystal.opacity = s.courtyardPbr && s.courtyardTransparency ? 0.23F : 1.0F;
    if (crystal.opacity < 1 && s.courtyardQuality > 0 && s.courtyardRefraction) {
      crystal.refractionIndex = 1.46F;
      crystal.refractionThickness = 0.65F;
      crystal.refractionFrontSurfaceOnly = true;
    }
    crystal.transparencyTint = {0.28F, 0.92F, 0.98F};
    crystal.metallic = 0.0F;
    crystal.roughness = 0.06F;
    s.materials.push_back(crystal);
    Nexora::Presentation::SceneMaterial water{};
    water.baseColor = {0.28F, 0.4F, 0.42F, 1};
    water.metallic = 1;
    water.roughness = 0.07F;
    water.castsShadow = false;
    s.materials.push_back(water);
    Nexora::Presentation::SceneMaterial waterfall{};
    waterfall.baseColor = {0.65F, 0.8F, 0.9F, 1};
    waterfall.roughness = 0.2F;
    waterfall.emission = {0.1F, 0.15F, 0.2F};
    waterfall.castsShadow = false;
    waterfall.transmissionThickness = s.courtyardPbr && s.courtyardTransmission ? 0.25F : 0;
    waterfall.transmissionColor = {0.65F, 0.8F, 0.9F};
    s.materials.push_back(waterfall);
    Nexora::Presentation::SceneMaterial cloth{};
    cloth.baseColor = {0.025F, 0.19F, 0.23F, 1};
    cloth.twoSidedLighting = s.courtyardPbr;
    cloth.roughness = 0.95F;
    cloth.windAmplitude = s.courtyardPbr && s.courtyardWind ? 0.12F : 0;
    s.materials.push_back(cloth);
    auto clothTrim = cloth;
    clothTrim.baseColor = {0.6F, 0.38F, 0.12F, 1};
    s.materials.push_back(clothTrim);
    auto ceramicPaint = clothTrim;
    ceramicPaint.windAmplitude = 0;
    ceramicPaint.baseColor = {0.12F, 0.25F, 0.29F, 1};
    s.materials.push_back(ceramicPaint);
    for (const auto radiance : {std::array{0.02F, 0.08F, 0.1F}, std::array{0.06F, 0.23F, 0.25F},
                                std::array{0.4F, 1.4F, 1.6F}}) {
      auto interior = crystal;
      interior.opacity = 1;
      interior.transparencyTint = {1, 1, 1};
      interior.refractionIndex = 1;
      interior.refractionThickness = 0;
      interior.refractionFrontSurfaceOnly = false;
      interior.transmissionThickness = 0;
      interior.baseColor = {0.04F, 0.25F, 0.28F, 1};
      interior.metallic = 0.35F;
      interior.roughness = 0.13F;
      for (unsigned channel = 0; channel < 3; ++channel)
        interior.emission[channel] = radiance[channel] * (s.courtyardActive ? pulse : 0.25F);
      s.materials.push_back(interior);
    }
    s.CourtyardGeometry();
    // Animate from the immutable cache each frame; pause/replay never accumulates drift.
    const float crystalAngle = static_cast<float>(s.courtyardSeconds) * 0.18F;
    const float crystalLift = 0.12F * std::sin(static_cast<float>(s.courtyardSeconds) * 1.4F);
    for (std::size_t i = s.courtyardCrystalFirst; i < s.courtyardCrystalEnd; ++i) {
      auto &v = s.vertices[i];
      for (auto *vector : {v.position, v.normal, v.tangent}) {
        const float x = vector[0], z = vector[2];
        vector[0] = std::cos(crystalAngle) * x - std::sin(crystalAngle) * z;
        vector[2] = std::sin(crystalAngle) * x + std::cos(crystalAngle) * z;
      }
      v.position[1] += crystalLift;
    }
  } else if (s.selected == "hub") {
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
  math::Vector3 eye{s.radius * std::sin(s.yaw) * std::cos(s.pitch), s.radius * std::sin(s.pitch),
                    s.radius * std::cos(s.yaw) * std::cos(s.pitch)};
  math::Vector3 target{s.selected == "courtyard" ? -1.65F : 0.0F,
                       s.selected == "courtyard" ? 1.7F : 0.8F, 0};
  if (s.selected == "courtyard" && s.courtyardFreeCamera) {
    eye = s.freeEye;
    target = eye + math::Vector3{-std::sin(s.yaw) * std::cos(s.pitch), -std::sin(s.pitch),
                                 -std::cos(s.yaw) * std::cos(s.pitch)};
  }
  if (s.selected == "courtyard") {
    s.CourtyardSplinters();
    s.CourtyardWaterfalls();
    if (!s.courtyardPbr || !s.courtyardReflections || s.courtyardQuality == 0)
      s.CourtyardWater();
    s.CourtyardSkybox(eye);
    s.CourtyardParticles();
    if (s.courtyardPbr && s.courtyardReflections && s.courtyardQuality != 0)
      s.CourtyardReflectionGeometry();
  }
  const auto mvp = math::PerspectiveRadians(0.85F, height ? static_cast<float>(width) / height : 1,
                                            0.1F, s.selected == "courtyard" ? 300.0F : 100.0F) *
                   math::LookAt(eye, target);
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
  if (s.selected == "courtyard") {
    data.pbr = s.courtyardPbr;
    data.vegetationTime = static_cast<float>(s.courtyardSeconds);
    data.hdr = data.pbr;
    data.exposure = s.courtyardExposure;
    data.offscreen = data.hdr;
    if (data.hdr && s.courtyardReflections && s.courtyardQuality != 0)
      data.planarReflection = s.CourtyardReflectionSettings();
    if (data.hdr && s.courtyardAtmosphere && s.courtyardQuality != 0)
      data.atmosphere = Nexora::Presentation::SceneAtmosphere{{0.55F, 0.48F, 0.46F}, 0.6F, 18, 58};
    if (data.hdr && s.courtyardStyled)
      data.colorGrade = Nexora::Presentation::SceneColorGrade{1.05F, 1.05F};
    if (data.hdr && s.courtyardBloom && s.courtyardQuality != 0)
      data.bloom = Nexora::Presentation::SceneBloom{s.courtyardQuality == 2 ? 0.28F : 0.22F, 1.0F,
                                                    s.courtyardQuality == 2 ? 20.0F : 12.0F};
    if (data.hdr && s.courtyardFocus && s.courtyardQuality != 0)
      data.depthOfField = Nexora::Presentation::SceneDepthOfField{
          math::Length(eye - math::Vector3{0, 2.9F, 0}), s.courtyardQuality == 2 ? 0.8F : 0.65F,
          s.courtyardQuality == 2 ? 8.0F : 6.0F};
    data.cameraPosition = {eye.x, eye.y, eye.z};
    if (data.hdr && s.courtyardQuality != 0 && s.courtyardActive && s.courtyardCrystalLight) {
      const float time = static_cast<float>(s.courtyardSeconds);
      const float pulse = 0.9F + 0.1F * std::sin(time * 1.7F);
      data.pointLight =
          Nexora::Presentation::ScenePointLight{{0, 2.9F + 0.12F * std::sin(time * 1.4F), 0},
                                                {0.3F * pulse, 8.0F * pulse, 12.0F * pulse},
                                                4.5F};
    }
    if (data.pbr) {
      for (unsigned axis = 0; axis < 3; ++axis) {
        data.light_direction[axis] = -courtyard_hero::sun_direction[axis];
        data.light_color[axis] = courtyard_hero::key_radiance[axis];
      }
      if (s.courtyardShadows) {
        const auto light =
            math::Orthographic(-7, 7, -7, 7, 0.1F, 40) *
            math::LookAt({courtyard_hero::sun_direction[0], courtyard_hero::sun_direction[1] + 1,
                          courtyard_hero::sun_direction[2]},
                         {0, 1, 0});
        data.shadow = Nexora::Presentation::SceneDirectionalShadow{};
        data.shadow->resolution = 512U << s.courtyardQuality;
        data.shadow->lightViewProjection = light.values;
        data.shadow->normalBias = s.courtyardShadowBias;
        data.shadow->slopeBias = s.courtyardShadowBias * 2;
      }
      if (s.courtyardStyled)
        data.lightingStyle = Nexora::Presentation::SceneLightingStyle{};
    }
    data.materials = s.materials;
    data.batches = s.batches;
#if NEXORA_ASSET_PIPELINE_ENABLED
    s.sceneUploads.push_back({2, 64, 64, 256, s.courtyardAtlas});
    for (std::size_t i = 0; i < s.courtyardDetail.size(); ++i)
      s.sceneUploads.push_back({10 + i,
                                i == 8  ? 384U
                                : i < 7 ? 256U
                                        : 64U,
                                i == 8 || i < 7 ? 256U : 64U,
                                i == 8  ? 1536U
                                : i < 7 ? 1024U
                                        : 256U,
                                s.courtyardDetail[i]});
    data.textureId = 2;
    data.textureUploads = s.sceneUploads;
    if (data.pbr && s.courtyardIbl && s.courtyardQuality != 0) {
      s.linearSceneUploads = {{3, 16, 8, 1, s.courtyardEnvironment[0]},
                              {4, 64, 32, 7, s.courtyardEnvironment[1]},
                              {5, 32, 32, 1, s.courtyardEnvironment[2]}};
      data.linearTextureUploads = s.linearSceneUploads;
      data.environment = Nexora::Presentation::SceneEnvironment{3, 4, 5, 1.1F, 0.0F, 7};
    }
#endif
    data.base_color[0] = 0.72F;
    data.base_color[1] = 0.57F;
    data.base_color[2] = 0.38F;
  }
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
  if (s.screenshotMode)
    return {};
  if (s.selected == "courtyard" && !s.overview && !s.profiler && !s.matrix) {
    s.Rect(18, 642, 1244, 60, 0xde241a10);
    s.Text(30, 652,
           "B Shots / C Explore / T Tour / Space Pause / Enter Activate / H Compare / F4 Photo",
           0xffe9ded4, 1.3F);
    const std::string viewing = s.tour ? "Courtyard tour " + Number(s.tourSeconds) + " / 100 s"
                                : s.courtyardFreeCamera ? "WASD Move / Drag Look / Arrows Up-Down"
                                                        : "Drag Orbit / Wheel Zoom";
    s.Text(30, 678, viewing + " / Q Quality: " + std::string(QualityName()), 0xffefdc80, 1.3F);
    if (s.courtyardCompare) {
      s.Rect(18, 531, 900, 100, 0xde241a10);
      s.Text(
          30, 544,
          "P Materials / O Environment / F6 Shadows / F7 Haze / F8 Refraction / F9 Crystal light",
          0xffe9ded4, 1.4F);
      s.Text(30, 573, "J Focus / V Reflection / U Crystal / K Glow / N Wind / M Backlight",
             0xffe9ded4, 1.4F);
      s.Text(30, 602, "Q Quality / Pause for comparisons / R Replay / F1-F3 Details", 0xffefdc80,
             1.2F);
    }
  } else {
    s.Rect(0, 0, 1280, 112, 0xf0271a10);
    s.Text(18, 14, s.localization.Resolve("title"), 0xffefdc80, 3);
    s.Text(18, 45,
           "9 courtyard / B shot / P material / O IBL / E exposure / F6 shadows / F7 haze / F8 "
           "refraction / F9 crystal light / G tone "
           "/ [ ] bias "
           "/ F4 hide UI / F1 overview / F2 "
           "profiler / F3 matrix");
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
      << ",\"tour\":{\"enabled\":" << s.tour << ",\"paused\":" << s.paused << ",\"kind\":\""
      << (s.visualTour ? "visual" : "engineering")
      << "\",\"duration_seconds\":" << (s.visualTour ? 100 : 210)
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
  out << "],\"courtyard\":{\"stage\":\"living_scene_in_progress\",\"shot\":" << s.courtyardShot
      << ",\"shading\":\""
      << (s.courtyardPbr
              ? (NEXORA_ASSET_PIPELINE_ENABLED && s.courtyardIbl && s.courtyardQuality != 0
                     ? "shared_pbr_ibl"
                     : "shared_pbr_direct")
              : "lambert")
      << "\""
      << ",\"scene_color_format\":\"" << (s.courtyardPbr ? "RGBA16F" : "RGBA8") << "\""
      << ",\"exposure\":" << s.courtyardExposure << ",\"camera_mode\":\""
      << (s.visualTour            ? "tour"
          : s.courtyardFreeCamera ? "free"
                                  : "orbit")
      << "\""
      << ",\"comparison_menu\":" << s.courtyardCompare
      << ",\"animation_paused\":" << (s.visualTour ? s.paused : s.courtyardPaused)
      << ",\"animation_seconds\":" << s.courtyardSeconds
      << ",\"wind_enabled\":" << (s.courtyardPbr && s.courtyardWind)
      << ",\"transmission_enabled\":" << (s.courtyardPbr && s.courtyardTransmission)
      << ",\"atmosphere_enabled\":"
      << (s.courtyardPbr && s.courtyardAtmosphere && s.courtyardQuality != 0)
      << ",\"crystal_light_enabled\":" << (s.courtyardCrystalLight ? "true" : "false")
      << ",\"refraction_enabled\":"
      << (s.courtyardPbr && s.courtyardTransparency && s.courtyardRefraction &&
          s.courtyardQuality != 0)
      << ",\"transparency_enabled\":" << (s.courtyardPbr && s.courtyardTransparency)
      << ",\"device_active\":" << s.courtyardActive
      << ",\"geometry_vertex_count\":" << s.vertices.size()
      << ",\"geometry_index_count\":" << s.indices.size()
      << ",\"geometry_batch_count\":" << s.batches.size()
      << ",\"geometry_material_count\":" << s.materials.size()
      << ",\"particle_budget\":" << (24U << s.courtyardQuality)
      << ",\"particle_count\":" << s.courtyardParticleCount
      << ",\"foliage_quad_count\":" << s.courtyardFoliageQuadCount << ",\"quality\":\""
      << QualityName() << "\""
      << ",\"skybox_face_count\":6,\"skybox_camera_centered\":true,\"background_environment\":true"
      << ",\"water_surface_count\":2,\"waterfall_count\":2,\"crystal_animation\":true"
      << ",\"shadow_resolution\":"
      << (s.courtyardPbr && s.courtyardShadows ? (512U << s.courtyardQuality) : 0)
      << ",\"depth_of_field_enabled\":"
      << (s.courtyardPbr && s.courtyardFocus && s.courtyardQuality != 0)
      << ",\"planar_reflection_enabled\":"
      << (s.courtyardPbr && s.courtyardReflections && s.courtyardQuality != 0)
      << ",\"bloom_enabled\":" << (s.courtyardPbr && s.courtyardBloom && s.courtyardQuality != 0)
      << ",\"shadows_enabled\":" << (s.courtyardPbr && s.courtyardShadows)
      << ",\"stylized_enabled\":" << (s.courtyardPbr && s.courtyardStyled)
      << ",\"shadow_bias\":" << s.courtyardShadowBias << ",\"screenshot_mode\":" << s.screenshotMode
#if NEXORA_ASSET_PIPELINE_ENABLED
      << ",\"representative_asset_loaded\":" << !s.assetMesh.vertices.empty()
      << ",\"asset_hash\":\"" << s.assetHash << "\""
      << ",\"hero_asset_loaded\":" << !s.courtyardCrystal.vertices.empty()
      << ",\"hero_detail_map_count\":6,\"hero_mask_count\":2,\"hero_sky_map_count\":1,\"hero_"
         "hashes\":[";
  for (std::size_t i = 0; i < s.courtyardHeroHashes.size(); ++i) {
    if (i)
      out << ',';
    out << '\"' << s.courtyardHeroHashes[i] << '\"';
  }
  out << "]"
      << ",\"adopted_mesh_count\":3,\"adopted_texture_count\":1,\"adopted_hashes\":[\""
      << s.courtyardHashes[0] << "\",\"" << s.courtyardHashes[1] << "\",\"" << s.courtyardHashes[2]
      << "\",\"" << s.courtyardHashes[3] << "\"]"
      << ",\"environment_loaded\":true,\"environment_enabled\":"
      << (s.courtyardPbr && s.courtyardIbl && s.courtyardQuality != 0)
      << ",\"environment_metadata_hash\":\"" << s.environmentMetadataHash << "\""
      << ",\"environment_hashes\":[\"" << s.environmentHashes[0] << "\",\""
      << s.environmentHashes[1] << "\",\"" << s.environmentHashes[2] << "\"]"
#else
      << ",\"representative_asset_loaded\":false"
#endif
      << "},\"integration_probes\":" << SerializeJson(s.probes) << "}";
  return out.str();
}
} // namespace nexora::showcase
