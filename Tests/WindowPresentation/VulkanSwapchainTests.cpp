// Compile the private adapter with Vulkan call tracing; production exports no test hooks.
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vulkan/vulkan.h>

namespace SyncTrace {
using Image = std::pair<VkSwapchainKHR, std::uint32_t>;
std::map<Image, VkSemaphore> imageSignals;
std::unordered_map<VkSemaphore, Image> acquisitions, submissions, presentations;
std::unordered_set<VkSemaphore> semaphores;
std::unordered_set<VkFence> fences;
std::unordered_set<VkSwapchainKHR> swapchains;
std::size_t violations{}, submits{}, presents{}, creations{}, failAt{};
std::size_t fenceCreations{}, failFenceAt{};
bool drained = true, failAcquire{}, suboptimalPresent{};
VkFence acquiredFence{};
bool imageReady{};
VkPresentModeKHR mode{};
std::uint32_t requestedImages{}, actualImages{};

VkResult MakeSemaphore(VkDevice device, const VkSemaphoreCreateInfo *info,
                       const VkAllocationCallbacks *allocator, VkSemaphore *semaphore) {
  if (++creations == failAt) {
    *semaphore = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  const auto result = vkCreateSemaphore(device, info, allocator, semaphore);
  if (result == VK_SUCCESS)
    semaphores.insert(*semaphore);
  return result;
}
void DestroySemaphore(VkDevice device, VkSemaphore semaphore,
                      const VkAllocationCallbacks *allocator) {
  violations += !drained || semaphores.erase(semaphore) != 1;
  acquisitions.erase(semaphore);
  submissions.erase(semaphore);
  presentations.erase(semaphore);
  vkDestroySemaphore(device, semaphore, allocator);
}
VkResult MakeFence(VkDevice device, const VkFenceCreateInfo *info,
                   const VkAllocationCallbacks *allocator, VkFence *fence) {
  if (++fenceCreations == failFenceAt) {
    *fence = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  const auto result = vkCreateFence(device, info, allocator, fence);
  if (result == VK_SUCCESS)
    fences.insert(*fence);
  return result;
}
void DestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *allocator) {
  violations += !drained || fences.erase(fence) != 1;
  vkDestroyFence(device, fence, allocator);
}
VkResult Wait(VkDevice device, std::uint32_t count, const VkFence *waited, VkBool32 all,
              std::uint64_t timeout) {
  const auto result = vkWaitForFences(device, count, waited, all, timeout);
  if (result == VK_SUCCESS && std::find(waited, waited + count, acquiredFence) != waited + count)
    imageReady = true;
  return result;
}
VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                         const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain) {
  mode = info->presentMode;
  requestedImages = info->minImageCount;
  const auto result = vkCreateSwapchainKHR(device, info, allocator, swapchain);
  if (result == VK_SUCCESS)
    swapchains.insert(*swapchain);
  return result;
}
VkResult GetImages(VkDevice device, VkSwapchainKHR swapchain, std::uint32_t *count,
                   VkImage *images) {
  const auto result = vkGetSwapchainImagesKHR(device, swapchain, count, images);
  if (result == VK_SUCCESS)
    actualImages = *count;
  return result;
}
void DestroySwapchain(VkDevice device, VkSwapchainKHR swapchain,
                      const VkAllocationCallbacks *allocator) {
  violations += !drained || swapchains.erase(swapchain) != 1;
  std::erase_if(imageSignals, [&](const auto &entry) { return entry.first.first == swapchain; });
  vkDestroySwapchainKHR(device, swapchain, allocator);
}
VkResult Acquire(VkDevice device, VkSwapchainKHR swapchain, std::uint64_t timeout,
                 VkSemaphore semaphore, VkFence fence, std::uint32_t *index) {
  if (std::exchange(failAcquire, false))
    return VK_ERROR_OUT_OF_DATE_KHR;
  const auto result = vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, index);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
    acquisitions[semaphore] = {swapchain, *index};
    acquiredFence = fence;
    imageReady = false;
  }
  return result;
}
VkResult Submit(VkQueue queue, std::uint32_t count, const VkSubmitInfo *infos, VkFence fence) {
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto &info = infos[i];
    for (std::uint32_t w = 0; w < info.waitSemaphoreCount; ++w) {
      const auto acquired = acquisitions.find(info.pWaitSemaphores[w]);
      if (acquired == acquisitions.end())
        continue;
      for (std::uint32_t s = 0; s < info.signalSemaphoreCount; ++s) {
        const auto signal = info.pSignalSemaphores[s];
        const auto pending = presentations.find(signal);
        // A submit fence alone cannot retire a presentation wait. Reuse requires the same
        // image's acquire semaphore in this submission's dependency chain.
        violations += pending != presentations.end() && pending->second != acquired->second;
        if (pending != presentations.end())
          presentations.erase(pending);
        const auto [previous, inserted] = imageSignals.emplace(acquired->second, signal);
        violations += !inserted && previous->second != signal;
        submissions[signal] = acquired->second;
      }
      acquisitions.erase(acquired);
    }
  }
  const auto result = vkQueueSubmit(queue, count, infos, fence);
  if (result == VK_SUCCESS) {
    drained = false;
    ++submits;
  }
  return result;
}
VkResult Present(VkQueue queue, const VkPresentInfoKHR *info) {
  for (std::uint32_t i = 0; i < info->swapchainCount; ++i) {
    const Image image{info->pSwapchains[i], info->pImageIndices[i]};
    for (std::uint32_t w = 0; w < info->waitSemaphoreCount; ++w) {
      const auto semaphore = info->pWaitSemaphores[w];
      const auto submitted = submissions.find(semaphore);
      violations += submitted == submissions.end() || submitted->second != image;
      presentations[semaphore] = image;
    }
  }
  const auto result = vkQueuePresentKHR(queue, info);
  if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
    ++presents;
  return result == VK_SUCCESS && std::exchange(suboptimalPresent, false) ? VK_SUBOPTIMAL_KHR
                                                                         : result;
}
VkResult Idle(VkDevice device) {
  const auto result = vkDeviceWaitIdle(device);
  if (result == VK_SUCCESS)
    drained = true;
  return result;
}
} // namespace SyncTrace

#define vkCreateSemaphore SyncTrace::MakeSemaphore
#define vkDestroySemaphore SyncTrace::DestroySemaphore
#define vkCreateFence SyncTrace::MakeFence
#define vkDestroyFence SyncTrace::DestroyFence
#define vkWaitForFences SyncTrace::Wait
#define vkCreateSwapchainKHR SyncTrace::CreateSwapchain
#define vkGetSwapchainImagesKHR SyncTrace::GetImages
#define vkDestroySwapchainKHR SyncTrace::DestroySwapchain
#define vkAcquireNextImageKHR SyncTrace::Acquire
#define vkQueueSubmit SyncTrace::Submit
#define vkQueuePresentKHR SyncTrace::Present
#define vkDeviceWaitIdle SyncTrace::Idle
#include "../../Engine/Presentation/src/VulkanSurface.cpp"
#undef vkCreateSemaphore
#undef vkDestroySemaphore
#undef vkCreateFence
#undef vkDestroyFence
#undef vkWaitForFences
#undef vkCreateSwapchainKHR
#undef vkGetSwapchainImagesKHR
#undef vkDestroySwapchainKHR
#undef vkAcquireNextImageKHR
#undef vkQueueSubmit
#undef vkQueuePresentKHR
#undef vkDeviceWaitIdle
#undef None

namespace {
using namespace Nexora;
using namespace Nexora::Presentation;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Clean() {
  Require(SyncTrace::semaphores.empty() && SyncTrace::fences.empty() &&
              SyncTrace::swapchains.empty() && SyncTrace::imageSignals.empty() &&
              SyncTrace::acquisitions.empty() && SyncTrace::submissions.empty() &&
              SyncTrace::presentations.empty() && SyncTrace::violations == 0,
          "swapchain synchronization ownership, drain or cleanup failed");
}
void Run(PresentMode mode, std::uint8_t slots) {
  auto windows = Window::CreateWindowSystem();
  const auto window = windows->Create({"Nexora Vulkan swapchain contracts", 320, 240, true, true});
  Require(bool(window), "swapchain fixture window unavailable");
  VulkanSurface surface(
      {window.handle, 320, 240, slots, mode, ColorSpace::Srgb, SurfaceBackend::Vulkan, true},
      *windows);
  const auto acquired = [&] {
    static_cast<void>(windows->PumpEvents());
    Require(surface.Acquire() == SurfaceStatus::Ready, "swapchain acquire failed");
    Require(SyncTrace::semaphores.size() ==
                SyncTrace::actualImages + std::min<unsigned>(slots, SyncTrace::actualImages),
            "image synchronization and frame upload slots have coupled lifetimes");
    const bool fifo = surface.Diagnostics().negotiatedPresentMode == PresentMode::VSync;
    Require(fifo ? SyncTrace::acquiredFence != VK_NULL_HANDLE && SyncTrace::imageReady
                 : SyncTrace::acquiredFence == VK_NULL_HANDLE,
            "FIFO admitted CPU work before image readiness or Immediate gained a host wait");
    Require(SyncTrace::fences.size() ==
                std::min<unsigned>(slots, SyncTrace::actualImages) + (fifo ? 1 : 0),
            "image readiness allocated an unbounded fence stream");
  };
  const auto presented = [&] {
    Require(surface.Present() == SurfaceStatus::Ready, "swapchain present failed");
  };
  for (unsigned i = 0; i < 48; ++i) {
    acquired();
    presented();
  }
  Require(SyncTrace::mode == (surface.Diagnostics().negotiatedPresentMode == PresentMode::VSync
                                  ? VK_PRESENT_MODE_FIFO_KHR
                                  : VK_PRESENT_MODE_IMMEDIATE_KHR),
          "presentation mode changed while choosing image capacity");
  Require(surface.Diagnostics().gpuTiming.completedSubmission > 0,
          "frame-slot change stopped completed GPU timing");
  Require(surface.ReleaseSpriteTextures({}) == SurfaceStatus::Ready,
          "between-frame resource drain failed");
  for (unsigned i = 0; i < 12; ++i) {
    acquired();
    presented();
  }
  Require(SyncTrace::violations == 0, "present semaphore was reused by a different image");
  SyncTrace::failAcquire = true;
  Require(surface.Acquire() == SurfaceStatus::OutOfDate, "acquire failure injection did not run");
  for (unsigned i = 0; i < 6; ++i) {
    acquired();
    presented();
  }
  acquired();
  SyncTrace::suboptimalPresent = true;
  Require(surface.Present() == SurfaceStatus::OutOfDate, "submitted recovery frame was lost");
  for (unsigned i = 0; i < 6; ++i) {
    acquired();
    presented();
  }
  Require(surface.NotifyWindowExtent(0, 0) == SurfaceStatus::ZeroExtent &&
              surface.Acquire() == SurfaceStatus::ZeroExtent,
          "zero extent did not suspend acquire");
  Require(surface.NotifyWindowExtent(320, 240) == SurfaceStatus::Ready, "swapchain resume failed");
  acquired(); // Abandon an acquired command buffer before teardown.
  Require(surface.DrainAndDestroy() == SurfaceStatus::Ready &&
              surface.DrainAndDestroy() == SurfaceStatus::Ready,
          "swapchain drain is not idempotent");
  Clean();
  // Both partially allocated image semaphores and partially allocated frame slots must unwind.
  for (const auto failure : {1U, 2U, SyncTrace::actualImages + 1}) {
    SyncTrace::failAt = SyncTrace::creations + failure;
    VulkanSurface failed(
        {window.handle, 320, 240, slots, mode, ColorSpace::Srgb, SurfaceBackend::Vulkan}, *windows);
    Require(failed.Acquire() == SurfaceStatus::Unsupported, "semaphore failure did not reject");
    Require(failed.DrainAndDestroy() == SurfaceStatus::Ready, "partial swapchain drain failed");
    Clean();
  }
  SyncTrace::failAt = 0;
  for (const auto failure : {1U, 2U}) {
    SyncTrace::failFenceAt = SyncTrace::fenceCreations + failure;
    VulkanSurface failed(
        {window.handle, 320, 240, slots, mode, ColorSpace::Srgb, SurfaceBackend::Vulkan}, *windows);
    Require(failed.Acquire() == SurfaceStatus::Unsupported, "fence failure did not reject");
    Require(failed.DrainAndDestroy() == SurfaceStatus::Ready, "partial fence drain failed");
    Clean();
  }
  SyncTrace::failFenceAt = 0;
  Require(windows->Destroy(window.handle) == Window::WindowError::None,
          "swapchain fixture teardown failed");
}
} // namespace

int main() {
  try {
    Run(PresentMode::VSync, 2);
    Run(PresentMode::VSync, 3);
    Run(PresentMode::Immediate, 2);
    Require(SyncTrace::submits >= 180 && SyncTrace::presents == SyncTrace::submits,
            "swapchain fixture omitted submitted frames");
    std::cout << "Vulkan FIFO/image semaphore, frame-slot, recovery and cleanup contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
