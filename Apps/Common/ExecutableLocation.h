#pragma once
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
namespace nexora::apps {
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

} // namespace nexora::apps
