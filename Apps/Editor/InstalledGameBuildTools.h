#pragma once
#include "../Common/ExecutableLocation.h"
#include "EditorGameTools.h"
#include "Nexora/Editor/GameBuild.h"
#include "Nexora/Runtime/GameLaunch.h"

namespace nexora::editor::preview {
inline std::optional<GameBuildToolchain> InstalledGameBuildTools(std::string *error) {
  const auto directory = apps::ExecutableDirectory();
  if (!directory) {
    if (error)
      *error = "Editor installation directory is unavailable.";
    return {};
  }
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
    const auto local = *directory / relative;
    return std::filesystem::is_regular_file(local)
               ? local
               : std::filesystem::path(std::u8string(configured.begin(), configured.end()));
  };
  const auto internal_profile = *directory / "Runtime/GameRuntime.profile";
  const auto profile = std::filesystem::is_regular_file(internal_profile)
                           ? internal_profile
                           : *directory / "GameRuntime.profile";
  return ReadGameBuildToolchain(profile, choose(cmake_name, kConfiguredGameCmake),
                                choose(ninja_name, kConfiguredGameNinja),
                                choose(zig_name, kConfiguredGameZig), error);
}
} // namespace nexora::editor::preview
