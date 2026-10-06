#pragma once

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <system_error>

namespace nexora::editor::test {
// Declare after the root path and before owners that may hold a file/lease in that directory.
// Reverse member destruction then closes those owners before cleanup, including on exceptions.
class TemporaryDirectoryCleanup final {
public:
  explicit TemporaryDirectoryCleanup(const std::filesystem::path &root) : root_(root) {}
  TemporaryDirectoryCleanup(const TemporaryDirectoryCleanup &) = delete;
  TemporaryDirectoryCleanup &operator=(const TemporaryDirectoryCleanup &) = delete;
  ~TemporaryDirectoryCleanup() noexcept {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
    if (error) {
      std::cerr << "fixture directory cleanup failed: " << error.message() << std::endl;
      std::abort();
    }
  }

private:
  const std::filesystem::path &root_;
};
} // namespace nexora::editor::test
