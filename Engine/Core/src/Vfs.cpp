#include "Nexora/Core/Vfs.h"

#include <algorithm>
#include <fstream>
#include <mutex>

namespace nexora::core {
struct AsyncReadHandle::State final {
  mutable std::mutex mutex;
  ReadResult result;
};
AsyncReadHandle::AsyncReadHandle() : state_(std::make_shared<State>()) {}
AsyncReadHandle::AsyncReadHandle(std::shared_ptr<State> state) : state_(std::move(state)) {}
ReadResult AsyncReadHandle::Get() const {
  std::lock_guard lock{state_->mutex};
  return state_->result;
}

bool VirtualFileSystem::Mount(std::string_view name, const std::filesystem::path &root) {
  if (name.empty() || name.find('/') != std::string_view::npos ||
      !std::filesystem::is_directory(root)) {
    return false;
  }
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(root, error);
  if (error)
    return false;
  std::lock_guard lock{mutex_};
  if (std::ranges::any_of(mounts_,
                          [name](const MountPoint &mount) { return mount.name == name; })) {
    return false;
  }
  mounts_.push_back({std::string{name}, canonical});
  return true;
}

bool VirtualFileSystem::Unmount(std::string_view name) {
  std::lock_guard lock{mutex_};
  return std::erase_if(mounts_, [name](const MountPoint &mount) { return mount.name == name; }) !=
         0;
}

std::pair<ReadResult::Status, std::filesystem::path>
VirtualFileSystem::Resolve(std::string_view virtual_path) const {
  const auto scheme = virtual_path.find("://");
  const auto separator = scheme == std::string_view::npos ? virtual_path.find('/') : scheme;
  const auto relative_start = scheme == std::string_view::npos ? separator + 1 : scheme + 3;
  if (separator == std::string_view::npos || separator == 0 ||
      relative_start >= virtual_path.size()) {
    return {ReadResult::Status::InvalidPath, {}};
  }
  const auto mount_name = virtual_path.substr(0, separator);
  const auto relative_text = virtual_path.substr(relative_start);
  std::filesystem::path relative{relative_text};
  if (relative.is_absolute() || relative_text.find('\\') != std::string_view::npos)
    return {ReadResult::Status::InvalidPath, {}};
  for (const auto &part : relative) {
    if (part == "..")
      return {ReadResult::Status::InvalidPath, {}};
  }
  std::lock_guard lock{mutex_};
  const auto found = std::ranges::find_if(
      mounts_, [mount_name](const MountPoint &mount) { return mount.name == mount_name; });
  if (found == mounts_.end())
    return {ReadResult::Status::NotFound, {}};
  std::error_code error;
  const auto resolved = std::filesystem::weakly_canonical(found->root / relative, error);
  if (error)
    return {ReadResult::Status::IoError, {}};
  auto root_part = found->root.begin();
  auto resolved_part = resolved.begin();
  for (; root_part != found->root.end(); ++root_part, ++resolved_part) {
    if (resolved_part == resolved.end() || *root_part != *resolved_part)
      return {ReadResult::Status::InvalidPath, {}};
  }
  return {ReadResult::Status::Completed, resolved};
}

std::pair<ReadResult::Status, FileMetadata>
VirtualFileSystem::Metadata(std::string_view virtual_path) const {
  const auto [status, path] = Resolve(virtual_path);
  if (status != ReadResult::Status::Completed)
    return {status, {}};
  std::error_code error;
  const auto info = std::filesystem::status(path, error);
  if (error || !std::filesystem::exists(info))
    return {ReadResult::Status::NotFound, {}};
  FileMetadata result{};
  result.is_directory = std::filesystem::is_directory(info);
  result.modified = std::filesystem::last_write_time(path, error);
  if (error)
    return {ReadResult::Status::IoError, {}};
  if (!result.is_directory) {
    result.size = std::filesystem::file_size(path, error);
    if (error)
      return {ReadResult::Status::IoError, {}};
  }
  return {ReadResult::Status::Completed, result};
}

std::pair<ReadResult::Status, std::vector<std::string>>
VirtualFileSystem::Enumerate(std::string_view virtual_directory) const {
  const auto [status, path] = Resolve(virtual_directory);
  if (status != ReadResult::Status::Completed)
    return {status, {}};
  std::error_code error;
  std::vector<std::string> entries;
  for (std::filesystem::directory_iterator it{path, error}, end; !error && it != end;
       it.increment(error))
    entries.push_back(it->path().filename().generic_string());
  if (error)
    return {ReadResult::Status::IoError, {}};
  std::ranges::sort(entries);
  return {ReadResult::Status::Completed, std::move(entries)};
}

ReadResult::Status VirtualFileSystem::WriteAtomic(std::string_view virtual_path,
                                                  std::span<const std::byte> bytes) const {
  const auto [status, path] = Resolve(virtual_path);
  if (status != ReadResult::Status::Completed)
    return status;
  auto temporary = path;
  temporary += ".nexora-tmp";
  std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
  if (!stream)
    return ReadResult::Status::IoError;
  if (!bytes.empty())
    stream.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  stream.close();
  if (!stream) {
    std::filesystem::remove(temporary);
    return ReadResult::Status::IoError;
  }
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
  }
  if (error) {
    std::filesystem::remove(temporary);
    return ReadResult::Status::IoError;
  }
  return ReadResult::Status::Completed;
}

ReadResult VirtualFileSystem::Read(std::string_view virtual_path) const {
  const auto [status, path] = Resolve(virtual_path);
  if (status != ReadResult::Status::Completed)
    return {status, {}};
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if (!stream)
    return {ReadResult::Status::NotFound, {}};
  const auto end = stream.tellg();
  if (end < 0)
    return {ReadResult::Status::IoError, {}};
  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  stream.seekg(0);
  if (!bytes.empty() && !stream.read(reinterpret_cast<char *>(bytes.data()), end)) {
    return {ReadResult::Status::IoError, {}};
  }
  return {ReadResult::Status::Completed, std::move(bytes)};
}

AsyncReadHandle VirtualFileSystem::ReadAsync(std::string virtual_path,
                                             const CancellationToken &cancellation) {
  auto state = std::make_shared<AsyncReadHandle::State>();
  (void)jobs_.Submit(
      {[this, state, path = std::move(virtual_path), cancellation](const CancellationToken &) {
         ReadResult result = cancellation.IsCancellationRequested()
                                 ? ReadResult{ReadResult::Status::Cancelled, {}}
                                 : Read(path);
         std::lock_guard lock{state->mutex};
         state->result = std::move(result);
       },
       JobPriority::Normal,
       {},
       "VFS read"});
  return AsyncReadHandle{std::move(state)};
}
} // namespace nexora::core
