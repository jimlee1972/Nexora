#include "Nexora/Editor/EditorWorkspace.h"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(const std::filesystem::path &path, bool camera, bool child) {
  runtime::World world;
  const auto id = world.LoadScene("Builtin creation");
  editor::SceneDocument scene(world, id);
  const auto parent = scene.Create("Parent");
  Require(scene.SetTransform(parent, {12, 3, -5, 0, 0, 0, 1, -2, 3, 4}) &&
              scene.SetEulerField(std::array{*scene.Key(parent)}, 1, 90),
          "Builtin parent fixture failed");
  const auto before = world.SaveScene(id);
  const auto created = camera ? scene.CreateCamera("Camera", child ? parent : 0)
                              : scene.CreateLight("Light", child ? parent : 0);
  Require(created != 0 && scene.Transform(created) == runtime::Transform{} &&
              scene.Parent(created) == (child ? parent : 0) &&
              scene.Name(created) == (camera ? "Camera" : "Light"),
          "Builtin creation lost initialized pose, parent or name");
  const auto verify = [&] {
    const auto key = *scene.Key(created);
    if (camera) {
      const auto lens = scene.Camera(key);
      Require(lens && lens->vertical_field_of_view == 60 && lens->near_plane == 0.1 &&
                  lens->far_plane == 1000 && !scene.Light(key) && !scene.MeshRenderer(key),
              "Created Camera lost defaults or added unrelated components");
    } else
      Require(scene.Light(key) && scene.Light(key)->intensity == 1 && !scene.Camera(key) &&
                  !scene.MeshRenderer(key),
              "Created Light lost defaults or added unrelated components");
  };
  verify();
  const auto initialized = world.SaveScene(id);
  for (int cycle = 0; cycle < 100; ++cycle) {
    Require(scene.Undo() && world.SaveScene(id) == before && !scene.Key(created) && scene.Redo() &&
                world.SaveScene(id) == initialized &&
                scene.Name(created) == (camera ? "Camera" : "Light"),
            "Builtin creation was not one initialized stable-ID Undo step");
    verify();
  }
  Require(scene.Undo(), "Builtin rejection Redo fixture failed");
  Require(scene.CreateCamera("") == 0 && scene.CreateLight("Bad\nName") == 0 &&
              scene.CreateCamera("Missing parent", created) == 0 &&
              scene.CreateLight("Missing parent", created) == 0 && world.SaveScene(id) == before &&
              scene.Redo(),
          "Rejected builtin creation mutated World or consumed Redo");
  Require(scene.Save(path) && scene.Reload(path), "Builtin persistence failed");
  verify();
  Require(scene.Parent(created) == (child ? parent : 0) && !scene.Dirty(),
          "Builtin reload changed parent or dirty state");
}
void RunRuntime() {
  runtime::World world;
  const auto first = world.LoadScene("First"), second = world.LoadScene("Second");
  runtime::SceneEditor editor(world);
  const auto foreign = editor.CreateEntity(second);
  const auto before = world.SaveScene(first);
  Require(editor.CreateCameraEntity(first, foreign) == 0 &&
              editor.CreateLightEntity(first, foreign) == 0 && editor.CreateCameraEntity(0) == 0 &&
              editor.CreateLightEntity(0) == 0 && world.SaveScene(first) == before,
          "Runtime builtin creation admitted another scene's parent or a missing scene");
  const auto camera = editor.CreateCameraEntity(first);
  Require(camera && editor.Undo() && world.SaveScene(first) == before && editor.Redo() &&
              world.FindEntity(camera),
          "Runtime Camera was not one initialized Undo step");
  const auto with_camera = world.SaveScene(first);
  const auto light = editor.CreateLightEntity(first, camera);
  Require(light && editor.Undo() && world.SaveScene(first) == with_camera && editor.Redo() &&
              world.FindEntity(light)->parent == camera,
          "Runtime Light was not one initialized child Undo step");
}
} // namespace
int main() {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("nexora-builtin-create-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".scene");
  try {
    RunRuntime();
    for (const bool camera : {true, false})
      for (const bool child : {true, false})
        Run(path, camera, child);
    std::filesystem::remove(path);
    std::cout << "Initialized Camera/Light creation contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove(path);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
