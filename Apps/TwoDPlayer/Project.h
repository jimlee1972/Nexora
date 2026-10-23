#pragma once
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/Assets.h"
#include "Nexora/TwoD/GameplayContent.h"
#include "Nexora/TwoD/GameplayUi.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nexora::player {
inline std::filesystem::path GamePath(std::string_view value) {
  return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
inline void RequireProject(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct ProjectAsset final {
  std::string kind, path;
  std::uint64_t id{};
  bool deferred{};
};
struct GameProject final {
  std::filesystem::path root, module;
  std::string id, title, scene, audio{"optional"};
  std::vector<ProjectAsset> assets;
  twod::AssetId tile_atlas{};
  twod::GameplayContent::Limits content_limits{};
  uint64_t staging_bytes{64 * 1024 * 1024};
  uint32_t pool_instances{1024};
  uint32_t width{640}, height{480};
  bool borderless{}, resizable{true};
  std::filesystem::path File(std::string_view value) const {
    RequireProject(
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c < 127; }),
        "packaged relative paths must use portable ASCII names");
    const auto relative = GamePath(value);
    RequireProject(!relative.empty() && value.size() <= 1024 && !relative.is_absolute() &&
                       !relative.has_root_name() && value.find('\\') == std::string_view::npos &&
                       value.find(':') == std::string_view::npos &&
                       value.find('\0') == std::string_view::npos,
                   "project paths must be relative forward-slash paths");
    auto path = root;
    for (const auto &part : relative) {
      RequireProject(part != ".." && part != "." && !part.empty(), "invalid project path");
      path /= part;
      RequireProject(!std::filesystem::is_symlink(std::filesystem::symlink_status(path)),
                     "project symlinks are not admitted");
    }
    RequireProject(std::filesystem::is_regular_file(path), "project file is missing");
    return path;
  }
  static GameProject Load(const std::filesystem::path &manifest) {
    RequireProject(std::filesystem::file_size(manifest) <= 1024 * 1024,
                   "project manifest exceeds 1 MiB");
    GameProject result;
    result.root = std::filesystem::canonical(manifest).parent_path();
    std::ifstream file(manifest);
    std::string line;
    RequireProject(bool(std::getline(file, line)) && line == "NEXORA_2D_GAME 1",
                   "unsupported 2D game manifest");
    std::set<std::string> fields, paths;
    std::set<std::pair<std::string, std::uint64_t>> ids;
    std::string module;
    while (std::getline(file, line)) {
      if (line.empty())
        continue;
      std::istringstream row(line);
      std::string key, value, trailing;
      row >> key;
      if (key == "capacity") {
        RequireProject(
            fields.insert(key).second &&
                bool(row >> result.content_limits.assets >> result.content_limits.active_bytes >>
                     result.content_limits.live_bytes >> result.staging_bytes >>
                     result.pool_instances) &&
                !(row >> trailing) && result.content_limits.assets > 0 &&
                result.content_limits.assets <= 4096 && result.content_limits.active_bytes > 0 &&
                result.content_limits.active_bytes <= 1024ULL * 1024 * 1024 &&
                result.content_limits.live_bytes >= result.content_limits.active_bytes &&
                result.content_limits.live_bytes <= 2ULL * 1024 * 1024 * 1024 &&
                result.staging_bytes > 0 && result.staging_bytes <= 128ULL * 1024 * 1024 &&
                result.pool_instances > 0 && result.pool_instances <= 100000,
            "invalid asset capacity");
      } else if (key == "display") {
        std::string mode;
        uint32_t resizable{};
        RequireProject(fields.insert(key).second &&
                           bool(row >> result.width >> result.height >> mode >> resizable) &&
                           !(row >> trailing) && result.width > 0 && result.width <= 16384 &&
                           result.height > 0 && result.height <= 16384 && resizable <= 1 &&
                           (mode == "windowed" || mode == "borderless"),
                       "invalid display settings");
        result.borderless = mode == "borderless";
        result.resizable = resizable != 0;
      } else if (key == "tile_atlas") {
        RequireProject(fields.insert(key).second &&
                           bool(row >> result.tile_atlas.high >> result.tile_atlas.low) &&
                           (result.tile_atlas.high || result.tile_atlas.low) && !(row >> trailing),
                       "invalid tile atlas identity");
      } else if (key == "atlas" || key == "clip" || key == "wave" || key == "font" ||
                 key == "deferred_atlas" || key == "deferred_clip" || key == "deferred_blob") {
        ProjectAsset asset;
        asset.deferred = key.starts_with("deferred_");
        asset.kind = asset.deferred ? key.substr(9) : key;
        if (key == "wave" || key == "font") {
          RequireProject(bool(row >> asset.id) && asset.id && (key != "font" || asset.id <= 32) &&
                             ids.emplace(key, asset.id).second,
                         "invalid or duplicate project service asset ID");
        }
        RequireProject(bool(row >> std::quoted(asset.path)) && !(row >> trailing) &&
                           paths.insert(asset.path).second && result.assets.size() < 4096,
                       "invalid or duplicate project asset");
        result.File(asset.path);
        result.assets.push_back(std::move(asset));
      } else {
        RequireProject(fields.insert(key).second && bool(row >> std::quoted(value)) &&
                           !(row >> trailing) && !value.empty() && value.size() <= 1024 &&
                           value.find('\0') == std::string::npos,
                       "invalid project field");
        if (key == "id")
          result.id = value;
        else if (key == "title")
          result.title = value;
        else if (key == "scene")
          result.scene = value;
        else if (key == "module")
          module = value;
        else if (key == "audio")
          result.audio = value;
        else
          throw std::runtime_error("unknown project field");
      }
    }
    RequireProject(
        file.eof() && !result.id.empty() && result.id.size() <= 128 && !result.title.empty() &&
            !result.scene.empty() && result.scene.size() <= 256 && !module.empty() &&
            (result.audio == "off" || result.audio == "optional" || result.audio == "required"),
        "incomplete project manifest");
    for (unsigned char c : result.id)
      RequireProject((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                         c == '_',
                     "project ID must be a portable save scope");
    const auto alnum = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
    RequireProject(alnum(result.id.front()) && alnum(result.id.back()), "invalid project ID");
    result.module = result.File(module);
    return result;
  }
};
inline twod::GameplayUiFont ReadGameFont(std::span<const std::byte> bytes, std::uint64_t id) {
  RequireProject(bytes.size() >= 28 && std::memcmp(bytes.data(), "NXFONT1\0", 8) == 0,
                 "invalid font atlas header");
  std::size_t offset = 8;
  const auto u32 = [&]() {
    RequireProject(offset + 4 <= bytes.size(), "truncated font atlas");
    std::uint32_t value{};
    for (unsigned n = 0; n < 4; ++n)
      value |= std::to_integer<std::uint32_t>(bytes[offset++]) << (n * 8);
    return value;
  };
  const auto number = [&]() { return std::bit_cast<float>(u32()); };
  twod::GameplayUiFont font;
  font.texture_id = (std::uint64_t{1} << 60) + id;
  font.width = u32();
  font.height = u32();
  font.pixel_size = number();
  font.line_height = number();
  const auto count = u32();
  RequireProject(id && id <= 32 && font.width && font.height && font.width <= 4096 &&
                     font.height <= 4096 && count && count <= 65536 &&
                     bytes.size() == 28ULL + count * 40ULL + font.width * 4ULL * font.height,
                 "invalid font atlas bounds");
  font.glyphs.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i)
    font.glyphs.push_back({u32(), number(), number(), number(), number(), number(), number(),
                           number(), number(), number()});
  font.rgba.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
  return font;
}
} // namespace nexora::player
