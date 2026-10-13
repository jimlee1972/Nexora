// Trace real Vulkan allocation/submission calls while compiling the private adapter, following
// the Metal pixel-test pattern. Production exposes no native handles or test-only entry points.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vulkan/vulkan.h>

namespace UploadTrace {
std::unordered_map<VkBuffer, VkDeviceSize> buffers;
std::unordered_map<VkBuffer, VkDeviceMemory> bindings;
std::unordered_set<VkDeviceMemory> memory;
std::unordered_map<VkDeviceMemory, VkDeviceSize> meshMemory;
std::unordered_map<VkCommandBuffer, std::unordered_set<VkBuffer>> commands;
std::unordered_map<VkDeviceMemory, VkFence> pending;
std::size_t creations{}, violations{};
bool geometryAllocation{}, failNextAllocation{};
bool immutableAllocation{}, failNextMeshMap{};
std::size_t peakMeshBytes{};
bool failNextIdle{};
std::size_t idleFailures{}, idleCalls{};
bool failNextQueryAllocation{}, failNextTimingRead{};
std::size_t timingReadFailures{}, queryAllocationFailures{};
std::unordered_set<VkQueryPool> queryPools;
std::unordered_map<VkCommandBuffer, VkQueryPool> queryCommands;
std::unordered_map<VkQueryPool, VkFence> pendingQueries;
VkResult CreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *info,
                         const VkAllocationCallbacks *allocator, VkQueryPool *pool) {
  if (std::exchange(failNextQueryAllocation, false)) {
    ++queryAllocationFailures;
    *pool = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  const auto result = vkCreateQueryPool(device, info, allocator, pool);
  if (result == VK_SUCCESS)
    queryPools.insert(*pool);
  return result;
}
void DestroyQueryPool(VkDevice device, VkQueryPool pool, const VkAllocationCallbacks *allocator) {
  violations += pendingQueries.contains(pool);
  queryPools.erase(pool);
  vkDestroyQueryPool(device, pool, allocator);
}
void ResetQueryPool(VkCommandBuffer command, VkQueryPool pool, std::uint32_t first,
                    std::uint32_t count) {
  violations += pendingQueries.contains(pool);
  queryCommands[command] = pool;
  vkCmdResetQueryPool(command, pool, first, count);
}
VkResult GetQueryPoolResults(VkDevice device, VkQueryPool pool, std::uint32_t first,
                             std::uint32_t count, std::size_t bytes, void *data,
                             VkDeviceSize stride, VkQueryResultFlags flags) {
  violations += pendingQueries.contains(pool) || (flags & VK_QUERY_RESULT_WAIT_BIT) != 0;
  if (std::exchange(failNextTimingRead, false)) {
    ++timingReadFailures;
    return VK_NOT_READY;
  }
  return vkGetQueryPoolResults(device, pool, first, count, bytes, data, stride, flags);
}
VkResult CreateBuffer(VkDevice device, const VkBufferCreateInfo *info,
                      const VkAllocationCallbacks *allocator, VkBuffer *buffer) {
  const auto result = vkCreateBuffer(device, info, allocator, buffer);
  geometryAllocation =
      result == VK_SUCCESS &&
      (info->usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) ==
          (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  immutableAllocation = geometryAllocation && !(info->usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  if (geometryAllocation) {
    buffers[*buffer] = info->size;
    ++creations;
  }
  return result;
}
void DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *allocator) {
  if (const auto found = bindings.find(buffer); found != bindings.end()) {
    violations += pending.contains(found->second);
    bindings.erase(found);
  }
  buffers.erase(buffer);
  vkDestroyBuffer(device, buffer, allocator);
}
VkResult AllocateMemory(VkDevice device, const VkMemoryAllocateInfo *info,
                        const VkAllocationCallbacks *allocator, VkDeviceMemory *allocated) {
  const bool geometry = std::exchange(geometryAllocation, false);
  const bool immutable = std::exchange(immutableAllocation, false);
  if (geometry && std::exchange(failNextAllocation, false)) {
    *allocated = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }
  const auto result = vkAllocateMemory(device, info, allocator, allocated);
  if (geometry && result == VK_SUCCESS)
    memory.insert(*allocated);
  if (immutable && result == VK_SUCCESS) {
    meshMemory[*allocated] = info->allocationSize;
    std::size_t total = 0;
    for (const auto &[handle, bytes] : meshMemory) {
      static_cast<void>(handle);
      total += bytes;
    }
    peakMeshBytes = std::max(peakMeshBytes, total);
  }
  return result;
}
void FreeMemory(VkDevice device, VkDeviceMemory allocated, const VkAllocationCallbacks *allocator) {
  meshMemory.erase(allocated);
  if (memory.erase(allocated))
    violations += pending.contains(allocated);
  pending.erase(allocated);
  vkFreeMemory(device, allocated, allocator);
}
VkResult BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory allocated,
                          VkDeviceSize offset) {
  const auto result = vkBindBufferMemory(device, buffer, allocated, offset);
  if (result == VK_SUCCESS && buffers.contains(buffer))
    bindings[buffer] = allocated;
  return result;
}
VkResult MapMemory(VkDevice device, VkDeviceMemory allocated, VkDeviceSize offset,
                   VkDeviceSize size, VkMemoryMapFlags flags, void **data) {
  if (memory.contains(allocated))
    violations += pending.contains(allocated);
  if (meshMemory.contains(allocated) && std::exchange(failNextMeshMap, false))
    return VK_ERROR_MEMORY_MAP_FAILED;
  return vkMapMemory(device, allocated, offset, size, flags, data);
}
VkResult WaitForFences(VkDevice device, std::uint32_t count, const VkFence *fences, VkBool32 all,
                       std::uint64_t timeout) {
  const auto result = vkWaitForFences(device, count, fences, all, timeout);
  if (result == VK_SUCCESS && (all || count == 1)) {
    const auto completed = [&](const auto &entry) {
      return std::find(fences, fences + count, entry.second) != fences + count;
    };
    std::erase_if(pending, completed);
    std::erase_if(pendingQueries, completed);
  }
  return result;
}
VkResult DeviceWaitIdle(VkDevice device) {
  ++idleCalls;
  if (std::exchange(failNextIdle, false)) {
    ++idleFailures;
    return VK_ERROR_DEVICE_LOST;
  }
  const auto result = vkDeviceWaitIdle(device);
  if (result == VK_SUCCESS) {
    pending.clear();
    pendingQueries.clear();
  }
  return result;
}
void BindVertexBuffers(VkCommandBuffer command, std::uint32_t first, std::uint32_t count,
                       const VkBuffer *bound, const VkDeviceSize *offsets) {
  for (std::uint32_t index = 0; index < count; ++index)
    if (buffers.contains(bound[index]))
      commands[command].insert(bound[index]);
  vkCmdBindVertexBuffers(command, first, count, bound, offsets);
}
VkResult ResetCommandBuffer(VkCommandBuffer command, VkCommandBufferResetFlags flags) {
  const auto result = vkResetCommandBuffer(command, flags);
  if (result == VK_SUCCESS) {
    commands.erase(command);
    queryCommands.erase(command);
  }
  return result;
}
void FreeCommandBuffers(VkDevice device, VkCommandPool pool, std::uint32_t count,
                        const VkCommandBuffer *freed) {
  for (std::uint32_t i = 0; i < count; ++i) {
    commands.erase(freed[i]);
    queryCommands.erase(freed[i]);
  }
  vkFreeCommandBuffers(device, pool, count, freed);
}
VkResult QueueSubmit(VkQueue queue, std::uint32_t count, const VkSubmitInfo *submits,
                     VkFence fence) {
  const auto result = vkQueueSubmit(queue, count, submits, fence);
  if (result == VK_SUCCESS)
    for (std::uint32_t i = 0; i < count; ++i)
      for (std::uint32_t j = 0; j < submits[i].commandBufferCount; ++j) {
        const auto query = queryCommands.find(submits[i].pCommandBuffers[j]);
        if (query != queryCommands.end())
          pendingQueries[query->second] = fence;
        const auto command = commands.find(submits[i].pCommandBuffers[j]);
        if (command != commands.end()) {
          for (const auto buffer : command->second) {
            const auto bound = bindings.find(buffer);
            if (bound != bindings.end())
              pending[bound->second] = fence;
          }
        }
      }
  return result;
}
} // namespace UploadTrace

#define vkCreateQueryPool UploadTrace::CreateQueryPool
#define vkDestroyQueryPool UploadTrace::DestroyQueryPool
#define vkCmdResetQueryPool UploadTrace::ResetQueryPool
#define vkGetQueryPoolResults UploadTrace::GetQueryPoolResults
#define vkCreateBuffer UploadTrace::CreateBuffer
#define vkDestroyBuffer UploadTrace::DestroyBuffer
#define vkAllocateMemory UploadTrace::AllocateMemory
#define vkFreeMemory UploadTrace::FreeMemory
#define vkBindBufferMemory UploadTrace::BindBufferMemory
#define vkMapMemory UploadTrace::MapMemory
#define vkWaitForFences UploadTrace::WaitForFences
#define vkDeviceWaitIdle UploadTrace::DeviceWaitIdle
#define vkCmdBindVertexBuffers UploadTrace::BindVertexBuffers
#define vkResetCommandBuffer UploadTrace::ResetCommandBuffer
#define vkFreeCommandBuffers UploadTrace::FreeCommandBuffers
#define vkQueueSubmit UploadTrace::QueueSubmit
#include "../../Engine/Presentation/src/VulkanSurface.cpp"
#undef vkCreateQueryPool
#undef vkDestroyQueryPool
#undef vkCmdResetQueryPool
#undef vkGetQueryPoolResults
#undef vkCreateBuffer
#undef vkDestroyBuffer
#undef vkAllocateMemory
#undef vkFreeMemory
#undef vkBindBufferMemory
#undef vkMapMemory
#undef vkWaitForFences
#undef vkDeviceWaitIdle
#undef vkCmdBindVertexBuffers
#undef vkResetCommandBuffer
#undef vkFreeCommandBuffers
#undef vkQueueSubmit
#undef None
#include <X11/Xutil.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace Nexora;
using namespace Nexora::Presentation;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
unsigned Channel(unsigned long pixel, unsigned long mask) {
  while (mask && !(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return mask ? static_cast<unsigned>((pixel & mask) * 255 / mask) : 0;
}
void Pixels(Display *display, ::Window window, unsigned width, unsigned height, bool green,
            const std::filesystem::path &capture = {}, std::optional<unsigned> blueMarker = {}) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
    Require(image != nullptr, "upload reuse readback failed");
    const auto pixel = XGetPixel(image, green ? width * 3 / 4 : width / 2, height / 2);
    const auto empty = XGetPixel(image, green ? width / 2 : width * 3 / 4, height / 2);
    bool matches =
        green ? Channel(pixel, image->green_mask) > 180 && Channel(pixel, image->red_mask) < 20
              : Channel(pixel, image->red_mask) > 180 && Channel(pixel, image->green_mask) < 20;
    if (blueMarker) {
      const auto observed = static_cast<int>(Channel(pixel, image->blue_mask));
      const float linear = static_cast<float>(*blueMarker) / 255;
      const auto srgb =
          static_cast<int>(std::lround(255 * (1.055F * std::pow(linear, 1 / 2.4F) - 0.055F)));
      matches = matches && (std::abs(observed - static_cast<int>(*blueMarker)) <= 3 ||
                            std::abs(observed - srgb) <= 3);
    }
    const bool background =
        Channel(empty, image->red_mask) < 80 && Channel(empty, image->green_mask) < 80;
    if (matches && background && !capture.empty()) {
      std::ofstream file(capture, std::ios::binary);
      file << "P6\n" << width << ' ' << height << "\n255\n";
      for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
          const auto sample = XGetPixel(image, x, y);
          const char rgb[]{static_cast<char>(Channel(sample, image->red_mask)),
                           static_cast<char>(Channel(sample, image->green_mask)),
                           static_cast<char>(Channel(sample, image->blue_mask))};
          file.write(rgb, sizeof(rgb));
        }
      Require(file.good(), "upload reuse capture write failed");
    }
    XDestroyImage(image);
    if (matches && background)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  throw std::runtime_error("reused scene storage retained stale geometry/instance pixels");
}
void RunResidency() {
  auto windows = Window::CreateWindowSystem();
  const auto window = windows->Create({"Nexora Vulkan mesh residency", 640, 480, true, true});
  Require(static_cast<bool>(window), "mesh residency window creation failed");
  auto surface = std::make_unique<VulkanSurface>(
      SurfaceDescriptor{window.handle, 640, 480, 2, PresentMode::Immediate, ColorSpace::Srgb,
                        SurfaceBackend::Vulkan},
      *windows);
  Display *display = XOpenDisplay(nullptr);
  Require(display && surface->SupportsSceneGeometryResidency(), "mesh residency unavailable");
  const auto native = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
  const std::array<SceneVertex, 3> triangle{{{{-0.8F, -0.8F, 0.5F}, {0, 0, 1}},
                                             {{0.8F, -0.8F, 0.5F}, {0, 0, 1}},
                                             {{0, 0.8F, 0.5F}, {0, 0, 1}}}};
  std::vector<SceneVertex> vertices(triangle.begin(), triangle.end());
  for (auto vertex : triangle) {
    vertex.position[0] += 3;
    vertices.push_back(vertex);
  }
  std::array<std::uint16_t, 6> indices{0, 1, 2, 3, 4, 5};
  std::array<SceneMeshBatch, 2> batches{{{0, 3, 0, 1}, {3, 3, 0, 1}}};
  std::array<SceneInstance, 1> instances{};
  std::array<SceneMaterial, 1> materials{};
  SceneDrawData draw{vertices, indices, instances};
  draw.batches = batches;
  draw.materials = materials;
  draw.retainMeshGeometry = true;
  draw.light_direction[0] = draw.light_direction[1] = 0;
  draw.light_direction[2] = -1;
  draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 1;
  unsigned width = 640, height = 480;
  unsigned submission = 0;
  const auto submit = [&](bool green, bool pixels = true, bool geometryShift = false) {
    Require(surface->Acquire() == SurfaceStatus::Ready, "mesh residency acquire failed");
    instances[0].translation[0] = green && !geometryShift ? 0.75F : 0;
    materials[0].baseColor =
        green ? std::array<float, 4>{0, 1, 0, 1} : std::array<float, 4>{1, 0, 0, 1};
    // Adjacent blue codes differ under both supported display transfers. This excludes an
    // asynchronously presented prior frame even when successive geometry/material poses match.
    const unsigned marker = (++submission % 8 + 1) * 8;
    materials[0].baseColor[2] = static_cast<float>(marker) / 255;
    draw.vertices = vertices;
    Require(surface->DrawScene(draw) == SurfaceStatus::Ready &&
                surface->Present() == SurfaceStatus::Ready,
            "mesh residency submit failed");
    if (pixels)
      Pixels(display, native, width, height, green, {}, marker);
  };
  for (int frame = 0; frame < 6; ++frame)
    submit(frame % 2 != 0);
  auto before = surface->Diagnostics().sceneGeometryResidency;
  Require(before.available && before.uploads == 2 && before.hits >= 10 &&
              before.nativeAllocationBytes <= 32 * 1024 * 1024 && before.retainedCpuBytes > 0 &&
              surface->Diagnostics().sceneGeometryUploadCache.uploads == 0,
          "mesh residency did not skip combined geometry uploads across frame slots");
  for (std::size_t i = 0; i < 3; ++i)
    std::swap(vertices[i], vertices[i + 3]);
  submit(false);
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads,
          "reordered combined upload reuploaded unchanged mesh content");
  // Fail a new offscreen mesh admission while retaining the visible cached triangle.
  vertices[0].uv[0] = 0.25F;
  UploadTrace::failNextAllocation = true;
  submit(false);
  Require(!UploadTrace::failNextAllocation &&
              surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads &&
              surface->Diagnostics().sceneGeometryResidency.nativeAllocationBytes ==
                  before.nativeAllocationBytes,
          "failed cache allocation discarded residents instead of using the legacy fallback");
  submit(true);
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads + 1,
          "cache allocation failure prevented later retry");
  before = surface->Diagnostics().sceneGeometryResidency;
  vertices[0].uv[0] = 0.5F;
  UploadTrace::failNextMeshMap = true;
  submit(false);
  Require(!UploadTrace::failNextMeshMap &&
              surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads &&
              surface->Diagnostics().sceneGeometryResidency.nativeAllocationBytes ==
                  before.nativeAllocationBytes,
          "failed candidate mapping mutated residency instead of falling back");
  vertices[0].uv[0] = 0;
  submit(false);
  before = surface->Diagnostics().sceneGeometryResidency;
  // Fill every frame slot with legacy keys, overwrite its old prefix with cached-mode instances,
  // then request the original legacy geometry again. A surviving key would skip corrupted bytes.
  for (const bool retained : {false, true, false}) {
    draw.retainMeshGeometry = retained;
    for (int frame = 0; frame < 6; ++frame)
      submit(frame % 2 != 0);
  }
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads &&
              surface->Diagnostics().sceneGeometryUploadCache.uploads >= 4,
          "legacy/cached switching retained stale prefix keys or reuploaded cached meshes");
  draw.retainMeshGeometry = true;
  before = surface->Diagnostics().sceneGeometryResidency;
  for (std::size_t i = 3; i < 6; ++i)
    vertices[i].position[0] += 0.75F;
  submit(true, true, true);
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads + 1,
          "changed visible geometry reused the old native buffer");
  std::copy(triangle.begin(), triangle.end(), vertices.begin() + 3);
  submit(false);
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads + 1,
          "restored geometry could not reuse its immutable prior version");
  // Large canonical intervals pressure the 32 MiB native allocation budget while rendering one
  // triangle. Only an unreferenced vertex changes: pixels remain an independent ownership oracle.
  vertices.assign(65535, {});
  vertices[0] = triangle[0];
  vertices[65534] = triangle[1];
  vertices[1] = triangle[2];
  const std::array<std::uint16_t, 3> wideIndices{0, 65534, 1};
  draw.indices = wideIndices;
  draw.batches = {};
  for (int version = 0; version < 10; ++version) {
    vertices[2].uv[0] = static_cast<float>(version);
    submit(false);
    const auto stats = surface->Diagnostics().sceneGeometryResidency;
    std::size_t actual = 0;
    for (const auto &[handle, bytes] : UploadTrace::meshMemory) {
      static_cast<void>(handle);
      actual += bytes;
    }
    Require(stats.nativeAllocationBytes <= 32 * 1024 * 1024 &&
                stats.nativeAllocationBytes == actual &&
                UploadTrace::peakMeshBytes <= 40 * 1024 * 1024 &&
                stats.retainedCpuBytes <= 32 * 1024 * 1024 && UploadTrace::violations == 0,
            "residency exceeded its budget or retired incomplete GPU references");
  }
  before = surface->Diagnostics().sceneGeometryResidency;
  Require(before.evictions > 0, "native allocation budget never exercised eviction");
  vertices[2].uv[0] = 11;
  UploadTrace::failNextIdle = true;
  submit(false);
  const auto failed = surface->Diagnostics().sceneGeometryResidency;
  Require(!UploadTrace::failNextIdle && UploadTrace::idleFailures == 1 &&
              failed.uploads == before.uploads && failed.evictions == before.evictions &&
              failed.retainedCpuBytes == before.retainedCpuBytes &&
              failed.nativeAllocationBytes == before.nativeAllocationBytes,
          "failed GPU completion published eviction or lost the retained cache");
  submit(true);
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads + 1,
          "completion failure prevented correct admission retry");
  before = surface->Diagnostics().sceneGeometryResidency;
  Require(windows->Resize(window.handle, 800, 600) == Window::WindowError::None,
          "mesh residency resize failed");
  static_cast<void>(windows->PumpEvents());
  width = 800;
  height = 600;
  Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
          "mesh residency resize publication failed");
  submit(false);
  Require(surface->Diagnostics().sceneGeometryResidency.uploads == before.uploads &&
              surface->Diagnostics().sceneGeometryResidency.nativeAllocationBytes ==
                  before.nativeAllocationBytes,
          "resize discarded immutable mesh residency");
  Require(surface->DrainAndDestroy() == SurfaceStatus::Ready &&
              surface->Diagnostics().sceneGeometryResidency.retainedCpuBytes == 0 &&
              surface->Diagnostics().sceneGeometryResidency.nativeAllocationBytes == 0 &&
              UploadTrace::buffers.empty() && UploadTrace::memory.empty() &&
              UploadTrace::meshMemory.empty() && UploadTrace::pending.empty() &&
              UploadTrace::violations == 0,
          "mesh residency teardown leaked CPU/native allocations or pending references");
  XCloseDisplay(display);
  Require(windows->Destroy(window.handle) == Window::WindowError::None,
          "mesh residency window teardown failed");
}
void Run(const std::filesystem::path &capture) {
  auto windows = Window::CreateWindowSystem();
  Require(windows != nullptr, "upload reuse window system unavailable");
  const auto window = windows->Create({"Nexora Vulkan upload reuse", 640, 480, true, true});
  Require(static_cast<bool>(window), "upload reuse window creation failed");
  UploadTrace::failNextQueryAllocation = true;
  auto surface = std::make_unique<VulkanSurface>(
      SurfaceDescriptor{window.handle, 640, 480, 2, PresentMode::Immediate, ColorSpace::Srgb,
                        SurfaceBackend::Vulkan, true},
      *windows);
  Display *display = XOpenDisplay(nullptr);
  Require(display != nullptr, "upload reuse X11 display unavailable");
  const auto native = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
  unsigned width = 640, height = 480;
  std::vector<SceneVertex> vertices{{{-0.8F, -0.8F, 0.5F}, {0, 0, 1}},
                                    {{0.8F, -0.8F, 0.5F}, {0, 0, 1}},
                                    {{0, 0.8F, 0.5F}, {0, 0, 1}}};
  vertices.resize(6);
  std::array<std::uint16_t, 3> indices{0, 1, 2};
  std::array<SceneInstance, 1> instances{};
  SceneDrawData draw{vertices, indices, instances};
  draw.light_direction[0] = draw.light_direction[1] = 0;
  draw.light_direction[2] = -1;
  draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 1;
  const auto submit = [&](bool green) {
    Require(surface->Acquire() == SurfaceStatus::Ready, "upload reuse acquire failed");
    instances[0].color[0] = green ? 0 : 1;
    instances[0].color[1] = green ? 1 : 0;
    instances[0].color[2] = 0;
    // The selected triangle moves from center to right; the unselected triangle is offscreen.
    // Reusing stale vertex, index or instance bytes independently fails the two-point pixel oracle.
    for (std::size_t i = 0; i < 3; ++i) {
      const float x = i == 0 ? -0.8F : i == 1 ? 0.8F : 0;
      const float y = i == 2 ? 0.8F : -0.8F;
      vertices[i] = {{x + (green ? -2.0F : 0), y, 0.5F}, {0, 0, 1}};
      vertices[i + 3] = {{x + (green ? 0.75F : -2.0F), y, 0.5F}, {0, 0, 1}};
      indices[i] = static_cast<std::uint16_t>(i + (green ? 3 : 0));
    }
    draw.vertices = vertices;
    Require(surface->DrawScene(draw) == SurfaceStatus::Ready &&
                surface->Present() == SurfaceStatus::Ready,
            "upload reuse scene submit failed");
    Pixels(display, native, width, height, green);
  };
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  const auto slots = UploadTrace::buffers.size();
  Require(slots >= 2 && slots <= 3 && UploadTrace::creations == slots,
          "scene upload did not warm one allocation per frame slot");
  Require(UploadTrace::queryAllocationFailures == 1 && !UploadTrace::failNextQueryAllocation,
          "optional query allocation failure broke rendering or fixture did not execute");
  UploadTrace::failNextTimingRead = true;
  for (int i = 0; i < 3; ++i) {
    const auto before = UploadTrace::timingReadFailures;
    submit(i % 2 != 0);
    if (UploadTrace::timingReadFailures > before)
      Require(!surface->Diagnostics().gpuTiming.milliseconds,
              "failed query read fabricated latest timing from a historical success");
  }
  Require(UploadTrace::timingReadFailures == 1 && !UploadTrace::failNextTimingRead,
          "failed optional native query read did not execute");
  for (int i = 0; i < 100; ++i)
    submit(i % 2 != 0);
  Require(UploadTrace::creations == slots && UploadTrace::violations == 0,
          "steady scene draws allocated buffers or overwrote pending GPU memory");
  // Keep geometry fixed while changing actual instance tint/translation. Cached geometry must
  // remain resident, but the two-point native pixel oracle must still observe each new instance.
  submit(false);
  instances[0].model_transform =
      std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const auto submitInstance = [&](bool green) {
    Require(surface->Acquire() == SurfaceStatus::Ready, "cached geometry acquire failed");
    instances[0].color[0] = green ? 0 : 1;
    instances[0].color[1] = green ? 1 : 0;
    (*instances[0].model_transform)[3] = green ? 0.75F : 0;
    Require(surface->DrawScene(draw) == SurfaceStatus::Ready &&
                surface->Present() == SurfaceStatus::Ready,
            "cached geometry instance submit failed");
    Pixels(display, native, width, height, green);
  };
  for (int i = 0; i < 6; ++i)
    submitInstance(i % 2 != 0);
  const auto warmCache = surface->Diagnostics().sceneGeometryUploadCache;
  for (int i = 0; i < 30; ++i)
    submitInstance(i % 2 != 0);
  const auto steadyCache = surface->Diagnostics().sceneGeometryUploadCache;
  Require(steadyCache.available && steadyCache.uploads == warmCache.uploads &&
              steadyCache.hits == warmCache.hits + 30 &&
              steadyCache.retainedCpuBytes ==
                  slots * (vertices.size() * sizeof(SceneVertex) + sizeof(indices)) &&
              UploadTrace::creations == slots && UploadTrace::violations == 0,
          "unchanged geometry was recopied, lost its bounded key or cached changed instances");
  // A larger instance region replaces the buffer even though its geometry prefix is identical.
  std::vector<SceneInstance> manyInstances(64, instances[0]);
  draw.instances = manyInstances;
  const auto beforeInstanceGrowth = surface->Diagnostics().sceneGeometryUploadCache;
  for (int i = 0; i < 6; ++i) {
    Require(surface->Acquire() == SurfaceStatus::Ready &&
                surface->DrawScene(draw) == SurfaceStatus::Ready &&
                surface->Present() == SurfaceStatus::Ready,
            "unchanged geometry instance growth failed");
    Pixels(display, native, width, height, true);
  }
  Require(UploadTrace::creations == 2 * slots &&
              surface->Diagnostics().sceneGeometryUploadCache.uploads >=
                  beforeInstanceGrowth.uploads + slots &&
              surface->Diagnostics().sceneGeometryUploadCache.retainedCpuBytes ==
                  steadyCache.retainedCpuBytes,
          "instance growth reused a retired geometry allocation or changed its bounded key");
  draw.instances = instances;
  instances[0].model_transform.reset();
  const auto beforeVertexGrowth = UploadTrace::creations;
  vertices.resize(300);
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  Require(UploadTrace::creations == beforeVertexGrowth + slots &&
              UploadTrace::buffers.size() == slots,
          "larger scenes did not replace only their completed slots");
  const auto afterVertexGrowth = UploadTrace::creations;
  const auto retained = UploadTrace::buffers;
  Require(surface->Acquire() == SurfaceStatus::Ready, "failed growth acquire failed");
  vertices.resize(3000);
  draw.vertices = vertices;
  UploadTrace::failNextAllocation = true;
  const auto beforeFailure = surface->Diagnostics().sceneGeometryUploadCache;
  Require(surface->DrawScene(draw) == SurfaceStatus::DeviceLost &&
              !UploadTrace::failNextAllocation && UploadTrace::buffers == retained,
          "failed growth destroyed the old allocation or leaked staged storage");
  const auto failedCache = surface->Diagnostics().sceneGeometryUploadCache;
  Require(failedCache.uploads == beforeFailure.uploads && failedCache.hits == beforeFailure.hits &&
              failedCache.retainedCpuBytes == beforeFailure.retainedCpuBytes,
          "failed growth changed the retained geometry key or upload counters");
  vertices.resize(300);
  draw.vertices = vertices;
  Require(surface->DrawScene(draw) == SurfaceStatus::Ready &&
              surface->Present() == SurfaceStatus::Ready,
          "failed grow consumed the frame or prevented a smaller retry");
  Pixels(display, native, width, height, true);
  const auto after_failure = UploadTrace::creations;
  Require(after_failure == afterVertexGrowth + 1 && UploadTrace::memory.size() == slots,
          "failed growth lost allocation/memory accounting");
  vertices.resize(3000);
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  Require(UploadTrace::creations == after_failure + slots,
          "successful growth failed to reuse later slot visits");
  vertices.resize(6);
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  const auto before_maximum = UploadTrace::creations;
  vertices.resize(65535);
  std::vector<std::uint16_t> maximum_indices(1048575);
  for (std::size_t i = 0; i < maximum_indices.size(); ++i)
    maximum_indices[i] = static_cast<std::uint16_t>(3 + i % 3);
  std::vector<SceneInstance> maximum_instances(4096);
  maximum_instances.front().color[0] = maximum_instances.front().color[2] = 0;
  const std::array batches{SceneMeshBatch{0, 3, 0, 1}};
  draw.indices = maximum_indices;
  draw.instances = maximum_instances;
  draw.batches = batches;
  for (int i = 0; i < 6; ++i)
    submit(true); // Upload the complete maximum descriptor, but draw one triangle/instance.
  Require(
      UploadTrace::creations == before_maximum + slots &&
          std::ranges::all_of(UploadTrace::buffers,
                              [](const auto &entry) { return entry.second == 8 * 1024 * 1024; }),
      "maximum descriptor exceeded the bounded reusable upload capacity");
  draw.indices = indices;
  draw.instances = instances;
  draw.batches = {};
  vertices.resize(6);
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  const auto peak_creations = UploadTrace::creations;
  for (int i = 0; i < 6; ++i)
    Require(surface->Acquire() == SurfaceStatus::Ready &&
                surface->Present() == SurfaceStatus::Ready,
            "scene-free frame failed");
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  Require(UploadTrace::creations == peak_creations && UploadTrace::buffers.size() == slots,
          "shrinking or scene-free frames discarded reusable storage");
  const auto beforeResize = surface->Diagnostics().sceneGeometryUploadCache;
  Require(windows->Resize(window.handle, 800, 600) == Window::WindowError::None,
          "upload reuse window resize failed");
  static_cast<void>(windows->PumpEvents());
  width = 800;
  height = 600;
  Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
          "upload reuse resize publication failed");
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  Require(UploadTrace::creations == peak_creations + slots &&
              UploadTrace::buffers.size() == slots && UploadTrace::memory.size() == slots,
          "resize did not drain and rebuild its upload generation");
  Require(surface->Diagnostics().sceneGeometryUploadCache.uploads >= beforeResize.uploads + slots,
          "resize reused geometry from a retired GPU allocation");
  Require(surface->NotifyWindowExtent(0, 0) == SurfaceStatus::ZeroExtent &&
              surface->Acquire() == SurfaceStatus::ZeroExtent,
          "zero extent did not suspend upload work");
  Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
          "upload reuse resume failed");
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  Pixels(display, native, width, height, true, capture);
  Require(surface->Acquire() == SurfaceStatus::Ready &&
              surface->DrawScene(draw) == SurfaceStatus::Ready,
          "abandoned recording fixture failed");
  Require(surface->DrainAndDestroy() == SurfaceStatus::Ready &&
              surface->DrainAndDestroy() == SurfaceStatus::Ready && UploadTrace::buffers.empty() &&
              UploadTrace::memory.empty() && UploadTrace::pending.empty() &&
              UploadTrace::queryPools.empty() && UploadTrace::pendingQueries.empty() &&
              UploadTrace::queryCommands.empty() && UploadTrace::violations == 0,
          "resize/teardown leaked storage or freed it before GPU completion");
  Require(surface->Diagnostics().sceneGeometryUploadCache.retainedCpuBytes == 0,
          "teardown retained owning geometry comparison bytes");
  // Existing consumers opt out: no query resources or completed timing stream are created.
  auto disabled = std::make_unique<VulkanSurface>(
      SurfaceDescriptor{window.handle, width, height, 2, PresentMode::Immediate, ColorSpace::Srgb,
                        SurfaceBackend::Vulkan},
      *windows);
  Require(UploadTrace::queryPools.empty(), "default surface allocated GPU profiling resources");
  for (int i = 0; i < 4; ++i)
    Require(disabled->Acquire() == SurfaceStatus::Ready &&
                disabled->Present() == SurfaceStatus::Ready,
            "default surface stopped rendering without GPU profiling");
  Require(disabled->DrainAndDestroy() == SurfaceStatus::Ready &&
              disabled->Diagnostics().gpuTiming.source == GpuTimingSource::Unavailable &&
              disabled->Diagnostics().gpuTiming.completedSubmission == 0 &&
              !disabled->Diagnostics().gpuTiming.milliseconds && UploadTrace::queryPools.empty() &&
              UploadTrace::violations == 0,
          "default surface fabricated measurements or retained query resources");
  XCloseDisplay(display);
  Require(windows->Destroy(window.handle) == Window::WindowError::None,
          "upload reuse window teardown failed");
}
} // namespace
int main(int argc, char **argv) {
  try {
    Run(argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path{});
    RunResidency();
    std::cout << "Vulkan scene upload reuse and immutable mesh residency contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
