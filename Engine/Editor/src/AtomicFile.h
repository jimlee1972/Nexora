#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nexora::editor::detail {
inline std::string PathUtf8(const std::filesystem::path &path) {
  const auto encoded = path.generic_u8string();
  return {encoded.begin(), encoded.end()};
}
inline bool AtomicWrite(const std::filesystem::path &path, std::string_view contents,
                        std::string *error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  auto temporary = path;
  temporary += ".tmp";
  // Do not truncate, follow, or remove a preexisting temporary path owned by another writer/file.
  const auto temporary_status = std::filesystem::symlink_status(temporary, ec);
  if (ec != std::errc::no_such_file_or_directory &&
      (ec || std::filesystem::exists(temporary_status))) {
    if (error)
      *error = "temporary destination is already occupied: " + PathUtf8(temporary);
    return false;
  }
  ec.clear();
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    // Close before checking so a failed flush (e.g. a full disk) is not renamed over a good file.
    if (!output || !(output << contents) || (output.close(), output.fail())) {
      output.close();
      std::filesystem::remove(temporary, ec);
      if (error)
        *error = "could not write " + PathUtf8(temporary);
      return false;
    }
  }
#if defined(_WIN32)
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
  std::filesystem::rename(temporary, path, ec);
#endif
  if (ec) {
    std::error_code cleanup;
    std::filesystem::remove(temporary, cleanup);
    if (error)
      *error = "could not replace " + PathUtf8(path) + ": " + ec.message();
  }
  return !ec;
}
} // namespace nexora::editor::detail
