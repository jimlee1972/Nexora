#if !defined(__linux__) && !defined(_WIN32)
#error "VulkanSurface.cpp requires a desktop Vulkan host"
#endif

#include "Nexora/Presentation/Surface.h"

#include <vulkan/vulkan.h>
#if defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#else
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <vulkan/vulkan_win32.h>
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <vector>

namespace Nexora::Presentation {
namespace {
class VulkanSurface final : public ISurface {
public:
  VulkanSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows)
      : thread_(std::this_thread::get_id()), width_(descriptor.width), height_(descriptor.height),
        requestedColorSpace_(descriptor.colorSpace), requestedPresentMode_(descriptor.presentMode) {
    if (!descriptor.window.IsValid() || descriptor.framesInFlight < 2 ||
        descriptor.framesInFlight > kMaxFrames)
      return;
#if defined(__linux__)
    display_ = XOpenDisplay(nullptr);
    nativeWindow_ = reinterpret_cast<::Window>(windows.NativeHandle(descriptor.window));
    if (!display_ || !nativeWindow_)
      return;
    const char *extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
#else
    nativeWindow_ = static_cast<HWND>(windows.NativeHandle(descriptor.window));
    if (!nativeWindow_)
      return;
    const char *extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
#endif
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "NexoraPresentation";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &app;
    create.enabledExtensionCount = 2;
    create.ppEnabledExtensionNames = extensions;
    if (vkCreateInstance(&create, nullptr, &instance_) != VK_SUCCESS)
      return;
#if defined(__linux__)
    VkXlibSurfaceCreateInfoKHR surfaceCreate{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
    surfaceCreate.dpy = display_;
    surfaceCreate.window = nativeWindow_;
    if (vkCreateXlibSurfaceKHR(instance_, &surfaceCreate, nullptr, &surface_) != VK_SUCCESS)
      return;
#else
    VkWin32SurfaceCreateInfoKHR surfaceCreate{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surfaceCreate.hinstance = GetModuleHandleW(nullptr);
    surfaceCreate.hwnd = nativeWindow_;
    if (vkCreateWin32SurfaceKHR(instance_, &surfaceCreate, nullptr, &surface_) != VK_SUCCESS)
      return;
#endif
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    for (auto device : devices) {
      std::uint32_t queueCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
      std::vector<VkQueueFamilyProperties> queues(queueCount);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queues.data());
      for (std::uint32_t index = 0; index < queueCount; ++index) {
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &present);
        if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
          physical_ = device;
          queueFamily_ = index;
          break;
        }
      }
      if (physical_)
        break;
    }
    if (!physical_)
      return;
    constexpr float priority = 1.0F;
    VkDeviceQueueCreateInfo queueCreate{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreate.queueFamilyIndex = queueFamily_;
    queueCreate.queueCount = 1;
    queueCreate.pQueuePriorities = &priority;
    const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreate{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreate.queueCreateInfoCount = 1;
    deviceCreate.pQueueCreateInfos = &queueCreate;
    deviceCreate.enabledExtensionCount = 1;
    deviceCreate.ppEnabledExtensionNames = deviceExtensions;
    if (vkCreateDevice(physical_, &deviceCreate, nullptr, &device_) != VK_SUCCESS)
      return;
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
    VkCommandPoolCreateInfo poolCreate{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreate.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreate.queueFamilyIndex = queueFamily_;
    if (vkCreateCommandPool(device_, &poolCreate, nullptr, &commandPool_) != VK_SUCCESS)
      return;
    valid_ = Recreate();
  }
  ~VulkanSurface() override { static_cast<void>(DrainAndDestroy()); }
  std::thread::id RenderThread() const noexcept override { return thread_; }
  SurfaceStatus NotifyWindowExtent(std::uint32_t width, std::uint32_t height) noexcept override {
    pendingWidth_.store(width);
    pendingHeight_.store(height);
    dirty_.store(true);
    return width && height ? SurfaceStatus::Ready : SurfaceStatus::ZeroExtent;
  }
  SurfaceStatus Acquire() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (destroyed_)
      return SurfaceStatus::SurfaceLost;
    if (!valid_)
      return SurfaceStatus::Unsupported;
    if (dirty_.exchange(false)) {
      width_ = pendingWidth_.load();
      height_ = pendingHeight_.load();
      if (!width_ || !height_)
        return SurfaceStatus::ZeroExtent;
      vkDeviceWaitIdle(device_);
      DestroySwapchain();
      if (!Recreate())
        return SurfaceStatus::OutOfDate;
      ++diagnostics_.resizeGenerations;
      ++diagnostics_.surfaceRecoveries;
    }
    if (!width_ || !height_)
      return SurfaceStatus::ZeroExtent;
    frame_ = (frame_ + 1) % frames_.size();
    auto &frame = frames_[frame_];
    if (vkWaitForFences(device_, 1, &frame.fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    ++diagnostics_.fenceWaits;
    vkResetFences(device_, 1, &frame.fence);
    const auto result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.available,
                                              VK_NULL_HANDLE, &imageIndex_);
    diagnostics_.lastPlatformResult = result;
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
      return SurfaceStatus::OutOfDate;
    if (result == VK_ERROR_SURFACE_LOST_KHR)
      return SurfaceStatus::SurfaceLost;
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
      return SurfaceStatus::DeviceLost;
    vkResetCommandBuffer(frame.commands, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(frame.commands, &begin);
    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = images_[imageIndex_];
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
    const VkClearColorValue color{{0.04F, 0.08F, 0.16F, 1.0F}};
    vkCmdClearColorImage(frame.commands, images_[imageIndex_], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &color, 1, &toTransfer.subresourceRange);
    VkImageMemoryBarrier toPresent = toTransfer;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask = 0;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toPresent);
    vkEndCommandBuffer(frame.commands);
    acquired_ = true;
    ++diagnostics_.acquiredFrames;
    if (result == VK_SUBOPTIMAL_KHR)
      dirty_.store(true);
    return SurfaceStatus::Ready;
  }
  SurfaceStatus Present() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_)
      return SurfaceStatus::OutOfDate;
    auto &frame = frames_[frame_];
    constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.available;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &frame.finished;
    if (vkQueueSubmit(queue_, 1, &submit, frame.fence) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &frame.finished;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &imageIndex_;
    const auto result = vkQueuePresentKHR(queue_, &present);
    diagnostics_.lastPlatformResult = result;
    acquired_ = false;
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
      return SurfaceStatus::OutOfDate;
    if (result == VK_ERROR_SURFACE_LOST_KHR)
      return SurfaceStatus::SurfaceLost;
    if (result != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    ++diagnostics_.presentedFrames;
    return SurfaceStatus::Ready;
  }
  SurfaceDiagnostics Diagnostics() const noexcept override { return diagnostics_; }
  SurfaceStatus DrainAndDestroy() override {
    if (destroyed_)
      return SurfaceStatus::Ready;
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (device_)
      vkDeviceWaitIdle(device_);
    DestroySwapchain();
    if (commandPool_)
      vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (device_)
      vkDestroyDevice(device_, nullptr);
    if (surface_)
      vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_)
      vkDestroyInstance(instance_, nullptr);
#if defined(__linux__)
    if (display_)
      XCloseDisplay(display_);
#endif
    destroyed_ = true;
    return SurfaceStatus::Ready;
  }

private:
  struct Frame {
    VkCommandBuffer commands{};
    VkSemaphore available{};
    VkSemaphore finished{};
    VkFence fence{};
  };
  static constexpr std::size_t kMaxFrames = 3;
  bool OnThread() const noexcept { return thread_ == std::this_thread::get_id(); }
  void DestroySwapchain() {
    for (auto &frame : frames_) {
      if (frame.fence)
        vkDestroyFence(device_, frame.fence, nullptr);
      if (frame.available)
        vkDestroySemaphore(device_, frame.available, nullptr);
      if (frame.finished)
        vkDestroySemaphore(device_, frame.finished, nullptr);
    }
    frames_.clear();
    images_.clear();
    if (swapchain_)
      vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
  bool Recreate() {
    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &capabilities) != VK_SUCCESS)
      return false;
    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &formatCount, formats.data());
    if (formats.empty())
      return false;
    auto selected = formats.front();
    if (requestedColorSpace_ == ColorSpace::Hdr10) {
      const auto hdr = std::find_if(formats.begin(), formats.end(), [](const auto &format) {
        return format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
      });
      if (hdr != formats.end()) {
        selected = *hdr;
        diagnostics_.negotiatedColorSpace = ColorSpace::Hdr10;
      }
    }
    std::uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &modeCount, modes.data());
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (requestedPresentMode_ == PresentMode::Immediate &&
        std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
      mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
      diagnostics_.negotiatedPresentMode = PresentMode::Immediate;
    }
    VkExtent2D extent{width_, height_};
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
      extent = capabilities.currentExtent;
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = surface_;
    create.minImageCount = std::clamp(2U, capabilities.minImageCount,
                                      capabilities.maxImageCount ? capabilities.maxImageCount : 3U);
    create.imageFormat = selected.format;
    create.imageColorSpace = selected.colorSpace;
    create.imageExtent = extent;
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = capabilities.currentTransform;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = mode;
    create.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device_, &create, nullptr, &swapchain_) != VK_SUCCESS)
      return false;
    std::uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    images_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images_.data());
    frames_.resize(std::min<std::size_t>(kMaxFrames, images_.size()));
    std::vector<VkCommandBuffer> commands(frames_.size());
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = commandPool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = static_cast<std::uint32_t>(commands.size());
    if (vkAllocateCommandBuffers(device_, &allocate, commands.data()) != VK_SUCCESS)
      return false;
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::size_t index = 0; index < frames_.size(); ++index) {
      frames_[index].commands = commands[index];
      if (vkCreateSemaphore(device_, &semaphore, nullptr, &frames_[index].available) !=
              VK_SUCCESS ||
          vkCreateSemaphore(device_, &semaphore, nullptr, &frames_[index].finished) != VK_SUCCESS ||
          vkCreateFence(device_, &fence, nullptr, &frames_[index].fence) != VK_SUCCESS)
        return false;
    }
    return true;
  }
  std::thread::id thread_;
  std::uint32_t width_{}, height_{}, queueFamily_{}, imageIndex_{}, frame_{};
  ColorSpace requestedColorSpace_{};
  PresentMode requestedPresentMode_{};
  bool valid_{}, destroyed_{}, acquired_{};
  std::atomic<std::uint32_t> pendingWidth_{}, pendingHeight_{};
  std::atomic_bool dirty_{};
#if defined(__linux__)
  Display *display_{};
  ::Window nativeWindow_{};
#else
  HWND nativeWindow_{};
#endif
  VkInstance instance_{};
  VkPhysicalDevice physical_{};
  VkDevice device_{};
  VkQueue queue_{};
  VkSurfaceKHR surface_{};
  VkSwapchainKHR swapchain_{};
  VkCommandPool commandPool_{};
  std::vector<VkImage> images_;
  std::vector<Frame> frames_;
  SurfaceDiagnostics diagnostics_{};
};
} // namespace

std::unique_ptr<ISurface> CreateVulkanSurface(const SurfaceDescriptor &descriptor,
                                              Window::IWindowSystem &windows) {
  auto surface = std::make_unique<VulkanSurface>(descriptor, windows);
  return surface;
}
} // namespace Nexora::Presentation
