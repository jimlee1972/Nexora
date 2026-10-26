#pragma once
#include "../Common/ExecutableLocation.h"
#include "EditorGameTools.h"
#include "Nexora/Editor/GameBuild.h"
#include "Nexora/Runtime/GameLaunch.h"

namespace nexora::editor::preview {
inline std::optional<GameBuildToolchain>
InstalledGameBuildToolsAt(const std::filesystem::path &directory, std::string *error) {
  const bool installed = std::filesystem::is_regular_file(directory / "build-info.json");
#if defined(_WIN32)
  const auto cmake_name = "Tools/cmake/bin/cmake.exe";
  const auto ninja_name = "Tools/ninja.exe";
  const auto zig_name = "Tools/zig/zig.exe";
#else
  const auto cmake_name = "Tools/cmake/bin/cmake";
  const auto ninja_name = "Tools/ninja";
  const auto zig_name = "Tools/zig/zig";
#endif
  const auto choose = [&](const char *relative, std::string_view configured) {
    const auto local = directory / relative;
    return installed || std::filesystem::is_regular_file(local)
               ? local
               : std::filesystem::path(std::u8string(configured.begin(), configured.end()));
  };
  const auto internal_profile = directory / "Runtime/GameRuntime.profile";
  auto profile = installed || std::filesystem::is_regular_file(internal_profile)
                     ? internal_profile
                     : directory / "GameRuntime.profile";
  if (!installed && !std::filesystem::is_regular_file(profile) &&
      !kConfiguredGameRuntimeProfile.empty())
    profile = std::filesystem::path(
        std::u8string(kConfiguredGameRuntimeProfile.begin(), kConfiguredGameRuntimeProfile.end()));
  return ReadGameBuildToolchain(profile, choose(cmake_name, kConfiguredGameCmake),
                                choose(ninja_name, kConfiguredGameNinja),
                                choose(zig_name, kConfiguredGameZig), error);
}
inline std::optional<GameBuildToolchain> InstalledGameBuildTools(std::string *error) {
  const auto directory = apps::ExecutableDirectory();
  if (!directory) {
    if (error)
      *error = "Editor installation directory is unavailable.";
    return {};
  }
  return InstalledGameBuildToolsAt(*directory, error);
}
} // namespace nexora::editor::preview
