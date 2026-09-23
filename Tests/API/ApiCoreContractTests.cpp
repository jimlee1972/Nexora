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
#include <thread>
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

  const auto range = vfs.ReadRange(mount + "://dir/large.bin", 1024, 4096);
  Require(range.status == ReadResult::Status::Completed && range.bytes.size() == 4096 &&
              range.bytes.front() == large[1024],
          "ReadRange must support 64-bit offsets and bounded partial reads");
  const auto stream = vfs.OpenRead(mount + "://dir/large.bin");
  std::vector<std::byte> streamed(16);
  Require(stream.IsValid() && stream.Size() == large.size() &&
              stream.ReadAt(2048, streamed).status == ReadResult::Status::Completed &&
              streamed.front() == large[2048],
          "OpenRead must provide positional stream reads without a shared cursor");
}

void RunAdvancedVfsTests() {
  JobSystem jobs{1};
  jobs.Start();
  VirtualFileSystem vfs{jobs};
  Require(vfs.MountMemory("advanced"), "advanced memory mount must succeed");
  int changes = 0;
  const auto watch = vfs.Watch("advanced://watched.bin", [&](const FileChange &change) {
    Require(change.kind == FileChangeKind::Created, "first watch event must be Created");
    ++changes;
  });
  const std::vector<std::byte> payload(8192, std::byte{0x2a});
  Require(vfs.WriteAtomic("advanced://watched.bin", payload) == ReadResult::Status::Completed,
          "watched write must succeed");
  vfs.PollWatches();
  Require(changes == 1 && vfs.Unwatch(watch),
          "watch polling must notify on its calling thread and support unsubscribe");

  const auto mapped = vfs.MapReadOnly("advanced://watched.bin");
  Require(mapped.Bytes().size() == payload.size() && mapped.Bytes().front() == payload.front(),
          "memory-backed MapReadOnly must expose a stable snapshot");

  AsyncReadOptions options{};
  options.offset = 4096;
  options.size = 4096;
  options.alignment = 4096;
  options.priority = JobPriority::High;
  auto async = vfs.ReadAsync("advanced://watched.bin", options);
  CancellationSource cancelled_source;
  auto cancelled_peer = vfs.ReadAsync("advanced://watched.bin", options, cancelled_source.Token());
  cancelled_source.Cancel();
  for (int attempt = 0; attempt < 100 && async.Get().status == ReadResult::Status::Pending;
       ++attempt)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  Require(async.Get().status == ReadResult::Status::Completed && async.Get().bytes.size() == 4096,
          "priority/aligned asynchronous ranged read must complete");
  Require(cancelled_peer.Get().status == ReadResult::Status::Cancelled,
          "coalesced asynchronous reads must retain per-handle cancellation results");

  const std::map<std::string, std::vector<std::byte>> package{{"asset.bin", payload}};
  Require(vfs.MountPackage("package", package), "platform package adapter mount must succeed");
  Require(vfs.Read("package://asset.bin").bytes == payload,
          "package mount must expose supplied platform-package bytes");
  Require(vfs.WriteAtomic("package://asset.bin", {}) == ReadResult::Status::IoError,
          "package mount must be read-only");

  const auto sparse_root =
      std::filesystem::temp_directory_path() / "nexora-api-vfs-large-offset-test";
  std::filesystem::remove_all(sparse_root);
  std::filesystem::create_directories(sparse_root);
  constexpr std::uint64_t large_offset = (std::uint64_t{4} << 30) + 17;
  {
    std::ofstream sparse{sparse_root / "sparse.bin", std::ios::binary};
    sparse.seekp(static_cast<std::streamoff>(large_offset));
    sparse.put('\x5a');
  }
  Require(vfs.Mount("sparse", sparse_root), "sparse-file directory mount must succeed");
  const auto tail = vfs.ReadRange("sparse://sparse.bin", large_offset, 1);
  Require(tail.status == ReadResult::Status::Completed && tail.bytes.size() == 1 &&
              tail.bytes.front() == std::byte{0x5a},
          "ReadRange must preserve offsets beyond the 32-bit boundary");
  const auto file_mapping = vfs.MapReadOnly("sparse://sparse.bin");
  Require(file_mapping.Bytes().size() == large_offset + 1 &&
              file_mapping.Bytes()[large_offset] == std::byte{0x5a},
          "directory MapReadOnly must map a sparse file beyond the 32-bit boundary");
  std::filesystem::remove_all(sparse_root);
  jobs.Stop();
}

// Regression for a real bug a review caught: allowing Enumerate to reach a
// mount's own root made WriteAtomic reach it too, and on the directory
// backend WriteAtomic's rename-fallback path can delete an *empty*
// directory and rename the temp file into its place -- silently replacing
// the mount point itself with a regular file. WriteAtomic must reject the
// root outright, on both backends, before ever touching the filesystem.
void RunMountRootWriteRejectionTest() {
  JobSystem jobs{1};
  VirtualFileSystem vfs{jobs};
  const std::vector<std::byte> payload{std::byte{1}};

  const auto temp_dir = std::filesystem::temp_directory_path() / "nexora-api-vfs-root-write-test";
  std::filesystem::remove_all(temp_dir);
  std::filesystem::create_directories(temp_dir);
  Require(vfs.Mount("root", temp_dir), "directory mount must succeed on a real empty directory");
  Require(vfs.WriteAtomic("root://", payload) == ReadResult::Status::InvalidPath,
          "WriteAtomic must reject a mount's own root instead of replacing the mount point");
  Require(std::filesystem::is_directory(temp_dir),
          "an empty mount root must survive a rejected WriteAtomic(\"mount://\") as a directory, "
          "not be replaced by a file");
  Require(vfs.Enumerate("root://").first == ReadResult::Status::Completed,
          "the mount must still be usable after the rejected write");
  std::filesystem::remove_all(temp_dir);

  Require(vfs.MountMemory("memroot"), "memory mount must succeed");
  Require(vfs.WriteAtomic("memroot://", payload) == ReadResult::Status::InvalidPath,
          "WriteAtomic must reject a memory mount's own root too, for the same backend-parity "
          "reason even though there is no directory to destroy on this backend");
}

// Regression for a P1 a review caught on the fix above itself: the empty-
// relative-key root check can be bypassed by a dot alias of the root, such
// as "mount://.", whose pre-canonicalization textual key is the nonempty
// "." even though it resolves to the mount root directory just like the
// bare "mount://" case -- and on the directory backend, resolving there
// means WriteAtomic's rename-fallback path can still delete that empty
// directory exactly as the original bug did.
void RunMountRootDotAliasWriteRejectionTest() {
  JobSystem jobs{1};
  VirtualFileSystem vfs{jobs};
  const std::vector<std::byte> payload{std::byte{1}};

  const auto temp_dir =
      std::filesystem::temp_directory_path() / "nexora-api-vfs-root-dot-alias-test";
  std::filesystem::remove_all(temp_dir);
  std::filesystem::create_directories(temp_dir);
  Require(vfs.Mount("root", temp_dir), "directory mount must succeed on a real empty directory");
  Require(vfs.WriteAtomic("root://.", payload) == ReadResult::Status::InvalidPath,
          "WriteAtomic must reject a dot alias of a mount's own root exactly like the bare root, "
          "since both canonicalize to the same destination");
  Require(std::filesystem::is_directory(temp_dir),
          "an empty mount root must survive a rejected WriteAtomic(\"mount://.\") as a directory, "
          "not be replaced by a file");
  std::filesystem::remove_all(temp_dir);
}

int Run() {
  RunHandleTests();
  RunServicesTests();
  RunMountRootWriteRejectionTest();
  RunMountRootDotAliasWriteRejectionTest();
  RunAdvancedVfsTests();

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
