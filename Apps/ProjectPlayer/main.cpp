#include "Nexora/Runtime/ProjectPackage.h"

#include <algorithm>
#include <iostream>

namespace {
int Usage() {
  std::cerr << "Usage: NexoraProjectPlayer --verify-package PACKAGE\n";
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
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  if (argc != 3 || std::wstring_view(argv[1]) != L"--verify-package")
    return Usage();
  return Run(std::filesystem::path(argv[2]));
}
#else
int main(int argc, char **argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--verify-package")
    return Usage();
  return Run(std::filesystem::path(
      std::u8string(argv[2], argv[2] + std::char_traits<char>::length(argv[2]))));
}
#endif
