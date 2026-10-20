#include "Nexora/RHI/Capabilities.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Renderer/RenderGraph.h"
#include <algorithm>
#include <array>
#include <barrier>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
using namespace nexora;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void Reject(F function, const char *message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception &) {
    rejected = true;
  }
  Require(rejected, message);
}
template <class F> void RejectContains(F function, const char *message) {
  try {
    function();
  } catch (const std::exception &error) {
    Require(std::string{error.what()}.find(message) != std::string::npos,
            "ray rejection missed the expected artifact boundary");
    return;
  }
  throw std::runtime_error("invalid ray artifact was admitted");
}
constexpr std::uint32_t Sentinel = 0xcdcdcdcdU;
std::vector<rhi::RayTriangle> Triangles(std::uint32_t count) {
  std::vector<rhi::RayTriangle> result;
  for (std::uint32_t i = 0; i < count; ++i) {
    const float z = 2 + 2 * static_cast<float>(i);
    result.push_back({{-1, -1, z, 1, -1, z, 0, 1, z}});
  }
  return result;
}
std::vector<rhi::RayInput> Rays(std::uint32_t count) {
  std::vector<rhi::RayInput> result;
  for (std::uint32_t i = 0; i < count; ++i) {
    rhi::RayInput ray{{-.25F, -.25F, 0}, 0, {0, 0, 1}, 100};
    switch (i % 10) {
    case 1:
      ray.origin[0] = 2;
      break;
    case 2:
      ray.origin[2] = 17;
      ray.direction[2] = -1;
      break;
    case 3:
      ray.maximum = 1;
      break;
    case 4:
      ray.minimum = 3;
      break;
    case 5:
      ray.direction = {0, 0, 0};
      break;
    case 6:
      ray.origin[0] = std::numeric_limits<float>::quiet_NaN();
      break;
    case 7:
      ray.minimum = -1;
      break;
    case 8:
      ray.direction[2] = 2;
      break;
    case 9:
      ray.maximum = std::numeric_limits<float>::infinity();
      break;
    }
    result.push_back(ray);
  }
  return result;
}
// Double-precision CPU oracle, independent from the GPU shader's float arithmetic.
rhi::RayHit Reference(std::span<const rhi::RayTriangle> triangles, const rhi::RayInput &ray) {
  using V = std::array<double, 3>;
  auto sub = [](V a, V b) -> V { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; };
  auto cross = [](V a, V b) -> V {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  };
  auto dot = [](V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  V origin{}, direction{};
  float length = 0;
  bool valid = std::isfinite(ray.minimum) && std::isfinite(ray.maximum) && ray.minimum >= 0 &&
               ray.maximum >= ray.minimum;
  for (std::size_t j = 0; j < 3; ++j) {
    origin[j] = ray.origin[j];
    direction[j] = ray.direction[j];
    valid = valid && std::isfinite(ray.origin[j]) && std::isfinite(ray.direction[j]);
    length += ray.direction[j] * ray.direction[j];
  }
  valid = valid && std::isfinite(length) && length > 0;
  if (!valid)
    return {rhi::RayInvalidPrimitive, 0, {0, 0}};
  rhi::RayHit result{rhi::RayMissPrimitive, ray.maximum, {0, 0}};
  double distance = ray.maximum;
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    const auto &v = triangles[i].vertices;
    V a{v[0], v[1], v[2]}, b{v[3], v[4], v[5]}, c{v[6], v[7], v[8]};
    const auto e1 = sub(b, a), e2 = sub(c, a), p = cross(direction, e2);
    const auto determinant = dot(e1, p);
    if (determinant == 0)
      continue;
    const auto t = sub(origin, a), q = cross(t, e1);
    const auto u = dot(t, p) / determinant, w = dot(direction, q) / determinant,
               hit = dot(e2, q) / determinant;
    if (u >= 0 && w >= 0 && u + w <= 1 && hit >= ray.minimum && hit <= distance) {
      distance = hit;
      result = {static_cast<std::uint32_t>(i),
                static_cast<float>(hit),
                {static_cast<float>(u), static_cast<float>(w)}};
    }
  }
  return result;
}
std::vector<std::uint32_t> Empty(std::uint32_t count) {
  return std::vector<std::uint32_t>((count + 1) * 4, Sentinel);
}
struct Workload final {
  std::array<rhi::BufferHandle, 5> buffers;
  std::uint32_t count;
  std::vector<rhi::RayHit> expected;
};
Workload Make(rhi::Device &device, std::span<const rhi::RayTriangle> triangles, std::uint32_t count,
              std::span<const rhi::RayInput> supplied = {}) {
  const auto rays =
      supplied.empty() ? Rays(count) : std::vector<rhi::RayInput>{supplied.begin(), supplied.end()};
  Require(rays.size() == count, "fixture ray count mismatch");
  std::vector<std::uint32_t> packed{count, static_cast<std::uint32_t>(triangles.size())};
  for (const auto &triangle : triangles)
    for (float v : triangle.vertices)
      packed.push_back(std::bit_cast<std::uint32_t>(v));
  for (const auto &ray : rays) {
    for (float v : ray.origin)
      packed.push_back(std::bit_cast<std::uint32_t>(v));
    packed.push_back(std::bit_cast<std::uint32_t>(ray.minimum));
    for (float v : ray.direction)
      packed.push_back(std::bit_cast<std::uint32_t>(v));
    packed.push_back(std::bit_cast<std::uint32_t>(ray.maximum));
  }
  Workload result{{device.CreateBuffer({packed.size() * 4, "ray reference input"}),
                   device.CreateBuffer({(count + 1) * sizeof(rhi::RayHit), "ray hits plus canary"}),
                   device.CreateBuffer({16, "reference unused2"}),
                   device.CreateBuffer({16, "reference unused3"}),
                   device.CreateBuffer({rays.size() * sizeof(rhi::RayInput), "native ray inputs"})},
                  count,
                  {}};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{packed}));
  device.WriteBuffer(result.buffers[4], 0, std::as_bytes(std::span{rays}));
  const auto empty = Empty(count);
  device.WriteBuffer(result.buffers[1], 0, std::as_bytes(std::span{empty}));
  for (const auto &ray : rays)
    result.expected.push_back(Reference(triangles, ray));
  return result;
}
void Destroy(rhi::Device &device, const Workload &work) {
  for (auto buffer : work.buffers)
    device.DestroyBuffer(buffer);
}
std::vector<std::uint32_t> Read(rhi::Device &device, const Workload &work) {
  auto words = Empty(work.count);
  device.ReadBufferForTesting(work.buffers[1], 0, std::as_writable_bytes(std::span{words}));
  return words;
}
void Compare(rhi::Device &device, const Workload &work) {
  const auto words = Read(device, work);
  for (std::uint32_t i = 0; i < work.count; ++i) {
    const auto &expected = work.expected[i];
    Require(words[i * 4] == expected.primitive,
            "ray primitive differs from double-precision CPU traversal");
    Require(std::abs(std::bit_cast<float>(words[i * 4 + 1]) - expected.distance) <= 1e-4F,
            "ray distance differs from CPU traversal");
    for (std::size_t j = 0; j < 2; ++j)
      Require(std::abs(std::bit_cast<float>(words[i * 4 + 2 + j]) - expected.barycentrics[j]) <=
                  1e-4F,
              "ray barycentrics differ from CPU traversal");
  }
  for (std::size_t i = work.count * 4; i < words.size(); ++i)
    Require(words[i] == Sentinel, "ray dispatch overwrote the output canary");
}
void Record(rhi::CommandList &commands, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
            rhi::RaySceneHandle scene, const Workload &work) {
  if (program.IsValid())
    commands.TraceRays(program, scene, work.buffers[4], work.buffers[1], work.count);
  else {
    commands.BindPipeline(compute);
    for (std::uint32_t slot = 0; slot < 4; ++slot)
      commands.BindStorageBuffer(slot, work.buffers[slot]);
    commands.Dispatch(work.count);
  }
}
void Execute(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
             rhi::RaySceneHandle scene, const Workload &work) {
  auto commands = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*commands, compute, program, scene, work);
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  Compare(device, work);
}
void TestRebuilds(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
                  std::span<const rhi::RayTriangle> triangles) {
  const auto scene = device.CreateRayScene(triangles);
  const auto before = device.Diagnostics();
  auto old = Make(device, triangles, 64);
  auto retained = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*retained, compute, program, scene, old);
  Reject([&] { device.RebuildRayScene(scene, {}); }, "empty rebuild admitted");
  auto replacement = Triangles(triangles.size() == 1 ? 2 : 1);
  for (auto &triangle : replacement)
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
      triangle.vertices[vertex * 3 + 2] += 0.125F;
  device.RebuildRayScene(scene, replacement);
  Require(device.Diagnostics().ray_scene_rebuilds == before.ray_scene_rebuilds + 1 &&
              device.Diagnostics().ray_scene_refits == before.ray_scene_refits,
          "full native rebuild was misreported as a refit");
  device.WaitForSubmission(device.Submit(*retained));
  retained.reset();
  Compare(device, old);
  auto updated = Make(device, replacement, 64);
  Require(old.expected.front().distance != updated.expected.front().distance,
          "rebuild oracle failed to distinguish scene generations");
  Execute(device, compute, program, scene, updated);
  Destroy(device, old);
  Destroy(device, updated);
  device.DestroyRayScene(scene);
  Reject([&] { device.RebuildRayScene(scene, replacement); }, "stale rebuild admitted");
  Require(device.Diagnostics().ray_scene_rebuilds == before.ray_scene_rebuilds + 1,
          "rejected rebuild published a counter");
  std::cout << "Native full rebuild old_triangles=" << triangles.size()
            << " new_triangles=" << replacement.size() << " CPU comparisons=128 passed\n";
}
void TestRefits(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
                std::span<const rhi::RayTriangle> triangles) {
  const auto scene = device.CreateRayScene(triangles);
  const auto before = device.Diagnostics().ray_scene_refits;
  auto current = std::vector<rhi::RayTriangle>{triangles.begin(), triangles.end()};
  Reject([&] { device.UpdateRayScene(scene, {}); }, "empty refit admitted");
  const auto changed_count = Triangles(triangles.size() == 1 ? 2 : 1);
  Reject([&] { device.UpdateRayScene(scene, changed_count); }, "changed refit topology admitted");
  auto invalid = current;
  invalid.front().vertices[0] = std::numeric_limits<float>::quiet_NaN();
  Reject([&] { device.UpdateRayScene(scene, invalid); }, "nonfinite refit admitted");
  invalid = current;
  invalid.front().vertices.fill(0);
  Reject([&] { device.UpdateRayScene(scene, invalid); }, "degenerate refit admitted");
  Require(device.Diagnostics().ray_scene_refits == before, "rejected refit published a counter");
  for (std::uint32_t iteration = 0; iteration < 4; ++iteration) {
    auto previous = Make(device, current, 64);
    auto retained = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*retained, compute, program, scene, previous);
    auto next = current;
    for (auto &triangle : next)
      for (std::size_t vertex = 0; vertex < 3; ++vertex)
        triangle.vertices[vertex * 3 + 2] += 0.125F;
    auto updated = Make(device, next, 64);
    Require(previous.expected.front().distance != updated.expected.front().distance,
            "refit oracle failed to distinguish old and updated geometry");
    device.UpdateRayScene(scene, next);
    Require(device.Diagnostics().ray_scene_refits == before + iteration + 1,
            "successful native refit did not publish exactly one counter");
    // Submission after publication must retain the geometry captured during earlier recording.
    device.WaitForSubmission(device.Submit(*retained));
    retained.reset();
    Compare(device, previous);
    Execute(device, compute, program, scene, updated);
    Destroy(device, previous);
    Destroy(device, updated);
    current = std::move(next);
  }
  auto pending = Make(device, current, 64);
  auto stale = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*stale, compute, program, scene, pending);
  device.DestroyRayScene(scene);
  Reject([&] { device.UpdateRayScene(scene, current); }, "destroyed refit handle admitted");
  const auto submitted = device.Diagnostics().submitted_command_lists;
  Reject([&] { (void)device.Submit(*stale); }, "destroyed updated scene snapshot submitted");
  Require(device.Diagnostics().submitted_command_lists == submitted &&
              device.Diagnostics().ray_scene_refits == before + 4,
          "stale refit or scene published counters");
  stale.reset();
  Destroy(device, pending);
  std::cout << "Native BLAS/TLAS out-of-place refits=4 triangles=" << triangles.size()
            << " old/new CPU comparisons=512; invalid/topology/stale/canaries passed\n";
}
void TestCompaction(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
                    std::span<const rhi::RayTriangle> triangles) {
  const auto unopted = device.CreateRayScene(triangles);
  const auto count = device.Diagnostics().ray_scene_compactions;
  Reject([&] { device.CompactRayScene(unopted); }, "unopted scene admitted compaction");
  Require(device.Diagnostics().ray_scene_compactions == count,
          "unopted compaction published state");
  device.DestroyRayScene(unopted);
  const auto scene = device.CreateRaySceneWithOptions(triangles, {true});
  auto original = Make(device, triangles, 64);
  auto retained = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*retained, compute, program, scene, original);
  const auto before = device.Diagnostics();
  device.CompactRayScene(scene);
  auto first = device.Diagnostics();
  Require(
      first.ray_scene_compactions == before.ray_scene_compactions + 1 &&
          first.ray_compaction_buffer_bytes_before > before.ray_compaction_buffer_bytes_before &&
          first.ray_compaction_buffer_bytes_after > before.ray_compaction_buffer_bytes_after &&
          first.ray_compaction_buffer_bytes_after - before.ray_compaction_buffer_bytes_after <=
              first.ray_compaction_buffer_bytes_before - before.ray_compaction_buffer_bytes_before,
      "native compaction did not report positive non-increasing AS buffer storage");
  device.WaitForSubmission(device.Submit(*retained));
  retained.reset();
  Compare(device, original);
  auto copied = Make(device, triangles, 64);
  Execute(device, compute, program, scene, copied);
  Destroy(device, original);
  Destroy(device, copied);
  auto moved = std::vector<rhi::RayTriangle>{triangles.begin(), triangles.end()};
  for (auto &triangle : moved)
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
      triangle.vertices[vertex * 3 + 2] += 0.125F;
  device.UpdateRayScene(scene, moved);
  auto updated = Make(device, moved, 64);
  Execute(device, compute, program, scene, updated);
  Destroy(device, updated);
  device.CompactRayScene(scene);
  auto repeated = Make(device, moved, 64);
  Execute(device, compute, program, scene, repeated);
  Destroy(device, repeated);
  const auto after = device.Diagnostics();
  Require(after.ray_scene_compactions == before.ray_scene_compactions + 2 &&
              after.ray_scene_refits == before.ray_scene_refits + 1 &&
              after.ray_compaction_buffer_bytes_after - first.ray_compaction_buffer_bytes_after <=
                  after.ray_compaction_buffer_bytes_before -
                      first.ray_compaction_buffer_bytes_before,
          "compaction/refit/compaction round trip lost native update permission or accounting");
  device.DestroyRayScene(scene);
  Reject([&] { device.CompactRayScene(scene); }, "destroyed compaction handle admitted");
  Require(device.Diagnostics().ray_scene_compactions == after.ray_scene_compactions,
          "stale compaction published a counter");
  std::cout << "Native BLAS/TLAS compactions=2 triangles=" << triangles.size()
            << " old/new CPU comparisons=256 compact/refit/compact passed; AS buffer bytes="
            << after.ray_compaction_buffer_bytes_before - before.ray_compaction_buffer_bytes_before
            << " -> "
            << after.ray_compaction_buffer_bytes_after - before.ray_compaction_buffer_bytes_after
            << '\n';
}
std::vector<rhi::RayTriangle> WorldTriangles(std::span<const rhi::RayTriangle> local,
                                             const rhi::RayInstanceTransform &transform) {
  std::vector<rhi::RayTriangle> world{local.begin(), local.end()};
  for (std::size_t triangle = 0; triangle < local.size(); ++triangle)
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
      for (std::size_t row = 0; row < 3; ++row) {
        double value = transform.matrix[row * 4 + 3];
        for (std::size_t column = 0; column < 3; ++column)
          value += static_cast<double>(transform.matrix[row * 4 + column]) *
                   local[triangle].vertices[vertex * 3 + column];
        world[triangle].vertices[vertex * 3 + row] = static_cast<float>(value);
      }
  return world;
}
std::array<rhi::RayInstanceTransform, 3> InstanceTransforms() {
  return {rhi::RayInstanceTransform{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, .25F}},
          rhi::RayInstanceTransform{{0, -.5F, 0, 0, 2, 0, 0, 0, 0, 0, 1.5F, .5F}},
          rhi::RayInstanceTransform{{-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, .75F, .75F}}};
}
void TestTransforms(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
                    std::span<const rhi::RayTriangle> triangles) {
  const auto scene = device.CreateRaySceneWithOptions(triangles, {true});
  const auto before = device.Diagnostics();
  auto invalid = rhi::RayInstanceTransform{};
  invalid.matrix[0] = std::numeric_limits<float>::quiet_NaN();
  Reject([&] { device.SetRaySceneTransform(scene, invalid); }, "nonfinite transform admitted");
  invalid = {};
  invalid.matrix[0] = 0;
  Reject([&] { device.SetRaySceneTransform(scene, invalid); }, "singular transform admitted");
  invalid = {};
  invalid.matrix[3] = 1e20F;
  Reject([&] { device.SetRaySceneTransform(scene, invalid); }, "unbounded translation admitted");
  Require(device.Diagnostics().ray_scene_transform_updates == before.ray_scene_transform_updates,
          "rejected transform published a counter");
  auto local = std::vector<rhi::RayTriangle>{triangles.begin(), triangles.end()};
  auto world = local;
  std::uint64_t transforms = 0;
  auto set = [&](const rhi::RayInstanceTransform &transform) {
    auto old = Make(device, world, 64);
    auto retained = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*retained, compute, program, scene, old);
    auto replacement = WorldTriangles(local, transform);
    auto updated = Make(device, replacement, 64);
    Require(old.expected.front().primitive == 0 && updated.expected.front().primitive == 0 &&
                old.expected.front().distance != updated.expected.front().distance,
            "instance transform oracle failed to distinguish scene snapshots");
    const auto counters = device.Diagnostics();
    device.SetRaySceneTransform(scene, transform);
    Require(device.Diagnostics().ray_scene_transform_updates ==
                    before.ray_scene_transform_updates + ++transforms &&
                device.Diagnostics().ray_scene_refits == counters.ray_scene_refits &&
                device.Diagnostics().ray_scene_rebuilds == counters.ray_scene_rebuilds &&
                device.Diagnostics().ray_scene_compactions == counters.ray_scene_compactions,
            "TLAS-only transform was misreported as another scene operation");
    device.WaitForSubmission(device.Submit(*retained));
    retained.reset();
    Compare(device, old);
    Execute(device, compute, program, scene, updated);
    Destroy(device, old);
    Destroy(device, updated);
    world = std::move(replacement);
  };
  const auto matrices = InstanceTransforms();
  for (const auto &matrix : matrices)
    set(matrix);
  auto compare_current = [&] {
    world = WorldTriangles(local, matrices.back());
    auto work = Make(device, world, 64);
    Execute(device, compute, program, scene, work);
    Destroy(device, work);
  };
  for (auto &triangle : local)
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
      triangle.vertices[vertex * 3 + 2] += .125F;
  device.UpdateRayScene(scene, local);
  compare_current();
  device.CompactRayScene(scene);
  compare_current();
  local = Triangles(triangles.size() == 1 ? 2 : 1);
  device.RebuildRayScene(scene, local);
  compare_current();
  set({}); // Reset must use object space, rather than compose with the prior matrix.
  auto last = Make(device, local, 64);
  auto stale = device.CreateCommandList(rhi::QueueType::Compute);
  Record(*stale, compute, program, scene, last);
  device.DestroyRayScene(scene);
  Reject([&] { device.SetRaySceneTransform(scene, {}); }, "destroyed transform handle admitted");
  const auto submitted = device.Diagnostics().submitted_command_lists;
  Reject([&] { (void)device.Submit(*stale); }, "destroyed transformed snapshot submitted");
  stale.reset();
  Destroy(device, last);
  const auto after = device.Diagnostics();
  Require(after.ray_scene_transform_updates == before.ray_scene_transform_updates + 4 &&
              after.ray_scene_refits == before.ray_scene_refits + 1 &&
              after.ray_scene_rebuilds == before.ray_scene_rebuilds + 1 &&
              after.ray_scene_compactions == before.ray_scene_compactions + 1 &&
              after.submitted_command_lists == submitted,
          "transform/geometry/compaction/rebuild state or stale submission accounting changed");
  std::cout << "Native TLAS-only transforms=4 triangles=" << triangles.size()
            << " old/new CPU comparisons=704; affine/refit/compact/rebuild/reset/lifetime passed\n";
}
void Parallel(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
              rhi::RaySceneHandle scene, std::span<const rhi::RayTriangle> triangles,
              std::span<const rhi::RayInput> supplied = {});
std::vector<rhi::RayTriangle>
InstancedWorld(std::span<const rhi::RayTriangle> local,
               std::span<const rhi::RayInstanceTransform> transforms) {
  std::vector<rhi::RayTriangle> world;
  for (const auto &transform : transforms) {
    const auto instance = WorldTriangles(local, transform);
    world.insert(world.end(), instance.begin(), instance.end());
  }
  return world;
}
void TestInstances(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
                   std::span<const rhi::RayTriangle> triangles) {
  const auto count = rhi::RayMaximumTriangles / triangles.size();
  std::vector<rhi::RayInstanceTransform> transforms(count);
  const auto matrices = InstanceTransforms();
  for (std::size_t i = 0; i < count; ++i) {
    transforms[i] = matrices[i % matrices.size()];
    transforms[i].matrix[3] = 4 * static_cast<float>(i);
  }
  std::vector<rhi::RayInput> rays;
  for (std::size_t i = 0; i < 64; ++i) {
    rhi::RayInput ray{{4 * static_cast<float>(i % count) - .25F, -.25F, 0}, 0, {0, 0, 1}, 100};
    switch ((i / count) % 8) {
    case 1:
      ray.origin[0] += 100;
      break;
    case 2:
      ray.maximum = .5F;
      break;
    case 3:
      ray.minimum = 3;
      break;
    case 4:
      ray.direction = {};
      break;
    case 5:
      ray.maximum = std::numeric_limits<float>::infinity();
      break;
    case 6:
      ray.direction[2] = 2;
      break;
    case 7:
      ray.origin[2] = 17;
      ray.direction[2] = -1;
      break;
    }
    rays.push_back(ray);
  }
  auto local = std::vector<rhi::RayTriangle>{triangles.begin(), triangles.end()};
  auto world = InstancedWorld(local, transforms);
  auto initial = Make(device, world, 64, rays);
  for (std::size_t i = 0; i < count; ++i)
    Require(initial.expected[i].primitive == i * triangles.size(),
            "instance oracle does not distinguish every flattened primitive base");
  const auto before = device.Diagnostics();
  const bool native = program.IsValid(), updatable = device.Capabilities().ray_scene_refit_enabled;
  rhi::RaySceneHandle scene{};
  if (native) {
    Reject([&] { (void)device.CreateInstancedRayScene(local, {}); }, "empty instance set admitted");
    const std::array<rhi::RayInstanceTransform, rhi::RayMaximumInstances + 1> excess{};
    Reject([&] { (void)device.CreateInstancedRayScene(local, excess); },
           "too many instances admitted");
    auto invalid = transforms;
    invalid.back().matrix[0] = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { (void)device.CreateInstancedRayScene(local, invalid); },
           "invalid instance admitted");
    scene = device.CreateInstancedRayScene(local, transforms, {updatable});
    Reject([&] { device.SetRaySceneTransforms(scene, {}); }, "empty instance update admitted");
    invalid = transforms;
    invalid.push_back({});
    Reject([&] { device.SetRaySceneTransforms(scene, invalid); }, "instance count update admitted");
    invalid = transforms;
    invalid.back().matrix[0] = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { device.SetRaySceneTransforms(scene, invalid); },
           "invalid instance update admitted");
    const auto over_budget =
        Triangles(static_cast<std::uint32_t>(rhi::RayMaximumTriangles / count + 1));
    Reject([&] { device.RebuildRayScene(scene, over_budget); },
           "instanced rebuild exceeded total triangle budget");
    Reject([&] { (void)device.CreateInstancedRayScene(over_budget, transforms); },
           "instanced creation exceeded total triangle budget");
    if (count > 1)
      Reject([&] { device.SetRaySceneTransform(scene, {}); },
             "singular setter replaced a multiple-instance scene");
  } else if (!rhi::CheckGraphicsRequirements(device.Capabilities(),
                                             rhi::EffectiveRayProgramRequirements({}))
                  .Succeeded())
    Reject([&] { (void)device.CreateInstancedRayScene(local, transforms); },
           "unavailable backend fabricated instanced AS");
  Execute(device, compute, program, scene, initial);
  Destroy(device, initial);
  Parallel(device, compute, program, scene, world, rays);
  // Keep old scene/instance storage recorded across each publication, with independent buffers.
  auto replace = [&](auto mutation, const std::vector<rhi::RayTriangle> &replacement,
                     bool distinct) {
    auto old = Make(device, world, 64, rays), updated = Make(device, replacement, 64, rays);
    if (distinct)
      Require(old.expected.front().distance != updated.expected.front().distance,
              "instanced mutation oracle failed to distinguish scene snapshots");
    auto retained = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*retained, compute, program, scene, old);
    mutation();
    device.WaitForSubmission(device.Submit(*retained));
    retained.reset();
    Compare(device, old);
    Execute(device, compute, program, scene, updated);
    Destroy(device, old);
    Destroy(device, updated);
    world = replacement;
  };
  if (native && updatable) {
    for (auto &transform : transforms)
      transform.matrix[11] += .5F;
    replace([&] { device.SetRaySceneTransforms(scene, transforms); },
            InstancedWorld(local, transforms), true);
    for (auto &triangle : local)
      for (std::size_t vertex = 0; vertex < 3; ++vertex)
        triangle.vertices[vertex * 3 + 2] += .125F;
    replace([&] { device.UpdateRayScene(scene, local); }, InstancedWorld(local, transforms), true);
    replace([&] { device.CompactRayScene(scene); }, world, false);
  } else if (native)
    Reject([&] { device.SetRaySceneTransforms(scene, transforms); },
           "unadmitted instanced refit accepted");
  else {
    for (auto &transform : transforms)
      transform.matrix[11] += .5F;
    world = InstancedWorld(local, transforms);
    auto fallback = Make(device, world, 64, rays);
    Execute(device, compute, {}, {}, fallback);
    Destroy(device, fallback);
  }
  local =
      Triangles(1); // Recompute per-instance primitive bases when the BLAS triangle count changes.
  for (auto &triangle : local)
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
      triangle.vertices[vertex * 3 + 2] += .5F;
  const auto rebuilt = InstancedWorld(local, transforms);
  if (native) {
    replace([&] { device.RebuildRayScene(scene, local); }, rebuilt, true);
    auto last = Make(device, rebuilt, 64, rays);
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, scene, last);
    device.DestroyRayScene(scene);
    Reject([&] { device.SetRaySceneTransforms(scene, transforms); },
           "destroyed instanced handle admitted");
    const auto submitted = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); }, "destroyed instanced snapshot submitted");
    stale.reset();
    Destroy(device, last);
    const auto after = device.Diagnostics();
    Require(after.ray_scene_transform_updates ==
                    before.ray_scene_transform_updates + (updatable ? 1 : 0) &&
                after.ray_scene_refits == before.ray_scene_refits + (updatable ? 1 : 0) &&
                after.ray_scene_compactions == before.ray_scene_compactions + (updatable ? 1 : 0) &&
                after.ray_scene_rebuilds == before.ray_scene_rebuilds + 1 &&
                after.submitted_command_lists == submitted,
            "instanced mutation or rejected/stale work accounting changed");
  } else {
    auto fallback = Make(device, rebuilt, 64, rays);
    Execute(device, compute, {}, {}, fallback);
    Destroy(device, fallback);
  }
  std::cout << "Instanced ray native=" << native << " updatable=" << updatable
            << " instances=" << count << " BLAS triangles=" << triangles.size()
            << " CPU comparisons=" << (native && updatable ? 704 : 320)
            << "; flattened identity, bounds, retained snapshots and lifetime passed\n";
}
void Parallel(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
              rhi::RaySceneHandle scene, std::span<const rhi::RayTriangle> triangles,
              std::span<const rhi::RayInput> supplied) {
  std::array work{Make(device, triangles, 64, supplied), Make(device, triangles, 64, supplied)};
  renderer::RenderGraph graph;
  std::barrier overlap{2};
  core::CancellationSource cancellation;
  bool fail = false, cancel = false;
  for (std::size_t worker = 0; worker < 2; ++worker) {
    std::array<renderer::GraphBuffer, 5> buffers;
    for (std::size_t slot = 0; slot < buffers.size(); ++slot)
      buffers[slot] = graph.ImportBuffer(work[worker].buffers[slot], rhi::BufferAccess::Undefined,
                                         rhi::QueueType::Compute);
    renderer::RenderGraph::PassDescriptor pass{
        "bounded rays", rhi::QueueType::Compute, {}, {}, [&, worker](auto &commands, auto) {
          Record(commands, compute, program, scene, work[worker]);
          overlap.arrive_and_wait();
          if (worker == 0 && fail)
            throw std::runtime_error("intentional ray cohort failure");
          if (worker == 1 && cancel)
            cancellation.Cancel();
        }};
    for (auto slot : {0U, 4U})
      pass.buffer_reads.push_back({buffers[slot], rhi::BufferAccess::ShaderRead});
    for (auto slot : {1U, 2U, 3U})
      pass.buffer_writes.push_back({buffers[slot], rhi::BufferAccess::StorageReadWrite});
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  graph.Compile();
  core::JobSystem jobs{2};
  jobs.Start();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 2};
  for (unsigned mode = 0; mode < 2; ++mode) {
    fail = mode == 0;
    cancel = mode == 1;
    options.cancellation = cancel ? cancellation.Token() : core::CancellationToken{};
    const auto before = device.Diagnostics();
    Reject([&] { graph.Execute(device, options); }, "aborted ray cohort succeeded");
    const auto after = device.Diagnostics();
    Require(before.submitted_command_lists == after.submitted_command_lists &&
                before.ray_dispatch_calls == after.ray_dispatch_calls &&
                before.inline_ray_dispatch_calls == after.inline_ray_dispatch_calls &&
                before.ser_ray_dispatch_calls == after.ser_ray_dispatch_calls &&
                before.opacity_ray_dispatch_calls == after.opacity_ray_dispatch_calls,
            "aborted ray cohort committed work");
    for (const auto &item : work)
      Require(Read(device, item) == Empty(item.count), "aborted ray cohort changed output");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2, "ray workers did not overlap");
  for (const auto &item : work)
    Compare(device, item);
  jobs.Stop();
  for (const auto &item : work)
    Destroy(device, item);
}
void TestLifetime(rhi::Device &device, rhi::PipelineHandle compute, rhi::RayProgramHandle program,
                  rhi::RaySceneHandle scene, const rhi::RayProgramDescriptor &descriptor,
                  std::span<const rhi::RayTriangle> triangles,
                  std::span<const std::uint8_t> opacity_masks = {}) {
  auto work = Make(device, triangles, 64);
  auto small = device.CreateBuffer({4, "small ray buffer"});
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Compute);
    Reject([&] { abandoned->TraceRays(program, scene, work.buffers[4], work.buffers[1], 0); },
           "zero rays admitted");
    Reject([&] { abandoned->TraceRays(program, scene, work.buffers[4], work.buffers[1], 65); },
           "oversized rays admitted");
    Reject([&] { abandoned->TraceRays(program, scene, small, work.buffers[1], 64); },
           "small input admitted");
    Reject([&] { abandoned->TraceRays(program, scene, work.buffers[4], small, 64); },
           "small output admitted");
    Reject([&] { abandoned->TraceRays(program, scene, work.buffers[4], work.buffers[4], 64); },
           "ray input/output alias admitted");
    Record(*abandoned, compute, program, scene, work);
    Reject([&] { abandoned->Dispatch(1); }, "ordinary dispatch inherited a ray program");
  }
  device.DestroyBuffer(small);
  Require(Read(device, work) == Empty(work.count), "abandoned ray list wrote output");
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, scene, work);
    const auto rays = Rays(work.count);
    device.WriteBuffer(work.buffers[4], 0, std::as_bytes(std::span{rays}));
    const auto before = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); }, "stale ray input submitted");
    Require(before == device.Diagnostics().submitted_command_lists, "stale ray input committed");
  }
  {
    auto temporary = opacity_masks.empty()
                         ? device.CreateRayScene(triangles)
                         : device.CreateOpacityMicromapScene(triangles, opacity_masks);
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, temporary, work);
    device.DestroyRayScene(temporary);
    const auto replacement = opacity_masks.empty()
                                 ? device.CreateRayScene(triangles)
                                 : device.CreateOpacityMicromapScene(triangles, opacity_masks);
    Require(replacement != temporary, "scene generation reused");
    Reject([&] { (void)device.Submit(*stale); }, "destroyed scene submitted");
    device.DestroyRayScene(replacement);
  }
  {
    auto temporary = device.CreateRayProgram(descriptor);
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, temporary, scene, work);
    device.DestroyRayProgram(temporary);
    const auto replacement = device.CreateRayProgram(descriptor);
    Require(replacement != temporary, "program generation reused");
    Reject([&] { (void)device.Submit(*stale); }, "destroyed ray program submitted");
    device.DestroyRayProgram(replacement);
  }
  Require(Read(device, work) == Empty(work.count), "stale rays wrote output");
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*stale, compute, program, scene, work);
    auto replacement = Empty(work.count);
    replacement[0] = 0x1234U;
    device.WriteBuffer(work.buffers[1], 0, std::as_bytes(std::span{replacement}));
    const auto before = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); }, "stale ray output submitted");
    Require(device.Diagnostics().submitted_command_lists == before &&
                Read(device, work) == replacement,
            "stale ray output overwrote CPU mutation");
  }
  {
    const auto reset = Empty(work.count);
    device.WriteBuffer(work.buffers[1], 0, std::as_bytes(std::span{reset}));
  }
  {
    std::vector<Workload> held;
    held.reserve(rhi::RayMaximumDispatchesPerList);
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    for (std::uint32_t i = 0; i < rhi::RayMaximumDispatchesPerList; ++i) {
      held.push_back(Make(device, triangles, i % 2 == 0 ? 1 : 64));
      Record(*commands, compute, program, scene, held.back());
    }
    Reject([&] { Record(*commands, compute, program, scene, work); },
           "ray list snapshot capacity exceeded");
    device.WaitForSubmission(device.Submit(*commands));
    commands.reset();
    for (const auto &item : held) {
      Compare(device, item);
      Destroy(device, item);
    }
  }
  // Dedicated ray roots must restore correctly across an ordinary compute pipeline switch.
  auto middle = Make(device, triangles, 1), last = Make(device, triangles, 64);
  {
    auto mixed = device.CreateCommandList(rhi::QueueType::Compute);
    Record(*mixed, compute, program, scene, work);
    Record(*mixed, compute, {}, {}, middle);
    Record(*mixed, compute, program, scene, last);
    device.WaitForSubmission(device.Submit(*mixed));
  }
  Compare(device, work);
  Compare(device, middle);
  Compare(device, last);
  Destroy(device, middle);
  Destroy(device, last);
  Destroy(device, work);
}
void MixedNodesAndRays(rhi::Device &device, rhi::PipelineHandle compute,
                       rhi::RayProgramHandle program, rhi::RaySceneHandle scene,
                       std::span<const rhi::RayTriangle> triangles) {
  if (!rhi::CheckGraphicsRequirements(device.Capabilities(),
                                      rhi::EffectiveWorkGraphRequirements({}))
           .Succeeded())
    return;
  const auto node = device.CreateWorkGraph({});
  constexpr std::size_t words = rhi::WorkGraphMaximumRecords * rhi::WorkGraphVisibleRecordWords;
  std::array<std::uint32_t, words> empty;
  empty.fill(Sentinel);
  std::array buffers{device.CreateBuffer({sizeof(empty), "node before rays"}),
                     device.CreateBuffer({sizeof(empty), "node after rays"})};
  for (auto buffer : buffers)
    device.WriteBuffer(buffer, 0, std::as_bytes(std::span{empty}));
  auto work = Make(device, triangles, 1);
  {
    auto commands = device.CreateCommandList(rhi::QueueType::Compute);
    commands->BindWorkGraph(node);
    commands->BindStorageBuffer(1, buffers[0]);
    commands->DispatchWorkGraph({1, 7});
    Record(*commands, compute, program, scene, work);
    Reject([&] { commands->DispatchWorkGraph({1, 9}); },
           "node program remained bound after ray selection");
    commands->BindWorkGraph(node);
    commands->BindStorageBuffer(1, buffers[1]);
    commands->DispatchWorkGraph({8, 23});
    device.WaitForSubmission(device.Submit(*commands));
  }
  Compare(device, work);
  for (std::size_t index = 0; index < 2; ++index) {
    auto actual = empty, expected = empty;
    const std::size_t count = index == 0 ? 1 : 8;
    const std::uint32_t first = index == 0 ? 7 : 23;
    for (std::size_t record = 0; record < count; ++record) {
      expected[record * 5] = first + static_cast<std::uint32_t>(record);
      expected[record * 5 + 1] = 1;
      expected[record * 5 + 2] = 11;
      expected[record * 5 + 3] = 13;
      expected[record * 5 + 4] = 0;
    }
    device.ReadBufferForTesting(buffers[index], 0, std::as_writable_bytes(std::span{actual}));
    Require(actual == expected, "Node/Ray/Node root/program state restoration failed");
  }
  Destroy(device, work);
  for (auto buffer : buffers)
    device.DestroyBuffer(buffer);
  device.DestroyWorkGraph(node);
  std::cout << "Native Node/Ray/Node program and root restoration passed\n";
}

constexpr std::array<std::array<float, 6>, 4> MicroBary{{{0, 0, .5F, 0, 0, .5F},
                                                         {.5F, 0, .5F, .5F, 0, .5F},
                                                         {.5F, 0, 1, 0, .5F, .5F},
                                                         {0, .5F, .5F, .5F, 0, 1}}};
struct MicroIdentity final {
  std::uint32_t primitive, micro;
};
rhi::RayHit OpacityReference(std::span<const rhi::RayTriangle> triangles,
                             std::span<const std::uint8_t> masks, const rhi::RayInput &ray) {
  auto result = Reference({}, ray);
  if (result.primitive == rhi::RayInvalidPrimitive)
    return result;
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    auto hit = Reference(triangles.subspan(i, 1), ray);
    if (hit.primitive != 0 || hit.distance > result.distance)
      continue;
    const auto u = hit.barycentrics[0], v = hit.barycentrics[1];
    const auto micro = u >= .5F ? 2U : v >= .5F ? 3U : u + v <= .5F ? 0U : 1U;
    if (masks[i] & (1U << micro)) {
      hit.primitive = static_cast<std::uint32_t>(i);
      result = hit;
    }
  }
  return result;
}
void OpacityMicromaps(rhi::Device &device, rhi::PipelineHandle compute) {
  rhi::RayProgramDescriptor descriptor;
  descriptor.mode = rhi::RayTracingMode::ReorderedOpacityPipeline;
  const bool enabled = rhi::CheckGraphicsRequirements(
                           device.Capabilities(), rhi::EffectiveRayProgramRequirements(descriptor))
                           .Succeeded();
#if defined(NEXORA_TEST_OPACITY_NATIVE)
  Require(enabled, "required native DXR1.2 opacity pipeline is unavailable");
#endif
  rhi::RayProgramHandle program{};
  if (enabled)
    program = device.CreateRayProgram(descriptor);
  else
    Reject([&] { (void)device.CreateRayProgram(descriptor); },
           "unavailable opacity program admitted");
  for (const auto count : {1U, 2U})
    for (std::uint8_t pattern = 0; pattern < 16; ++pattern) {
      const auto triangles = Triangles(count);
      std::vector<std::uint8_t> masks(count, 15);
      masks[0] = pattern;
      std::vector<rhi::RayTriangle> micro_triangles;
      std::vector<MicroIdentity> identities;
      for (std::uint32_t primitive = 0; primitive < count; ++primitive)
        for (std::uint32_t micro = 0; micro < 4; ++micro)
          if (masks[primitive] & (1U << micro)) {
            rhi::RayTriangle triangle{};
            const auto &source = triangles[primitive].vertices;
            for (std::size_t corner = 0; corner < 3; ++corner)
              for (std::size_t axis = 0; axis < 3; ++axis)
                triangle.vertices[corner * 3 + axis] =
                    source[axis] +
                    MicroBary[micro][corner * 2] * (source[3 + axis] - source[axis]) +
                    MicroBary[micro][corner * 2 + 1] * (source[6 + axis] - source[axis]);
            micro_triangles.push_back(triangle);
            identities.push_back({primitive, micro});
          }
      rhi::RaySceneHandle scene{};
      if (enabled)
        scene = device.CreateOpacityMicromapScene(triangles, masks);
      else
        Reject([&] { (void)device.CreateOpacityMicromapScene(triangles, masks); },
               "unavailable OMM AS admitted");
      for (const auto ray_count : {1U, 64U}) {
        std::vector<rhi::RayInput> rays;
        constexpr std::array<std::array<float, 2>, 4> centroids{
            {{1.F / 6, 1.F / 6}, {1.F / 3, 1.F / 3}, {2.F / 3, 1.F / 6}, {1.F / 6, 2.F / 3}}};
        for (std::uint32_t i = 0; i < ray_count; ++i) {
          const auto u = centroids[i % 4][0], v = centroids[i % 4][1];
          rhi::RayInput ray{{-1 + 2 * u + v, -1 + 2 * v, 0}, 0, {0, 0, 1}, 100};
          switch (i % 16) {
          case 4:
            ray.origin[0] = 3;
            break;
          case 5:
            ray.direction[2] = 2;
            break;
          case 6:
            ray.origin[2] = 20;
            ray.direction[2] = -1;
            break;
          case 7:
            ray.direction.fill(0);
            break;
          case 8:
            ray.origin[0] = std::numeric_limits<float>::quiet_NaN();
            break;
          case 9:
            ray.maximum = 1;
            break;
          case 10:
            ray.minimum = 3;
            break;
          case 11:
            ray.maximum = std::numeric_limits<float>::infinity();
            break;
          default:
            break;
          }
          rays.push_back(ray);
        }
        auto fallback = Make(device, micro_triangles, ray_count, rays);
        Execute(device, compute, {}, {}, fallback);
        const auto words = Read(device, fallback);
        std::vector<rhi::RayHit> expected;
        for (std::uint32_t i = 0; i < ray_count; ++i) {
          const auto oracle = OpacityReference(triangles, masks, rays[i]);
          expected.push_back(oracle);
          auto primitive = words[i * 4];
          float u = std::bit_cast<float>(words[i * 4 + 2]),
                v = std::bit_cast<float>(words[i * 4 + 3]);
          if (primitive < identities.size()) {
            const auto identity = identities[primitive];
            primitive = identity.primitive;
            const auto &b = MicroBary[identity.micro];
            const auto original_u = b[0] + u * (b[2] - b[0]) + v * (b[4] - b[0]);
            v = b[1] + u * (b[3] - b[1]) + v * (b[5] - b[1]);
            u = original_u;
          }
          Require(primitive == oracle.primitive &&
                      std::abs(std::bit_cast<float>(words[i * 4 + 1]) - oracle.distance) <= 1e-4F &&
                      std::abs(u - oracle.barycentrics[0]) <= 1e-4F &&
                      std::abs(v - oracle.barycentrics[1]) <= 1e-4F,
                  "explicit microgeometry GPU oracle differs from CPU opacity classification");
        }
        Destroy(device, fallback);
        if (enabled) {
          auto actual = Make(device, triangles, ray_count, rays);
          actual.expected = expected;
          Execute(device, compute, program, scene, actual);
          Destroy(device, actual);
        }
      }
      if (scene.IsValid())
        device.DestroyRayScene(scene);
    }
  if (enabled) {
    const auto triangles = Triangles(8);
    const std::array<std::uint8_t, 8> opaque{15, 15, 15, 15, 15, 15, 15, 15};
    const auto scene = device.CreateOpacityMicromapScene(triangles, opaque);
    const auto ordinary_scene = device.CreateRayScene(triangles);
    const auto refits = device.Diagnostics().ray_scene_refits;
    Reject([&] { device.UpdateRayScene(scene, triangles); }, "OMM scene accepted ordinary refit");
    const auto transforms = device.Diagnostics().ray_scene_transform_updates;
    Reject([&] { device.SetRaySceneTransform(scene, {}); }, "OMM instance transform admitted");
    Require(device.Diagnostics().ray_scene_transform_updates == transforms,
            "OMM transform published state");
    const auto rebuilds = device.Diagnostics().ray_scene_rebuilds;
    Reject([&] { device.RebuildRayScene(scene, triangles); },
           "OMM scene accepted ordinary rebuild");
    Require(device.Diagnostics().ray_scene_rebuilds == rebuilds, "OMM rebuild published state");
    const auto compactions = device.Diagnostics().ray_scene_compactions;
    Reject([&] { device.CompactRayScene(scene); }, "OMM scene accepted ordinary compaction");
    Require(device.Diagnostics().ray_scene_compactions == compactions,
            "rejected OMM compaction published state");
    Require(device.Diagnostics().ray_scene_refits == refits, "rejected OMM refit published state");
    auto work = Make(device, triangles, 64);
    {
      auto mismatch = device.CreateCommandList(rhi::QueueType::Compute);
      Reject(
          [&] {
            mismatch->TraceRays(program, ordinary_scene, work.buffers[4], work.buffers[1], 64);
          },
          "opacity program accepted ordinary scene");
      const auto ordinary = device.CreateRayProgram({});
      Reject([&] { mismatch->TraceRays(ordinary, scene, work.buffers[4], work.buffers[1], 64); },
             "ordinary program accepted linked OMM scene");
      device.DestroyRayProgram(ordinary);
    }
    Destroy(device, work);
    device.DestroyRayScene(ordinary_scene);
    auto invalid = opaque;
    invalid[0] = 16;
    Reject([&] { (void)device.CreateOpacityMicromapScene(triangles, invalid); },
           "high opacity mask bits admitted");
    Reject([&] { (void)device.CreateOpacityMicromapScene(triangles, {}); },
           "missing opacity masks admitted");
    Parallel(device, compute, program, scene, triangles);
    TestLifetime(device, compute, program, scene, descriptor, triangles, opaque);
    device.DestroyRayScene(scene);
    device.DestroyRayProgram(program);
  }
  std::cout << "Opacity native=" << enabled
            << "; all16 patterns,4 micro-triangles,1/2 originals,1/64 rays, "
               "CPU/explicit-geometry GPU parity, nearest/miss/barycentric/invalid/canary passed\n";
}

void Portable() {
  auto triangle = Triangles(1);
  Require(rhi::IsRaySceneValid(triangle), "valid triangle rejected");
  Require(!rhi::IsRaySceneValid({}), "empty native scene admitted");
  auto bad = Triangles(9);
  Require(!rhi::IsRaySceneValid(bad), "oversized scene admitted");
  bad = triangle;
  bad[0].vertices[0] = std::numeric_limits<float>::infinity();
  Require(!rhi::IsRaySceneValid(bad), "nonfinite scene admitted");
  bad = triangle;
  bad[0].vertices.fill(0);
  Require(!rhi::IsRaySceneValid(bad), "degenerate scene admitted");
  auto descriptor = rhi::RayProgramDescriptor{};
  auto requirements = rhi::EffectiveRayProgramRequirements(descriptor);
  const auto index = static_cast<std::size_t>(rhi::GraphicsFeature::RayTracing);
  Require(requirements.minimum_tiers[index] == rhi::FeatureVersion{1, 0} &&
              requirements.minimum_dxil_shader_model == rhi::FeatureVersion{6, 3},
          "DXR implicit admission lost");
  descriptor.mode = rhi::RayTracingMode::InlineQuery;
  requirements = rhi::EffectiveRayProgramRequirements(descriptor);
  Require(requirements.minimum_tiers[index] == rhi::FeatureVersion{1, 1} &&
              requirements.minimum_dxil_shader_model == rhi::FeatureVersion{6, 5},
          "query implicit admission lost");
  descriptor.mode = rhi::RayTracingMode::ReorderedPipeline;
  requirements = rhi::EffectiveRayProgramRequirements(descriptor);
  Require(requirements.minimum_dxil_shader_model == rhi::FeatureVersion{6, 9} &&
              requirements.minimum_tiers[static_cast<std::size_t>(
                  rhi::GraphicsFeature::ShaderExecutionReordering)] == rhi::FeatureVersion{1, 0},
          "SER lost independent feature and profile admission");
  descriptor.mode = rhi::RayTracingMode::ReorderedOpacityPipeline;
  requirements = rhi::EffectiveRayProgramRequirements(descriptor);
  Require(requirements.minimum_tiers[index] == rhi::FeatureVersion{1, 2},
          "OMM lost actual DXR1.2 admission");
  const std::array<std::uint8_t, 1> opaque{15}, invalid_masks{16};
  Require(rhi::IsRayOpacitySceneValid(triangle, opaque) &&
              !rhi::IsRayOpacitySceneValid(triangle, {}) &&
              !rhi::IsRayOpacitySceneValid(triangle, invalid_masks),
          "opacity mask bounds lost");
  descriptor.mode = rhi::RayTracingMode::InlineQuery;
  rhi::GraphicsDeviceCapabilities capabilities;
  capabilities.features[index] = {rhi::CapabilitySupport::Supported, {1, 2}, true, true, {1, 1}};
  capabilities.dxil_shader_model_support = rhi::CapabilitySupport::Supported;
  capabilities.highest_dxil_shader_model = {6, 8};
  descriptor.graphics_requirements.minimum_tiers[index] = {1, 2};
  Require(
      rhi::CheckGraphicsRequirements(capabilities, rhi::EffectiveRayProgramRequirements(descriptor))
              .failure == rhi::CapabilityFailure::InsufficientImplementedTier,
      "unimplemented DXR1.2 admitted");
  Require(rhi::IsRayInstanceTransformValid({}), "identity instance transform rejected");
  for (const auto &transform : InstanceTransforms())
    Require(rhi::IsRayInstanceTransformValid(transform), "finite affine transform rejected");
  auto invalid_transform = rhi::RayInstanceTransform{};
  invalid_transform.matrix[0] = std::numeric_limits<float>::infinity();
  Require(!rhi::IsRayInstanceTransformValid(invalid_transform), "nonfinite matrix admitted");
  invalid_transform = {};
  invalid_transform.matrix[0] = 0;
  Require(!rhi::IsRayInstanceTransformValid(invalid_transform), "singular matrix admitted");
  invalid_transform = {};
  invalid_transform.matrix[4] = 1;
  invalid_transform.matrix[5] = 0;
  Require(!rhi::IsRayInstanceTransformValid(invalid_transform), "dependent basis rows admitted");
  invalid_transform = {};
  invalid_transform.matrix[0] = 1e5F;
  Require(!rhi::IsRayInstanceTransformValid(invalid_transform), "unbounded basis admitted");
  auto model = rhi::CreateValidationDevice();
  const std::array<rhi::RayInstanceTransform, 4> instances{};
  Require(rhi::IsRayInstanceSetValid(2, instances) && !rhi::IsRayInstanceSetValid(8, instances) &&
              !rhi::IsRayInstanceSetValid(0, instances) && !rhi::IsRayInstanceSetValid(1, {}),
          "bounded instance/triangle admission is incorrect");
  Reject([&] { (void)model->CreateInstancedRayScene(triangle, instances); },
         "Validation fabricated instanced AS");
  Reject([&] { model->SetRaySceneTransforms({}, instances); },
         "Validation fabricated instanced TLAS updates");
  Require(!rhi::GraphicsDeviceCapabilities{}.ray_scene_refit_enabled &&
              !model->Capabilities().ray_scene_refit_enabled,
          "unadmitted refit runtime must default closed");
  Reject([&] { (void)model->CreateRayScene(triangle); },
         "Validation fabricated acceleration structure");
  Reject([&] { model->UpdateRayScene({}, triangle); }, "Validation fabricated native AS refit");
  Reject([&] { model->RebuildRayScene({}, triangle); }, "Validation fabricated native AS rebuild");
  Reject([&] { model->SetRaySceneTransform({}, {}); }, "Validation fabricated native TLAS update");
  Reject([&] { model->CompactRayScene({}); }, "Validation fabricated native AS compaction");
  Reject([&] { (void)model->CreateRaySceneWithOptions(triangle, {true}); },
         "Validation fabricated compactable native AS storage");
  Reject([&] { (void)model->CreateRayProgram({}); }, "Validation fabricated ray pipeline");
  rhi::PipelineDescriptor pipeline{1, 2, rhi::TextureFormat::Rgba8Unorm, "ray cache admission"};
  pipeline.type = rhi::PipelineType::Compute;
  const auto ordinary = renderer::MakePipelineCacheKey(pipeline);
  pipeline.compute_shader = rhi::ComputeShaderVariant::RayReference;
  Require(ordinary != renderer::MakePipelineCacheKey(pipeline), "ray reference cache collided");
  pipeline.type = rhi::PipelineType::Graphics;
  Reject([&] { (void)rhi::EffectivePipelineRequirements(pipeline); },
         "graphics pipeline admitted compute reference");
}
#if defined(_WIN32)
class Environment final {
public:
  Environment(const char *name, const char *value) : name_(name) {
    if (const auto *old = std::getenv(name)) {
      previous_ = old;
      existed_ = true;
    }
    Require(_putenv_s(name, value ? value : "") == 0, "environment override failed");
  }
  ~Environment() { (void)_putenv_s(name_.c_str(), existed_ ? previous_.c_str() : ""); }

private:
  std::string name_, previous_;
  bool existed_{};
};
#endif
void Native(bool enhanced) {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
  constexpr auto reference = "NEXORA_SLANG_RAY_REFERENCE_DXIL_PATH";
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
  constexpr auto reference = "NEXORA_SLANG_RAY_REFERENCE_METAL_PATH";
#else
  constexpr auto backend = rhi::Backend::Vulkan;
  constexpr auto reference = "NEXORA_SLANG_RAY_REFERENCE_SPIRV_PATH";
#endif
  const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
  const auto *path = std::getenv(reference);
#if defined(NEXORA_TEST_SER_NATIVE)
  const bool available =
      true; // The explicit private factory below must succeed; no baseline probe.
#else
  const bool available = rhi::IsBackendAvailable(backend);
#endif
  if (!path || !*path || !available) {
    Require(!required || *required != '1', "required ray/reference native backend unavailable");
    std::cout << "Native ray/reference unavailable\n";
    return;
  }
  const std::string reference_path{path};
  rhi::DeviceCreationOptions options{true, false, enhanced};
#if defined(NEXORA_TEST_SER_NATIVE)
  options.enable_d3d12_preview_runtime = true;
#endif
  auto device = rhi::CreateDeviceWithOptions(backend, options);
  const auto caps = device->Capabilities();
  std::cout << "Ray adapter vendor=" << caps.vendor_id << " device=" << caps.device_id
            << " software=" << caps.software_device
            << " refit_enabled=" << caps.ray_scene_refit_enabled << '\n';
#if defined(NEXORA_TEST_SER_NATIVE)
  std::cout << "Preview adapter vendor=" << caps.vendor_id << " device=" << caps.device_id
            << " driver=" << caps.driver_version << " software=" << caps.software_device
            << " SM=" << caps.highest_dxil_shader_model.major << '.'
            << caps.highest_dxil_shader_model.minor << '\n';
  for (std::size_t i = 0; i < caps.features.size(); ++i) {
    const auto &feature = caps.features[i];
    std::cout << "Preview feature=" << i << " support=" << static_cast<unsigned>(feature.support)
              << " hardware_tier=" << feature.tier.major << '.' << feature.tier.minor
              << " enabled=" << feature.enabled << " implemented=" << feature.implemented
              << " ceiling=" << feature.implemented_tier.major << '.'
              << feature.implemented_tier.minor << '\n';
  }
  Require(
      caps.d3d12_preview_runtime_enabled &&
          caps.features[static_cast<std::size_t>(rhi::GraphicsFeature::ShaderExecutionReordering)]
              .enabled &&
          caps.actual_shader_reordering_support != rhi::CapabilitySupport::Unknown,
      "required native SER or independent reordering query unavailable");
  std::cout << "SER functional=1 actual_reordering="
            << static_cast<unsigned>(caps.actual_shader_reordering_support)
            << " software=" << caps.software_device << '\n';
#endif
  rhi::PipelineDescriptor pipeline{1, 2, rhi::TextureFormat::Rgba8Unorm, "portable ray traversal",
                                   rhi::PipelineType::Compute};
  pipeline.compute_shader = rhi::ComputeShaderVariant::RayReference;
  const auto compute = device->CreatePipeline(pipeline);
  for (std::uint32_t size : {1U, 2U, 8U}) {
    const auto triangles = Triangles(size);
    rhi::RaySceneHandle scene{};
    const bool trace =
        rhi::CheckGraphicsRequirements(caps, rhi::EffectiveRayProgramRequirements({})).Succeeded();
    if (trace) {
      Reject([&] { (void)device->CreateRayScene({}); }, "empty AS admitted");
      scene = device->CreateRayScene(triangles);
    } else
      Reject([&] { (void)device->CreateRayScene(triangles); }, "unavailable AS admitted");
    std::vector modes{rhi::RayTracingMode::Pipeline, rhi::RayTracingMode::InlineQuery};
#if defined(NEXORA_TEST_SER_NATIVE)
    modes.push_back(rhi::RayTracingMode::ReorderedPipeline);
#endif
    for (auto mode : modes) {
      rhi::RayProgramDescriptor descriptor{mode, {}, "bounded ray oracle"};
      const bool enabled =
          rhi::CheckGraphicsRequirements(caps, rhi::EffectiveRayProgramRequirements(descriptor))
              .Succeeded();
#if defined(NEXORA_TEST_SER_NATIVE)
      Require(enabled, "required private native ray/refit mode unavailable");
#endif
      rhi::RayProgramHandle program{};
      if (enabled) {
#if defined(_WIN32)
        const auto *environment =
            mode == rhi::RayTracingMode::ReorderedPipeline ? "NEXORA_SER_RAY_LIBRARY_DXIL_PATH"
            : mode == rhi::RayTracingMode::Pipeline        ? "NEXORA_SLANG_RAY_LIBRARY_DXIL_PATH"
                                                           : "NEXORA_SLANG_RAY_QUERY_DXIL_PATH";
        {
          Environment missing{environment, nullptr};
          RejectContains([&] { (void)device->CreateRayProgram(descriptor); },
                         "ray artifact is unset");
        }
        if (mode != rhi::RayTracingMode::InlineQuery) {
          Environment wrong{environment, reference_path.c_str()};
          RejectContains([&] { (void)device->CreateRayProgram(descriptor); },
                         "DXIL library artifact");
        }
#endif
#if defined(_WIN32)
        if (mode != rhi::RayTracingMode::InlineQuery) {
          const auto *negative = std::getenv(mode == rhi::RayTracingMode::ReorderedPipeline
                                                 ? "NEXORA_SER_RAY_NEGATIVE_DXIL_PATH"
                                                 : "NEXORA_SLANG_RAY_NEGATIVE_DXIL_PATH");
          Require(negative && *negative, "required compiled ray identity negative is missing");
          const std::string negative_path{negative};
          Environment wrong{environment, negative_path.c_str()};
          RejectContains([&] { (void)device->CreateRayProgram(descriptor); },
                         "ray recipe identity");
        } else {
          Environment wrong{environment, reference_path.c_str()};
          RejectContains([&] { (void)device->CreateRayProgram(descriptor); },
                         "shader model is too low");
        }
#endif
        program = device->CreateRayProgram(descriptor);
      } else
        Reject([&] { (void)device->CreateRayProgram(descriptor); },
               "unavailable ray program admitted");
      for (auto count : {1U, 64U}) {
        auto fallback = Make(*device, triangles, count);
        Execute(*device, compute, {}, {}, fallback);
        Destroy(*device, fallback);
        auto actual = Make(*device, triangles, count);
        Execute(*device, compute, program, scene, actual);
        Destroy(*device, actual);
      }
      Parallel(*device, compute, program, scene, triangles);
      TestInstances(*device, compute, program, triangles);
      if (!enabled) {
        for (const auto &transform : InstanceTransforms()) {
          const auto world = WorldTriangles(triangles, transform);
          auto fallback = Make(*device, world, 64);
          Execute(*device, compute, {}, {}, fallback);
          Destroy(*device, fallback);
        }
        std::cout << "World-space affine compute fallback triangles=" << size
                  << " CPU comparisons=192; no native instance update claimed\n";
      }
      if (enabled) {
        TestLifetime(*device, compute, program, scene, descriptor, triangles);
        TestRebuilds(*device, compute, program, triangles);
        if (caps.ray_scene_refit_enabled) {
          TestRefits(*device, compute, program, triangles);
          TestCompaction(*device, compute, program, triangles);
          TestTransforms(*device, compute, program, triangles);
        } else {
          const auto before = device->Diagnostics();
          Reject([&] { device->UpdateRayScene(scene, triangles); },
                 "unadmitted system runtime accepted a native refit");
          Reject([&] { device->SetRaySceneTransform(scene, {}); },
                 "unadmitted system runtime accepted a TLAS transform");
          Reject([&] { device->CompactRayScene(scene); },
                 "unadmitted system runtime accepted compaction");
          Reject([&] { (void)device->CreateRaySceneWithOptions(triangles, {true}); },
                 "unadmitted system runtime accepted compactable storage");
          Require(device->Diagnostics().ray_scene_refits == before.ray_scene_refits &&
                      device->Diagnostics().ray_scene_compactions == before.ray_scene_compactions &&
                      device->Diagnostics().ray_scene_transform_updates ==
                          before.ray_scene_transform_updates,
                  "unadmitted runtime published update/compaction counters");
        }
        MixedNodesAndRays(*device, compute, program, scene, triangles);
      }
      std::cout << "Ray backend=" << static_cast<unsigned>(backend)
                << " mode=" << static_cast<unsigned>(mode) << " triangles=" << size
                << " enabled=" << enabled
                << "; CPU/compute hit,miss,nearest,barycentric,invalid-ray and cancellation/replay "
                   "passed\n";
      if (program.IsValid())
        device->DestroyRayProgram(program);
    }
    if (scene.IsValid())
      device->DestroyRayScene(scene);
  }
  OpacityMicromaps(*device, compute);
  const auto diagnostics = device->Diagnostics();
  std::cout << "Native ray scene refits=" << diagnostics.ray_scene_refits
            << " rebuilds=" << diagnostics.ray_scene_rebuilds
            << " compactions=" << diagnostics.ray_scene_compactions
            << " transforms=" << diagnostics.ray_scene_transform_updates << '\n';
#if defined(NEXORA_TEST_SER_NATIVE)
  Require(diagnostics.ray_scene_refits == 63 && diagnostics.ray_scene_rebuilds == 27 &&
              diagnostics.ray_scene_compactions == 36 &&
              diagnostics.ray_scene_transform_updates == 45,
          "required private BLAS/TLAS refit/compaction round trips were not executed");
#endif
  const auto feature = caps.features[static_cast<std::size_t>(rhi::GraphicsFeature::RayTracing)];
  Require(feature.enabled ? diagnostics.ray_dispatch_calls > 0
                          : diagnostics.ray_dispatch_calls == 0,
          "ray dispatch diagnostics disagree with admission");
  const bool query =
      rhi::CheckGraphicsRequirements(
          caps, rhi::EffectiveRayProgramRequirements(
                    {rhi::RayTracingMode::InlineQuery, {}, "inline capability probe"}))
          .Succeeded();
  Require(query ? diagnostics.inline_ray_dispatch_calls > 0
                : diagnostics.inline_ray_dispatch_calls == 0,
          "inline ray diagnostics disagree with admission");
  const auto barrier =
      caps.features[static_cast<std::size_t>(rhi::GraphicsFeature::EnhancedBarriers)];
  if (enhanced && barrier.enabled)
    Require(diagnostics.enhanced_buffer_barriers > 0, "requested ray enhanced barriers unused");
  else
    Require(diagnostics.enhanced_buffer_barriers == 0, "legacy rays fabricated enhanced barriers");
#if defined(NEXORA_TEST_SER_NATIVE)
  Require(diagnostics.ser_ray_dispatch_calls > 0, "required native SER was not dispatched");
  std::cout << "SER accepted_calls=" << diagnostics.ser_ray_dispatch_calls << '\n';
#endif
#if defined(NEXORA_TEST_OPACITY_NATIVE)
  Require(diagnostics.opacity_ray_dispatch_calls > 0,
          "required native opacity traversal was not dispatched");
  std::cout << "Opacity accepted_calls=" << diagnostics.opacity_ray_dispatch_calls << '\n';
#endif
  std::cout << "Ray dispatches=" << diagnostics.ray_dispatch_calls
            << " inline=" << diagnostics.inline_ray_dispatch_calls
            << " enhanced=" << barrier.enabled
            << " enhanced_buffers=" << diagnostics.enhanced_buffer_barriers << '\n';
  device->DestroyPipeline(compute);
}
} // namespace
int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  try {
    const bool enhanced = argc == 2 && std::string_view{argv[1]} == "--enhanced-barriers";
    Require(argc == 1 || enhanced, "usage: [--enhanced-barriers]");
    Portable();
    Native(enhanced);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
