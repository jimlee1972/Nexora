#include "Nexora/Core/Vfs.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <limits>
#include <mutex>
#include <unordered_set>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nexora::core {
struct FileStream::State final {
  std::uint64_t size{};
  std::function<ReadResult(std::uint64_t, std::uint64_t)> read;
};
bool FileStream::IsValid() const noexcept { return state_ != nullptr; }
std::uint64_t FileStream::Size() const noexcept { return state_ ? state_->size : 0; }
ReadResult FileStream::ReadAt(std::uint64_t offset, std::span<std::byte> destination) const {
  if (!state_)
    return {ReadResult::Status::NotFound, {}};
  auto result = state_->read(offset, destination.size());
  if (result.status == ReadResult::Status::Completed)
    std::ranges::copy(result.bytes, destination.begin());
  return result;
}

struct MappedFile::State final {
  const std::byte *data{};
  std::size_t size{};
  std::vector<std::byte> owned;
#if defined(_WIN32)
  HANDLE file{INVALID_HANDLE_VALUE};
  HANDLE mapping{};
#else
  int descriptor{-1};
#endif
  ~State() {
#if defined(_WIN32)
    if (data)
      UnmapViewOfFile(data);
    if (mapping)
      CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE)
      CloseHandle(file);
#else
    if (data && owned.empty())
      munmap(const_cast<std::byte *>(data), size);
    if (descriptor >= 0)
      close(descriptor);
#endif
  }
};
MappedFile::~MappedFile() = default;
MappedFile::MappedFile(std::unique_ptr<State> state) : state_(std::move(state)) {}
MappedFile::MappedFile(MappedFile &&) noexcept = default;
MappedFile &MappedFile::operator=(MappedFile &&) noexcept = default;
std::span<const std::byte> MappedFile::Bytes() const noexcept {
  return state_ ? std::span<const std::byte>{state_->data, state_->size}
                : std::span<const std::byte>{};
}

struct VirtualFileSystem::WatchState final {
  std::uint64_t id{};
  std::string path;
  WatchCallback callback;
  ReadResult::Status status{ReadResult::Status::NotFound};
  FileMetadata metadata{};
};

struct AsyncReadHandle::State final {
  struct Operation final {
    mutable std::mutex mutex;
    ReadResult result;
  };
  std::shared_ptr<Operation> operation{std::make_shared<Operation>()};
  CancellationToken cancellation;
};
AsyncReadHandle::AsyncReadHandle() : state_(std::make_shared<State>()) {}
AsyncReadHandle::AsyncReadHandle(std::shared_ptr<State> state) : state_(std::move(state)) {}
ReadResult AsyncReadHandle::Get() const {
  if (state_->cancellation.IsCancellationRequested())
    return {ReadResult::Status::Cancelled, {}};
  std::lock_guard lock{state_->operation->mutex};
  return state_->operation->result;
}

VirtualFileSystem::~VirtualFileSystem() = default;

bool VirtualFileSystem::Mount(std::string_view name, const std::filesystem::path &root) {
#if defined(NEXORA_BUILD_SHIPPING)
  (void)name;
  (void)root;
  return false;
#else
  return MountHost(name, root);
#endif
}

bool VirtualFileSystem::MountHost(std::string_view name, const std::filesystem::path &root) {
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
  mounts_.push_back({std::string{name}, canonical, nullptr});
  return true;
}

bool VirtualFileSystem::MountMemory(std::string_view name) {
  if (name.empty() || name.find('/') != std::string_view::npos)
    return false;
  std::lock_guard lock{mutex_};
  if (std::ranges::any_of(mounts_,
                          [name](const MountPoint &mount) { return mount.name == name; })) {
    return false;
  }
  mounts_.push_back({std::string{name}, {}, std::make_shared<MemoryBackend>()});
  return true;
}

bool VirtualFileSystem::MountPackage(std::string_view name,
                                     const std::map<std::string, std::vector<std::byte>> &files) {
  if (!MountMemory(name))
    return false;
  std::shared_ptr<MemoryBackend> backend;
  {
    std::lock_guard lock{mutex_};
    const auto found = std::ranges::find_if(
        mounts_, [name](const MountPoint &mount) { return mount.name == name; });
    backend = found->memory;
  }
  for (const auto &[path, bytes] : files) {
    const auto parsed = ParseVirtualPath(std::string{name} + "://" + path);
    if (!parsed || parsed->relative.empty()) {
      Unmount(name);
      return false;
    }
    backend->files.emplace(parsed->relative,
                           MemoryFile{bytes, std::filesystem::file_time_type::clock::now()});
  }
  backend->read_only = true;
  return true;
}

bool VirtualFileSystem::Unmount(std::string_view name) {
  std::lock_guard lock{mutex_};
  return std::erase_if(mounts_, [name](const MountPoint &mount) { return mount.name == name; }) !=
         0;
}

std::optional<VirtualFileSystem::ParsedPath>
VirtualFileSystem::ParseVirtualPath(std::string_view virtual_path) {
  const auto scheme = virtual_path.find("://");
  const auto separator = scheme == std::string_view::npos ? virtual_path.find('/') : scheme;
  const auto relative_start = scheme == std::string_view::npos ? separator + 1 : scheme + 3;
  // relative_start == size() is the mount's own root ("mount://" with
  // nothing after it) -- allowed, not a missing path, so Enumerate can list
  // everything a mount contains without a caller having to know or guess a
  // subpath. Only relative_start > size() (impossible from the arithmetic
  // above, kept as a defensive bound) or no separator at all is rejected.
  if (separator == std::string_view::npos || separator == 0 ||
      relative_start > virtual_path.size()) {
    return std::nullopt;
  }
  const auto mount_name = virtual_path.substr(0, separator);
  const auto relative_text = virtual_path.substr(relative_start);
  if (relative_text.find('\\') != std::string_view::npos)
    return std::nullopt;
  std::filesystem::path relative{relative_text};
  if (relative.is_absolute())
    return std::nullopt;
  for (const auto &part : relative) {
    if (part == "..")
      return std::nullopt;
  }
  return ParsedPath{std::string(mount_name), relative.generic_string()};
}

VirtualFileSystem::Located VirtualFileSystem::Locate(std::string_view virtual_path) const {
  const auto parsed = ParseVirtualPath(virtual_path);
  if (!parsed)
    return {ReadResult::Status::InvalidPath, {}, {}, {}};
  std::lock_guard lock{mutex_};
  const auto found = std::ranges::find_if(
      mounts_, [&parsed](const MountPoint &mount) { return mount.name == parsed->mount_name; });
  if (found == mounts_.end())
    return {ReadResult::Status::NotFound, {}, {}, {}};
  if (found->memory)
    return {ReadResult::Status::Completed, {}, found->memory, parsed->relative};
  std::error_code error;
  const auto resolved = std::filesystem::weakly_canonical(found->root / parsed->relative, error);
  if (error)
    return {ReadResult::Status::IoError, {}, {}, {}};
  auto root_part = found->root.begin();
  auto resolved_part = resolved.begin();
  for (; root_part != found->root.end(); ++root_part, ++resolved_part) {
    if (resolved_part == resolved.end() || *root_part != *resolved_part)
      return {ReadResult::Status::InvalidPath, {}, {}, {}};
  }
  // resolved_part == resolved.end() here means the canonicalized destination
  // has no path components left beyond the mount root -- i.e. it IS the
  // root, however the caller spelled it. A dot alias such as "mount://."
  // produces a nonempty textual relative_text ("."), but weakly_canonical
  // collapses it back to found->root, so trusting the pre-canonicalization
  // text here would let WriteAtomic's empty-relative_key root check be
  // bypassed by such an alias while it still resolves to (and can still
  // destroy) the real mount root directory.
  const bool is_root = resolved_part == resolved.end();
  return {ReadResult::Status::Completed, resolved, {}, is_root ? std::string{} : parsed->relative};
}

std::pair<ReadResult::Status, FileMetadata>
VirtualFileSystem::Metadata(std::string_view virtual_path) const {
  const auto located = Locate(virtual_path);
  if (located.status != ReadResult::Status::Completed)
    return {located.status, {}};
  if (located.memory) {
    std::lock_guard lock{located.memory->mutex};
    const auto found = located.memory->files.find(located.relative_key);
    if (found == located.memory->files.end())
      return {ReadResult::Status::NotFound, {}};
    FileMetadata result{};
    result.size = found->second.bytes.size();
    result.modified = found->second.modified;
    return {ReadResult::Status::Completed, result};
  }
  std::error_code error;
  const auto info = std::filesystem::status(located.path, error);
  if (error || !std::filesystem::exists(info))
    return {ReadResult::Status::NotFound, {}};
  FileMetadata result{};
  result.is_directory = std::filesystem::is_directory(info);
  result.modified = std::filesystem::last_write_time(located.path, error);
  if (error)
    return {ReadResult::Status::IoError, {}};
  if (!result.is_directory) {
    result.size = std::filesystem::file_size(located.path, error);
    if (error)
      return {ReadResult::Status::IoError, {}};
  }
  return {ReadResult::Status::Completed, result};
}

std::pair<ReadResult::Status, std::vector<std::string>>
VirtualFileSystem::Enumerate(std::string_view virtual_directory) const {
  const auto located = Locate(virtual_directory);
  if (located.status != ReadResult::Status::Completed)
    return {located.status, {}};
  if (located.memory) {
    // Memory mounts are a flat key/value store; enumeration lists the
    // distinct immediate segment past the given prefix, so directory-style
    // nesting still works for callers that write "a/b/c" style keys.
    const std::string prefix =
        located.relative_key.empty() ? std::string{} : located.relative_key + "/";
    std::lock_guard lock{located.memory->mutex};
    std::vector<std::string> entries;
    std::unordered_set<std::string> seen;
    for (const auto &[key, file] : located.memory->files) {
      if (key.rfind(prefix, 0) != 0)
        continue;
      const auto rest = key.substr(prefix.size());
      const auto slash = rest.find('/');
      const auto immediate = slash == std::string::npos ? rest : rest.substr(0, slash);
      if (immediate.empty() || !seen.insert(immediate).second)
        continue;
      entries.push_back(immediate);
    }
    std::ranges::sort(entries);
    return {ReadResult::Status::Completed, std::move(entries)};
  }
  std::error_code error;
  std::vector<std::string> entries;
  for (std::filesystem::directory_iterator it{located.path, error}, end; !error && it != end;
       it.increment(error))
    entries.push_back(it->path().filename().generic_string());
  if (error)
    return {ReadResult::Status::IoError, {}};
  std::ranges::sort(entries);
  return {ReadResult::Status::Completed, std::move(entries)};
}

ReadResult::Status VirtualFileSystem::WriteAtomic(std::string_view virtual_path,
                                                  std::span<const std::byte> bytes) const {
  const auto located = Locate(virtual_path);
  if (located.status != ReadResult::Status::Completed)
    return located.status;
  // A mount's own root ("mount://" with nothing after it) is a valid target
  // for Read/Metadata/Enumerate but not for WriteAtomic: on the directory
  // backend, located.path would be the mount's root directory itself, and
  // this function's own rename-fallback path (below) can delete an empty
  // directory and rename the temp file into its place, silently turning the
  // mount point into a regular file. Reject before touching the host
  // filesystem at all, and do the same for the memory backend so both
  // backends keep agreeing on which paths are writable.
  if (located.relative_key.empty())
    return ReadResult::Status::InvalidPath;
  if (located.memory) {
    std::lock_guard lock{located.memory->mutex};
    if (located.memory->read_only)
      return ReadResult::Status::IoError;
    auto &file = located.memory->files[located.relative_key];
    file.bytes.assign(bytes.begin(), bytes.end());
    file.modified = std::filesystem::file_time_type::clock::now();
    return ReadResult::Status::Completed;
  }
  // Create the parent directory if it doesn't exist yet: a memory mount's
  // flat key/value store has no notion of a missing parent, so requiring
  // one here would make an operation that succeeds on one backend silently
  // fail on the other for the exact same virtual path.
  std::error_code parent_error;
  std::filesystem::create_directories(located.path.parent_path(), parent_error);
  if (parent_error)
    return ReadResult::Status::IoError;
  static std::atomic<std::uint64_t> temporary_id{0};
  auto temporary = located.path;
  temporary += ".nexora-tmp-" + std::to_string(temporary_id.fetch_add(1));
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
  std::filesystem::rename(temporary, located.path, error);
  if (error) {
#if defined(_WIN32)
    error.clear();
    if (MoveFileExW(temporary.c_str(), located.path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return ReadResult::Status::Completed;
#endif
    // Some standard-library implementations do not replace an existing
    // destination. This compatibility fallback is only reached there.
    std::filesystem::remove(located.path, error);
    error.clear();
    std::filesystem::rename(temporary, located.path, error);
  }
  if (error) {
    std::filesystem::remove(temporary);
    return ReadResult::Status::IoError;
  }
  return ReadResult::Status::Completed;
}

ReadResult VirtualFileSystem::Read(std::string_view virtual_path) const {
  return ReadRange(virtual_path, 0, 0);
}

ReadResult VirtualFileSystem::ReadRange(std::string_view virtual_path, std::uint64_t offset,
                                        std::uint64_t size) const {
  const auto located = Locate(virtual_path);
  if (located.status != ReadResult::Status::Completed)
    return {located.status, {}};
  if (located.memory) {
    std::lock_guard lock{located.memory->mutex};
    const auto found = located.memory->files.find(located.relative_key);
    if (found == located.memory->files.end())
      return {ReadResult::Status::NotFound, {}};
    const auto &source = found->second.bytes;
    if (offset > source.size())
      return {ReadResult::Status::IoError, {}};
    const auto available = static_cast<std::uint64_t>(source.size()) - offset;
    const auto count = size == 0 ? available : std::min(size, available);
    const auto begin = source.begin() + static_cast<std::ptrdiff_t>(offset);
    return {ReadResult::Status::Completed,
            std::vector<std::byte>{begin, begin + static_cast<std::ptrdiff_t>(count)}};
  }
  std::ifstream stream{located.path, std::ios::binary | std::ios::ate};
  if (!stream)
    return {ReadResult::Status::NotFound, {}};
  const auto end = stream.tellg();
  if (end < 0)
    return {ReadResult::Status::IoError, {}};
  const auto file_size = static_cast<std::uint64_t>(end);
  if (offset > file_size)
    return {ReadResult::Status::IoError, {}};
  const auto available = file_size - offset;
  const auto count = size == 0 ? available : std::min(size, available);
  if (count > std::numeric_limits<std::size_t>::max())
    return {ReadResult::Status::IoError, {}};
  std::vector<std::byte> bytes(static_cast<std::size_t>(count));
  stream.seekg(static_cast<std::streamoff>(offset));
  if (!bytes.empty() &&
      !stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(count))) {
    return {ReadResult::Status::IoError, {}};
  }
  return {ReadResult::Status::Completed, std::move(bytes)};
}

FileStream VirtualFileSystem::OpenRead(std::string_view virtual_path) const {
  const auto [status, metadata] = Metadata(virtual_path);
  if (status != ReadResult::Status::Completed || metadata.is_directory)
    return {};
  auto state = std::make_shared<FileStream::State>();
  state->size = metadata.size;
  state->read = [this, path = std::string{virtual_path}](std::uint64_t offset, std::uint64_t size) {
    return ReadRange(path, offset, size);
  };
  return FileStream{std::move(state)};
}

MappedFile VirtualFileSystem::MapReadOnly(std::string_view virtual_path) const {
  const auto located = Locate(virtual_path);
  if (located.status != ReadResult::Status::Completed)
    return {};
  auto state = std::make_unique<MappedFile::State>();
  if (located.memory) {
    const auto read = Read(virtual_path);
    if (read.status != ReadResult::Status::Completed)
      return {};
    state->owned = read.bytes;
    state->data = state->owned.data();
    state->size = state->owned.size();
    return MappedFile{std::move(state)};
  }
#if defined(_WIN32)
  state->file = CreateFileW(located.path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (state->file == INVALID_HANDLE_VALUE)
    return {};
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(state->file, &size) || size.QuadPart <= 0)
    return {};
  state->size = static_cast<std::size_t>(size.QuadPart);
  state->mapping = CreateFileMappingW(state->file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (!state->mapping)
    return {};
  state->data =
      static_cast<const std::byte *>(MapViewOfFile(state->mapping, FILE_MAP_READ, 0, 0, 0));
#else
  state->descriptor = open(located.path.c_str(), O_RDONLY);
  struct stat info{};
  if (state->descriptor < 0 || fstat(state->descriptor, &info) != 0 || info.st_size <= 0)
    return {};
  state->size = static_cast<std::size_t>(info.st_size);
  const auto mapping = mmap(nullptr, state->size, PROT_READ, MAP_PRIVATE, state->descriptor, 0);
  if (mapping == MAP_FAILED)
    return {};
  state->data = static_cast<const std::byte *>(mapping);
#endif
  return state->data ? MappedFile{std::move(state)} : MappedFile{};
}

AsyncReadHandle VirtualFileSystem::ReadAsync(std::string virtual_path,
                                             const CancellationToken &cancellation) {
  return ReadAsync(std::move(virtual_path), {}, cancellation);
}

AsyncReadHandle VirtualFileSystem::ReadAsync(std::string virtual_path, AsyncReadOptions options,
                                             const CancellationToken &cancellation) {
  auto state = std::make_shared<AsyncReadHandle::State>();
  state->cancellation = cancellation;
  const auto deadline = options.deadline.time_since_epoch().count();
  const auto key = std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "|" + virtual_path +
                   "|" + std::to_string(options.offset) + "|" + std::to_string(options.size) + "|" +
                   std::to_string(options.alignment) + "|" +
                   std::to_string(static_cast<unsigned>(options.priority)) + "|" +
                   std::to_string(deadline);
  bool submit = false;
  {
    // Exact requests share the underlying read, while each handle retains
    // its own cancellation token. Weak values keep the process-wide table
    // from owning completed operations; the VFS address scopes every key.
    static std::mutex operations_mutex;
    static std::map<std::string, std::weak_ptr<AsyncReadHandle::State::Operation>> operations;
    std::lock_guard lock{operations_mutex};
    if (auto existing = operations[key].lock())
      state->operation = std::move(existing);
    else {
      operations[key] = state->operation;
      submit = true;
    }
  }
  if (!submit)
    return AsyncReadHandle{std::move(state)};
  auto operation = state->operation;
  (void)jobs_.Submit(
      {[this, operation, path = std::move(virtual_path), options](const CancellationToken &) {
         const bool expired = options.deadline != std::chrono::steady_clock::time_point{} &&
                              std::chrono::steady_clock::now() > options.deadline;
         const bool unaligned = options.alignment == 0 || options.offset % options.alignment != 0 ||
                                (options.size != 0 && options.size % options.alignment != 0);
         ReadResult result = expired     ? ReadResult{ReadResult::Status::Cancelled, {}}
                             : unaligned ? ReadResult{ReadResult::Status::InvalidPath, {}}
                                         : ReadRange(path, options.offset, options.size);
         std::lock_guard lock{operation->mutex};
         operation->result = std::move(result);
       },
       options.priority,
       {},
       "VFS read"});
  return AsyncReadHandle{std::move(state)};
}

std::uint64_t VirtualFileSystem::Watch(std::string virtual_path, WatchCallback callback) {
  if (!callback)
    return 0;
  auto watch = std::make_shared<WatchState>();
  watch->path = std::move(virtual_path);
  watch->callback = std::move(callback);
  const auto current = Metadata(watch->path);
  watch->status = current.first;
  watch->metadata = current.second;
  std::lock_guard lock{mutex_};
  watch->id = next_watch_id_++;
  watches_.push_back(watch);
  return watch->id;
}

bool VirtualFileSystem::Unwatch(std::uint64_t watch_id) {
  std::lock_guard lock{mutex_};
  return std::erase_if(watches_, [watch_id](const auto &watch) { return watch->id == watch_id; }) !=
         0;
}

void VirtualFileSystem::PollWatches() {
  std::vector<std::shared_ptr<WatchState>> watches;
  {
    std::lock_guard lock{mutex_};
    watches = watches_;
  }
  for (const auto &watch : watches) {
    const auto current = Metadata(watch->path);
    std::optional<FileChangeKind> change;
    if (watch->status != ReadResult::Status::Completed &&
        current.first == ReadResult::Status::Completed)
      change = FileChangeKind::Created;
    else if (watch->status == ReadResult::Status::Completed &&
             current.first != ReadResult::Status::Completed)
      change = FileChangeKind::Removed;
    else if (watch->status == ReadResult::Status::Completed &&
             (watch->metadata.size != current.second.size ||
              watch->metadata.modified != current.second.modified))
      change = FileChangeKind::Modified;
    watch->status = current.first;
    watch->metadata = current.second;
    if (change)
      watch->callback({watch->path, *change});
  }
}
} // namespace nexora::core
