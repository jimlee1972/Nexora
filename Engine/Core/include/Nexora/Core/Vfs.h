#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/JobSystem.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::core {
class Engine;

struct ReadResult final {
  enum class Status { Pending, Completed, Cancelled, NotFound, InvalidPath, IoError };
  Status status{Status::Pending};
  std::vector<std::byte> bytes;
};

struct FileMetadata final {
  std::uintmax_t size{};
  bool is_directory{};
  std::filesystem::file_time_type modified{};
};

enum class FileChangeKind : std::uint8_t { Created, Modified, Removed };
struct FileChange final {
  std::string virtual_path;
  FileChangeKind kind{};
};

struct AsyncReadOptions final {
  std::uint64_t offset{};
  std::uint64_t size{}; // Zero reads through end of file.
  std::size_t alignment{1};
  JobPriority priority{JobPriority::Normal};
  std::chrono::steady_clock::time_point deadline{}; // Empty means no deadline.
};

class NEXORA_CORE_API FileStream final {
public:
  FileStream() = default;
  [[nodiscard]] bool IsValid() const noexcept;
  [[nodiscard]] std::uint64_t Size() const noexcept;
  [[nodiscard]] ReadResult ReadAt(std::uint64_t offset, std::span<std::byte> destination) const;

private:
  struct State;
  explicit FileStream(std::shared_ptr<State> state) : state_(std::move(state)) {}
  std::shared_ptr<State> state_;
  friend class VirtualFileSystem;
};

class NEXORA_CORE_API MappedFile final {
public:
  MappedFile() = default;
  ~MappedFile();
  MappedFile(MappedFile &&) noexcept;
  MappedFile &operator=(MappedFile &&) noexcept;
  MappedFile(const MappedFile &) = delete;
  MappedFile &operator=(const MappedFile &) = delete;
  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept;

private:
  struct State;
  explicit MappedFile(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend class VirtualFileSystem;
};

class NEXORA_CORE_API AsyncReadHandle final {
public:
  AsyncReadHandle();
  [[nodiscard]] ReadResult Get() const;

private:
  struct State;
  explicit AsyncReadHandle(std::shared_ptr<State> state);
  std::shared_ptr<State> state_;
  friend class VirtualFileSystem;
};

class NEXORA_CORE_API VirtualFileSystem final {
public:
  explicit VirtualFileSystem(JobSystem &jobs) : jobs_(jobs) {}
  ~VirtualFileSystem();
  bool Mount(std::string_view name, const std::filesystem::path &root);
  // In-memory mount: content lives only for this VirtualFileSystem's lifetime
  // (never touches the host filesystem) and is visible through the same
  // Read/WriteAtomic/Enumerate/Metadata surface a directory mount uses, so
  // callers and tests don't need a backend-specific code path. Intended for
  // cache/temp-style roots and for hosting cooked bundle content in memory.
  bool MountMemory(std::string_view name);
  // Read-only package backend used by platform asset/package adapters. The
  // adapter supplies canonical package-relative names and their bytes.
  bool MountPackage(std::string_view name,
                    const std::map<std::string, std::vector<std::byte>> &files);
  bool Unmount(std::string_view name);
  [[nodiscard]] ReadResult Read(std::string_view virtual_path) const;
  [[nodiscard]] ReadResult ReadRange(std::string_view virtual_path, std::uint64_t offset,
                                     std::uint64_t size) const;
  [[nodiscard]] FileStream OpenRead(std::string_view virtual_path) const;
  [[nodiscard]] MappedFile MapReadOnly(std::string_view virtual_path) const;
  [[nodiscard]] std::pair<ReadResult::Status, FileMetadata>
  Metadata(std::string_view virtual_path) const;
  [[nodiscard]] std::pair<ReadResult::Status, std::vector<std::string>>
  Enumerate(std::string_view virtual_directory) const;
  [[nodiscard]] ReadResult::Status WriteAtomic(std::string_view virtual_path,
                                               std::span<const std::byte> bytes) const;
  [[nodiscard]] AsyncReadHandle ReadAsync(std::string virtual_path,
                                          const CancellationToken &cancellation = {});
  [[nodiscard]] AsyncReadHandle ReadAsync(std::string virtual_path, AsyncReadOptions options,
                                          const CancellationToken &cancellation = {});
  using WatchCallback = std::function<void(const FileChange &)>;
  [[nodiscard]] std::uint64_t Watch(std::string virtual_path, WatchCallback callback);
  bool Unwatch(std::uint64_t watch_id);
  // Polling is explicit and callbacks run synchronously on the calling thread.
  void PollWatches();

private:
  struct MemoryFile final {
    std::vector<std::byte> bytes;
    std::filesystem::file_time_type modified{};
  };
  struct MemoryBackend final {
    std::mutex mutex;
    std::map<std::string, MemoryFile> files;
    bool read_only{};
  };
  struct MountPoint final {
    std::string name;
    std::filesystem::path root;            // Directory backend when memory is null.
    std::shared_ptr<MemoryBackend> memory; // Non-null selects the memory backend.
  };
  struct ParsedPath final {
    std::string mount_name;
    std::string relative;
  };
  // Located result of resolving a virtual path against the mount table:
  // either a canonicalized host path (directory backend) or a memory backend
  // plus the key to use within it. `status` is Completed only when exactly
  // one of the two is populated. `relative_key` is empty whenever the
  // destination is the mount's own root -- either because the caller wrote
  // no path after the scheme ("mount://") or, on the directory backend,
  // because the path canonicalized back to the root (a dot alias such as
  // "mount://." resolves to found->root even though its pre-canonicalization
  // text is the nonempty "."). Read/Metadata/Enumerate treat an empty key as
  // the root, and WriteAtomic rejects it: writing "to" a mount's root would
  // mean replacing the mount point itself, not a file within it.
  struct Located final {
    ReadResult::Status status{ReadResult::Status::InvalidPath};
    std::filesystem::path path;
    std::shared_ptr<MemoryBackend> memory;
    std::string relative_key;
  };
  [[nodiscard]] static std::optional<ParsedPath> ParseVirtualPath(std::string_view virtual_path);
  [[nodiscard]] Located Locate(std::string_view virtual_path) const;
  JobSystem &jobs_;
  mutable std::mutex mutex_;
  std::vector<MountPoint> mounts_;
  struct WatchState;
  std::vector<std::shared_ptr<WatchState>> watches_;
  std::uint64_t next_watch_id_{1};

  bool MountHost(std::string_view name, const std::filesystem::path &root);
  friend class Engine;
};

} // namespace nexora::core
