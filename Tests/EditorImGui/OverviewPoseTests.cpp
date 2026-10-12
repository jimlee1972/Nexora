#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
constexpr std::size_t kCount = 100000;
constexpr runtime::Id kFirst = 100;
std::string Source(bool deep) {
  std::string source = "NEXORA_EDITOR_SCENE 3\n";
  for (std::size_t i = 0; i < kCount; ++i)
    source += "node " + std::to_string(kFirst + i) + ' ' +
              std::to_string(deep && i ? kFirst + i - 1 : 0) + " Node " + std::to_string(i) + '\n';
  source += "world\nNEXORA_SCENE 3 \"Overview poses\" 0 " + std::to_string(kCount) + '\n';
  for (std::size_t i = 0; i < kCount; ++i) {
    const auto x = deep && i ? 1 : static_cast<long long>(i) - static_cast<long long>(kCount - 1);
    source += std::to_string(kFirst + i) + ' ' + std::to_string(deep && i ? kFirst + i - 1 : 0) +
              ' ' + std::to_string(x) + " 0 0 0 0 0 1 1 1 1 0 0 0 60 .1 1000 1 0 0\n";
  }
  return source;
}
void Large(bool deep, float dpi) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-overview-poses-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  runtime::World world;
  const auto scene_id = world.LoadScene("Overview poses");
  editor::SceneDocument scene{world, scene_id};
  Require(workspace.Create(root, "Overview poses") && world.Activate(scene_id),
          "overview fixture setup failed");
  const auto path = root / "Content/Large.scene";
  const auto source = Source(deep);
  std::ofstream output(path, std::ios::binary);
  output << source;
  output.close();
  Require(!output.fail() && scene.Reload(path) && scene.Nodes().size() == kCount,
          "real 100k overview scene failed to load");
  const auto poses = scene.WorldPoses();
  Require(poses && poses->size() == kCount && poses->back().transform.x == 0,
          "owning document world poses missing their exact final origin");
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  ui.SetDisplay(1280 * dpi, 900 * dpi, dpi);
  Access::ConfigureSyntheticInput(ui);
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, &workspace);
    static_cast<void>(ui.EndFrame());
  };
  for (int i = 0; i < 4; ++i)
    draw();
  Access::FocusScene(ui);
  draw();
  draw();
  const auto key = scene.Key(kFirst + kCount - 1);
  Require(key.has_value(), "final node identity missing");
  const auto marker = Access::SceneMarkerPosition(ui, *key);
  const auto canvas = ui.SceneCanvasViewport();
  Require(marker && canvas &&
              std::abs((*marker)[0] -
                       (static_cast<float>(canvas->x) + static_cast<float>(canvas->width) * .5F) /
                           dpi) < 1.5F &&
              std::abs((*marker)[1] -
                       (static_cast<float>(canvas->y) + static_cast<float>(canvas->height) * .5F) /
                           dpi) < 1.5F &&
              !Access::SceneMarkerPosition(ui, *scene.Key(kFirst)),
          "overview did not render the exact leaf origin or cull the distant root");
  const auto before_generation = scene.Generation();
  Require(scene.Reload(path) && scene.Generation() != before_generation,
          "document replacement failed");
  draw();
  Require(!Access::SceneMarkerPosition(ui, *key) && poses->back().transform.x == 0,
          "stale markers survived reload or owning pose observations changed");
  std::ifstream input(path, std::ios::binary);
  const std::string retained{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>{}};
  Require(retained == source && !scene.Dirty() && !scene.Undo() && !scene.Redo(),
          "overview observation changed source, saved baseline or history");
}
void UntrackedAncestor() {
  runtime::World world;
  const auto id = world.LoadScene("Untracked ancestor");
  Require(world.Activate(id), "untracked fixture setup failed");
  const auto parent = world.CreateEntity(id).id;
  editor::SceneDocument scene{world, id};
  const auto child = scene.Create("Tracked child");
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(parent, {8, 0, -3});
  commands.SetParent(child, parent, false);
  Require(child && commands.Apply(world), "untracked ancestor placement failed");
  const auto poses = scene.WorldPoses();
  Require(poses && poses->size() == 1 && poses->front().id == child &&
              poses->front().transform.x == 8 && poses->front().transform.z == -3,
          "document bulk poses dropped an untracked ancestor or exposed it as a node");
}
} // namespace
int main() {
  try {
    UntrackedAncestor();
    Large(true, 1);
    Large(true, 2);
    Large(false, 1);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
