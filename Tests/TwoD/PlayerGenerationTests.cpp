#include "GenerationHost.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/GameplayContent.h"
#include "PlayerGenerationNative.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

namespace {
using namespace nexora;
using playerreload::Require;
using Path = std::filesystem::path;
std::string Read(const Path &path) {
  std::ifstream file(path, std::ios::binary);
  Require(file.good(), "Player generation guard file missing");
  return {std::istreambuf_iterator<char>(file), {}};
}
void Register(twod::GameplayContent &content, bool repack = false) {
  NexoraGameplayContentApiV1 api{};
  api.struct_size = sizeof(api);
  NexoraGameplaySpriteContentApiV1 sources{};
  sources.struct_size = sizeof(sources);
  Require(content.GetApi(&api) == 0 && content.GetSpriteApi(&sources) == 0,
          "Player generation content tables");
  twod::Image image{repack ? 73U : 901U, 2, 2, twod::Bytes(16, std::byte{255})};
  const auto atlas = twod::PackAtlas(repack ? twod::AssetId{18, 19} : twod::AssetId{8, 9},
                                     std::span(&image, 1), 8, 8, true);
  Require(bool(atlas), "Player generation Atlas");
  auto bytes = twod::EncodeAtlas(*atlas);
  std::uint64_t high{}, low{};
  Require(api.register_atlas(api.context, bytes.data(), bytes.size(), &high, &low) == 0,
          "Player generation Atlas registration");
  twod::SpriteAsset source;
  source.atlas = atlas->id;
  source.region = image.id;
  source.sourceWidth = source.sourceHeight = 6;
  source.trimX = source.trimY = 1;
  source.trimWidth = source.trimHeight = 2;
  source.pivotX = repack ? 3.F : 2.F;
  source.pivotY = source.pixelsPerUnit = 1;
  for (const auto id : {twod::AssetId{11, 12}, twod::AssetId{13, 14}}) {
    if (repack && id.high == 13)
      continue;
    source.id = id;
    bytes = twod::EncodeSpriteAsset(source);
    Require(sources.register_sprite_source(sources.context, bytes.data(), bytes.size(), &high,
                                           &low) == 0,
            "Player generation source registration");
  }
  if (!repack) {
    const twod::Clip clip{
        {15, 16},
        {},
        true,
        {{0, .25, "second", twod::AssetId{11, 12}}, {0, .5, "first", twod::AssetId{13, 14}}}};
    bytes = twod::EncodeClip(clip);
    Require(api.register_clip(api.context, bytes.data(), bytes.size(), &high, &low) == 0,
            "Player generation source Clip registration");
  }
}
struct External final {
  unsigned audio{}, display{}, ui{};
};
void WithoutTiles() {
  auto values = twod::GameplayGeneration::CreateSingleScene("NoTiles", 8, {}, false);
  Require(bool(values), "Player optional Tilemap owner");
  for (unsigned copy = 0; copy < 2; ++copy) {
    NexoraTwoDApiV1 api{};
    api.struct_size = sizeof(api);
    NexoraTwoDTile tile{-2, -1, -1, 1, 1};
    Require(values->Bridge().GetApi(&api) == 0 &&
                api.paint_tiles(api.context, &tile, 1) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
            "Player staging enabled an absent Tilemap");
    values = values->Stage();
    Require(bool(values), "Player optional Tilemap staging");
  }
}
} // namespace

int main(int argc, char **argv) try {
  Require(argc == 7 || (argc == 8 && std::string_view(argv[7]) == "--native"),
          "Expected five SDK libraries, evidence directory and optional --native");
  WithoutTiles();
  const Path output(argv[6]);
  const auto root =
      output /
      ("run-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Require(std::filesystem::create_directories(root), "Player generation evidence directory");
  twod::GameplayContent content;
  Register(content);
  runtime::GameplayInput input;
  runtime::GameplayStorage storage(root / "storage", "player.generation");
  runtime::GameplayServices services;
  services.input = input.Api();
  services.storage.struct_size = sizeof(services.storage);
  services.storage.abi_version = 1;
  Require(storage.GetApi(services.storage) == 0, "Player generation services");
  // GetApi uses caller-provided size even though GameplayServices itself starts with zero tables.
  services.content.struct_size = sizeof(services.content);
  Require(content.GetApi(&services.content) == 0, "Player generation content negotiation");
  services.sprite_content.struct_size = sizeof(services.sprite_content);
  Require(content.GetSpriteApi(&services.sprite_content) == 0, "Player generation source service");
  NexoraGameplayInputBindingV1 binding{};
  binding.struct_size = sizeof(binding);
  binding.abi_version = 1;
  binding.device = NEXORA_INPUT_DEVICE_KEYBOARD;
  binding.control = NEXORA_INPUT_KEY_D;
  binding.scale = 1;
  Require(services.input.bind(services.input.context, 0, 250, &binding) == 0 &&
              input.BeginFrame() &&
              input.Submit(0, NEXORA_INPUT_DEVICE_KEYBOARD, 0, NEXORA_INPUT_KEY_D, 1),
          "Player generation input sentinel");
  const std::array<std::byte, 3> saved{std::byte{1}, std::byte{17}, std::byte{255}};
  Require(services.storage.save(services.storage.context, "generation-guard", 16, 7, saved.data(),
                                static_cast<std::uint32_t>(saved.size())) == 0,
          "Player generation storage sentinel");
  const auto guard = root / "storage" / "generation-guard.nxsav";
  const auto original = Read(guard);
  External external;
  services.audio.struct_size = sizeof(services.audio);
  services.audio.abi_version = 1;
  services.audio.context = &external;
  services.audio.stop_all = [](void *p) -> std::int32_t {
    ++static_cast<External *>(p)->audio;
    return 0;
  };
  services.display.struct_size = sizeof(services.display);
  services.display.abi_version = 1;
  services.display.context = &external;
  services.display.request_window = [](void *p, const NexoraDisplayWindowV1 *) -> std::int32_t {
    ++static_cast<External *>(p)->display;
    return 0;
  };
  services.ui.struct_size = sizeof(services.ui);
  services.ui.abi_version = 1;
  services.ui.context = &external;
  services.ui.begin_frame = [](void *p, float, float) -> std::int32_t {
    ++static_cast<External *>(p)->ui;
    return 0;
  };
  auto values = twod::GameplayGeneration::CreateSingleScene(
      "Main", 8, [&content](auto id) { return content.ResolveClip(id); });
  player::detail::GenerationHost host(std::move(values), services, "Main",
                                      player::detail::CreatePlayerHost);
  runtime::GameplayModuleHost module(host.Host(), {}, host.Transaction());
  Require(!host.Load(module, root / "missing-module.so") && host.Context().allocated == 0,
          "Missing library snapshot invoked gameplay callbacks");
  Require(host.Load(module, Path(argv[1])), "Player generation initial SDK load");
  Require(host.LoadedLibrary() != Path(argv[1]) &&
              std::filesystem::is_regular_file(host.LoadedLibrary()),
          "Initial Player library is not an independent owned image");
  host.Context().input.sequence = 42;
  const auto scene = host.Values().Sprites()->SceneId();
  const auto entity = host.Values().Sprites()->Sprites().begin()->first;
  playerreload::Native native(argc == 8, root);
  const auto unchanged = [&] {
    NexoraGameplayActionStateV1 action{};
    action.struct_size = sizeof(action);
    action.abi_version = 1;
    Require(services.input.read_action(services.input.context, 0, 250, &action) == 0 &&
                action.value == 1 && Read(guard) == original && !external.audio &&
                !external.display && !external.ui,
            "Candidate/retired code changed live external services");
    const auto &active = host.Values();
    Require(active.Sprites()->SceneId() == scene && active.Sprites()->Sprites().size() == 1 &&
                active.Pool().Size() == 1 && active.World().FindEntity(entity) &&
                active.World().FindEntity(entity)->transform.x == -2 &&
                active.World().FindEntity(entity)->transform.y == 2 &&
                host.Context().input.sequence == 42,
            "Player generation lost scene/entity/pool/anchor/input state");
  };
  unsigned rejected{};
  Path retainedSource;
  std::ofstream copies(root / "library-generations.txt");
  for (unsigned generation = 0; generation < 32; ++generation) {
    Require(module.Update(1.0 / 60), "Player generation live SDK update");
    if (generation == 16)
      Register(content, true);
    unchanged();
    const auto beforePixels = native.Draw(host.Values(), content);
    const auto beforeWorld = host.Values().World().SaveScene(scene);
    const auto beforeSprites = host.Values().Sprites()->Snapshot();
    const auto beforeGeneration = module.Generation();
    const auto beforeImage = host.LoadedLibrary();
    for (unsigned failure = 0; failure < 4; ++failure) {
      const auto candidate =
          root / ("reject-" + std::to_string(generation) + "-" + std::to_string(failure) +
                  Path(argv[failure + 2]).extension().string());
      Require(std::filesystem::copy_file(Path(argv[failure + 2]), candidate),
              "Player failed candidate copy");
      copies << candidate.filename().string() << ' ' << std::filesystem::file_size(candidate)
             << " rejected\n";
      Require(!host.Reload(module, candidate) && module.Generation() == beforeGeneration &&
                  host.Values().World().SaveScene(scene) == beforeWorld &&
                  host.Values().Sprites()->Snapshot() == beforeSprites,
              "Player failed SDK candidate mutated the published generation");
      unchanged();
      Require(host.LoadedLibrary() == beforeImage && std::filesystem::exists(beforeImage),
              "Failed candidate retired the active library image");
      Require(native.Draw(host.Values(), content) == beforePixels,
              "Player failed SDK candidate changed native source/pool pixels");
      ++rejected;
      Require(std::filesystem::remove(candidate), "Player failed library did not retire");
    }
    const auto candidate =
        root / ("accepted-" + std::to_string(generation) + Path(argv[1]).extension().string());
    Require(std::filesystem::copy_file(Path(argv[1]), candidate), "Player SDK candidate copy");
    copies << candidate.filename().string() << ' ' << std::filesystem::file_size(candidate)
           << " accepted\n";
    Require(host.Reload(module, candidate) && module.Generation() == beforeGeneration + 1 &&
                host.Values().World().SaveScene(scene) == beforeWorld &&
                host.Values().Sprites()->Snapshot() == beforeSprites,
            "Player SDK publication changed World/Sprite values");
    unchanged();
    Require(host.LoadedLibrary() != beforeImage && host.LoadedLibrary() != candidate &&
                std::filesystem::exists(host.LoadedLibrary()) &&
                !std::filesystem::exists(beforeImage.parent_path()),
            "Successful replacement retained or reused the old library image");
    Require(native.Draw(host.Values(), content) == beforePixels,
            "Player SDK publication changed native source/pool pixels");
    if (!retainedSource.empty())
      Require(std::filesystem::remove(retainedSource), "Player old library did not retire");
    retainedSource = candidate;
  }
#if defined(__linux__) || defined(__APPLE__)
  constexpr unsigned finalizers = 1;
#else
  constexpr unsigned finalizers = 0;
#endif
  std::cerr << "service rejections capture=" << host.ServiceStats().capture_rejections
            << " candidate=" << host.ServiceStats().candidate_rejections
            << " retired=" << host.ServiceStats().retired_rejections << '\n';
  Require(host.ServiceStats().capture_rejections == 160 * 2 * 8,
          "Player save_state side effects escaped the read-only capture phase");
  // Failed prepare owns its partial cleanup: the SDK frees an uninitialized instance without
  // deinit. Other rejected candidates run deinit; each actual image still runs its finalizer.
  Require(host.ServiceStats().candidate_rejections ==
                  32 * 5 + 32 * (5 + finalizers) + 96 * (10 + finalizers) &&
              host.ServiceStats().retired_rejections == 32 * (10 + finalizers),
          "Player candidate/retired cached service or library finalizer coverage");
  Require(module.GetReloadStats().successful_reloads == 32 &&
              module.GetReloadStats().migrated_bytes == 80 && rejected == 128,
          "Player real SDK reload acceptance count");
  Require(module.Update(1.0 / 60), "Player final migrated SDK state is not callable");
  const auto finalImage = host.LoadedLibrary();
  host.Unload(module);
  Require(host.LoadedLibrary().empty() && !std::filesystem::exists(finalImage.parent_path()),
          "Final Player library image did not retire");
  Require(std::filesystem::remove(retainedSource) && copies.good(),
          "Player final library or generation receipt did not retire");
  Require(host.Context().allocated == host.Context().freed && external.audio == 2 &&
              external.ui == 2 && external.display == 2 && !std::filesystem::exists(guard),
          "Player normal Stop/Destroy lost live cleanup or allocation ownership");
  native.Finish();
  std::ofstream report(root / "acceptance.json");
  report << "{\"accepted\":32,\"rejected\":128,\"migrated_bytes\":80,\"native_frames\":"
         << native.Frames()
         << ",\"capture_service_rejections\":" << host.ServiceStats().capture_rejections
         << ",\"candidate_service_rejections\":" << host.ServiceStats().candidate_rejections
         << ",\"retired_service_rejections\":" << host.ServiceStats().retired_rejections
         << ",\"finalizers_per_library\":" << finalizers
         << ",\"allocations_balanced\":true,\"independent_library_images\":true,"
            "\"image_cleanup_verified\":true}\n";
  Require(report.good(), "Player generation receipt write");
  std::cout << "PASS production Player host:32 SDK generations,128 rejected candidates,"
               "cached service/finalizer quarantine,source repack and "
            << native.Frames() << " native frames; evidence=" << root.string() << '\n';
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
