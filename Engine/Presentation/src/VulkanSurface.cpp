#if !defined(__linux__) && !defined(_WIN32)
#error "VulkanSurface.cpp requires a desktop Vulkan host"
#endif

#include "Nexora/Presentation/Surface.h"

#include "SceneVulkanShaders.h"
#include "UiVulkanShaders.h"
#include <vulkan/vulkan.h>
#define NEXORA_HAS_NATIVE_UI_SHADERS 1
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
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
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
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "NexoraPresentation";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &app;
    create.enabledExtensionCount = 2;
    create.ppEnabledExtensionNames = extensions;
    if (vkCreateInstance(&create, nullptr, &instance_) != VK_SUCCESS)
      return;
#if defined(__linux__)
    VkXlibSurfaceCreateInfoKHR surfaceCreate{};
    surfaceCreate.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceCreate.dpy = display_;
    surfaceCreate.window = nativeWindow_;
    if (vkCreateXlibSurfaceKHR(instance_, &surfaceCreate, nullptr, &surface_) != VK_SUCCESS)
      return;
#else
    VkWin32SurfaceCreateInfoKHR surfaceCreate{};
    surfaceCreate.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
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
    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(physical_, &deviceProperties);
    diagnostics_.softwareRasterizer = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    constexpr float priority = 1.0F;
    VkDeviceQueueCreateInfo queueCreate{};
    queueCreate.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreate.queueFamilyIndex = queueFamily_;
    queueCreate.queueCount = 1;
    queueCreate.pQueuePriorities = &priority;
    const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreate{};
    deviceCreate.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreate.queueCreateInfoCount = 1;
    deviceCreate.pQueueCreateInfos = &queueCreate;
    deviceCreate.enabledExtensionCount = 1;
    deviceCreate.ppEnabledExtensionNames = deviceExtensions;
    if (vkCreateDevice(physical_, &deviceCreate, nullptr, &device_) != VK_SUCCESS)
      return;
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
    VkCommandPoolCreateInfo poolCreate{};
    poolCreate.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
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
    if (acquired_)
      return SurfaceStatus::InvalidDescriptor;
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
    DestroyFrameRetirements(frame);
    ++diagnostics_.fenceWaits;
    const auto result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.available,
                                              VK_NULL_HANDLE, &imageIndex_);
    diagnostics_.lastPlatformResult = result;
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
      return SurfaceStatus::OutOfDate;
    if (result == VK_ERROR_SURFACE_LOST_KHR)
      return SurfaceStatus::SurfaceLost;
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
      return SurfaceStatus::DeviceLost;
    // Only reset the fence once we are committed to resubmitting on it below --
    // resetting it earlier and then bailing out on an acquire failure would leave
    // it permanently unsignaled, so the next Acquire() cycling back to this frame
    // slot would block forever in the vkWaitForFences call above.
    vkResetFences(device_, 1, &frame.fence);
    vkResetCommandBuffer(frame.commands, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(frame.commands, &begin);
    VkImageMemoryBarrier toRender{};
    toRender.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toRender.srcAccessMask = 0;
    toRender.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toRender.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toRender.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toRender.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRender.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRender.image = images_[imageIndex_];
    toRender.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &toRender);
    transferTarget_ = false;
    sceneRendered_ = false;
    sceneOffscreen_ = sceneComposited_ = false;
    acquired_ = true;
    ++diagnostics_.acquiredFrames;
    if (result == VK_SUBOPTIMAL_KHR)
      dirty_.store(true);
    return SurfaceStatus::Ready;
  }
  SurfaceStatus CompositeRgba8(std::span<const std::byte> pixels, std::uint32_t width,
                               std::uint32_t height) override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || sceneRendered_ || transferTarget_ || frames_[frame_].uiFramebuffer ||
        width != width_ || height != height_ ||
        pixels.size() != static_cast<std::size_t>(width) * height * 4U)
      return SurfaceStatus::InvalidDescriptor;
    auto &frame = frames_[frame_];
    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = images_[imageIndex_];
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
    transferTarget_ = true;
    if (frame.upload)
      vkDestroyBuffer(device_, frame.upload, nullptr);
    if (frame.uploadMemory)
      vkFreeMemory(device_, frame.uploadMemory, nullptr);
    frame.upload = VK_NULL_HANDLE;
    frame.uploadMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferCreate{};
    bufferCreate.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreate.size = pixels.size();
    bufferCreate.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreate.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferCreate, nullptr, &frame.upload) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, frame.upload, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_, &properties);
    std::uint32_t memoryType = properties.memoryTypeCount;
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
      const auto flags = properties.memoryTypes[index].propertyFlags;
      if ((requirements.memoryTypeBits & (1U << index)) &&
          (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
              (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        memoryType = index;
        break;
      }
    }
    if (memoryType == properties.memoryTypeCount)
      return SurfaceStatus::Unsupported;
    VkMemoryAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = memoryType;
    if (vkAllocateMemory(device_, &allocate, nullptr, &frame.uploadMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, frame.upload, frame.uploadMemory, 0) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    std::vector<std::byte> converted;
    if (swapchainFormat_ == VK_FORMAT_B8G8R8A8_UNORM ||
        swapchainFormat_ == VK_FORMAT_B8G8R8A8_SRGB) {
      converted.assign(pixels.begin(), pixels.end());
      for (std::size_t offset = 0; offset < converted.size(); offset += 4)
        std::swap(converted[offset], converted[offset + 2]);
      pixels = converted;
    }
    void *mapped = nullptr;
    if (vkMapMemory(device_, frame.uploadMemory, 0, pixels.size(), 0, &mapped) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    std::memcpy(mapped, pixels.data(), pixels.size());
    vkUnmapMemory(device_, frame.uploadMemory);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(frame.commands, frame.upload, images_[imageIndex_],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    return SurfaceStatus::Ready;
  }
  SurfaceStatus DrawScene(const SceneDrawData &data) override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || !scenePipeline_ || sceneRendered_ || transferTarget_ ||
        frames_[frame_].uiFramebuffer || data.vertices.empty() || data.indices.empty() ||
        data.vertices.size() > 65535 || data.indices.size() > 1048576 ||
        data.indices.size() % 3 != 0)
      return SurfaceStatus::InvalidDescriptor;
    for (const auto index : data.indices)
      if (index >= data.vertices.size())
        return SurfaceStatus::InvalidDescriptor;
    for (const auto &vertex : data.vertices) {
      for (const auto value : vertex.position)
        if (!std::isfinite(value))
          return SurfaceStatus::InvalidDescriptor;
      for (const auto value : vertex.normal)
        if (!std::isfinite(value))
          return SurfaceStatus::InvalidDescriptor;
    }
    for (const auto value : data.model_view_projection)
      if (!std::isfinite(value))
        return SurfaceStatus::InvalidDescriptor;
    for (const auto value : data.light_direction)
      if (!std::isfinite(value))
        return SurfaceStatus::InvalidDescriptor;
    for (const auto value : data.light_color)
      if (!std::isfinite(value))
        return SurfaceStatus::InvalidDescriptor;
    for (const auto value : data.base_color)
      if (!std::isfinite(value))
        return SurfaceStatus::InvalidDescriptor;
    if (data.light_direction[0] == 0 && data.light_direction[1] == 0 &&
        data.light_direction[2] == 0)
      return SurfaceStatus::InvalidDescriptor;
    if (data.instances.size() > 4096)
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &instance : data.instances) {
      for (const auto value : instance.translation)
        if (!std::isfinite(value))
          return SurfaceStatus::InvalidDescriptor;
      for (const auto value : instance.scale)
        if (!std::isfinite(value) || std::abs(value) < 0.00001F)
          return SurfaceStatus::InvalidDescriptor;
      for (const auto value : instance.color)
        if (!std::isfinite(value))
          return SurfaceStatus::InvalidDescriptor;
    }
    if (data.textureUploads.size() > 16)
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &upload : data.textureUploads)
      if (upload.textureId == 0 || upload.textureId == UINT64_MAX || upload.width == 0 ||
          upload.height == 0 || upload.width > 1024 || upload.height > 1024 ||
          upload.rowPitch != upload.width * 4U ||
          upload.pixels.size() != static_cast<std::size_t>(upload.rowPitch) * upload.height)
        return SurfaceStatus::InvalidDescriptor;
    if (data.textureId == UINT64_MAX)
      return SurfaceStatus::InvalidDescriptor;
    const auto textureId = data.textureId ? data.textureId : UINT64_MAX;
    if (data.textureId && !sceneTextures_.contains(textureId) &&
        std::none_of(data.textureUploads.begin(), data.textureUploads.end(),
                     [textureId](const auto &upload) { return upload.textureId == textureId; }))
      return SurfaceStatus::InvalidDescriptor;
    for (const auto &vertex : data.vertices)
      for (const auto value : vertex.uv)
        if (!std::isfinite(value))
          return SurfaceStatus::InvalidDescriptor;
    const SceneInstance identity{};
    const auto instances =
        data.instances.empty() ? std::span<const SceneInstance>(&identity, 1) : data.instances;
    const auto instanceBytes = std::as_bytes(instances);
    auto &frame = frames_[frame_];
    std::size_t additional = textureId == UINT64_MAX && !sceneTextures_.contains(textureId) ? 1 : 0;
    for (std::size_t i = 0; i < data.textureUploads.size(); ++i) {
      const auto id = data.textureUploads[i].textureId;
      for (std::size_t j = 0; j < i; ++j)
        if (data.textureUploads[j].textureId == id)
          return SurfaceStatus::InvalidDescriptor;
      additional += sceneTextures_.contains(id) ? 0 : 1;
    }
    if (sceneTextures_.size() + additional > 64)
      return SurfaceStatus::Unsupported;
    for (const auto &upload : data.textureUploads)
      if (!sceneTextures_.contains(upload.textureId) && !UploadUiTexture(frame, upload, true))
        return SurfaceStatus::DeviceLost;
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    if (!sceneTextures_.contains(textureId) &&
        !UploadUiTexture(frame, {UINT64_MAX, 1, 1, 4, white}, true))
      return SurfaceStatus::DeviceLost;
    // Acquire has waited this frame's fence. Never release another in-flight slot's resources.
    DestroySceneFrame(frame);
    const auto vertexBytes = std::as_bytes(data.vertices);
    const auto indexBytes = std::as_bytes(data.indices);
    const auto instanceOffset = (vertexBytes.size() + indexBytes.size() + 3) & ~std::size_t{3};
    const auto bytes = instanceOffset + instanceBytes.size();
    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = bytes;
    buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (vkCreateBuffer(device_, &buffer, nullptr, &frame.sceneUpload) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, frame.sceneUpload, &requirements);
    const auto memory =
        FindMemoryType(requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory == UINT32_MAX)
      return SurfaceStatus::Unsupported;
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                  requirements.size, memory};
    if (vkAllocateMemory(device_, &allocate, nullptr, &frame.sceneMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, frame.sceneUpload, frame.sceneMemory, 0) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    void *mapped{};
    if (vkMapMemory(device_, frame.sceneMemory, 0, bytes, 0, &mapped) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    std::memcpy(mapped, vertexBytes.data(), vertexBytes.size());
    std::memcpy(static_cast<std::byte *>(mapped) + vertexBytes.size(), indexBytes.data(),
                indexBytes.size());
    std::memcpy(static_cast<std::byte *>(mapped) + instanceOffset, instanceBytes.data(),
                instanceBytes.size());
    vkUnmapMemory(device_, frame.sceneMemory);
    VkImageCreateInfo image{};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = VK_FORMAT_D32_SFLOAT;
    image.extent = {width_, height_, 1};
    image.mipLevels = image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (vkCreateImage(device_, &image, nullptr, &frame.sceneDepth) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    vkGetImageMemoryRequirements(device_, frame.sceneDepth, &requirements);
    const auto depthMemory =
        FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (depthMemory == UINT32_MAX)
      return SurfaceStatus::Unsupported;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = depthMemory;
    if (vkAllocateMemory(device_, &allocate, nullptr, &frame.sceneDepthMemory) != VK_SUCCESS ||
        vkBindImageMemory(device_, frame.sceneDepth, frame.sceneDepthMemory, 0) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = frame.sceneDepth;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_D32_SFLOAT;
    view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device_, &view, nullptr, &frame.sceneDepthView) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    if (data.offscreen) {
      image.format = swapchainFormat_;
      image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      if (vkCreateImage(device_, &image, nullptr, &frame.sceneColor) != VK_SUCCESS)
        return SurfaceStatus::DeviceLost;
      vkGetImageMemoryRequirements(device_, frame.sceneColor, &requirements);
      allocate.allocationSize = requirements.size;
      allocate.memoryTypeIndex =
          FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      if (allocate.memoryTypeIndex == UINT32_MAX)
        return SurfaceStatus::Unsupported;
      if (vkAllocateMemory(device_, &allocate, nullptr, &frame.sceneColorMemory) != VK_SUCCESS ||
          vkBindImageMemory(device_, frame.sceneColor, frame.sceneColorMemory, 0) != VK_SUCCESS)
        return SurfaceStatus::DeviceLost;
      view.image = frame.sceneColor;
      view.format = swapchainFormat_;
      view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      if (vkCreateImageView(device_, &view, nullptr, &frame.sceneColorView) != VK_SUCCESS)
        return SurfaceStatus::DeviceLost;
      VkImageMemoryBarrier color{};
      color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      color.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      color.srcQueueFamilyIndex = color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      color.image = frame.sceneColor;
      color.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                           1, &color);
    }
    const VkImageView attachments[] = {
        data.offscreen ? frame.sceneColorView : imageViews_[imageIndex_], frame.sceneDepthView};
    VkFramebufferCreateInfo framebuffer{};
    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass = sceneRenderPass_;
    framebuffer.attachmentCount = 2;
    framebuffer.pAttachments = attachments;
    framebuffer.width = width_;
    framebuffer.height = height_;
    framebuffer.layers = 1;
    if (vkCreateFramebuffer(device_, &framebuffer, nullptr, &frame.sceneFramebuffer) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{0.025F, 0.045F, 0.09F, 1.0F}};
    clears[1].depthStencil = {1.0F, 0};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = sceneRenderPass_;
    begin.framebuffer = frame.sceneFramebuffer;
    begin.renderArea.extent = {width_, height_};
    begin.clearValueCount = 2;
    begin.pClearValues = clears.data();
    vkCmdBeginRenderPass(frame.commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline_);
    const VkViewport viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_), 0, 1};
    const VkRect2D scissor{{0, 0}, {width_, height_}};
    vkCmdSetViewport(frame.commands, 0, 1, &viewport);
    vkCmdSetScissor(frame.commands, 0, 1, &scissor);
    const VkBuffer buffers[]{frame.sceneUpload, frame.sceneUpload};
    const VkDeviceSize offsets[]{0, instanceOffset};
    vkCmdBindVertexBuffers(frame.commands, 0, 2, buffers, offsets);
    vkCmdBindIndexBuffer(frame.commands, frame.sceneUpload, vertexBytes.size(),
                         VK_INDEX_TYPE_UINT16);
    std::array<float, 28> constants{};
    std::copy(std::begin(data.model_view_projection), std::end(data.model_view_projection),
              constants.begin());
    std::copy(std::begin(data.light_direction), std::end(data.light_direction),
              constants.begin() + 16);
    std::copy(std::begin(data.light_color), std::end(data.light_color), constants.begin() + 20);
    std::copy(std::begin(data.base_color), std::end(data.base_color), constants.begin() + 24);
    vkCmdPushConstants(frame.commands, scenePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(constants), constants.data());
    const auto descriptor = sceneTextures_.at(textureId).descriptor;
    vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipelineLayout_,
                            0, 1, &descriptor, 0, nullptr);
    vkCmdDrawIndexed(frame.commands, static_cast<std::uint32_t>(data.indices.size()),
                     static_cast<std::uint32_t>(instances.size()), 0, 0, 0);
    vkCmdEndRenderPass(frame.commands);
    sceneRendered_ = true;
    sceneOffscreen_ = data.offscreen;
    diagnostics_.sceneOffscreenDrawCalls += data.offscreen ? 1 : 0;
    ++diagnostics_.sceneDrawCalls;
    diagnostics_.sceneInstances += instances.size();
    return SurfaceStatus::Ready;
  }
  SurfaceStatus CompositeScene() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_ || !sceneRendered_ || !sceneOffscreen_ || sceneComposited_ || transferTarget_ ||
        frames_[frame_].uiFramebuffer)
      return SurfaceStatus::InvalidDescriptor;
    auto &frame = frames_[frame_];
    VkImageMemoryBarrier barriers[2]{};
    for (auto &barrier : barriers) {
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    barriers[0].image = frame.sceneColor;
    barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].image = images_[imageIndex_];
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);
    VkImageCopy copy{};
    copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.extent = {width_, height_, 1};
    vkCmdCopyImage(frame.commands, frame.sceneColor, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   images_[imageIndex_], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &barriers[1]);
    sceneComposited_ = true;
    ++diagnostics_.sceneComposites;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus RenderUi(const UiDrawData &drawData) override {
#if !defined(NEXORA_HAS_NATIVE_UI_SHADERS)
    (void)drawData;
    return SurfaceStatus::Unsupported;
#else
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if ((sceneOffscreen_ && !sceneComposited_) || !acquired_ || transferTarget_ ||
        frames_[frame_].uiFramebuffer || drawData.vertices.empty() || drawData.indices.empty())
      return SurfaceStatus::InvalidDescriptor;
    auto &frame = frames_[frame_];
    for (const auto &upload : drawData.textureUploads)
      if (!UploadUiTexture(frame, upload))
        return SurfaceStatus::DeviceLost;
    const auto vertexBytes = std::as_bytes(drawData.vertices);
    const auto required = vertexBytes.size() + drawData.indices.size();
    if (!EnsureUiUpload(frame, required))
      return SurfaceStatus::DeviceLost;
    void *mapped = nullptr;
    if (vkMapMemory(device_, frame.uiMemory, 0, required, 0, &mapped) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    std::memcpy(mapped, vertexBytes.data(), vertexBytes.size());
    std::memcpy(static_cast<std::byte *>(mapped) + vertexBytes.size(), drawData.indices.data(),
                drawData.indices.size());
    vkUnmapMemory(device_, frame.uiMemory);

    VkImageView attachment = imageViews_[imageIndex_];
    VkFramebufferCreateInfo framebufferCreate{};
    framebufferCreate.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreate.renderPass = uiRenderPass_;
    framebufferCreate.attachmentCount = 1;
    framebufferCreate.pAttachments = &attachment;
    framebufferCreate.width = width_;
    framebufferCreate.height = height_;
    framebufferCreate.layers = 1;
    if (vkCreateFramebuffer(device_, &framebufferCreate, nullptr, &frame.uiFramebuffer) !=
        VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    const VkClearValue clear{{{0.04F, 0.08F, 0.16F, 1.0F}}};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = uiRenderPass_;
    begin.framebuffer = frame.uiFramebuffer;
    begin.renderArea.extent = {width_, height_};
    begin.clearValueCount = 1;
    begin.pClearValues = &clear;
    vkCmdBeginRenderPass(frame.commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    if (!sceneRendered_) {
      const VkClearAttachment clearAttachment{VK_IMAGE_ASPECT_COLOR_BIT, 0, clear};
      const VkClearRect rect{{{0, 0}, {width_, height_}}, 0, 1};
      vkCmdClearAttachments(frame.commands, 1, &clearAttachment, 1, &rect);
    }
    vkCmdBindPipeline(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline_);
    const VkViewport viewport{0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_),
                              0.0F, 1.0F};
    vkCmdSetViewport(frame.commands, 0, 1, &viewport);
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(frame.commands, 0, 1, &frame.uiUpload, &vertexOffset);
    vkCmdBindIndexBuffer(frame.commands, frame.uiUpload, vertexBytes.size(),
                         drawData.indices32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    const float transform[4] = {2.0F / static_cast<float>(width_),
                                2.0F / static_cast<float>(height_), -1.0F, -1.0F};
    vkCmdPushConstants(frame.commands, uiPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(transform), transform);
    for (const auto &command : drawData.commands) {
      const auto texture = uiTextures_.find(command.textureId);
      if (texture == uiTextures_.end()) {
        ++diagnostics_.nativeUiRejectedTextures;
        continue;
      }
      vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipelineLayout_, 0,
                              1, &texture->second.descriptor, 0, nullptr);
      const VkRect2D scissor{{command.clipX, command.clipY},
                             {command.clipWidth, command.clipHeight}};
      vkCmdSetScissor(frame.commands, 0, 1, &scissor);
      vkCmdDrawIndexed(frame.commands, command.elementCount, 1, command.indexOffset,
                       command.vertexOffset, 0);
      ++diagnostics_.nativeUiDrawCalls;
    }
    vkCmdEndRenderPass(frame.commands);
    transferTarget_ = false;
    return SurfaceStatus::Ready;
#endif
  }
  SurfaceStatus Present() override {
    if (!OnThread())
      return SurfaceStatus::WrongThread;
    if (!acquired_)
      return SurfaceStatus::OutOfDate;
    if (sceneOffscreen_ && !sceneComposited_)
      return SurfaceStatus::InvalidDescriptor;
    auto &frame = frames_[frame_];
    VkImageMemoryBarrier toPresent{};
    toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toPresent.srcAccessMask =
        transferTarget_ ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toPresent.oldLayout = transferTarget_ ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                          : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = images_[imageIndex_];
    toPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (sceneComposited_)
      toPresent.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto sourceStage = transferTarget_
                                 ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                 : (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                    (sceneComposited_ ? VK_PIPELINE_STAGE_TRANSFER_BIT : 0));
    vkCmdPipelineBarrier(frame.commands, sourceStage, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &toPresent);
    vkEndCommandBuffer(frame.commands);
    constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.available;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &frame.finished;
    if (vkQueueSubmit(queue_, 1, &submit, frame.fence) != VK_SUCCESS)
      return SurfaceStatus::DeviceLost;
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
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
    acquired_ = false;
    valid_ = false;
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
    VkBuffer upload{};
    VkDeviceMemory uploadMemory{};
    VkBuffer uiUpload{};
    VkDeviceMemory uiMemory{};
    VkDeviceSize uiCapacity{};
    VkFramebuffer uiFramebuffer{};
    VkBuffer sceneUpload{};
    VkDeviceMemory sceneMemory{};
    VkImage sceneDepth{};
    VkDeviceMemory sceneDepthMemory{};
    VkImageView sceneDepthView{};
    VkFramebuffer sceneFramebuffer{};
    VkImage sceneColor{};
    VkDeviceMemory sceneColorMemory{};
    VkImageView sceneColorView{};
    std::vector<VkBuffer> retiredBuffers;
    std::vector<VkDeviceMemory> retiredBufferMemory;
    std::vector<VkImage> retiredImages;
    std::vector<VkImageView> retiredImageViews;
    std::vector<VkDeviceMemory> retiredImageMemory;
    std::vector<VkDescriptorSet> retiredDescriptors;
  };
  struct UiTexture final {
    VkImage image{};
    VkImageView view{};
    VkDeviceMemory memory{};
    VkDescriptorSet descriptor{};
  };
  static constexpr std::size_t kMaxFrames = 3;
  bool OnThread() const noexcept { return thread_ == std::this_thread::get_id(); }
  std::uint32_t FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags required) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_, &properties);
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index)
      if ((bits & (1U << index)) &&
          (properties.memoryTypes[index].propertyFlags & required) == required)
        return index;
    return std::numeric_limits<std::uint32_t>::max();
  }
  void DestroySceneFrame(Frame &frame) {
    if (frame.sceneFramebuffer)
      vkDestroyFramebuffer(device_, frame.sceneFramebuffer, nullptr);
    if (frame.sceneColorView)
      vkDestroyImageView(device_, frame.sceneColorView, nullptr);
    if (frame.sceneColor)
      vkDestroyImage(device_, frame.sceneColor, nullptr);
    if (frame.sceneColorMemory)
      vkFreeMemory(device_, frame.sceneColorMemory, nullptr);
    frame.sceneColorView = VK_NULL_HANDLE;
    frame.sceneColor = VK_NULL_HANDLE;
    frame.sceneColorMemory = VK_NULL_HANDLE;
    if (frame.sceneDepthView)
      vkDestroyImageView(device_, frame.sceneDepthView, nullptr);
    if (frame.sceneDepth)
      vkDestroyImage(device_, frame.sceneDepth, nullptr);
    if (frame.sceneDepthMemory)
      vkFreeMemory(device_, frame.sceneDepthMemory, nullptr);
    if (frame.sceneUpload)
      vkDestroyBuffer(device_, frame.sceneUpload, nullptr);
    if (frame.sceneMemory)
      vkFreeMemory(device_, frame.sceneMemory, nullptr);
    frame.sceneFramebuffer = VK_NULL_HANDLE;
    frame.sceneDepthView = VK_NULL_HANDLE;
    frame.sceneDepth = VK_NULL_HANDLE;
    frame.sceneDepthMemory = VK_NULL_HANDLE;
    frame.sceneUpload = VK_NULL_HANDLE;
    frame.sceneMemory = VK_NULL_HANDLE;
  }
  void DestroyFrameRetirements(Frame &frame) {
    if (frame.uiFramebuffer)
      vkDestroyFramebuffer(device_, frame.uiFramebuffer, nullptr);
    frame.uiFramebuffer = VK_NULL_HANDLE;
    for (const auto view : frame.retiredImageViews)
      vkDestroyImageView(device_, view, nullptr);
    for (const auto image : frame.retiredImages)
      vkDestroyImage(device_, image, nullptr);
    for (const auto memory : frame.retiredImageMemory)
      vkFreeMemory(device_, memory, nullptr);
    for (const auto buffer : frame.retiredBuffers)
      vkDestroyBuffer(device_, buffer, nullptr);
    for (const auto memory : frame.retiredBufferMemory)
      vkFreeMemory(device_, memory, nullptr);
    if (!frame.retiredDescriptors.empty())
      vkFreeDescriptorSets(device_, uiDescriptorPool_,
                           static_cast<std::uint32_t>(frame.retiredDescriptors.size()),
                           frame.retiredDescriptors.data());
    frame.retiredImageViews.clear();
    frame.retiredImages.clear();
    frame.retiredImageMemory.clear();
    frame.retiredBuffers.clear();
    frame.retiredBufferMemory.clear();
    frame.retiredDescriptors.clear();
  }
  bool EnsureUiUpload(Frame &frame, VkDeviceSize required) {
    if (frame.uiCapacity >= required)
      return true;
    if (frame.uiUpload)
      vkDestroyBuffer(device_, frame.uiUpload, nullptr);
    if (frame.uiMemory)
      vkFreeMemory(device_, frame.uiMemory, nullptr);
    frame.uiCapacity = 4096;
    while (frame.uiCapacity < required)
      frame.uiCapacity *= 2;
    VkBufferCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = frame.uiCapacity;
    create.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &create, nullptr, &frame.uiUpload) != VK_SUCCESS)
      return false;
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, frame.uiUpload, &requirements);
    const auto type =
        FindMemoryType(requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                  requirements.size, type};
    if (type == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocate, nullptr, &frame.uiMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, frame.uiUpload, frame.uiMemory, 0) != VK_SUCCESS)
      return false;
    ++diagnostics_.nativeUiBufferReallocations;
    return true;
  }
  bool UploadUiTexture(Frame &frame, const UiTextureUpload &upload, bool scene = false) {
    auto &textures = scene ? sceneTextures_ : uiTextures_;
    if (upload.textureId == 0 || upload.width == 0 || upload.height == 0 ||
        upload.rowPitch != upload.width * 4U ||
        upload.pixels.size() != static_cast<std::size_t>(upload.rowPitch) * upload.height)
      return false;
    UiTexture next;
    VkImageCreateInfo imageCreate{};
    imageCreate.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreate.imageType = VK_IMAGE_TYPE_2D;
    imageCreate.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreate.extent = {upload.width, upload.height, 1};
    imageCreate.mipLevels = 1;
    imageCreate.arrayLayers = 1;
    imageCreate.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreate.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreate.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (vkCreateImage(device_, &imageCreate, nullptr, &next.image) != VK_SUCCESS)
      return false;
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, next.image, &requirements);
    const auto type =
        FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                  requirements.size, type};
    if (type == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocate, nullptr, &next.memory) != VK_SUCCESS ||
        vkBindImageMemory(device_, next.image, next.memory, 0) != VK_SUCCESS)
      return false;
    VkImageViewCreateInfo viewCreate{};
    viewCreate.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreate.image = next.image;
    viewCreate.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreate.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCreate.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device_, &viewCreate, nullptr, &next.view) != VK_SUCCESS)
      return false;
    VkBuffer staging{};
    VkDeviceMemory stagingMemory{};
    VkBufferCreateInfo bufferCreate{};
    bufferCreate.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreate.size = upload.pixels.size();
    bufferCreate.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreate.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferCreate, nullptr, &staging) != VK_SUCCESS)
      return false;
    vkGetBufferMemoryRequirements(device_, staging, &requirements);
    const auto stagingType =
        FindMemoryType(requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    allocate = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size, stagingType};
    if (stagingType == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocate, nullptr, &stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, staging, stagingMemory, 0) != VK_SUCCESS)
      return false;
    void *mapped = nullptr;
    if (vkMapMemory(device_, stagingMemory, 0, upload.pixels.size(), 0, &mapped) != VK_SUCCESS)
      return false;
    std::memcpy(mapped, upload.pixels.data(), upload.pixels.size());
    vkUnmapMemory(device_, stagingMemory);
    VkImageMemoryBarrier toCopy{};
    toCopy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toCopy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toCopy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toCopy.image = next.image;
    toCopy.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toCopy);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {upload.width, upload.height, 1};
    vkCmdCopyBufferToImage(frame.commands, staging, next.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    VkImageMemoryBarrier toRead = toCopy;
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toRead);
    frame.retiredBuffers.push_back(staging);
    frame.retiredBufferMemory.push_back(stagingMemory);
    const auto previous = textures.find(upload.textureId);
    if (previous != textures.end()) {
      frame.retiredImages.push_back(previous->second.image);
      frame.retiredImageViews.push_back(previous->second.view);
      frame.retiredImageMemory.push_back(previous->second.memory);
      frame.retiredDescriptors.push_back(previous->second.descriptor);
    }
    VkDescriptorSetAllocateInfo descriptorAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   nullptr, uiDescriptorPool_, 1,
                                                   &uiDescriptorLayout_};
    if (vkAllocateDescriptorSets(device_, &descriptorAllocate, &next.descriptor) != VK_SUCCESS)
      return false;
    const VkDescriptorImageInfo imageInfo{uiSampler_, next.view,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = next.descriptor;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    textures[upload.textureId] = next;
    if (scene)
      ++diagnostics_.sceneTextureUploads;
    else
      ++diagnostics_.nativeUiTextureUploads;
    return true;
  }
  void DestroyUpload(Frame &frame) {
    if (frame.upload)
      vkDestroyBuffer(device_, frame.upload, nullptr);
    if (frame.uploadMemory)
      vkFreeMemory(device_, frame.uploadMemory, nullptr);
    frame.upload = VK_NULL_HANDLE;
    frame.uploadMemory = VK_NULL_HANDLE;
    if (frame.uiUpload)
      vkDestroyBuffer(device_, frame.uiUpload, nullptr);
    if (frame.uiMemory)
      vkFreeMemory(device_, frame.uiMemory, nullptr);
    frame.uiUpload = VK_NULL_HANDLE;
    frame.uiMemory = VK_NULL_HANDLE;
    frame.uiCapacity = 0;
    DestroyFrameRetirements(frame);
  }
  void DestroyUiResources() {
    for (auto &[id, texture] : uiTextures_) {
      (void)id;
      if (texture.view)
        vkDestroyImageView(device_, texture.view, nullptr);
      if (texture.image)
        vkDestroyImage(device_, texture.image, nullptr);
      if (texture.memory)
        vkFreeMemory(device_, texture.memory, nullptr);
    }
    uiTextures_.clear();
    for (auto &[id, texture] : sceneTextures_) {
      (void)id;
      if (texture.view)
        vkDestroyImageView(device_, texture.view, nullptr);
      if (texture.image)
        vkDestroyImage(device_, texture.image, nullptr);
      if (texture.memory)
        vkFreeMemory(device_, texture.memory, nullptr);
    }
    sceneTextures_.clear();
    if (uiPipeline_)
      vkDestroyPipeline(device_, uiPipeline_, nullptr);
    if (uiPipelineLayout_)
      vkDestroyPipelineLayout(device_, uiPipelineLayout_, nullptr);
    if (uiRenderPass_)
      vkDestroyRenderPass(device_, uiRenderPass_, nullptr);
    if (uiDescriptorPool_)
      vkDestroyDescriptorPool(device_, uiDescriptorPool_, nullptr);
    if (uiSampler_)
      vkDestroySampler(device_, uiSampler_, nullptr);
    if (uiDescriptorLayout_)
      vkDestroyDescriptorSetLayout(device_, uiDescriptorLayout_, nullptr);
    uiPipeline_ = VK_NULL_HANDLE;
    uiPipelineLayout_ = VK_NULL_HANDLE;
    uiRenderPass_ = VK_NULL_HANDLE;
    uiDescriptorPool_ = VK_NULL_HANDLE;
    uiSampler_ = VK_NULL_HANDLE;
    uiDescriptorLayout_ = VK_NULL_HANDLE;
  }
#if defined(NEXORA_HAS_NATIVE_UI_SHADERS)
  bool CreateUiResources() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo descriptorLayout{};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.bindingCount = 1;
    descriptorLayout.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device_, &descriptorLayout, nullptr, &uiDescriptorLayout_) !=
        VK_SUCCESS)
      return false;
    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod = VK_LOD_CLAMP_NONE;
    if (vkCreateSampler(device_, &sampler, nullptr, &uiSampler_) != VK_SUCCESS)
      return false;
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256};
    VkDescriptorPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = 256;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &pool, nullptr, &uiDescriptorPool_) != VK_SUCCESS)
      return false;
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.size = sizeof(float) * 4U;
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &uiDescriptorLayout_;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(device_, &layout, nullptr, &uiPipelineLayout_) != VK_SUCCESS)
      return false;
    VkAttachmentDescription attachment{};
    attachment.format = swapchainFormat_;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color;
    VkRenderPassCreateInfo renderPass{};
    renderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPass.attachmentCount = 1;
    renderPass.pAttachments = &attachment;
    renderPass.subpassCount = 1;
    renderPass.pSubpasses = &subpass;
    if (vkCreateRenderPass(device_, &renderPass, nullptr, &uiRenderPass_) != VK_SUCCESS)
      return false;
    const auto makeShader = [&](const std::uint32_t *code, std::size_t bytes, VkShaderModule &out) {
      const VkShaderModuleCreateInfo create{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                            bytes, code};
      return vkCreateShaderModule(device_, &create, nullptr, &out) == VK_SUCCESS;
    };
    VkShaderModule vertex{}, fragment{};
    if (!makeShader(ui_vert, sizeof(ui_vert), vertex) ||
        !makeShader(ui_frag, sizeof(ui_frag), fragment))
      return false;
    const VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr},
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          nullptr,
          0,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          fragment,
          "main",
          nullptr }};
    const VkVertexInputBindingDescription vertexBinding{0, sizeof(UiVertex),
                                                        VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UiVertex, position)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UiVertex, uv)},
        { 2,
          0,
          VK_FORMAT_R8G8B8A8_UNORM,
          offsetof(UiVertex, color) }};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;
    const VkPipelineInputAssemblyStateCreateInfo assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;
    const VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamics;
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertexInput;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &rasterizer;
    pipeline.pMultisampleState = &multisample;
    pipeline.pColorBlendState = &blending;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = uiPipelineLayout_;
    pipeline.renderPass = uiRenderPass_;
    const auto result =
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &uiPipeline_);
    vkDestroyShaderModule(device_, fragment, nullptr);
    vkDestroyShaderModule(device_, vertex, nullptr);
    return result == VK_SUCCESS;
  }
#endif
  bool CreateSceneResources() {
    const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, 112};
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &uiDescriptorLayout_;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(device_, &layout, nullptr, &scenePipelineLayout_) != VK_SUCCESS)
      return false;
    std::array<VkAttachmentDescription, 2> attachments{};
    attachments[0].format = swapchainFormat_;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthAttachment{1,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color;
    subpass.pDepthStencilAttachment = &depthAttachment;
    const VkSubpassDependency dependency{
        VK_SUBPASS_EXTERNAL,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        0};
    VkRenderPassCreateInfo pass{};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = 2;
    pass.pAttachments = attachments.data();
    pass.subpassCount = 1;
    pass.pSubpasses = &subpass;
    pass.dependencyCount = 1;
    pass.pDependencies = &dependency;
    if (vkCreateRenderPass(device_, &pass, nullptr, &sceneRenderPass_) != VK_SUCCESS)
      return false;
    const auto makeShader = [&](const std::uint32_t *code, std::size_t bytes, VkShaderModule &out) {
      const VkShaderModuleCreateInfo create{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                            bytes, code};
      return vkCreateShaderModule(device_, &create, nullptr, &out) == VK_SUCCESS;
    };
    VkShaderModule vertex{}, fragment{};
    if (!makeShader(scene_vert, sizeof(scene_vert), vertex) ||
        !makeShader(scene_frag, sizeof(scene_frag), fragment))
      return false;
    const VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr}};
    const VkVertexInputBindingDescription vertexBindings[]{
        {0, sizeof(SceneVertex), VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(SceneInstance), VK_VERTEX_INPUT_RATE_INSTANCE}};
    const VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, normal)},
        {2, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneInstance, translation)},
        {3, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneInstance, scale)},
        {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SceneInstance, color)},
        {5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, uv)}};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 2;
    vertexInput.pVertexBindingDescriptions = vertexBindings;
    vertexInput.vertexAttributeDescriptionCount = 6;
    vertexInput.pVertexAttributeDescriptions = attributes;
    const VkPipelineInputAssemblyStateCreateInfo assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_FALSE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;
    const VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamics;
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2;
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertexInput;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &rasterizer;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &blending;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = scenePipelineLayout_;
    pipeline.renderPass = sceneRenderPass_;
    const auto result =
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &scenePipeline_);
    vkDestroyShaderModule(device_, fragment, nullptr);
    vkDestroyShaderModule(device_, vertex, nullptr);
    return result == VK_SUCCESS;
  }
  void DestroySwapchain() {
    for (auto &frame : frames_) {
      DestroySceneFrame(frame);
      DestroyUpload(frame);
      if (frame.commands)
        vkFreeCommandBuffers(device_, commandPool_, 1, &frame.commands);
      if (frame.fence)
        vkDestroyFence(device_, frame.fence, nullptr);
      if (frame.available)
        vkDestroySemaphore(device_, frame.available, nullptr);
      if (frame.finished)
        vkDestroySemaphore(device_, frame.finished, nullptr);
    }
    frames_.clear();
    if (scenePipeline_)
      vkDestroyPipeline(device_, scenePipeline_, nullptr);
    if (scenePipelineLayout_)
      vkDestroyPipelineLayout(device_, scenePipelineLayout_, nullptr);
    if (sceneRenderPass_)
      vkDestroyRenderPass(device_, sceneRenderPass_, nullptr);
    scenePipeline_ = VK_NULL_HANDLE;
    scenePipelineLayout_ = VK_NULL_HANDLE;
    sceneRenderPass_ = VK_NULL_HANDLE;
    DestroyUiResources();
    for (const auto view : imageViews_)
      vkDestroyImageView(device_, view, nullptr);
    imageViews_.clear();
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
    if (const auto rgba = std::find_if(formats.begin(), formats.end(),
                                       [](const auto &format) {
                                         return format.format == VK_FORMAT_R8G8B8A8_UNORM ||
                                                format.format == VK_FORMAT_R8G8B8A8_SRGB;
                                       });
        rgba != formats.end())
      selected = *rgba;
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
    width_ = extent.width;
    height_ = extent.height;
    VkSwapchainCreateInfoKHR create{};
    create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create.surface = surface_;
    create.minImageCount = std::clamp(2U, capabilities.minImageCount,
                                      capabilities.maxImageCount ? capabilities.maxImageCount : 3U);
    create.imageFormat = selected.format;
    create.imageColorSpace = selected.colorSpace;
    create.imageExtent = extent;
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = capabilities.currentTransform;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = mode;
    create.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device_, &create, nullptr, &swapchain_) != VK_SUCCESS)
      return false;
    swapchainFormat_ = selected.format;
    std::uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    images_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images_.data());
    imageViews_.resize(imageCount);
    for (std::size_t index = 0; index < images_.size(); ++index) {
      VkImageViewCreateInfo view{};
      view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view.image = images_[index];
      view.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view.format = swapchainFormat_;
      view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      if (vkCreateImageView(device_, &view, nullptr, &imageViews_[index]) != VK_SUCCESS)
        return false;
    }
#if defined(NEXORA_HAS_NATIVE_UI_SHADERS)
    if (!CreateUiResources() || !CreateSceneResources())
      return false;
#endif
    frames_.resize(std::min<std::size_t>(kMaxFrames, images_.size()));
    std::vector<VkCommandBuffer> commands(frames_.size());
    VkCommandBufferAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = commandPool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = static_cast<std::uint32_t>(commands.size());
    if (vkAllocateCommandBuffers(device_, &allocate, commands.data()) != VK_SUCCESS)
      return false;
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
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
  VkFormat swapchainFormat_{};
  std::vector<VkImage> images_;
  std::vector<VkImageView> imageViews_;
  std::vector<Frame> frames_;
  std::unordered_map<std::uint64_t, UiTexture> uiTextures_;
  std::unordered_map<std::uint64_t, UiTexture> sceneTextures_;
  VkDescriptorSetLayout uiDescriptorLayout_{};
  VkDescriptorPool uiDescriptorPool_{};
  VkSampler uiSampler_{};
  VkPipelineLayout uiPipelineLayout_{};
  VkRenderPass uiRenderPass_{};
  VkPipeline uiPipeline_{};
  VkPipelineLayout scenePipelineLayout_{};
  VkRenderPass sceneRenderPass_{};
  VkPipeline scenePipeline_{};
  bool sceneRendered_{};
  bool sceneOffscreen_{}, sceneComposited_{};
  bool transferTarget_{};
  SurfaceDiagnostics diagnostics_{};
};
} // namespace

std::unique_ptr<ISurface> CreateVulkanSurface(const SurfaceDescriptor &descriptor,
                                              Window::IWindowSystem &windows) {
  auto surface = std::make_unique<VulkanSurface>(descriptor, windows);
  return surface;
}
} // namespace Nexora::Presentation
