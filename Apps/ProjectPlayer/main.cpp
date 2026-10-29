#include "Nexora/Runtime/GameProjectPackage.h"
#include "Nexora/Runtime/ProjectPackage.h"
#if defined(NEXORA_PROJECT_PLAYER_2D)
#include "TwoDPlayer.h"
#endif
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
#include "InstalledGame.h"
#include "NativeGamePlayer.h"
#include "NativePlayer.h"
#endif

#include <algorithm>
#include <cstdio>
#include <exception>
#include <iostream>
#include <type_traits>

#if defined(_WIN32) && defined(NEXORA_PROJECT_PLAYER_NATIVE)
#include <shellapi.h>
#include <windows.h>
#endif

namespace {
int Usage() {
  std::cerr << "Usage: NexoraProjectPlayer --verify-package PACKAGE\n"
               "       NexoraProjectPlayer --verify-game-package PACKAGE\n"
               "       NexoraProjectPlayer --verify-native-game-package PACKAGE\n"
               "       NexoraProjectPlayer --verify-game-module MODULE\n"
               "       NexoraProjectPlayer --run-game-package PACKAGE [native options]\n"
               "       NexoraProjectPlayer --run-package PACKAGE [--frames=N] "
               "[--backend=automatic|vulkan|dx12|metal] [--gameplay-module=PATH] "
               "[--save-directory=PATH]\n";
  return 2;
}

int VerifyGame(const std::filesystem::path &path) {
  std::string error;
  const auto project = nexora::runtime::ReadGameProjectPackage(path, &error);
  if (!project) {
    std::cerr << "Game package verification failed: " << error << '\n';
    return 1;
  }
  std::cout << "{\"status\":\"VERIFIED_GAME_PACKAGE\",\"schema_version\":1,\"capability\":2,"
               "\"start_scene\":\""
            << project->StartupScene().ToString()
            << "\",\"scene_count\":" << project->Scenes().size()
            << ",\"asset_count\":" << project->AssetCount()
            << ",\"native_rendering\":false,\"gameplay_loaded\":false}\n";
  return 0;
}
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
int VerifyNativeGame(const std::filesystem::path &path) {
  std::string error;
  const auto project = nexora::runtime::ReadGameProjectPackage(path, &error);
  if (!project) {
    std::cerr << "Game data verification failed: " << error << '\n';
    return 1;
  }
  for (const auto &entry : project->Scenes()) {
    const auto scene = project->ResolveScene(entry.id);
    auto view = scene ? nexora::player::PrepareSceneView(scene->RenderItems(), scene->WorldView(),
                                                         scene->SceneId(), &error, true)
                      : std::nullopt;
    if (!view || !view->DrawData(scene->WorldView(), 16.0F / 9)) {
      std::cerr << "Native game data admission failed for " << entry.id.ToString() << ": " << error
                << '\n';
      return 1;
    }
  }
  std::cout << "{\"status\":\"VERIFIED_NATIVE_GAME_DATA\",\"scene_count\":"
            << project->Scenes().size()
            << ",\"native_rendering\":false,\"gameplay_loaded\":false}\n";
  std::cout.flush();
  return std::cout ? 0 : 1;
}
#endif
int Run(const std::filesystem::path &path) {
  std::string error;
  const auto loaded = nexora::runtime::ReadStaticProjectPackage(path, &error);
  if (!loaded) {
    std::cerr << "Static project verification failed: " << error << '\n';
    return 1;
  }
  const auto *scene = loaded->WorldView().FindScene(loaded->SceneId());
  std::cout << "{\"status\":\"VERIFIED_STATIC_VIEW\",\"schema_version\":1,"
               "\"project\":\""
            << loaded->ProjectId().ToString() << "\",\"scene_asset\":\""
            << loaded->SceneAssetId().ToString() << "\",\"asset_count\":" << loaded->AssetCount()
            << ",\"entity_count\":" << scene->entities.size()
            << ",\"resolved_mesh_renderers\":" << loaded->RenderItems().size()
            << ",\"inactive_components\":" << loaded->InactiveComponentCount()
            << ",\"native_rendering\":false,\"gameplay_loaded\":false,\"render_items\":[";
  constexpr std::size_t kMaximumReportedItems = 64;
  const auto reported = std::min(loaded->RenderItems().size(), kMaximumReportedItems);
  for (std::size_t index = 0; index < reported; ++index) {
    const auto &item = loaded->RenderItems()[index];
    const auto *entity = loaded->WorldView().FindEntity(item.entity);
    std::cout << (index ? "," : "") << "{\"entity\":\"" << item.entity << "\",\"legacy_shader\":\""
              << entity->mesh_data.material.shader
              << "\",\"vertices\":" << item.mesh->vertices.size()
              << ",\"indices\":" << item.mesh->indices.size()
              << ",\"scalar_pbr\":" << (item.material ? "true" : "false") << '}';
  }
  std::cout << "],\"unreported_render_items\":" << loaded->RenderItems().size() - reported
            << ",\"inactive_component_details\":[";
  std::size_t reported_inactive = 0;
  constexpr char kHex[] = "0123456789abcdef";
  for (const auto &record : loaded->SceneData().opaque) {
    if (record.type == 0x45444d41544c0001ULL && record.type_name == "editor.material.asset")
      continue;
    if (reported_inactive == kMaximumReportedItems)
      break;
    std::cout << (reported_inactive ? "," : "") << "{\"entity\":\"" << record.entity
              << "\",\"type\":\"" << record.type << "\",\"type_name_hex\":\"";
    // Existing opaque names may contain arbitrary non-control bytes. Hex is lossless and keeps
    // the report valid JSON without silently imposing a new UTF-8 contract on preserved names.
    for (const unsigned char value : record.type_name)
      std::cout << kHex[value >> 4] << kHex[value & 15];
    std::cout << "\",\"payload_bytes\":" << record.data.size() << '}';
    ++reported_inactive;
  }
  std::cout << "],\"unreported_inactive_components\":"
            << loaded->InactiveComponentCount() - reported_inactive << "}\n";
  std::cout.flush();
  if (!std::cout) {
    std::cerr << "Static project verification report output failed\n";
    return 1;
  }
  return 0;
}

template <typename Char>
bool EqualAscii(std::basic_string_view<Char> value, std::string_view ascii) {
  return value.size() == ascii.size() &&
         std::equal(value.begin(), value.end(), ascii.begin(),
                    [](Char left, char right) { return left == static_cast<Char>(right); });
}
template <typename Char> std::filesystem::path ArgumentPath(const Char *argument) {
  if constexpr (std::is_same_v<Char, wchar_t>)
    return std::filesystem::path(argument);
  else
    return std::filesystem::path(
        std::u8string(argument, argument + std::char_traits<Char>::length(argument)));
}
template <typename Char> int Dispatch(int argc, Char **argv) try {
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
  if (argc == 1) {
    if (const auto game = nexora::player::TryRunInstalledGame())
      return *game;
    return Usage();
  }
#endif
  if (argc < 3)
    return Usage();
  const std::basic_string_view<Char> mode(argv[1]);
#if defined(NEXORA_PROJECT_PLAYER_2D)
  if (EqualAscii(mode, "--run-2d-project"))
    return nexora::player::DispatchTwoDPlayer(argc, argv);
#endif
  if (EqualAscii(mode, "--verify-package"))
    return argc == 3 ? Run(ArgumentPath(argv[2])) : Usage();
  if (EqualAscii(mode, "--verify-game-package"))
    return argc == 3 ? VerifyGame(ArgumentPath(argv[2])) : Usage();
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
  if (EqualAscii(mode, "--verify-game-module")) {
    if (argc != 3)
      return Usage();
    if (!nexora::runtime::GameplayModuleHost::Inspect(ArgumentPath(argv[2]))) {
      std::cerr << "Game module V3 admission failed\n";
      return 1;
    }
    std::cout << "{\"status\":\"VERIFIED_GAME_MODULE_ABI\",\"gameplay_started\":false}\n";
    std::cout.flush();
    return std::cout ? 0 : 1;
  }
  if (EqualAscii(mode, "--verify-native-game-package"))
    return argc == 3 ? VerifyNativeGame(ArgumentPath(argv[2])) : Usage();
#endif
  const bool game_package = EqualAscii(mode, "--run-game-package");
  if (!EqualAscii(mode, "--run-package") && !game_package)
    return Usage();
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
  nexora::player::NativeOptions options;
  bool has_frames{}, has_backend{}, has_gameplay{}, has_save_directory{};
  for (int index = 3; index < argc; ++index) {
    const std::basic_string_view<Char> value(argv[index]);
    if (value.size() > 9 && EqualAscii(value.substr(0, 9), "--frames=")) {
      if (has_frames)
        return Usage();
      has_frames = true;
      std::uint32_t frames{};
      for (const auto digit : value.substr(9)) {
        if (digit < static_cast<Char>('0') || digit > static_cast<Char>('9'))
          return Usage();
        frames = frames * 10 + static_cast<std::uint32_t>(digit - static_cast<Char>('0'));
        if (frames > 1000000)
          return Usage();
      }
      if (!frames)
        return Usage();
      options.maximum_frames = frames;
    } else if (value.size() > 10 && EqualAscii(value.substr(0, 10), "--backend=")) {
      if (has_backend)
        return Usage();
      has_backend = true;
      const auto name = value.substr(10);
      using Nexora::Presentation::SurfaceBackend;
      if (EqualAscii(name, "automatic"))
        options.backend = SurfaceBackend::Automatic;
      else if (EqualAscii(name, "vulkan"))
        options.backend = SurfaceBackend::Vulkan;
      else if (EqualAscii(name, "dx12"))
        options.backend = SurfaceBackend::Dx12;
      else if (EqualAscii(name, "metal"))
        options.backend = SurfaceBackend::Metal;
      else
        return Usage();
    } else if (value.size() > 18 && EqualAscii(value.substr(0, 18), "--gameplay-module=")) {
      if (has_gameplay)
        return Usage();
      has_gameplay = true;
      options.gameplay_module = std::filesystem::absolute(ArgumentPath(argv[index] + 18));
    } else if (value.size() > 17 && EqualAscii(value.substr(0, 17), "--save-directory=")) {
      if (has_save_directory)
        return Usage();
      has_save_directory = true;
      options.save_directory = ArgumentPath(argv[index] + 17);
    } else
      return Usage();
  }
  if (has_save_directory && !has_gameplay)
    return Usage();
  std::string error;
  if (game_package) {
    auto project = nexora::runtime::ReadGameProjectPackage(ArgumentPath(argv[2]), &error);
    if (!project) {
      std::cerr << "Game project loading failed: " << error << '\n';
      return 1;
    }
    return nexora::player::RunNativeGame(
        std::make_shared<const nexora::runtime::LoadedGameProject>(std::move(*project)), options);
  }
  const auto project = nexora::runtime::ReadStaticProjectPackage(ArgumentPath(argv[2]), &error);
  if (!project) {
    std::cerr << "Static project loading failed: " << error << '\n';
    return 1;
  }
  return nexora::player::RunNative(*project, options);
#else
  std::cerr << "Native ProjectPlayer rendering is unavailable in this build\n";
  return 2;
#endif
} catch (const std::exception &error) {
  std::cerr << "ProjectPlayer failed: " << error.what() << '\n';
  return 1;
}
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) { return Dispatch(argc, argv); }
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  // GUI subsystem prevents console creation at process startup. Never allocate, attach,
  // hide or detach a console; inherited redirected handles remain the CLI's streams.
  // Explorer supplies no standard streams. Give only absent handles a NUL sink so
  // normal close does not become a failed diagnostic write. Real redirected handles,
  // including read-only/failing sinks, keep their existing error semantics.
  for (const auto stream : {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
    const auto handle = GetStdHandle(stream);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
      FILE *sink{};
      if (_wfreopen_s(&sink, L"NUL", L"w", stream == STD_OUTPUT_HANDLE ? stdout : stderr))
        return 1;
    }
  }
  if (GetEnvironmentVariableW(L"NEXORA_PLAYER_STARTUP_EVIDENCE", nullptr, 0))
    std::cerr << "player GUI startup: console_attached=" << (GetConsoleWindow() != nullptr) << '\n';
  int argc{};
  auto **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) {
    std::cerr << "ProjectPlayer command-line decoding failed\n";
    return 2;
  }
  const int result = Dispatch(argc, argv);
  LocalFree(argv);
  return result;
}
#endif
#else
int main(int argc, char **argv) { return Dispatch(argc, argv); }
#endif
