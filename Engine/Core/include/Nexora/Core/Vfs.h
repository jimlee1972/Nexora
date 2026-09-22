#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/JobSystem.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::core {

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
  bool Mount(std::string_view name, const std::filesystem::path &root);
  // In-memory mount: content lives only for this VirtualFileSystem's lifetime
  // (never touches the host filesystem) and is visible through the same
  // Read/WriteAtomic/Enumerate/Metadata surface a directory mount uses, so
  // callers and tests don't need a backend-specific code path. Intended for
  // cache/temp-style roots and for hosting cooked bundle content in memory.
  bool MountMemory(std::string_view name);
  bool Unmount(std::string_view name);
  [[nodiscard]] ReadResult Read(std::string_view virtual_path) const;
  [[nodiscard]] std::pair<ReadResult::Status, FileMetadata>
  Metadata(std::string_view virtual_path) const;
  [[nodiscard]] std::pair<ReadResult::Status, std::vector<std::string>>
  Enumerate(std::string_view virtual_directory) const;
  [[nodiscard]] ReadResult::Status WriteAtomic(std::string_view virtual_path,
                                               std::span<const std::byte> bytes) const;
  [[nodiscard]] AsyncReadHandle ReadAsync(std::string virtual_path,
                                          const CancellationToken &cancellation = {});

private:
  struct MemoryFile final {
    std::vector<std::byte> bytes;
    std::filesystem::file_time_type modified{};
  };
  struct MemoryBackend final {
    std::mutex mutex;
    std::map<std::string, MemoryFile> files;
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
};

} // namespace nexora::core
