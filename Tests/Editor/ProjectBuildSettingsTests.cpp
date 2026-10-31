#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/ProjectBuildSettings.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file << bytes;
  Require(static_cast<bool>(file), "fixture write failed");
}
void Codec() {
  const runtime::AssetUuid a{1, 2}, b{3, 4};
  editor::ProjectBuildSettings settings{
      b, {{a, "Content/Title.scene", true}, {b, "Content/Level01.scene", true}}};
  auto bytes = editor::SerializeProjectBuildSettings(settings);
  Require(bytes && editor::ParseProjectBuildSettings(*bytes) == settings, "settings roundtrip");
  Require(editor::SerializeProjectBuildSettings(*editor::ParseProjectBuildSettings(*bytes)) ==
              bytes,
          "canonical encoding unstable");
  Require(bytes->find("\"schema\": 1") != bytes->npos &&
              editor::ParseProjectBuildSettings(*bytes)->build == editor::GameBuildSettings{},
          "legacy settings no longer default to scene-only");
  auto scripted = settings;
  scripted.build = {editor::GameBuildMode::ScriptProject, "Scripts/game project.json",
                    "Build/My Game"};
  const auto script_bytes = editor::SerializeProjectBuildSettings(scripted);
  Require(script_bytes && script_bytes->find("\"schema\": 2") != script_bytes->npos &&
              editor::ParseProjectBuildSettings(*script_bytes) == scripted,
          "versioned script configuration lost on roundtrip");
  for (const auto &[before, after] : std::array<std::pair<std::string_view, std::string_view>, 5>{
           {{"\"mode\": \"script-project\"", "\"mode\": \"unknown\""},
            {"\"schema\": 2", "\"schema\": 3"},
            {"\"output\": \"Build/My Game\"",
             "\"output\": \"Build/My Game\", \"output\": \"Other\""},
            {"\"script_manifest\"", "\"unknown\""},
            {"\"schema\": 2", "\"schema\": 1"}}}) {
    auto malformed = *script_bytes;
    malformed.replace(malformed.find(before), before.size(), after);
    Require(!editor::ParseProjectBuildSettings(malformed),
            "malformed build configuration accepted");
  }
  for (const auto *path : {"../Game", "/Game", "Build//Game", ".git/Game", ".nexora/scenes/Game",
                           "Build/CON", "Build/Game/", "Build/Bad\\Name"}) {
    auto bad = scripted;
    bad.build.output = path;
    Require(!editor::SerializeProjectBuildSettings(bad), "unsafe configured output accepted");
  }
  for (const auto *path : {"project.json", "../Scripts/project.json", "Scripts/game.zig",
                           ".nexora/scenes/project.json", "Scripts/NUL.json"}) {
    auto bad = scripted;
    bad.build.script_manifest = path;
    Require(!editor::SerializeProjectBuildSettings(bad), "unsafe manifest accepted");
  }
  scripted.build.output = "Build/\xe9\x81\x8a\xe6\x88\xb2";
  scripted.build.script_manifest = "Scripts/\xe8\x85\xb3\xe6\x9c\xac/project.json";
  Require(editor::ParseProjectBuildSettings(*editor::SerializeProjectBuildSettings(scripted)) ==
              scripted,
          "Unicode script configuration roundtrip failed");
  scripted.build.mode = editor::GameBuildMode::SceneOnly;
  Require(editor::ParseProjectBuildSettings(*editor::SerializeProjectBuildSettings(scripted)) ==
              scripted,
          "scene-only choice discarded remembered manifest/output");
  auto invalid_config = scripted;
  invalid_config.build.script_manifest = "Scripts/\xff.json";
  Require(!editor::SerializeProjectBuildSettings(invalid_config),
          "invalid UTF-8 manifest accepted");
  invalid_config = scripted;
  invalid_config.build.output = std::string(1025, 'a');
  Require(!editor::SerializeProjectBuildSettings(invalid_config),
          "oversized build output accepted");
  auto invalid = settings;
  invalid.scenes[1].enabled = false;
  Require(!editor::SerializeProjectBuildSettings(invalid), "disabled startup accepted");
  invalid = settings;
  invalid.scenes[1].id = a;
  Require(!editor::SerializeProjectBuildSettings(invalid), "duplicate UUID accepted");
  for (const auto *path :
       {"../Escape.scene", "Content/../Escape.scene", "/Absolute.scene", "C:/Root.scene",
        "Content\\Hidden.scene", "Content//Bad.scene", "Content/CON.scene", ".git/Hidden.scene",
        ".nexora/workspace.scene", "Content/Trailing./Bad.scene", "Content/Foo.txt"}) {
    invalid = settings;
    invalid.scenes[1].path = path;
    Require(!editor::SerializeProjectBuildSettings(invalid), "unsafe relative path accepted");
  }
  invalid = settings;
  invalid.scenes[1].path = "content/TITLE.scene";
  Require(!editor::SerializeProjectBuildSettings(invalid), "case alias accepted");
  invalid = settings;
  invalid.scenes[1].path = std::string(1025, 'a') + ".scene";
  Require(!editor::SerializeProjectBuildSettings(invalid), "oversized path accepted");
  for (const auto *replacement : {"\"schema\": 2", "\"schema\": 1, \"schema\": 1", "\"unknown\": 1",
                                  "\"schema\": 1.0", "\"schema\": 01"}) {
    auto text = *bytes;
    text.replace(text.find("\"schema\": 1"), 11, replacement);
    Require(!editor::ParseProjectBuildSettings(text), "unknown/duplicate/invalid schema accepted");
  }
  Require(!editor::ParseProjectBuildSettings(*bytes + "garbage") &&
              !editor::ParseProjectBuildSettings(std::string(256 * 1024 + 1, ' ')),
          "trailing/oversized JSON accepted");
  auto unicode = *bytes;
  unicode.replace(unicode.find("Title.scene"), 11, "\\u9670\\u967d\\ud83c\\udf19.scene");
  const auto parsed = editor::ParseProjectBuildSettings(unicode);
  Require(parsed && parsed->scenes[0].path ==
                        std::string("Content/\xe9\x99\xb0\xe9\x99\xbd\xf0\x9f\x8c\x99.scene"),
          "Unicode/surrogate JSON decode failed");
  unicode.replace(unicode.find("\\ud83c\\udf19"), 12, "\\ud83c\\u0041");
  Require(!editor::ParseProjectBuildSettings(unicode), "unpaired surrogate accepted");
  invalid = settings;
  invalid.scenes.resize(257, settings.scenes.front());
  Require(!editor::SerializeProjectBuildSettings(invalid), "excessive scene count accepted");
  auto padded = *bytes;
  padded.resize(editor::ProjectBuildSettings::kMaximumBytes, ' ');
  Require(editor::ParseProjectBuildSettings(padded) == settings, "exact byte boundary rejected");
  padded += ' ';
  Require(!editor::ParseProjectBuildSettings(padded), "byte boundary exceeded");
}
void Session(const std::filesystem::path &root) {
  editor::ProjectWorkspace writer;
  Require(writer.Create(root, "Build Settings"), "project create");
  const auto identity = Read(root / "project.nexora");
  Write(root / ".nexora/scenes/Title.scene", "saved scene A");
  Write(root / "Scenes/Level01.scene", "saved scene B");
  Write(root / ".nexora/scene-session.ini", "editor session must not change");
  editor::ContentBrowserModel catalog;
  Require(catalog.Reset({}, 1), "catalog reset");
  editor::ProjectBuildSettingsSession session(writer);
  std::string error;
  Require(session.Reload(&error) && error.empty() && !session.Settings(),
          "legacy missing settings");
  const auto a = session.Enroll(".nexora/scenes/Title.scene", catalog);
  const auto b = session.Enroll("Scenes/Level01.scene", catalog);
  Require(a && b && a->id != b->id, "root scene enrollment");
  editor::ProjectBuildSettings settings{b->id, {*a, *b}};
  settings.build = {editor::GameBuildMode::ScriptProject, "Scripts/project.json",
                    "Build/Native Game"};
  if (!session.Save(settings, catalog, &error))
    throw std::runtime_error(error);
  auto saved = Read(root / "project.build.json");
  Require(Read(root / "project.nexora") == identity &&
              Read(root / ".nexora/scene-session.ini") == "editor session must not change",
          "game settings modified identity/editor last scene");
  editor::ProjectBuildSettingsSession reopened(writer);
  Require(reopened.Reload() && reopened.Settings() == settings &&
              reopened.Enroll(a->path, catalog)->id == a->id,
          "startup/enrollment not retained");
  Write(root / "project.build.json", saved + " ");
  settings.start_scene = a->id;
  Require(!session.Save(settings, catalog, &error) && error.find("changed on disk") != error.npos &&
              Read(root / "project.build.json") == saved + " ",
          "external settings overwritten");
  Require(session.Reload() && session.Save(settings, catalog),
          "explicit reload did not resolve baseline");
  saved = Read(root / "project.build.json");
  Write(root / "project.build.json.tmp", "another owner");
  Require(!session.Save(settings, catalog) && Read(root / "project.build.json") == saved &&
              Read(root / "project.build.json.tmp") == "another owner",
          "occupied staging destroyed");
  std::filesystem::remove(root / "project.build.json.tmp");
  std::filesystem::remove(root / "Scenes/Level01.scene");
  Require(!session.Save(settings, catalog) && Read(root / "project.build.json") == saved,
          "enabled missing scene accepted");
  settings.scenes[1].enabled = false;
  Require(session.Save(settings, catalog), "disabled missing scene prevents repair");
  editor::ProjectWorkspace reader;
  Require(reader.Open(root, editor::ProjectAccess::ReadOnly), "read-only project open");
  editor::ProjectBuildSettingsSession observer(reader);
  Require(observer.Reload() && observer.Settings() == settings && !observer.Save(settings, catalog),
          "read-only settings mutated");
  Write(root / "project.build.json", "{broken}");
  Require(!session.Reload() && !session.Ready() && !session.Save(settings, catalog) &&
              Read(root / "project.build.json") == "{broken}",
          "corrupt settings overwritten");
  Write(root / "project.build.json", *editor::SerializeProjectBuildSettings(settings));
  Require(session.Reload(), "restore valid settings");
  Write(root / "Content/Old.scene", "content scene");
  const runtime::AssetUuid content_id{12, 34};
  Require(catalog.Reset(std::array{editor::ContentItem{
                            content_id, "Content/Old.scene", ".scene", "saved", {}, {}}},
                        1),
          "persistent content identity fixture");
  const auto entry = session.Enroll("Content/Old.scene", catalog);
  Require(entry && entry->id == content_id, "Content identity not reused");
  auto renamed = settings;
  renamed.scenes.push_back(*entry);
  Require(catalog.Rename(content_id, "Renamed.scene"), "catalog rename");
  std::filesystem::rename(root / "Content/Old.scene", root / "Content/Renamed.scene");
  Require(session.Reconcile(renamed, catalog) && renamed.scenes.back().id == content_id &&
              renamed.scenes.back().path == "Content/Renamed.scene" &&
              session.Save(renamed, catalog),
          "Content rename lost stable build identity");
  auto conflicting = renamed;
  conflicting.scenes.back().id = {99, 88};
  Require(!session.Reconcile(conflicting, catalog), "same-path/different-ID accepted");
  conflicting = renamed;
  conflicting.scenes.back().path = "Other.scene";
  Require(!session.Reconcile(conflicting, catalog), "root identity hijacked catalog identity");
  auto excluded = renamed;
  excluded.scenes.back().enabled = false;
  editor::ContentBrowserModel missing_catalog;
  Require(missing_catalog.Reset({}, 1) && session.Reconcile(excluded, missing_catalog),
          "excluded scene with missing catalog entry prevented list repair");
  excluded.scenes.back().enabled = true;
  Require(!session.Reconcile(excluded, missing_catalog),
          "enabled missing catalog identity accepted");
  const std::string unicode_path = "Scenes/\xe9\x99\xb0\xe9\x99\xbd.scene";
  Write(root / std::filesystem::path(std::u8string(unicode_path.begin(), unicode_path.end())),
        "unicode scene");
  const auto unicode_entry = session.Enroll(unicode_path, catalog);
  Require(unicode_entry.has_value(), "native Unicode path enrollment failed");
  renamed.scenes.push_back(*unicode_entry);
  Require(session.Save(renamed, catalog) && session.Reload() && session.Settings() == renamed,
          "native Unicode save/reopen failed");
  settings = renamed;
  std::error_code link_error;
  std::filesystem::create_directory_symlink(root / "Content", root / "Alias", link_error);
  if (!link_error) {
    auto alias = settings;
    alias.scenes.back().path = "Alias/Renamed.scene";
    Require(!session.ValidateFiles(alias), "scene symlink traversal accepted");
    std::filesystem::remove(root / "Alias");
  } else
    std::cout << "Scene alias fixture unavailable on this host: " << link_error.message() << '\n';
  Write(root / ".nexora/workspace.recovery", "schema=1\n");
  const auto before_recovery = Read(root / "project.build.json");
  Require(!session.Save(settings, catalog) && !session.Reload() &&
              Read(root / "project.build.json") == before_recovery,
          "recovery allowed settings mutation");
  std::filesystem::remove(root / ".nexora/workspace.recovery");
  reader = editor::ProjectWorkspace{};
  writer = editor::ProjectWorkspace{};
  Require(!session.Reload(), "stale project session accepted");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-build-settings-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Codec();
    Session(root);
    std::filesystem::remove_all(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 1;
  }
}
