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
  const auto separator = virtual_path.find('/');
  if (separator == std::string_view::npos || separator == 0 ||
      separator + 1 >= virtual_path.size()) {
    return {ReadResult::Status::InvalidPath, {}};
  }
  const auto mount_name = virtual_path.substr(0, separator);
  std::filesystem::path relative{virtual_path.substr(separator + 1)};
  if (relative.is_absolute())
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
  return {ReadResult::Status::Completed, found->root / relative};
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
