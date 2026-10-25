#pragma once
#include "NativeGamePlayer.h"
#include "Nexora/Runtime/GameLaunch.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace nexora::player {
inline std::optional<std::filesystem::path> ExecutableDirectory() {
  std::filesystem::path executable;
#if defined(_WIN32)
  std::vector<wchar_t> buffer(32768);
  const auto count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (!count || count >= buffer.size())
    return {};
  executable = std::wstring(buffer.data(), count);
#elif defined(__APPLE__)
  std::vector<char> buffer(4096);
  auto count = static_cast<std::uint32_t>(buffer.size());
  if (_NSGetExecutablePath(buffer.data(), &count) != 0) {
    if (count > 1024 * 1024)
      return {};
    buffer.resize(count);
    if (_NSGetExecutablePath(buffer.data(), &count) != 0)
      return {};
  }
  const std::string_view text(buffer.data());
  executable = std::filesystem::path(std::u8string(text.begin(), text.end()));
#elif defined(__linux__)
  std::error_code read_error;
  executable = std::filesystem::read_symlink("/proc/self/exe", read_error);
  if (read_error)
    return {};
#else
  return {};
#endif
  std::error_code error;
  const auto physical = std::filesystem::canonical(executable, error);
  if (error)
    return {};
  return physical.parent_path();
}

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
