#pragma once
#include "../Common/ExecutableLocation.h"
#include "NativeGamePlayer.h"
#include "Nexora/Runtime/GameLaunch.h"

namespace nexora::player {
using apps::ExecutableDirectory;

// Only the engine-generated adjacent descriptor opts in. Explicit CLI modes remain unchanged.
inline std::optional<int> TryRunInstalledGame() {
  const auto directory = ExecutableDirectory();
  if (!directory) {
    std::cerr << "Game executable directory is unavailable\n";
    return 1;
  }
  const auto descriptor = *directory / runtime::kGameLaunchFilename;
  std::error_code status_error;
  const auto status = std::filesystem::symlink_status(descriptor, status_error);
  if (status_error == std::errc::no_such_file_or_directory ||
      (!status_error && status.type() == std::filesystem::file_type::not_found))
    return {};
  std::string error;
  const auto launch = runtime::ReadGameLaunch(descriptor, &error);
  if (!launch) {
    std::cerr << "Installed game descriptor failed: " << error << '\n';
    return 1;
  }
  auto project =
      runtime::ReadGameProjectPackage(*directory / runtime::kGamePackageFilename, &error);
  if (!project) {
    std::cerr << "Installed game package failed: " << error << '\n';
    return 1;
  }
  NativeOptions options;
  options.maximum_frames = launch->maximum_frames;
  if (launch->gameplay)
    options.gameplay_module = *directory / runtime::kGameModuleFilename;
  return RunNativeGame(std::make_shared<const runtime::LoadedGameProject>(std::move(*project)),
                       options);
}
} // namespace nexora::player
