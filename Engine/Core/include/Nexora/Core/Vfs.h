#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/JobSystem.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
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
  bool Unmount(std::string_view name);
  [[nodiscard]] ReadResult Read(std::string_view virtual_path) const;
  [[nodiscard]] AsyncReadHandle ReadAsync(std::string virtual_path,
                                          const CancellationToken &cancellation = {});

private:
  struct MountPoint final {
    std::string name;
    std::filesystem::path root;
  };
  [[nodiscard]] std::pair<ReadResult::Status, std::filesystem::path>
  Resolve(std::string_view virtual_path) const;
  JobSystem &jobs_;
  mutable std::mutex mutex_;
  std::vector<MountPoint> mounts_;
};

} // namespace nexora::core
