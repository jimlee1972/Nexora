#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

int Run() {
  using namespace nexora;
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() /
                    ("nexora-editor-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    fs::path root;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(root, ec);
    }
  } cleanup{root};

  editor::ProductShell shell;
  Require(editor::ProductShell::Panels().size() == 8 &&
              editor::ProductShell::IsStablePanelId("nexora.scene") &&
              !editor::ProductShell::IsStablePanelId("Scene"),
          "stable panel contract failed");
  Require(shell.RouteCommand("editor.scene.save") && shell.LastCommand() == "editor.scene.save" &&
              !shell.RouteCommand("game.save"),
          "command routing failed");

  editor::ProjectWorkspace project;
  std::string error;
  Require(project.Create(root, "Preview", &error), "project creation failed");
  const std::vector<std::string> documents{"Content/Main.scene", "Content/Hero.prefab"};
  Require(project.SaveWorkspace(documents, &error), "workspace save failed");
  editor::ProjectWorkspace reopened;
  Require(reopened.Open(root, &error) && reopened.Project().name == "Preview" &&
              reopened.OpenDocuments().size() == 2,
          "project open failed");
  {
    std::ofstream recovery(root / ".nexora/workspace.recovery", std::ios::trunc);
    recovery << "schema=1\ndocument=Content/Recovered.scene\n";
  }
  Require(reopened.RecoverWorkspace(&error) &&
              reopened.OpenDocuments().front() == "Content/Recovered.scene",
          "workspace recovery failed");

  {
    std::ofstream(root / "Content/Hero.mesh") << "mesh";
    std::ofstream(root / "Content/Hero.material") << "material";
  }
  editor::AssetWorkspace assets;
  std::size_t progress{};
  Require(assets.ImportTree(root / "Content", {},
                            [&](std::size_t current, std::size_t) { progress = current; }) &&
              assets.Entries().size() == 2 && progress == 2,
          "asset import failed");
  Require(assets.Search("hero").size() == 2 && assets.Search({}, ".mesh").size() == 1 &&
              assets.Find(assets.Entries().front().id),
          "asset search failed");
  editor::AssetWorkspace cancelled;
  Require(cancelled.ImportTree(root / "Content", [] { return true; }) &&
              cancelled.Entries().front().state == editor::ImportState::Cancelled,
          "asset cancellation failed");

  runtime::World world;
  const auto scene = world.LoadScene("Main");
  Require(world.Activate(scene), "scene activation failed");
  editor::SceneDocument document(world, scene);
  const auto parent = document.Create("Parent");
  const auto child = document.Create("Child", parent);
  Require(parent && child && document.Parent(child) == parent && !document.Reparent(parent, child),
          "hierarchy cycle policy failed");
  const std::vector<runtime::Id> selected{child};
  Require(document.Select(selected) && document.SetTransform(child, {1, 2, 3}) &&
              document.CopySelection() && document.Paste(),
          "scene editing failed");
  Require(document.Selection().size() == 1 &&
              document.Name(document.Selection().front()) == "Child Copy",
          "clipboard did not create a stable selection");
  Require(document.Undo(), "scene undo failed");
  const auto scene_path = root / "Content/Main.scene";
  Require(document.Save(scene_path), "scene atomic save failed");
  runtime::World loaded_world;
  const auto placeholder = loaded_world.LoadScene("Placeholder");
  editor::SceneDocument loaded(loaded_world, placeholder);
  Require(loaded.Reload(scene_path) && loaded.Name(child) == "Child", "scene reload failed");
  Require(document.Create("Bad\nName") == 0 && document.Create("Bad\rName") == 0,
          "a node name containing a newline must be rejected, since Save()/Reload() use a "
          "line-oriented format that a newline would silently corrupt");

  runtime::PlaySession play(world);
  Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }) && play.Pause() &&
              play.Step() && play.Stop(),
          "PIE controls failed");

  editor::SpecializedToolRegistry tools;
  Require(
      tools.Register({"material", "Material Graph", editor::CapabilityState::ReadOnly,
                      "renderer graph editing is unavailable"}) &&
          tools.Register({"physics", "Physics Debug", editor::CapabilityState::Implemented, {}}) &&
          !tools.Register({"physics", "Duplicate", editor::CapabilityState::Implemented, {}}) &&
          tools.Find("material")->state == editor::CapabilityState::ReadOnly,
      "specialized tool capability policy failed");

  editor::BuildManifest manifest{
      1,
      {"linux-dev", "linux-x64", "Development", "cmake --build --preset linux-development"},
      {{"bin/game", "sha256:game", 42}}};
  const auto manifest_path = root / "build-manifest.json";
  Require(editor::BuildFrontend::Write(manifest, manifest_path, &error) &&
              fs::file_size(manifest_path) > 0,
          "build manifest failed");
  manifest.artifacts.push_back({"../escape", "bad", 1});
  Require(!editor::BuildFrontend::Validate(manifest, &error), "unsafe build artifact accepted");
#if defined(_WIN32)
  // std::filesystem::path only parses a drive letter as a root-name on Windows,
  // so this rejection is inherently platform-specific and cannot be exercised
  // by the Linux gate.
  manifest.artifacts.back() = {"C:/Windows/System32/evil.dll", "bad", 1};
  Require(!editor::BuildFrontend::Validate(manifest, &error),
          "a Windows drive-letter-rooted artifact path must be rejected even though it starts "
          "with neither '/' nor '\\\\', or it can escape the sandbox root it gets joined onto");
#endif

  editor::ProfileSession profile;
  Require(profile.Add({1, 2.0, 3.0, 100}) && profile.Add({2, 8.0, 4.0, 200}) &&
              !profile.Add({2, 1.0, 1.0, 1}) && profile.Peak()->frame == 2,
          "profile session failed");
  editor::VirtualHierarchy hierarchy(100000);
  Require(hierarchy.Visible(99990, 50) == std::pair<std::size_t, std::size_t>{99990, 10},
          "virtual hierarchy bounds failed");
  editor::ExtensionPolicy policy{true, {"Nexora"}};
  Require(policy.Allows("Nexora", true) && !policy.Allows("Nexora", false) &&
              !policy.Allows("Unknown", true),
          "extension signature policy failed");
  editor::TelemetryConsent telemetry;
  Require(!telemetry.Record("startup") && telemetry.Events().empty(), "telemetry was not opt-in");
  telemetry.Set(true);
  Require(telemetry.Record("startup") && telemetry.Events().size() == 1, "opt-in telemetry failed");
  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    return error.what()[0] == '\0' ? 0 : 1;
  }
}
