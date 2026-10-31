#include "Nexora/Editor/ProjectBuildSettings.h"
#include "AtomicFile.h"
#include "Nexora/Editor/ContentBrowser.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <unordered_set>

namespace nexora::editor {
namespace {
bool Fail(std::string *error, std::string_view text) {
  if (error)
    *error = text;
  return false;
}
std::string Fold(std::string_view text) {
  std::string result(text);
  for (auto &c : result)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  return result;
}
bool SafePath(std::string_view text, bool scene = true) {
  if (text.empty() || text.size() > ProjectBuildSettings::kMaximumPathBytes ||
      !foundation::IsValidUtf8(text) || text.find_first_of("\\:<>\"|?*") != text.npos ||
      std::ranges::any_of(text, [](unsigned char c) { return c < 32 || c == 127; }))
    return false;
  const auto path = std::filesystem::path(std::u8string(text.begin(), text.end()));
  if (path.has_root_path() || (scene && path.extension() != ".scene"))
    return false;
  // Portable spellings also reject Windows device aliases and case-only duplicate paths.
  std::size_t start = 0;
  while (start < text.size()) {
    const auto end = text.find('/', start);
    const auto part = text.substr(start, end == text.npos ? text.size() - start : end - start);
    if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
      return false;
    const auto lower = Fold(part);
    const auto stem = lower.substr(0, lower.find('.'));
    if (lower == ".git" || stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
         stem[3] >= '1' && stem[3] <= '9'))
      return false;
    if (start == 0 && lower == ".nexora" && (!scene || !text.starts_with(".nexora/scenes/")))
      return false;
    if (end == text.npos)
      break;
    start = end + 1;
  }
  return !text.ends_with('/');
}
// A fixed-depth/schema reader: unknown/duplicate keys, excess rows and strings reject before
// growth. Unicode escapes are decoded including surrogate pairs, then validated as UTF-8.
class Reader final {
public:
  explicit Reader(std::string_view text) : rest_(text) {}
  bool Take(std::string_view token) {
    Space();
    if (!rest_.starts_with(token))
      return false;
    rest_.remove_prefix(token.size());
    return true;
  }
  std::optional<std::string> String() {
    if (!Take("\""))
      return {};
    std::string out;
    while (!rest_.empty()) {
      auto c = rest_.front();
      rest_.remove_prefix(1);
      if (c == '"')
        return foundation::IsValidUtf8(out) ? std::optional{out} : std::nullopt;
      if (static_cast<unsigned char>(c) < 32)
        return {};
      if (c == '\\') {
        if (rest_.empty())
          return {};
        c = rest_.front();
        rest_.remove_prefix(1);
        if (c == 'u') {
          auto scalar = Hex();
          if (!scalar)
            return {};
          if (*scalar >= 0xd800 && *scalar <= 0xdbff) {
            if (!rest_.starts_with("\\u"))
              return {};
            rest_.remove_prefix(2);
            const auto low = Hex();
            if (!low || *low < 0xdc00 || *low > 0xdfff)
              return {};
            *scalar = 0x10000 + ((*scalar - 0xd800) << 10) + (*low - 0xdc00);
          } else if (*scalar >= 0xdc00 && *scalar <= 0xdfff)
            return {};
          if (*scalar < 32 || *scalar == 127)
            return {};
          if (*scalar < 0x80)
            out += static_cast<char>(*scalar);
          else {
            if (*scalar >= 0x10000)
              out += static_cast<char>(0xf0 | (*scalar >> 18));
            if (*scalar >= 0x800)
              out += static_cast<char>((*scalar >= 0x10000 ? 0x80 : 0xe0) |
                                       ((*scalar >> 12) & (*scalar >= 0x10000 ? 0x3f : 0xf)));
            out += static_cast<char>((*scalar >= 0x800 ? 0x80 : 0xc0) | ((*scalar >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (*scalar & 0x3f));
          }
          if (out.size() > ProjectBuildSettings::kMaximumPathBytes)
            return {};
          continue;
        }
        if (c != '"' && c != '\\' && c != '/')
          return {};
      }
      if (out.size() == ProjectBuildSettings::kMaximumPathBytes)
        return {};
      out += c;
    }
    return {};
  }
  template <std::size_t N, typename Read>
  bool Object(const std::array<std::string_view, N> &keys, Read read) {
    if (!Take("{"))
      return false;
    unsigned seen = 0;
    for (std::size_t i = 0; i < keys.size(); ++i) {
      if (i && !Take(","))
        return false;
      const auto key = String();
      if (!key || !Take(":"))
        return false;
      const auto found = std::ranges::find(keys, *key);
      if (found == keys.end())
        return false;
      const auto field = static_cast<unsigned>(found - keys.begin());
      if ((seen & (1U << field)) || !read(field))
        return false;
      seen |= 1U << field;
    }
    return Take("}");
  }
  std::optional<ProjectBuildSettings> Read(unsigned schema) {
    ProjectBuildSettings settings;
    const auto read_field = [&](unsigned key) {
      if (key == 0)
        return Take(schema == 1 ? "1" : "2");
      if (key == 1)
        return Uuid(settings.start_scene);
      if (key == 3)
        return Object<3>({"mode", "script_manifest", "output"}, [&](unsigned build_key) {
          const auto value = String();
          if (!value)
            return false;
          if (build_key == 0) {
            if (*value == "scene-only")
              settings.build.mode = GameBuildMode::SceneOnly;
            else if (*value == "script-project")
              settings.build.mode = GameBuildMode::ScriptProject;
            else
              return false;
          } else if (build_key == 1)
            settings.build.script_manifest = *value;
          else
            settings.build.output = *value;
          return true;
        });
      if (!Take("["))
        return false;
      if (Take("]"))
        return true;
      do {
        if (settings.scenes.size() == ProjectBuildSettings::kMaximumScenes)
          return false;
        BuildScene scene;
        if (!Object<3>({"id", "path", "enabled"}, [&](unsigned field) {
              if (field == 0)
                return Uuid(scene.id);
              if (field == 1) {
                const auto path = String();
                if (!path)
                  return false;
                scene.path = *path;
                return true;
              }
              if (Take("true")) {
                scene.enabled = true;
                return true;
              }
              scene.enabled = false;
              return Take("false");
            }))
          return false;
        settings.scenes.push_back(std::move(scene));
      } while (Take(","));
      return Take("]");
    };
    const bool complete = schema == 1
                              ? Object<3>({"schema", "start_scene", "scenes"}, read_field)
                              : Object<4>({"schema", "start_scene", "scenes", "build"}, read_field);
    if (!complete)
      return {};
    Space();
    return rest_.empty() ? std::optional{std::move(settings)} : std::nullopt;
  }

private:
  void Space() {
    const auto first = rest_.find_first_not_of(" \t\r\n");
    rest_.remove_prefix(first == rest_.npos ? rest_.size() : first);
  }
  std::optional<unsigned> Hex() {
    if (rest_.size() < 4)
      return {};
    unsigned code{};
    const auto result = std::from_chars(rest_.data(), rest_.data() + 4, code, 16);
    if (result.ec != std::errc{} || result.ptr != rest_.data() + 4)
      return {};
    rest_.remove_prefix(4);
    return code;
  }
  bool Uuid(runtime::AssetUuid &id) {
    const auto text = String();
    const auto parsed = text ? runtime::AssetUuid::Parse(*text) : std::nullopt;
    if (parsed)
      id = *parsed;
    return parsed.has_value();
  }
  std::string_view rest_;
};
bool Missing(const std::filesystem::file_status &status, std::error_code ec) {
  return ec == std::errc::no_such_file_or_directory || (!ec && !std::filesystem::exists(status));
}
bool Alias(const std::filesystem::path &path, const std::filesystem::file_status &status) {
  if (std::filesystem::is_symlink(status))
    return true;
#if defined(_WIN32)
  const auto attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
  static_cast<void>(path);
  return false;
#endif
}
bool File(const std::filesystem::path &root, std::string_view relative, bool required,
          std::string *error) {
  auto absolute = root;
  const auto path = std::filesystem::path(std::u8string(relative.begin(), relative.end()));
  for (const auto &part : path) {
    absolute /= part;
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(absolute, ec);
    if (Missing(status, ec))
      return !required || Fail(error, "Build scene is missing: " + std::string(relative));
    if (ec || Alias(absolute, status) ||
        (absolute != root / path && !std::filesystem::is_directory(status)) ||
        (absolute == root / path && !std::filesystem::is_regular_file(status)))
      return Fail(error, "Build scene path is unavailable or unsafe: " + std::string(relative));
  }
  if (required && !std::ifstream(absolute, std::ios::binary))
    return Fail(error, "Build scene cannot be read: " + std::string(relative));
  return true;
}
} // namespace

bool ValidateProjectBuildSettings(const ProjectBuildSettings &settings, std::string *error) {
  if (error)
    error->clear();
  if (settings.scenes.empty() || settings.scenes.size() > ProjectBuildSettings::kMaximumScenes ||
      settings.start_scene == runtime::AssetUuid{})
    return Fail(error, "Select an enabled startup scene and at most 256 build scenes.");
  const auto &build = settings.build;
  if ((build.mode != GameBuildMode::SceneOnly && build.mode != GameBuildMode::ScriptProject) ||
      !SafePath(build.output, false) || !SafePath(build.script_manifest, false))
    return Fail(error, "Build configuration requires safe portable project-relative paths.");
  const auto manifest = std::filesystem::path(
      std::u8string(build.script_manifest.begin(), build.script_manifest.end()));
  if (!manifest.has_parent_path() || manifest.extension() != ".json")
    return Fail(error, "Build configuration requires a safe output folder and a .json manifest in "
                       "a dedicated source directory.");
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> ids;
  std::unordered_set<std::string> paths;
  bool startup = false;
  for (const auto &scene : settings.scenes) {
    if (scene.id == runtime::AssetUuid{} || !SafePath(scene.path) || !ids.insert(scene.id).second ||
        !paths.insert(Fold(scene.path)).second)
      return Fail(error, "Build scenes require unique nonzero IDs and safe unique .scene paths.");
    startup = startup || (scene.id == settings.start_scene && scene.enabled);
  }
  return startup || Fail(error, "The startup scene must be included and enabled.");
}
std::optional<ProjectBuildSettings> ParseProjectBuildSettings(std::string_view bytes,
                                                              std::string *error) {
  if (error)
    error->clear();
  if (bytes.size() <= ProjectBuildSettings::kMaximumBytes && foundation::IsValidUtf8(bytes)) {
    auto result = Reader(bytes).Read(1);
    if (!result)
      result = Reader(bytes).Read(2);
    if (result && ValidateProjectBuildSettings(*result, error))
      return result;
  }
  Fail(error, "Invalid, unsupported or oversized project.build.json; original settings preserved.");
  return {};
}
std::optional<std::string> SerializeProjectBuildSettings(const ProjectBuildSettings &settings,
                                                         std::string *error) {
  if (!ValidateProjectBuildSettings(settings, error))
    return {};
  const bool extended = settings.build != GameBuildSettings{};
  std::string text = "{\n  \"schema\": " + std::string(extended ? "2" : "1") +
                     ",\n  \"start_scene\": \"" + settings.start_scene.ToString() +
                     "\",\n  \"scenes\": [\n";
  for (std::size_t i = 0; i < settings.scenes.size(); ++i) {
    const auto &scene = settings.scenes[i];
    // SafePath has excluded JSON control, quote and backslash bytes.
    text += "    {\"id\": \"" + scene.id.ToString() + "\", \"path\": \"" + scene.path +
            "\", \"enabled\": " + (scene.enabled ? "true" : "false") + "}" +
            (i + 1 == settings.scenes.size() ? "\n" : ",\n");
    if (text.size() + 6 > ProjectBuildSettings::kMaximumBytes) {
      Fail(error, "Build settings exceed 256 KiB.");
      return {};
    }
  }
  text += "  ]";
  if (extended) {
    text += ",\n  \"build\": {\"mode\": \"" +
            std::string(settings.build.mode == GameBuildMode::ScriptProject ? "script-project"
                                                                            : "scene-only") +
            "\", \"script_manifest\": \"" + settings.build.script_manifest + "\", \"output\": \"" +
            settings.build.output + "\"}";
  }
  text += "\n}\n";
  if (text.size() > ProjectBuildSettings::kMaximumBytes) {
    Fail(error, "Build settings exceed 256 KiB.");
    return {};
  }
  return text;
}

ProjectBuildSettingsSession::ProjectBuildSettingsSession(const ProjectWorkspace &workspace)
    : workspace_(workspace), root_(workspace.Root()), project_(workspace.Project().id) {}
bool ProjectBuildSettingsSession::Live(std::string *error) const {
  std::error_code ec;
  return (!root_.empty() && workspace_.Root() == root_ && workspace_.Project().id == project_ &&
          std::filesystem::canonical(root_, ec) == root_ && !ec &&
          !workspace_.HasRecoveryJournal()) ||
         Fail(error, "Build settings require the same open project with resolved recovery.");
}
bool ProjectBuildSettingsSession::Read(std::optional<std::string> &bytes,
                                       std::string *error) const {
  if (!Live(error))
    return false;
  const auto path = root_ / "project.build.json";
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (Missing(status, ec)) {
    bytes.reset();
    return true;
  }
  if (ec || Alias(path, status) || !std::filesystem::is_regular_file(status))
    return Fail(error, "Build settings file is unavailable or unsafe.");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return Fail(error, "Could not read project.build.json.");
  std::string value(ProjectBuildSettings::kMaximumBytes + 1, '\0');
  input.read(value.data(), static_cast<std::streamsize>(value.size()));
  const auto count = static_cast<std::size_t>(input.gcount());
  if (input.bad() || count > ProjectBuildSettings::kMaximumBytes)
    return Fail(error, "Build settings are unreadable or exceed 256 KiB.");
  value.resize(count);
  bytes = std::move(value);
  return true;
}
bool ProjectBuildSettingsSession::Reload(std::string *error) {
  if (error)
    error->clear();
  ready_ = false;
  settings_.reset();
  std::optional<std::string> bytes;
  if (!Read(bytes, error))
    return false;
  if (bytes) {
    settings_ = ParseProjectBuildSettings(*bytes, error);
    if (!settings_)
      return false;
  }
  baseline_ = std::move(bytes);
  ready_ = true;
  return true;
}
bool ProjectBuildSettingsSession::ValidateFiles(const ProjectBuildSettings &settings,
                                                std::string *error) const {
  if (!Live(error) || !ValidateProjectBuildSettings(settings, error))
    return false;
  for (const auto &scene : settings.scenes)
    if (!File(root_, scene.path, scene.enabled, error))
      return false;
  return true;
}
bool ProjectBuildSettingsSession::Reconcile(ProjectBuildSettings &settings,
                                            const ContentBrowserModel &content,
                                            std::string *error) const {
  auto candidate = settings;
  for (auto &scene : candidate.scenes) {
    if (const auto *item = content.Find(scene.id)) {
      if (item->type != ".scene")
        return Fail(error, "Build scene ID refers to another asset type.");
      if (!scene.path.starts_with("Content/") && scene.path != detail::PathUtf8(item->path))
        return Fail(error, "Explicit project scene ID conflicts with a different catalog asset.");
      scene.path = detail::PathUtf8(item->path);
    }
    for (const auto &item : content.Items())
      if (Fold(detail::PathUtf8(item.path)) == Fold(scene.path) && item.id != scene.id)
        return Fail(error, "Build scene identity conflicts with the Content catalog.");
    if (scene.enabled && scene.path.starts_with("Content/") && !content.Find(scene.id))
      return Fail(error, "Content scene identity is missing; refresh or repair the catalog.");
  }
  if (!ValidateProjectBuildSettings(candidate, error))
    return false;
  settings = std::move(candidate);
  return true;
}
std::optional<BuildScene> ProjectBuildSettingsSession::Enroll(std::string_view relative,
                                                              const ContentBrowserModel &content,
                                                              std::string *error) const {
  if (error)
    error->clear();
  if (!Live(error) || !SafePath(relative) || !File(root_, relative, true, error)) {
    if (error && error->empty())
      *error = "Choose a saved project-relative .scene file.";
    return {};
  }
  for (const auto &item : content.Items())
    if (detail::PathUtf8(item.path) == relative && item.type == ".scene")
      return BuildScene{item.id, std::string(relative), true};
  if (relative.starts_with("Content/")) {
    Fail(error, "Import or open this Content scene before adding its persistent identity.");
    return {};
  }
  if (settings_)
    for (const auto &scene : settings_->scenes)
      if (scene.path == relative)
        return scene;
  // Initial enrollment only. Stored UUIDs remain authoritative across explicit path repairs.
  auto high = project_.high ^ 1469598103934665603ULL, low = project_.low ^ 1099511628211ULL;
  for (const unsigned char c : std::string("build-scene:") + std::string(relative)) {
    high = (high ^ c) * 1099511628211ULL;
    low = (low ^ c) * 0x9e3779b185ebca87ULL;
  }
  if (!high && !low)
    low = 1;
  return BuildScene{{high, low}, std::string(relative), true};
}
bool ProjectBuildSettingsSession::Save(const ProjectBuildSettings &settings,
                                       const ContentBrowserModel &content, std::string *error) {
  if (error)
    error->clear();
  if (!ready_ || !Live(error) || !workspace_.Writable())
    return Fail(error, "Reload valid settings in a writable project with resolved recovery.");
  auto candidate = settings;
  if (!Reconcile(candidate, content, error) || !ValidateFiles(candidate, error))
    return false;
  const auto encoded = SerializeProjectBuildSettings(candidate, error);
  std::optional<std::string> current;
  if (!encoded || !Read(current, error))
    return false;
  if (current != baseline_)
    return Fail(error, "Build settings changed on disk. Reload before saving; original preserved.");
  if (!detail::AtomicWrite(root_ / "project.build.json", *encoded, error))
    return false;
  baseline_ = *encoded;
  settings_ = std::move(candidate);
  return true;
}
} // namespace nexora::editor
