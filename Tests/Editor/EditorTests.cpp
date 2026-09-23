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

  runtime::PlaySession play(world);
  Require(play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }) && play.Pause() &&
              play.Step() && play.Stop(),
          "PIE controls failed");
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
