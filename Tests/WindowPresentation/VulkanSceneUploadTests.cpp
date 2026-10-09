// Trace real Vulkan allocation/submission calls while compiling the private adapter, following
// the Metal pixel-test pattern. Production exposes no native handles or test-only entry points.
#include <algorithm>
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
std::unordered_map<VkCommandBuffer, VkBuffer> commands;
std::unordered_map<VkDeviceMemory, VkFence> pending;
std::size_t creations{}, violations{};
bool geometryAllocation{}, failNextAllocation{};
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
  if (geometry && std::exchange(failNextAllocation, false)) {
    *allocated = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }
  const auto result = vkAllocateMemory(device, info, allocator, allocated);
  if (geometry && result == VK_SUCCESS)
    memory.insert(*allocated);
  return result;
}
void FreeMemory(VkDevice device, VkDeviceMemory allocated, const VkAllocationCallbacks *allocator) {
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
  const auto result = vkDeviceWaitIdle(device);
  if (result == VK_SUCCESS) {
    pending.clear();
    pendingQueries.clear();
  }
  return result;
}
void BindVertexBuffers(VkCommandBuffer command, std::uint32_t first, std::uint32_t count,
                       const VkBuffer *bound, const VkDeviceSize *offsets) {
  if (first == 0 && count && buffers.contains(bound[0]))
    commands[command] = bound[0];
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
          const auto bound = bindings.find(command->second);
          if (bound != bindings.end())
            pending[bound->second] = fence;
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
            const std::filesystem::path &capture = {}) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
    Require(image != nullptr, "upload reuse readback failed");
    const auto pixel = XGetPixel(image, green ? width * 3 / 4 : width / 2, height / 2);
    const auto empty = XGetPixel(image, green ? width / 2 : width * 3 / 4, height / 2);
    const bool matches =
        green ? Channel(pixel, image->green_mask) > 180 && Channel(pixel, image->red_mask) < 20
              : Channel(pixel, image->red_mask) > 180 && Channel(pixel, image->green_mask) < 20;
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
  vertices.resize(300);
  for (int i = 0; i < 6; ++i)
    submit(i % 2 != 0);
  Require(UploadTrace::creations == 2 * slots && UploadTrace::buffers.size() == slots,
          "larger scenes did not replace only their completed slots");
  const auto retained = UploadTrace::buffers;
  Require(surface->Acquire() == SurfaceStatus::Ready, "failed growth acquire failed");
  vertices.resize(3000);
  draw.vertices = vertices;
  UploadTrace::failNextAllocation = true;
  Require(surface->DrawScene(draw) == SurfaceStatus::DeviceLost &&
              !UploadTrace::failNextAllocation && UploadTrace::buffers == retained,
          "failed growth destroyed the old allocation or leaked staged storage");
  vertices.resize(300);
  draw.vertices = vertices;
  Require(surface->DrawScene(draw) == SurfaceStatus::Ready &&
              surface->Present() == SurfaceStatus::Ready,
          "failed grow consumed the frame or prevented a smaller retry");
  Pixels(display, native, width, height, true);
  const auto after_failure = UploadTrace::creations;
  Require(after_failure == 2 * slots + 1 && UploadTrace::memory.size() == slots,
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
    std::cout << "Vulkan fence-owned scene upload reuse contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
