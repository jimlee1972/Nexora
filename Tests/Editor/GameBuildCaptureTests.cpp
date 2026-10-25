#include "Nexora/Editor/GameBuildCapture.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"
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
void Write(const std::filesystem::path &path, std::string_view text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.close();
  Require(static_cast<bool>(out), "fixture write failed");
}
std::string Text(const editor::GameBuildSource &source) {
  Require(source.bytes.has_value(), "fixture baseline missing");
  return {reinterpret_cast<const char *>(source.bytes->data()), source.bytes->size()};
}
void Run(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Build capture"), "workspace");
  std::filesystem::create_directories(root / "Content");
  constexpr std::string_view obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
  Write(root / "Content/Triangle.obj", obj);
  editor::AssetWorkspace assets;
  Require(
      assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
      "asset import");
  editor::ProjectContentSession content;
  Require(content.Open(workspace, assets, 17), "content session");
  editor::MeshAssetCatalog meshes;
  editor::MaterialAssetCatalog materials;
  Require(meshes.PublishContent(content.Browser()) && materials.PublishContent(content.Browser()),
          "catalogs");
  const auto mesh_id = assets.Entries().front().id;
  runtime::World a, b;
  const auto a_scene = a.LoadScene("Title"), b_scene = b.LoadScene("Level01");
  editor::SceneDocument title(a, a_scene), level(b, b_scene);
  for (auto *document : {&title, &level}) {
    Require(document->CreateCamera("Camera") != 0, "camera");
    const auto entity = document->Create("Shared mesh");
    Require(
        document->SetMeshRenderer(*document->Key(entity),
                                  runtime::MeshComponent{runtime::MeshResourceId(mesh_id), {0}}),
        "mesh binding");
  }
  Require(title.Save(root / "Title.scene") && level.Save(root / "Level01.scene"), "saved scenes");
  editor::ProjectBuildSettingsSession settings(workspace);
  Require(settings.Reload(), "settings reload");
  const auto first = settings.Enroll("Title.scene", content.Browser());
  const auto second = settings.Enroll("Level01.scene", content.Browser());
  Require(first && second, "scene enrollment");
  Require(settings.Save({second->id, {*first, *second, {{9, 9}, "Excluded.scene", false}}},
                        content.Browser()),
          "settings save");
  std::string error;
  const auto captured =
      editor::CaptureGameBuild(workspace, content.Browser(), meshes, materials, &error);
  Require(captured.has_value(), error.c_str());
  Require(captured->project.start_scene == second->id && captured->project.scenes.size() == 2 &&
              captured->project.meshes.size() == 1 && captured->sources.size() == 8 &&
              editor::GameBuildSourcesCurrent(root, captured->sources),
          "owning capture/baseline");
  const auto package = editor::CookGameProject(captured->project);
  Require(package && runtime::DecodeGameProjectPackage(*package)->AssetCount() == 3,
          "shared cooked closure");
  const auto find = [&](std::string_view name) -> const editor::GameBuildSource & {
    for (const auto &source : captured->sources)
      if (source.relative == name)
        return source;
    throw std::runtime_error("missing baseline path");
  };
  const auto saved_settings = Text(find("project.build.json"));
  Write(root / "project.build.json", saved_settings + " ");
  Require(!editor::GameBuildSourcesCurrent(root, captured->sources),
          "same-semantics settings edit escaped exact baseline");
  Write(root / "project.build.json", saved_settings);
  Write(root / "Content/Triangle.obj", std::string(obj) + "# external edit\n");
  Require(!editor::GameBuildSourcesCurrent(root, captured->sources) &&
              !editor::CaptureGameBuild(workspace, content.Browser(), meshes, materials),
          "unreimported source admitted");
  Write(root / "Content/Triangle.obj", obj);
  const auto meta_path = std::filesystem::path("Content/Triangle.obj.meta");
  const auto metadata = Text(find("Content/Triangle.obj.meta"));
  Write(root / meta_path, "schema=1\nuuid=00000000-0000-0001-0000-000000000099\ntype=.obj\n");
  Require(!editor::CaptureGameBuild(workspace, content.Browser(), meshes, materials),
          "changed identity admitted");
  Write(root / meta_path, metadata);
  Write(root / "Title.scene.meta", "new optional identity");
  Require(!editor::GameBuildSourcesCurrent(root, captured->sources),
          "previously absent sidecar creation ignored");
  std::filesystem::remove(root / "Title.scene.meta");
  Require(editor::GameBuildSourcesCurrent(root, captured->sources), "restored sources not current");
  for (const auto *path : {"../escape", "LPT1.txt", "name.", "file:stream"})
    Require(!editor::ReadGameBuildSource(root, path, 1024), "unsafe source path admitted");
  Require(!editor::ReadGameBuildSource(root, "project.build.json", 1), "source size guard");
  auto oversized = captured->sources;
  oversized.resize(editor::kMaximumGameBuildSourceFiles + 1);
  Require(!editor::GameBuildSourcesCurrent(root, oversized), "source count guard");
  meshes.Clear();
  materials.Clear();
  Require(editor::CookGameProject(captured->project) == package,
          "capture retained catalog borrows");
}
} // namespace
int main() {
  const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
                    ("nexora-build-capture-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root);
    std::filesystem::remove_all(root);
    std::cout << "Owning game build capture, exact sources, identities and stale catalog rejection "
                 "passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return 1;
  }
}
