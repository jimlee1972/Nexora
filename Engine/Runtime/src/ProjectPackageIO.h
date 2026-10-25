#pragma once
#include "ProjectPackageWire.h"
#include <array>
#include <fstream>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace nexora::runtime::package_detail {
inline std::optional<ByteBuffer>
ReadPackageFile(const std::filesystem::path &path, std::string *error,
                std::size_t maximum_bytes = kMaximumProjectPackageBytes) {
  if (error)
    error->clear();
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  if (ec || path.empty())
    return Fail<ByteBuffer>(error, "invalid package input path");
#if defined(__unix__) || defined(__APPLE__)
  struct FileDescriptor {
    int value{-1};
    ~FileDescriptor() {
      if (value >= 0)
        close(value);
    }
  };
  FileDescriptor descriptor{open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
  if (descriptor.value < 0)
    return Fail<ByteBuffer>(error, "package root could not be opened");
  const auto relative = absolute.relative_path();
  for (auto part = relative.begin(); part != relative.end(); ++part) {
    auto next = part;
    ++next;
    const bool final = next == relative.end();
    const auto opened =
        openat(descriptor.value, part->c_str(),
               O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | (final ? 0 : O_DIRECTORY));
    if (opened < 0)
      return Fail<ByteBuffer>(error, "package path contains an alias or unavailable component");
    close(descriptor.value);
    descriptor.value = opened;
  }
  struct stat status{};
  if (fstat(descriptor.value, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) > maximum_bytes)
    return Fail<ByteBuffer>(error, "package input is not a bounded regular file");
  ByteBuffer bytes;
  std::array<std::byte, 16384> chunk;
  for (;;) {
    const auto read_count = read(descriptor.value, chunk.data(), chunk.size());
    if (read_count == 0)
      break;
    if (read_count < 0) {
      if (errno == EINTR)
        continue;
      return Fail<ByteBuffer>(error, "package input read failed");
    }
    const auto count = static_cast<std::size_t>(read_count);
    if (count > maximum_bytes - bytes.size())
      return Fail<ByteBuffer>(error, "package grew past byte budget");
    bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
  }
  return bytes;
#else
  std::filesystem::path prefix;
  for (const auto &part : absolute) {
    prefix /= part;
    const auto status = std::filesystem::symlink_status(prefix, ec);
    if (ec || std::filesystem::is_symlink(status))
      return Fail<ByteBuffer>(error, "package path contains an alias or missing component");
#if defined(_WIN32)
    const auto attributes = GetFileAttributesW(prefix.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
      return Fail<ByteBuffer>(error,
                              "package path contains an unavailable component or reparse point");
#endif
  }
  if (!std::filesystem::is_regular_file(absolute, ec) || ec)
    return Fail<ByteBuffer>(error, "package input is not a regular file");
  const auto size = std::filesystem::file_size(absolute, ec);
  if (ec || size > maximum_bytes)
    return Fail<ByteBuffer>(error, "package input byte budget exceeded");
  std::ifstream input(absolute, std::ios::binary);
  if (!input)
    return Fail<ByteBuffer>(error, "package input could not be opened");
  ByteBuffer bytes;
  std::array<char, 16384> chunk;
  while (input.read(chunk.data(), chunk.size()) || input.gcount() > 0) {
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > maximum_bytes - bytes.size())
      return Fail<ByteBuffer>(error, "package grew past byte budget");
    const auto *begin = reinterpret_cast<const std::byte *>(chunk.data());
    bytes.insert(bytes.end(), begin, begin + count);
  }
  if (!input.eof())
    return Fail<ByteBuffer>(error, "package input read failed");
  return bytes;
#endif
}
} // namespace nexora::runtime::package_detail
