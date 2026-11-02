#pragma once
#include "InstalledGameBuildTools.h"
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
inline std::string BuildJsonPath(const std::filesystem::path &path) {
  const auto bytes = path.generic_u8string();
  constexpr char hex[] = "0123456789abcdef";
  std::string value{"\""};
  for (const auto raw : bytes) {
    const auto byte = static_cast<unsigned char>(raw);
    if (byte == '"' || byte == '\\') {
      value += '\\';
      value += static_cast<char>(byte);
    } else if (byte < 0x20) {
      value += "\\u00";
      value += hex[byte >> 4];
      value += hex[byte & 15];
    } else
      value += static_cast<char>(byte);
  }
  return value + '"';
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
        name != "--script-project" && name != "--cmake" && name != "--ninja" && name != "--zig" &&
        name != "--build-records") {
      std::cerr << "Unknown game build option: " << name << '\n';
      return 2;
    }
  }
  const bool scripted = options.contains("--script-project");
  if (!options.contains("--build-game") || !options.contains("--output") ||
      scene_only == scripted ||
      (!options.contains("--runtime-profile") &&
       (options.contains("--cmake") || options.contains("--ninja") || options.contains("--zig"))) ||
      (scripted && options.contains("--runtime-profile") &&
       (!options.contains("--cmake") || !options.contains("--ninja") ||
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
  const auto tools =
      options.contains("--runtime-profile")
          ? ReadGameBuildToolchain(absolute("--runtime-profile"), absolute("--cmake"),
                                   absolute("--ninja"), absolute("--zig"), &error)
          : InstalledGameBuildTools(&error);
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
  request.records_parent = absolute("--build-records");
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
  const auto &receipt = *prepared->Receipt();
  std::cout << "{\"status\":\"PUBLISHED_GAME\",\"source_required_at_runtime\":false,\"receipt\":"
            << BuildJsonPath(receipt.file) << ",\"records\":" << BuildJsonPath(receipt.directory)
            << "}\n";
  std::cout.flush();
  return std::cout ? 0 : 1;
}
} // namespace nexora::editor::preview
