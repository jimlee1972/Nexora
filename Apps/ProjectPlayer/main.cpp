#include "Nexora/Runtime/ProjectPackage.h"
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
#include "NativePlayer.h"
#endif

#include <algorithm>
#include <exception>
#include <iostream>
#include <type_traits>

namespace {
int Usage() {
  std::cerr << "Usage: NexoraProjectPlayer --verify-package PACKAGE\n"
               "       NexoraProjectPlayer --run-package PACKAGE [--frames=N] "
               "[--backend=automatic|vulkan|dx12|metal]\n";
  return 2;
}

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
  if (argc < 3)
    return Usage();
  const std::basic_string_view<Char> mode(argv[1]);
  if (EqualAscii(mode, "--verify-package"))
    return argc == 3 ? Run(ArgumentPath(argv[2])) : Usage();
  if (!EqualAscii(mode, "--run-package"))
    return Usage();
#if defined(NEXORA_PROJECT_PLAYER_NATIVE)
  nexora::player::NativeOptions options;
  bool has_frames{}, has_backend{};
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
    } else
      return Usage();
  }
  std::string error;
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
#else
int main(int argc, char **argv) { return Dispatch(argc, argv); }
#endif
