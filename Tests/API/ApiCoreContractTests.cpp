// API-M2/API-M3 conformance: the generational-handle ABI gate (Handle.h lives
// in Core, so it can't be covered from Tests/API/ApiFoundationTests.cpp which
// only links Foundation) and a VFS backend contract suite proving the memory
// mount behaves the same as a directory mount through the same public
// surface, plus the traversal/escape and error-injection cases the roadmap
// calls out for API-M3.
#include "Nexora/Core/Handle.h"
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Core/Services.h"
#include "Nexora/Core/Vfs.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace nexora::core;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

struct SlotTag {};
static_assert(sizeof(Handle<SlotTag>) == 8 && alignof(Handle<SlotTag>) == 4);
static_assert(offsetof(Handle<SlotTag>, index) == 0 && offsetof(Handle<SlotTag>, generation) == 4);

void RunHandleTests() {
  Handle<SlotTag> invalid{};
  Require(!invalid.IsValid(), "default-constructed handle is unexpectedly valid");

  HandlePool<SlotTag> pool;
  const auto first = pool.Create();
  Require(first.IsValid() && pool.Contains(first), "freshly created handle must be valid");
  Require(pool.Destroy(first), "live handle must be destroyed");
  Require(!pool.Contains(first), "destroyed handle must no longer be contained");

  const auto second = pool.Create();
  Require(second.index == first.index && second.generation != first.generation,
          "recycled slot must keep the index but advance the generation");
  Require(!(first == second),
          "a stale handle must not compare equal to the live handle recycled into its slot");
}

std::string g_last_marker_name; // NOLINT: test-only capture for the plain-function-pointer sink.
std::uint64_t g_last_marker_nanoseconds{};
int g_sink_calls{};
void RecordMarker(std::string_view name, std::uint64_t nanoseconds) {
  g_last_marker_name.assign(name);
  g_last_marker_nanoseconds = nanoseconds;
  ++g_sink_calls;
}

void RunServicesTests() {
  const auto first = MonotonicNanoseconds();
  const auto second = MonotonicNanoseconds();
  Require(second >= first, "MonotonicNanoseconds must never go backwards");

  ProfilingMarker::SetSink(&RecordMarker);
  {
    ProfilingMarker marker("api.services.test");
    Require(marker.Name() == "api.services.test", "ProfilingMarker must report its own name");
  }
  Require(g_sink_calls == 1 && g_last_marker_name == "api.services.test",
          "ProfilingMarker must call the registered sink exactly once, on destruction");
  Require(g_last_marker_nanoseconds < 1'000'000'000ULL,
          "a marker with no work inside it took implausibly long");
  ProfilingMarker::SetSink(nullptr);
  {
    ProfilingMarker marker("api.services.no_sink");
    (void)marker;
  }
  Require(g_sink_calls == 1, "clearing the sink must stop further emission");
}

// Runs the same read/write/metadata/enumerate/error sequence against a VFS
// callback so directory and memory backends are proven behaviorally
// equivalent through the one public surface, not just individually correct.
void RunBackendContractSuite(VirtualFileSystem &vfs, const std::string &mount) {
  const std::vector<std::byte> payload{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  Require(vfs.WriteAtomic(mount + "://dir/file.bin", payload) == ReadResult::Status::Completed,
          "WriteAtomic must succeed on a fresh mount");
  const auto read = vfs.Read(mount + "://dir/file.bin");
  Require(read.status == ReadResult::Status::Completed && read.bytes == payload,
          "Read must return exactly what WriteAtomic wrote");

  const auto [metadata_status, metadata] = vfs.Metadata(mount + "://dir/file.bin");
  Require(metadata_status == ReadResult::Status::Completed && metadata.size == payload.size(),
          "Metadata must report the written size");

  const auto [enumerate_status, entries] = vfs.Enumerate(mount + "://dir");
  Require(enumerate_status == ReadResult::Status::Completed && entries.size() == 1 &&
              entries.front() == "file.bin",
          "Enumerate must list the file written under its directory prefix");

  const auto [root_status, root_entries] = vfs.Enumerate(mount + "://");
  Require(root_status == ReadResult::Status::Completed && root_entries.size() == 1 &&
              root_entries.front() == "dir",
          "Enumerate on a mount's own root (\"mount://\" with nothing after it) must list its "
          "top-level contents, not be rejected as an invalid path");

  // ---- Error injection ----
  Require(vfs.Read(mount + "://missing.bin").status == ReadResult::Status::NotFound,
          "reading a file that was never written must be NotFound");
  Require(vfs.Read(mount + "://../escape").status == ReadResult::Status::InvalidPath,
          "a '..' traversal must be rejected regardless of backend");
  Require(vfs.Read("nonexistent-mount://anything").status == ReadResult::Status::NotFound,
          "reading through an unmounted scheme must be NotFound, not a crash");

  // ---- Overwrite is visible atomically: no reader ever sees a partial write ----
  const std::vector<std::byte> replacement{std::byte{9}};
  Require(vfs.WriteAtomic(mount + "://dir/file.bin", replacement) == ReadResult::Status::Completed,
          "overwrite must succeed");
  Require(vfs.Read(mount + "://dir/file.bin").bytes == replacement,
          "overwrite must be visible on the next read");

  // ---- Multi-MiB smoke: a stand-in for the roadmap's huge-file/offset case.
  // This is a bounded smoke test (a few MiB), not a true multi-GB/offset
  // boundary test -- that remains unverified and should not be read as
  // covered by this suite. ----
  std::vector<std::byte> large(4 * 1024 * 1024);
  for (std::size_t i = 0; i < large.size(); ++i)
    large[i] = static_cast<std::byte>(i & 0xFF);
  Require(vfs.WriteAtomic(mount + "://dir/large.bin", large) == ReadResult::Status::Completed,
          "a multi-MiB write must succeed");
  const auto large_read = vfs.Read(mount + "://dir/large.bin");
  Require(large_read.status == ReadResult::Status::Completed && large_read.bytes == large,
          "a multi-MiB read must return the exact bytes written");
}

int Run() {
  RunHandleTests();
  RunServicesTests();

  JobSystem jobs{1};
  VirtualFileSystem vfs{jobs};

  const auto temp_dir = std::filesystem::temp_directory_path() / "nexora-api-vfs-contract-test";
  std::filesystem::remove_all(temp_dir);
  std::filesystem::create_directories(temp_dir);
  // No "dir" subdirectory is pre-created here: WriteAtomic creates missing
  // parent directories on the directory backend, matching a memory mount's
  // flat key/value store where "dir/file.bin" never needed one to exist.
  // This is exactly what proves the two backends are equivalent rather than
  // only individually correct -- see RunBackendContractSuite's first write.
  Require(vfs.Mount("directory", temp_dir), "directory mount must succeed on a real directory");
  RunBackendContractSuite(vfs, "directory");
  std::filesystem::remove_all(temp_dir);

  Require(vfs.MountMemory("memory"), "memory mount must succeed");
  RunBackendContractSuite(vfs, "memory");

  // ---- Mount-table invariants shared by both backend kinds ----
  Require(!vfs.MountMemory("memory"), "a duplicate memory mount name must be rejected");
  Require(!vfs.Mount("memory", std::filesystem::temp_directory_path()),
          "a directory mount must not silently shadow an existing memory mount of the same name");
  Require(vfs.Unmount("memory") &&
              vfs.Read("memory://dir/file.bin").status == ReadResult::Status::NotFound,
          "unmounting must make the mount's content unreachable");

  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
