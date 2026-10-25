#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/GameProjectExport.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"

#include <algorithm>
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
void Run(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Multi Scene"), "workspace fixture");
  editor::ContentBrowserModel content;
  Require(content.Reset({}, 1), "empty content catalog");
  editor::MeshAssetCatalog meshes;
  editor::MaterialAssetCatalog materials;
  Require(meshes.PublishContent(content) && materials.PublishContent(content), "catalogs");
  runtime::World a, b;
  const auto a_id = a.LoadScene("Title"), b_id = b.LoadScene("Level01");
  editor::SceneDocument title(a, a_id), level(b, b_id);
  Require(title.CreateCamera("Title camera") && level.CreateCamera("Level camera"),
          "camera fixture");
  Require(title.Save(root / "Title.scene") && level.Save(root / "Level01.scene"), "saved scenes");
  editor::ProjectBuildSettingsSession session(workspace);
  Require(session.Reload(), "load legacy settings");
  const auto first = session.Enroll("Title.scene", content),
             second = session.Enroll("Level01.scene", content);
  Require(first && second, "root scene identity enrollment");
  editor::ProjectBuildSettings settings{second->id, {*first, *second}};
  // Disabled missing authored files do not enter the capture or game package.
  settings.scenes.push_back({{9, 9}, "Excluded.scene", false});
  Require(session.Save(settings, content), "save build settings");
  std::string error;
  const auto captured =
      editor::CaptureGameProject(workspace, settings, content, meshes, materials, &error);
  Require(captured && captured->scenes.size() == 2 && captured->start_scene == second->id,
          "saved build list/startup/exclusion not used by capture");
  const auto bytes = editor::CookGameProject(*captured, &error);
  Require(bytes.has_value(), error.c_str());
  const auto loaded = runtime::DecodeGameProjectPackage(*bytes, &error);
  Require(loaded && loaded->Scenes().size() == 2 && loaded->AssetCount() == 2 &&
              loaded->StartupScene() == second->id,
          "actual saved scenes did not reach Runtime");
  // Supply one shared imported mesh, referenced from both isolated authoring scenes.
  const runtime::AssetUuid mesh_id{55, 66};
  const auto geometry = editor::ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
  Require(geometry.geometry.has_value(), "shared OBJ import");
  const auto a_entity = title.Create("Mesh A"), b_entity = level.Create("Mesh B");
  Require(title.SetMeshRenderer(*title.Key(a_entity),
                                runtime::MeshComponent{runtime::MeshResourceId(mesh_id), {0}}) &&
              level.SetMeshRenderer(*level.Key(b_entity),
                                    runtime::MeshComponent{runtime::MeshResourceId(mesh_id), {0}}),
          "shared mesh bindings");
  auto pure = *captured;
  pure.meshes.push_back({mesh_id, *geometry.geometry});
  pure.scenes[0].capture = *title.CaptureRuntimeScene();
  pure.scenes[1].capture = *level.CaptureRuntimeScene();
  const auto shared = editor::CookGameProject(pure, &error);
  Require(shared.has_value(), error.c_str());
  const auto shared_loaded = runtime::DecodeGameProjectPackage(*shared);
  Require(shared_loaded && shared_loaded->AssetCount() == 3 &&
              shared_loaded->ResolveScene(first->id)->RenderItems()[0].mesh ==
                  shared_loaded->ResolveScene(second->id)->RenderItems()[0].mesh,
          "producer did not deduplicate shared mesh bytes/ownership");
  std::ranges::reverse(pure.scenes);
  Require(editor::CookGameProject(pure) == shared, "producer scene order changed canonical bytes");
  pure.start_scene = {100, 200};
  Require(!editor::CookGameProject(pure), "producer accepted unlisted startup");
  auto missing = *captured;
  missing.scenes[0].capture = *title.CaptureRuntimeScene();
  Require(!editor::CookGameProject(missing), "producer accepted unresolved mesh");
  std::filesystem::remove(root / "Title.scene");
  Require(!editor::CaptureGameProject(workspace, settings, content, meshes, materials),
          "capture accepted missing enabled scene");
  const auto retained = runtime::DecodeGameProjectPackage(*shared);
  Require(retained && retained->ResolveScene(first->id),
          "cooked consumer required authoring source");
  workspace = editor::ProjectWorkspace{};
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-game-export-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root);
    std::filesystem::remove_all(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 1;
  }
}
