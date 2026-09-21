// A small, compiled-and-run sample exercising the API-M1~M4 Engine API
// Foundation surface (Math, Types, VFS, Services) through public headers
// only, the same way Plugins/Example proves the plugin ABI by actually
// building and loading a real .so rather than only asserting in a unit
// test. This is not the windowed NexoraShowcase demo (see
// Roadmap/V1-Visual-Showcase-Long-Term-Plan.md for that, separate, V1-M0~M12
// effort) -- it is the roadmap's per-API "sample" deliverable: minimal,
// headless, and safe to run in CI.
#include "Nexora/Core/Handle.h"
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Core/Services.h"
#include "Nexora/Core/Vfs.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Math/Math.h"

#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>

namespace {
using namespace nexora::math;
using namespace nexora::foundation;
using namespace nexora::core;

void PrintMarker(std::string_view name, std::uint64_t nanoseconds) {
  std::printf("  [profiling] %.*s took %llu ns\n", static_cast<int>(name.size()), name.data(),
              static_cast<unsigned long long>(nanoseconds));
}

bool RunMathSample() {
  std::printf("-- Math --\n");
  const Transform transform{
      {1.0F, 2.0F, 3.0F}, Quaternion::FromAxisAngleRadians({0, 1, 0}, Radians(45.0F)), {1, 1, 1}};
  const auto matrix = Compose(transform);
  const auto decomposed = Decompose(matrix);
  std::printf("  translation round trip: (%.3f, %.3f, %.3f)\n", decomposed.translation.x,
              decomposed.translation.y, decomposed.translation.z);

  const auto view = LookAt({0, 2, 5}, {0, 0, 0});
  const auto projection = PerspectiveRadians(Radians(60.0F), 16.0F / 9.0F, 0.1F, 1000.0F);
  const auto frustum = ExtractFrustum(projection * view);
  const bool origin_visible = Intersects(frustum, Sphere{{0, 0, 0}, 1.0F});
  std::printf("  origin sphere visible in camera frustum: %s\n", origin_visible ? "yes" : "no");
  return origin_visible;
}

bool RunTypesSample() {
  std::printf("-- Types --\n");
  const auto uuid = Uuid::Parse("01234567-89ab-cdef-0123-456789abcdef");
  if (!uuid.HasValue()) {
    std::printf("  Uuid::Parse unexpectedly failed\n");
    return false;
  }
  std::printf("  Uuid round trip: %s\n", uuid.Value().ToString().c_str());
  const bool valid_utf8 = IsValidUtf8("Nexora \xE2\x9C\x93");
  std::printf("  IsValidUtf8: %s\n", valid_utf8 ? "true" : "false");
  return valid_utf8;
}

bool RunVfsSample() {
  std::printf("-- VFS --\n");
  JobSystem jobs{1};
  VirtualFileSystem vfs{jobs};
  if (!vfs.MountMemory("sample")) {
    std::printf("  MountMemory failed\n");
    return false;
  }
  const std::string_view text = "hello from the Nexora API foundation sample";
  const std::span<const std::byte> bytes{reinterpret_cast<const std::byte *>(text.data()),
                                         text.size()};
  if (vfs.WriteAtomic("sample://greeting.txt", bytes) != ReadResult::Status::Completed) {
    std::printf("  WriteAtomic failed\n");
    return false;
  }
  const auto read = vfs.Read("sample://greeting.txt");
  const bool ok = read.status == ReadResult::Status::Completed && read.bytes.size() == text.size();
  std::printf("  memory-mounted read back %zu bytes: %s\n", read.bytes.size(),
              ok ? "ok" : "FAILED");
  return ok;
}

bool RunServicesSample() {
  std::printf("-- Services --\n");
  RandomStream random{42};
  const auto first = random.NextU32();
  RandomStream replay{42};
  const auto replayed = replay.NextU32();
  std::printf("  RandomStream replay match: %s\n", first == replayed ? "yes" : "no");

  ProfilingMarker::SetSink(&PrintMarker);
  {
    ProfilingMarker marker("sample.services");
    Configuration configuration;
    configuration.Set("sample.key", "sample.value");
    (void)configuration.Get("sample.key");
  }
  ProfilingMarker::SetSink(nullptr);
  return first == replayed;
}

struct SampleTag {};
} // namespace

int main() {
  bool ok = true;
  ok &= RunMathSample();
  ok &= RunTypesSample();
  ok &= RunVfsSample();
  ok &= RunServicesSample();

  std::printf("-- Handle (Core) --\n");
  HandlePool<SampleTag> pool;
  const auto handle = pool.Create();
  ok &= pool.Contains(handle);
  std::printf("  fresh handle valid and contained: %s\n", pool.Contains(handle) ? "yes" : "no");

  std::printf("%s\n",
              ok ? "Nexora API foundation sample: PASS" : "Nexora API foundation sample: FAIL");
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
