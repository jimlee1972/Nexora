#pragma once
#include "Nexora/Editor/GameBuild.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"
#include <iostream>
#include <map>

namespace nexora::editor::preview {
inline std::filesystem::path BuildArgumentPath(std::string_view text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
// The Editor executable is also the CLI build entry. Both hosts call PrepareGameBuild/Publish;
// this command supplies no alternate cooker, SDK compiler adapter or publication implementation.
inline int RunGameBuildCommand(int argc, char **argv) {
  std::map<std::string, std::string> options;
  bool scene_only{};
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--scene-only" && !scene_only) {
      scene_only = true;
      continue;
    }
    const auto separator = argument.find('=');
    if (separator == argument.npos || separator + 1 == argument.size() ||
        !options.emplace(argument.substr(0, separator), argument.substr(separator + 1)).second) {
      std::cerr << "Game build options require unique --name=value arguments.\n";
      return 2;
    }
  }
  for (const auto &[name, value] : options) {
    static_cast<void>(value);
    if (name != "--build-game" && name != "--output" && name != "--runtime-profile" &&
        name != "--script-project" && name != "--cmake" && name != "--ninja" && name != "--zig") {
      std::cerr << "Unknown game build option: " << name << '\n';
      return 2;
    }
  }
  const bool scripted = options.contains("--script-project");
  if (!options.contains("--build-game") || !options.contains("--output") ||
      !options.contains("--runtime-profile") || scene_only == scripted ||
      (scripted && (!options.contains("--cmake") || !options.contains("--ninja") ||
                    !options.contains("--zig")))) {
    std::cerr << "NexoraEditor --build-game=PROJECT --output=DIRECTORY --runtime-profile=PROFILE "
                 "(--scene-only | --script-project=RELATIVE_MANIFEST --cmake=PATH --ninja=PATH "
                 "--zig=PATH)\n";
    return 2;
  }
  const auto absolute = [&](std::string_view key) {
    const auto found = options.find(std::string(key));
    return found == options.end()
               ? std::filesystem::path{}
               : std::filesystem::absolute(BuildArgumentPath(found->second)).lexically_normal();
  };
  std::string error;
  const auto fail = [&] {
    std::cerr << "Game build failed: " << error << '\n';
    return 1;
  };
  const auto tools = ReadGameBuildToolchain(absolute("--runtime-profile"), absolute("--cmake"),
                                            absolute("--ninja"), absolute("--zig"), &error);
  if (!tools)
    return fail();
  ProjectWorkspace workspace;
  if (!workspace.Open(absolute("--build-game"), ProjectAccess::ReadOnly, &error))
    return fail();
  AssetWorkspace assets;
  if (!assets.ImportTree(workspace.Root() / "Content", {}, {},
                         AssetIdentityMode::PersistentReadOnly, &error))
    return fail();
  ProjectContentSession content;
  if (!content.Open(workspace, assets, 1, false, &error))
    return fail();
  MeshAssetCatalog meshes;
  MaterialAssetCatalog materials;
  if (!meshes.PublishContent(content.Browser(), &error) ||
      !materials.PublishContent(content.Browser(), &error))
    return fail();
  auto capture = CaptureGameBuild(workspace, content.Browser(), meshes, materials, &error);
  if (!capture)
    return fail();
  GameBuildRequest request;
  request.project_root = workspace.Root();
  const auto output = absolute("--output");
  request.temporary_parent = output.parent_path();
  request.capture = std::move(*capture);
  request.tools = *tools;
  request.mode = scene_only ? GameBuildMode::SceneOnly : GameBuildMode::ScriptProject;
  if (scripted)
    request.script_manifest = BuildArgumentPath(options.at("--script-project"));
  auto prepared = PrepareGameBuild(std::move(request), {}, &error);
  if (!prepared)
    return fail();
  if (!prepared->Publish(
          output, {},
          [&] { return !workspace.HasRecoveryJournal() && !workspace.HasExternalChange(); },
          &error))
    return fail();
  std::cout << "{\"status\":\"PUBLISHED_GAME\",\"source_required_at_runtime\":false}\n";
  std::cout.flush();
  return std::cout ? 0 : 1;
}
} // namespace nexora::editor::preview
