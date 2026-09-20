#if !defined(NEXORA_ENABLE_NATIVE_BACKENDS)
#error "VulkanDevice.cpp requires NEXORA_ENABLE_NATIVE_BACKENDS"
#endif

#define VK_NO_PROTOTYPES
#include "Nexora/RHI/Device.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace nexora::rhi {
namespace {

template <typename Function, typename Pointer>
Function FunctionCast(Pointer pointer) {
  static_assert(sizeof(Function) == sizeof(Pointer));
  Function function{};
  std::memcpy(&function, &pointer, sizeof(function));
  return function;
}

[[noreturn]] void ThrowVulkan(VkResult result, const char *operation) {
  throw std::runtime_error(std::string(operation) + " failed with VkResult " +
                           std::to_string(static_cast<int>(result)));
}

void Check(VkResult result, const char *operation) {
  if (result != VK_SUCCESS)
    ThrowVulkan(result, operation);
}

std::uint64_t Key(TextureHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

std::uint64_t Key(PipelineHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

VkFormat ToFormat(TextureFormat format) {
  switch (format) {
  case TextureFormat::Rgba8Unorm:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case TextureFormat::Bgra8Unorm:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case TextureFormat::Depth32Float:
    throw std::invalid_argument("Vulkan triangle backend only supports color textures");
  }
  throw std::invalid_argument("unsupported Vulkan texture format");
}

VkImageLayout ToLayout(ResourceState state) {
  switch (state) {
  case ResourceState::Undefined:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case ResourceState::CopySource:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case ResourceState::CopyDestination:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case ResourceState::ShaderRead:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case ResourceState::RenderTarget:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case ResourceState::Present:
    return VK_IMAGE_LAYOUT_GENERAL;
  }
  throw std::invalid_argument("unsupported Vulkan resource state");
}

struct StageAccess final {
  VkPipelineStageFlags stages{};
  VkAccessFlags access{};
};

StageAccess StageFor(ResourceState state) {
  switch (state) {
  case ResourceState::Undefined:
    return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};
  case ResourceState::CopySource:
    return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT};
  case ResourceState::CopyDestination:
    return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
  case ResourceState::ShaderRead:
    return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};
  case ResourceState::RenderTarget:
    return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
  case ResourceState::Present:
    return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT};
  }
  throw std::invalid_argument("unsupported Vulkan resource state");
}

class VulkanLoader final {
public:
  VulkanLoader() {
#if defined(_WIN32)
    library_ = LoadLibraryA("vulkan-1.dll");
#else
    library_ = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!library_)
      library_ = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif
    if (!library_)
      throw std::runtime_error("Vulkan loader library was not found");
    get_instance_proc_addr_ = Global<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    if (!get_instance_proc_addr_)
      throw std::runtime_error("vkGetInstanceProcAddr was not found");
  }

  ~VulkanLoader() {
#if defined(_WIN32)
    if (library_)
      FreeLibrary(library_);
#else
    if (library_)
      dlclose(library_);
#endif
  }

  VulkanLoader(const VulkanLoader &) = delete;
  VulkanLoader &operator=(const VulkanLoader &) = delete;

  template <typename Function> Function Global(const char *name) const {
#if defined(_WIN32)
    return FunctionCast<Function>(GetProcAddress(library_, name));
#else
    return FunctionCast<Function>(dlsym(library_, name));
#endif
  }

  template <typename Function> Function Instance(VkInstance instance, const char *name) const {
    return FunctionCast<Function>(get_instance_proc_addr_(instance, name));
  }

private:
#if defined(_WIN32)
  HMODULE library_{};
#else
  void *library_{};
#endif
  PFN_vkGetInstanceProcAddr get_instance_proc_addr_{};
};

struct VulkanFunctions final {
  PFN_vkGetInstanceProcAddr GetInstanceProcAddr{};
  PFN_vkCreateInstance CreateInstance{};
  PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion{};
  PFN_vkDestroyInstance DestroyInstance{};
  PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices{};
  PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties{};
  PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties{};
  PFN_vkCreateDevice CreateDevice{};
  PFN_vkGetDeviceProcAddr GetDeviceProcAddr{};
  PFN_vkDestroyDevice DestroyDevice{};
  PFN_vkGetDeviceQueue GetDeviceQueue{};
  PFN_vkCreateCommandPool CreateCommandPool{};
  PFN_vkDestroyCommandPool DestroyCommandPool{};
  PFN_vkAllocateCommandBuffers AllocateCommandBuffers{};
  PFN_vkFreeCommandBuffers FreeCommandBuffers{};
  PFN_vkBeginCommandBuffer BeginCommandBuffer{};
  PFN_vkEndCommandBuffer EndCommandBuffer{};
  PFN_vkResetCommandPool ResetCommandPool{};
  PFN_vkQueueSubmit QueueSubmit{};
  PFN_vkQueueWaitIdle QueueWaitIdle{};
  PFN_vkDeviceWaitIdle DeviceWaitIdle{};
  PFN_vkCreateFence CreateFence{};
  PFN_vkDestroyFence DestroyFence{};
  PFN_vkWaitForFences WaitForFences{};
  PFN_vkResetFences ResetFences{};
  PFN_vkCreateImage CreateImage{};
  PFN_vkDestroyImage DestroyImage{};
  PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements{};
  PFN_vkAllocateMemory AllocateMemory{};
  PFN_vkFreeMemory FreeMemory{};
  PFN_vkBindImageMemory BindImageMemory{};
  PFN_vkCreateImageView CreateImageView{};
  PFN_vkDestroyImageView DestroyImageView{};
  PFN_vkCreateBuffer CreateBuffer{};
  PFN_vkDestroyBuffer DestroyBuffer{};
  PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements{};
  PFN_vkBindBufferMemory BindBufferMemory{};
  PFN_vkMapMemory MapMemory{};
  PFN_vkUnmapMemory UnmapMemory{};
  PFN_vkCreateShaderModule CreateShaderModule{};
  PFN_vkDestroyShaderModule DestroyShaderModule{};
  PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout{};
  PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout{};
  PFN_vkCreateDescriptorPool CreateDescriptorPool{};
  PFN_vkDestroyDescriptorPool DestroyDescriptorPool{};
  PFN_vkAllocateDescriptorSets AllocateDescriptorSets{};
  PFN_vkUpdateDescriptorSets UpdateDescriptorSets{};
  PFN_vkCreatePipelineLayout CreatePipelineLayout{};
  PFN_vkDestroyPipelineLayout DestroyPipelineLayout{};
  PFN_vkCreateRenderPass CreateRenderPass{};
  PFN_vkDestroyRenderPass DestroyRenderPass{};
  PFN_vkCreateFramebuffer CreateFramebuffer{};
  PFN_vkDestroyFramebuffer DestroyFramebuffer{};
  PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines{};
  PFN_vkDestroyPipeline DestroyPipeline{};
  PFN_vkCmdPipelineBarrier CmdPipelineBarrier{};
  PFN_vkCmdBeginRenderPass CmdBeginRenderPass{};
  PFN_vkCmdEndRenderPass CmdEndRenderPass{};
  PFN_vkCmdBindPipeline CmdBindPipeline{};
  PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets{};
  PFN_vkCmdSetViewport CmdSetViewport{};
  PFN_vkCmdSetScissor CmdSetScissor{};
  PFN_vkCmdDraw CmdDraw{};
};

template <typename Function>
Function RequireFunction(Function function, const char *name) {
  if (!function)
    throw std::runtime_error(std::string("Vulkan function was not found: ") + name);
  return function;
}

class VulkanDevice;

class VulkanCommandList final : public CommandList {
public:
  VulkanCommandList(VulkanDevice &device, QueueType queue);
  ~VulkanCommandList() override;

  void Transition(const Barrier &barrier) override;
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void EndRendering() override;

  [[nodiscard]] bool IsClosed() const noexcept { return closed_; }
  [[nodiscard]] bool IsSubmitted() const noexcept { return submitted_; }
  [[nodiscard]] bool BelongsTo(const VulkanDevice &device) const noexcept {
    return &device_ == &device;
  }
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  void MarkSubmitted() noexcept { submitted_ = true; }
  void Close();

private:
  friend class VulkanDevice;
  VulkanDevice &device_;
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  VkFramebuffer framebuffer_{VK_NULL_HANDLE};
  bool rendering_{false};
  bool pipeline_bound_{false};
  bool submitted_{false};
  bool closed_{false};
  TextureHandle render_target_{};
  std::uint32_t width_{};
  std::uint32_t height_{};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
};

class VulkanDevice final : public Device {
  struct TextureRecord;
  struct PipelineRecord;

public:
  VulkanDevice();
  ~VulkanDevice() override;

  Backend GetBackend() const noexcept override { return Backend::Vulkan; }
  TextureHandle CreateTexture(const TextureDescriptor &descriptor) override;
  void DestroyTexture(TextureHandle texture) override;
  PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) override;
  void DestroyPipeline(PipelineHandle pipeline) override;
  std::unique_ptr<CommandList> CreateCommandList(QueueType queue) override;
  void Submit(CommandList &commands) override;
  void Present(TextureHandle texture) override;
  void WaitIdle() override;
  [[nodiscard]] DeviceDiagnostics Diagnostics() const noexcept override;

private:
  friend class VulkanCommandList;
  void LoadInstanceFunctions();
  void LoadDeviceFunctions();
  void SelectPhysicalDevice();
  void CreateCoreObjects();
  void CreateRenderPasses();
  void CreateUniformResources();
  void LoadShaderModule();
  void TransitionTextureImmediately(TextureRecord &texture, ResourceState state);
  void Require(bool condition, const char *message);
  TextureRecord &RecordTransition(const Barrier &barrier);
  TextureRecord &ValidateRenderTarget(TextureHandle texture);
  PipelineRecord &ValidatePipeline(PipelineHandle pipeline);
  VkFramebuffer CreateFramebuffer(const TextureRecord &texture, VkRenderPass render_pass,
                                  std::uint32_t width, std::uint32_t height);
  void DestroyFramebuffer(VkFramebuffer framebuffer);
  VkRenderPass RenderPassFor(TextureFormat format) const;
  std::uint32_t FindMemoryType(std::uint32_t type_bits, VkMemoryPropertyFlags properties) const;
  void DestroyTextureRecord(TextureRecord &texture);
  void DestroyPipelineRecord(PipelineRecord &pipeline);

  VulkanLoader loader_;
  VulkanFunctions functions_{};
  VkInstance instance_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  std::uint32_t graphics_queue_family_{};
  VkCommandPool immediate_command_pool_{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
  VkBuffer uniform_buffer_{VK_NULL_HANDLE};
  VkDeviceMemory uniform_memory_{VK_NULL_HANDLE};
  VkShaderModule shader_module_{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkRenderPass rgba_render_pass_{VK_NULL_HANDLE};
  VkRenderPass bgra_render_pass_{VK_NULL_HANDLE};
  mutable std::mutex mutex_;
  core::HandlePool<TextureTag> texture_pool_;
  core::HandlePool<PipelineTag> pipeline_pool_;
  std::unordered_map<std::uint64_t, TextureRecord> textures_;
  std::unordered_map<std::uint64_t, PipelineRecord> pipelines_;
  DeviceDiagnostics diagnostics_{};
};

struct VulkanDevice::TextureRecord final {
  TextureDescriptor descriptor;
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkImageView view{VK_NULL_HANDLE};
  ResourceState logical_state{ResourceState::Undefined};
};

struct VulkanDevice::PipelineRecord final {
  PipelineDescriptor descriptor;
  VkPipeline pipeline{VK_NULL_HANDLE};
  VkRenderPass render_pass{VK_NULL_HANDLE};
};

void VulkanDevice::LoadInstanceFunctions() {
  functions_.DestroyInstance = RequireFunction(
      loader_.Instance<PFN_vkDestroyInstance>(instance_, "vkDestroyInstance"),
      "vkDestroyInstance");
  functions_.EnumeratePhysicalDevices = RequireFunction(
      loader_.Instance<PFN_vkEnumeratePhysicalDevices>(instance_, "vkEnumeratePhysicalDevices"),
      "vkEnumeratePhysicalDevices");
  functions_.GetPhysicalDeviceQueueFamilyProperties = RequireFunction(
      loader_.Instance<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          instance_, "vkGetPhysicalDeviceQueueFamilyProperties"),
      "vkGetPhysicalDeviceQueueFamilyProperties");
  functions_.GetPhysicalDeviceMemoryProperties = RequireFunction(
      loader_.Instance<PFN_vkGetPhysicalDeviceMemoryProperties>(
          instance_, "vkGetPhysicalDeviceMemoryProperties"),
      "vkGetPhysicalDeviceMemoryProperties");
  functions_.CreateDevice = RequireFunction(
      loader_.Instance<PFN_vkCreateDevice>(instance_, "vkCreateDevice"), "vkCreateDevice");
  functions_.GetDeviceProcAddr = RequireFunction(
      loader_.Instance<PFN_vkGetDeviceProcAddr>(instance_, "vkGetDeviceProcAddr"),
      "vkGetDeviceProcAddr");
}

void VulkanDevice::LoadDeviceFunctions() {
#define LOAD_DEVICE(member, symbol)                                                        \
  functions_.member = RequireFunction(                                                     \
      FunctionCast<decltype(functions_.member)>(functions_.GetDeviceProcAddr(device_, symbol)), \
      symbol)
  LOAD_DEVICE(DestroyDevice, "vkDestroyDevice");
  LOAD_DEVICE(GetDeviceQueue, "vkGetDeviceQueue");
  LOAD_DEVICE(CreateCommandPool, "vkCreateCommandPool");
  LOAD_DEVICE(DestroyCommandPool, "vkDestroyCommandPool");
  LOAD_DEVICE(AllocateCommandBuffers, "vkAllocateCommandBuffers");
  LOAD_DEVICE(FreeCommandBuffers, "vkFreeCommandBuffers");
  LOAD_DEVICE(BeginCommandBuffer, "vkBeginCommandBuffer");
  LOAD_DEVICE(EndCommandBuffer, "vkEndCommandBuffer");
  LOAD_DEVICE(ResetCommandPool, "vkResetCommandPool");
  LOAD_DEVICE(QueueSubmit, "vkQueueSubmit");
  LOAD_DEVICE(QueueWaitIdle, "vkQueueWaitIdle");
  LOAD_DEVICE(DeviceWaitIdle, "vkDeviceWaitIdle");
  LOAD_DEVICE(CreateFence, "vkCreateFence");
  LOAD_DEVICE(DestroyFence, "vkDestroyFence");
  LOAD_DEVICE(WaitForFences, "vkWaitForFences");
  LOAD_DEVICE(ResetFences, "vkResetFences");
  LOAD_DEVICE(CreateImage, "vkCreateImage");
  LOAD_DEVICE(DestroyImage, "vkDestroyImage");
  LOAD_DEVICE(GetImageMemoryRequirements, "vkGetImageMemoryRequirements");
  LOAD_DEVICE(AllocateMemory, "vkAllocateMemory");
  LOAD_DEVICE(FreeMemory, "vkFreeMemory");
  LOAD_DEVICE(BindImageMemory, "vkBindImageMemory");
  LOAD_DEVICE(CreateImageView, "vkCreateImageView");
  LOAD_DEVICE(DestroyImageView, "vkDestroyImageView");
  LOAD_DEVICE(CreateBuffer, "vkCreateBuffer");
  LOAD_DEVICE(DestroyBuffer, "vkDestroyBuffer");
  LOAD_DEVICE(GetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
  LOAD_DEVICE(BindBufferMemory, "vkBindBufferMemory");
  LOAD_DEVICE(MapMemory, "vkMapMemory");
  LOAD_DEVICE(UnmapMemory, "vkUnmapMemory");
  LOAD_DEVICE(CreateShaderModule, "vkCreateShaderModule");
  LOAD_DEVICE(DestroyShaderModule, "vkDestroyShaderModule");
  LOAD_DEVICE(CreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
  LOAD_DEVICE(DestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
  LOAD_DEVICE(CreateDescriptorPool, "vkCreateDescriptorPool");
  LOAD_DEVICE(DestroyDescriptorPool, "vkDestroyDescriptorPool");
  LOAD_DEVICE(AllocateDescriptorSets, "vkAllocateDescriptorSets");
  LOAD_DEVICE(UpdateDescriptorSets, "vkUpdateDescriptorSets");
  LOAD_DEVICE(CreatePipelineLayout, "vkCreatePipelineLayout");
  LOAD_DEVICE(DestroyPipelineLayout, "vkDestroyPipelineLayout");
  LOAD_DEVICE(CreateRenderPass, "vkCreateRenderPass");
  LOAD_DEVICE(DestroyRenderPass, "vkDestroyRenderPass");
  LOAD_DEVICE(CreateFramebuffer, "vkCreateFramebuffer");
  LOAD_DEVICE(DestroyFramebuffer, "vkDestroyFramebuffer");
  LOAD_DEVICE(CreateGraphicsPipelines, "vkCreateGraphicsPipelines");
  LOAD_DEVICE(DestroyPipeline, "vkDestroyPipeline");
  LOAD_DEVICE(CmdPipelineBarrier, "vkCmdPipelineBarrier");
  LOAD_DEVICE(CmdBeginRenderPass, "vkCmdBeginRenderPass");
  LOAD_DEVICE(CmdEndRenderPass, "vkCmdEndRenderPass");
  LOAD_DEVICE(CmdBindPipeline, "vkCmdBindPipeline");
  LOAD_DEVICE(CmdBindDescriptorSets, "vkCmdBindDescriptorSets");
  LOAD_DEVICE(CmdSetViewport, "vkCmdSetViewport");
  LOAD_DEVICE(CmdSetScissor, "vkCmdSetScissor");
  LOAD_DEVICE(CmdDraw, "vkCmdDraw");
#undef LOAD_DEVICE
}

VulkanDevice::VulkanDevice() {
  const auto *shader_path = std::getenv("NEXORA_SLANG_SPIRV_PATH");
  if (!shader_path || *shader_path == '\0')
    throw std::runtime_error("NEXORA_SLANG_SPIRV_PATH is required for the Vulkan backend");
  std::ifstream shader_file(shader_path, std::ios::binary | std::ios::ate);
  if (!shader_file)
    throw std::runtime_error(std::string("cannot open Vulkan shader artifact: ") + shader_path);
  functions_.GetInstanceProcAddr =
      RequireFunction(loader_.Global<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"),
                      "vkGetInstanceProcAddr");
  const VkApplicationInfo application_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "Nexora", VK_MAKE_VERSION(0, 1, 0),
      "Nexora", VK_MAKE_VERSION(0, 1, 0), VK_API_VERSION_1_0};
  const VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &application_info, 0, nullptr, 0,
      nullptr};
  const auto create_instance =
      RequireFunction(loader_.Global<PFN_vkCreateInstance>("vkCreateInstance"),
                      "vkCreateInstance");
  Check(create_instance(&instance_info, nullptr, &instance_), "vkCreateInstance");
  LoadInstanceFunctions();
  SelectPhysicalDevice();
  CreateCoreObjects();
  CreateRenderPasses();
  CreateUniformResources();
  LoadShaderModule();
}

void VulkanDevice::SelectPhysicalDevice() {
  std::uint32_t count = 0;
  Check(functions_.EnumeratePhysicalDevices(instance_, &count, nullptr),
        "vkEnumeratePhysicalDevices(count)");
  if (count == 0)
    throw std::runtime_error("Vulkan reported no physical devices");
  std::vector<VkPhysicalDevice> devices(count);
  Check(functions_.EnumeratePhysicalDevices(instance_, &count, devices.data()),
        "vkEnumeratePhysicalDevices");
  for (const auto candidate : devices) {
    std::uint32_t queue_count = 0;
    functions_.GetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queue_count);
    functions_.GetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());
    for (std::uint32_t index = 0; index < queue_count; ++index) {
      if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
        physical_device_ = candidate;
        graphics_queue_family_ = index;
        return;
      }
    }
  }
  throw std::runtime_error("Vulkan has no graphics queue family");
}

void VulkanDevice::CreateCoreObjects() {
  constexpr float priority = 1.0F;
  const VkDeviceQueueCreateInfo queue_info{
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, graphics_queue_family_, 1,
      &priority};
  const VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, 1,
                                       &queue_info, 0, nullptr, 0, nullptr, nullptr};
  Check(functions_.CreateDevice(physical_device_, &device_info, nullptr, &device_),
        "vkCreateDevice");
  LoadDeviceFunctions();
  functions_.GetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);

  const VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          nullptr,
                                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          graphics_queue_family_};
  Check(functions_.CreateCommandPool(device_, &pool_info, nullptr, &immediate_command_pool_),
        "vkCreateCommandPool");
}

void VulkanDevice::CreateRenderPasses() {
  const auto create = [this](VkFormat format) {
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    const VkRenderPassCreateInfo render_pass_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, &attachment, 1, &subpass, 1,
        &dependency};
    VkRenderPass render_pass = VK_NULL_HANDLE;
    Check(functions_.CreateRenderPass(device_, &render_pass_info, nullptr, &render_pass),
          "vkCreateRenderPass");
    return render_pass;
  };
  rgba_render_pass_ = create(VK_FORMAT_R8G8B8A8_UNORM);
  bgra_render_pass_ = create(VK_FORMAT_B8G8R8A8_UNORM);
}

std::uint32_t VulkanDevice::FindMemoryType(std::uint32_t type_bits,
                                           VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  functions_.GetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
  for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
    if ((type_bits & (1U << index)) != 0 &&
        (memory_properties.memoryTypes[index].propertyFlags & properties) == properties)
      return index;
  }
  throw std::runtime_error("Vulkan memory type was not found");
}

void VulkanDevice::CreateUniformResources() {
  const VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                       nullptr,
                                       0,
                                       64,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VK_SHARING_MODE_EXCLUSIVE,
                                       0,
                                       nullptr};
  Check(functions_.CreateBuffer(device_, &buffer_info, nullptr, &uniform_buffer_),
        "vkCreateBuffer(uniform)");
  VkMemoryRequirements requirements{};
  functions_.GetBufferMemoryRequirements(device_, uniform_buffer_, &requirements);
  const VkMemoryAllocateInfo allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                           nullptr,
                                           requirements.size,
                                           FindMemoryType(requirements.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
  Check(functions_.AllocateMemory(device_, &allocate_info, nullptr, &uniform_memory_),
        "vkAllocateMemory(uniform)");
  Check(functions_.BindBufferMemory(device_, uniform_buffer_, uniform_memory_, 0),
        "vkBindBufferMemory(uniform)");
  void *mapped = nullptr;
  Check(functions_.MapMemory(device_, uniform_memory_, 0, 64, 0, &mapped),
        "vkMapMemory(uniform)");
  const float identity[16] = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  std::memcpy(mapped, identity, sizeof(identity));
  functions_.UnmapMemory(device_, uniform_memory_);

  const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                             VK_SHADER_STAGE_VERTEX_BIT, nullptr};
  const VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &binding};
  Check(functions_.CreateDescriptorSetLayout(device_, &layout_info, nullptr,
                                             &descriptor_set_layout_),
        "vkCreateDescriptorSetLayout");
  const VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
  const VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                             nullptr,
                                             0,
                                             1,
                                             1,
                                             &pool_size};
  Check(functions_.CreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
        "vkCreateDescriptorPool");
  const VkDescriptorSetAllocateInfo set_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             nullptr,
                                             descriptor_pool_,
                                             1,
                                             &descriptor_set_layout_};
  Check(functions_.AllocateDescriptorSets(device_, &set_info, &descriptor_set_),
        "vkAllocateDescriptorSets");
  const VkDescriptorBufferInfo descriptor_buffer{uniform_buffer_, 0, 64};
  const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                   nullptr,
                                   descriptor_set_,
                                   0,
                                   0,
                                   1,
                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   nullptr,
                                   &descriptor_buffer,
                                   nullptr};
  functions_.UpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &descriptor_set_layout_, 0,
      nullptr};
  Check(functions_.CreatePipelineLayout(device_, &pipeline_layout_info, nullptr,
                                        &pipeline_layout_),
        "vkCreatePipelineLayout");
}

void VulkanDevice::LoadShaderModule() {
  const auto *path = std::getenv("NEXORA_SLANG_SPIRV_PATH");
  if (!path || *path == '\0')
    throw std::runtime_error("NEXORA_SLANG_SPIRV_PATH is required for the Vulkan backend");
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    throw std::runtime_error(std::string("cannot open Vulkan shader artifact: ") + path);
  const auto size = file.tellg();
  if (size <= 0 || (size % static_cast<std::streamoff>(sizeof(std::uint32_t))) != 0)
    throw std::runtime_error("Vulkan shader artifact has an invalid size");
  std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), size);
  const VkShaderModuleCreateInfo module_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                             nullptr,
                                             0,
                                             words.size() * sizeof(std::uint32_t),
                                             words.data()};
  Check(functions_.CreateShaderModule(device_, &module_info, nullptr, &shader_module_),
        "vkCreateShaderModule");
}

void VulkanDevice::TransitionTextureImmediately(TextureRecord &texture, ResourceState state) {
  if (state == ResourceState::Undefined)
    return;
  const VkCommandBufferAllocateInfo allocate_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, immediate_command_pool_,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  Check(functions_.AllocateCommandBuffers(device_, &allocate_info, &command_buffer),
        "vkAllocateCommandBuffers(texture transition)");
  const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                            nullptr,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                            nullptr};
  try {
    Check(functions_.BeginCommandBuffer(command_buffer, &begin_info),
          "vkBeginCommandBuffer(texture transition)");
    const auto destination = StageFor(state);
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = destination.access;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = ToLayout(state);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    functions_.CmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                   destination.stages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    Check(functions_.EndCommandBuffer(command_buffer),
          "vkEndCommandBuffer(texture transition)");
    const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    VkFence fence = VK_NULL_HANDLE;
    Check(functions_.CreateFence(device_, &fence_info, nullptr, &fence),
          "vkCreateFence(texture transition)");
    const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr,
                                   1, &command_buffer, 0, nullptr};
    Check(functions_.QueueSubmit(graphics_queue_, 1, &submit_info, fence),
          "vkQueueSubmit(texture transition)");
    Check(functions_.WaitForFences(device_, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
          "vkWaitForFences(texture transition)");
    functions_.DestroyFence(device_, fence, nullptr);
  } catch (...) {
    functions_.FreeCommandBuffers(device_, immediate_command_pool_, 1, &command_buffer);
    throw;
  }
  functions_.FreeCommandBuffers(device_, immediate_command_pool_, 1, &command_buffer);
  texture.logical_state = state;
}

TextureHandle VulkanDevice::CreateTexture(const TextureDescriptor &descriptor) {
  if (descriptor.width == 0 || descriptor.height == 0)
    throw std::invalid_argument("invalid texture extent");
  const auto format = ToFormat(descriptor.format);
  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = format;
  image_info.extent = {descriptor.width, descriptor.height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  TextureRecord record;
  record.descriptor = descriptor;
  Check(functions_.CreateImage(device_, &image_info, nullptr, &record.image),
        "vkCreateImage");
  VkMemoryRequirements requirements{};
  functions_.GetImageMemoryRequirements(device_, record.image, &requirements);
  const VkMemoryAllocateInfo allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                           nullptr,
                                           requirements.size,
                                           FindMemoryType(requirements.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
  Check(functions_.AllocateMemory(device_, &allocate_info, nullptr, &record.memory),
        "vkAllocateMemory(image)");
  Check(functions_.BindImageMemory(device_, record.image, record.memory, 0),
        "vkBindImageMemory");
  const VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                        nullptr,
                                        0,
                                        record.image,
                                        VK_IMAGE_VIEW_TYPE_2D,
                                        format,
                                        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  Check(functions_.CreateImageView(device_, &view_info, nullptr, &record.view),
        "vkCreateImageView");
  try {
    TransitionTextureImmediately(record, descriptor.initial_state);
  } catch (...) {
    DestroyTextureRecord(record);
    throw;
  }
  record.logical_state = descriptor.initial_state;
  std::lock_guard lock{mutex_};
  const auto handle = texture_pool_.Create();
  textures_.emplace(Key(handle), std::move(record));
  return handle;
}

void VulkanDevice::DestroyTextureRecord(TextureRecord &texture) {
  if (texture.view != VK_NULL_HANDLE)
    functions_.DestroyImageView(device_, texture.view, nullptr);
  if (texture.image != VK_NULL_HANDLE)
    functions_.DestroyImage(device_, texture.image, nullptr);
  if (texture.memory != VK_NULL_HANDLE)
    functions_.FreeMemory(device_, texture.memory, nullptr);
  texture.view = VK_NULL_HANDLE;
  texture.image = VK_NULL_HANDLE;
  texture.memory = VK_NULL_HANDLE;
}

void VulkanDevice::DestroyTexture(TextureHandle texture) {
  std::lock_guard lock{mutex_};
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "destroying invalid Vulkan texture");
  DestroyTextureRecord(found->second);
  textures_.erase(found);
  Require(texture_pool_.Destroy(texture), "destroying stale Vulkan texture");
}

PipelineHandle VulkanDevice::CreatePipeline(const PipelineDescriptor &descriptor) {
  if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0)
    throw std::invalid_argument("pipeline hashes must be non-zero");
  const auto render_pass = RenderPassFor(descriptor.color_format);
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = shader_module_;
  stages[0].pName = "vertexMain";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = shader_module_;
  stages[1].pName = "fragmentMain";
  const VkPipelineVertexInputStateCreateInfo vertex_input{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 0, nullptr, 0,
      nullptr};
  const VkPipelineInputAssemblyStateCreateInfo input_assembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
  VkPipelineViewportStateCreateInfo viewport_state{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
  VkPipelineRasterizationStateCreateInfo rasterizer{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0F;
  const VkPipelineMultisampleStateCreateInfo multisample{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT,
      VK_FALSE, 1.0F, nullptr, VK_FALSE, VK_FALSE};
  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.blendEnable = VK_FALSE;
  blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  const VkPipelineColorBlendStateCreateInfo blend{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE,
      VK_LOGIC_OP_COPY, 1, &blend_attachment, {0.0F, 0.0F, 0.0F, 0.0F}};
  const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamic_state{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dynamic_states};
  const VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      nullptr,
      0,
      2,
      stages,
      &vertex_input,
      &input_assembly,
      nullptr,
      &viewport_state,
      &rasterizer,
      &multisample,
      nullptr,
      &blend,
      &dynamic_state,
      pipeline_layout_,
      render_pass,
      0,
      VK_NULL_HANDLE,
      -1};
  PipelineRecord record;
  record.descriptor = descriptor;
  record.render_pass = render_pass;
  Check(functions_.CreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                           &record.pipeline),
        "vkCreateGraphicsPipelines");
  std::lock_guard lock{mutex_};
  const auto handle = pipeline_pool_.Create();
  pipelines_.emplace(Key(handle), std::move(record));
  return handle;
}

void VulkanDevice::DestroyPipelineRecord(PipelineRecord &pipeline) {
  if (pipeline.pipeline != VK_NULL_HANDLE)
    functions_.DestroyPipeline(device_, pipeline.pipeline, nullptr);
  pipeline.pipeline = VK_NULL_HANDLE;
}

void VulkanDevice::DestroyPipeline(PipelineHandle pipeline) {
  std::lock_guard lock{mutex_};
  const auto found = pipelines_.find(Key(pipeline));
  Require(pipeline_pool_.Contains(pipeline) && found != pipelines_.end(),
          "destroying invalid Vulkan pipeline");
  DestroyPipelineRecord(found->second);
  pipelines_.erase(found);
  Require(pipeline_pool_.Destroy(pipeline), "destroying stale Vulkan pipeline");
}

std::unique_ptr<CommandList> VulkanDevice::CreateCommandList(QueueType queue) {
  if (queue != QueueType::Graphics)
    throw std::invalid_argument("Vulkan triangle backend only supports graphics queue");
  return std::make_unique<VulkanCommandList>(*this, queue);
}

VkRenderPass VulkanDevice::RenderPassFor(TextureFormat format) const {
  switch (format) {
  case TextureFormat::Rgba8Unorm:
    return rgba_render_pass_;
  case TextureFormat::Bgra8Unorm:
    return bgra_render_pass_;
  case TextureFormat::Depth32Float:
    break;
  }
  throw std::invalid_argument("Vulkan render pass requires a color format");
}

VkFramebuffer VulkanDevice::CreateFramebuffer(const TextureRecord &texture, VkRenderPass render_pass,
                                              std::uint32_t width, std::uint32_t height) {
  const VkImageView attachment = texture.view;
  const VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                 nullptr,
                                                 0,
                                                 render_pass,
                                                 1,
                                                 &attachment,
                                                 width,
                                                 height,
                                                 1};
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  Check(functions_.CreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffer),
        "vkCreateFramebuffer");
  return framebuffer;
}

void VulkanDevice::DestroyFramebuffer(VkFramebuffer framebuffer) {
  if (framebuffer != VK_NULL_HANDLE)
    functions_.DestroyFramebuffer(device_, framebuffer, nullptr);
}

void VulkanDevice::Require(bool condition, const char *message) {
  if (condition)
    return;
  ++diagnostics_.validation_errors;
  throw std::logic_error(message);
}

VulkanDevice::TextureRecord &VulkanDevice::RecordTransition(const Barrier &barrier) {
  std::lock_guard lock{mutex_};
  const auto found = textures_.find(Key(barrier.texture));
  Require(texture_pool_.Contains(barrier.texture) && found != textures_.end(),
          "barrier references invalid Vulkan texture");
  Require(found->second.logical_state == barrier.before,
          "Vulkan barrier before-state mismatch");
  found->second.logical_state = barrier.after;
  return found->second;
}

VulkanDevice::TextureRecord &VulkanDevice::ValidateRenderTarget(TextureHandle texture) {
  std::lock_guard lock{mutex_};
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "rendering references invalid Vulkan texture");
  Require(found->second.logical_state == ResourceState::RenderTarget,
          "Vulkan render target is not in RenderTarget state");
  return found->second;
}

VulkanDevice::PipelineRecord &VulkanDevice::ValidatePipeline(PipelineHandle pipeline) {
  std::lock_guard lock{mutex_};
  const auto found = pipelines_.find(Key(pipeline));
  Require(pipeline_pool_.Contains(pipeline) && found != pipelines_.end(),
          "binding invalid Vulkan pipeline");
  return found->second;
}

void VulkanDevice::Submit(CommandList &commands) {
  auto *validated = dynamic_cast<VulkanCommandList *>(&commands);
  VkFence fence = VK_NULL_HANDLE;
  {
    std::lock_guard lock{mutex_};
    Require(validated != nullptr, "command list belongs to another device");
    Require(validated->BelongsTo(*this), "command list belongs to another device");
    Require(validated->IsClosed(), "cannot submit an open command list");
    Require(!validated->IsSubmitted(), "command list was already submitted");
    const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    Check(functions_.CreateFence(device_, &fence_info, nullptr, &fence), "vkCreateFence(submit)");
    const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr,
                                   1, &validated->command_buffer_, 0, nullptr};
    try {
      Check(functions_.QueueSubmit(graphics_queue_, 1, &submit_info, fence), "vkQueueSubmit");
    } catch (...) {
      functions_.DestroyFence(device_, fence, nullptr);
      throw;
    }
    validated->MarkSubmitted();
    ++diagnostics_.submitted_command_lists;
    diagnostics_.barriers += validated->Barriers();
    diagnostics_.draw_calls += validated->DrawCalls();
  }
  try {
    Check(functions_.WaitForFences(device_, 1, &fence, VK_TRUE,
                                   std::numeric_limits<std::uint64_t>::max()),
          "vkWaitForFences(submit)");
  } catch (...) {
    functions_.DestroyFence(device_, fence, nullptr);
    throw;
  }
  functions_.DestroyFence(device_, fence, nullptr);
  if (validated->framebuffer_ != VK_NULL_HANDLE) {
    DestroyFramebuffer(validated->framebuffer_);
    validated->framebuffer_ = VK_NULL_HANDLE;
  }
}

void VulkanDevice::Present(TextureHandle texture) {
  std::lock_guard lock{mutex_};
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "presenting invalid Vulkan texture");
  Require(found->second.logical_state == ResourceState::Present,
          "present texture is not in Present state");
  ++diagnostics_.presents;
}

void VulkanDevice::WaitIdle() {
  if (device_ != VK_NULL_HANDLE)
    Check(functions_.DeviceWaitIdle(device_), "vkDeviceWaitIdle");
}

DeviceDiagnostics VulkanDevice::Diagnostics() const noexcept {
  std::lock_guard lock{mutex_};
  return diagnostics_;
}

VulkanDevice::~VulkanDevice() {
  if (device_ == VK_NULL_HANDLE)
    return;
  try {
    WaitIdle();
  } catch (...) {
  }
  for (auto &[key, pipeline] : pipelines_)
    (void)key, DestroyPipelineRecord(pipeline);
  for (auto &[key, texture] : textures_)
    (void)key, DestroyTextureRecord(texture);
  if (shader_module_ != VK_NULL_HANDLE)
    functions_.DestroyShaderModule(device_, shader_module_, nullptr);
  if (pipeline_layout_ != VK_NULL_HANDLE)
    functions_.DestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  if (descriptor_pool_ != VK_NULL_HANDLE)
    functions_.DestroyDescriptorPool(device_, descriptor_pool_, nullptr);
  if (descriptor_set_layout_ != VK_NULL_HANDLE)
    functions_.DestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
  if (uniform_buffer_ != VK_NULL_HANDLE)
    functions_.DestroyBuffer(device_, uniform_buffer_, nullptr);
  if (uniform_memory_ != VK_NULL_HANDLE)
    functions_.FreeMemory(device_, uniform_memory_, nullptr);
  if (rgba_render_pass_ != VK_NULL_HANDLE)
    functions_.DestroyRenderPass(device_, rgba_render_pass_, nullptr);
  if (bgra_render_pass_ != VK_NULL_HANDLE)
    functions_.DestroyRenderPass(device_, bgra_render_pass_, nullptr);
  if (immediate_command_pool_ != VK_NULL_HANDLE)
    functions_.DestroyCommandPool(device_, immediate_command_pool_, nullptr);
  functions_.DestroyDevice(device_, nullptr);
  device_ = VK_NULL_HANDLE;
  if (instance_ != VK_NULL_HANDLE)
    functions_.DestroyInstance(instance_, nullptr);
  instance_ = VK_NULL_HANDLE;
}

VulkanCommandList::VulkanCommandList(VulkanDevice &device, QueueType queue) : device_(device) {
  (void)queue;
  const VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          nullptr,
                                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          device_.graphics_queue_family_};
  Check(device_.functions_.CreateCommandPool(device_.device_, &pool_info, nullptr, &command_pool_),
        "vkCreateCommandPool(command list)");
  const VkCommandBufferAllocateInfo allocate_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, command_pool_,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
  try {
    Check(device_.functions_.AllocateCommandBuffers(device_.device_, &allocate_info,
                                                    &command_buffer_),
          "vkAllocateCommandBuffers(command list)");
    const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                              nullptr,
                                              VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                              nullptr};
    Check(device_.functions_.BeginCommandBuffer(command_buffer_, &begin_info),
          "vkBeginCommandBuffer(command list)");
  } catch (...) {
    if (command_buffer_ != VK_NULL_HANDLE)
      device_.functions_.FreeCommandBuffers(device_.device_, command_pool_, 1, &command_buffer_);
    device_.functions_.DestroyCommandPool(device_.device_, command_pool_, nullptr);
    command_buffer_ = VK_NULL_HANDLE;
    command_pool_ = VK_NULL_HANDLE;
    throw;
  }
}

VulkanCommandList::~VulkanCommandList() {
  if (framebuffer_ != VK_NULL_HANDLE)
    device_.DestroyFramebuffer(framebuffer_);
  if (command_buffer_ != VK_NULL_HANDLE)
    device_.functions_.FreeCommandBuffers(device_.device_, command_pool_, 1, &command_buffer_);
  if (command_pool_ != VK_NULL_HANDLE)
    device_.functions_.DestroyCommandPool(device_.device_, command_pool_, nullptr);
}

void VulkanCommandList::Transition(const Barrier &barrier) {
  if (submitted_)
    throw std::logic_error("cannot record a submitted command list");
  if (rendering_)
    throw std::logic_error("Vulkan barriers cannot occur inside rendering");
  if (barrier.before == barrier.after)
    return;
  auto &texture = device_.RecordTransition(barrier);
  const auto source = StageFor(barrier.before);
  const auto destination = StageFor(barrier.after);
  VkImageMemoryBarrier native{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  native.srcAccessMask = source.access;
  native.dstAccessMask = destination.access;
  native.oldLayout = ToLayout(barrier.before);
  native.newLayout = ToLayout(barrier.after);
  native.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  native.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  native.image = texture.image;
  native.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  native.subresourceRange.levelCount = 1;
  native.subresourceRange.layerCount = 1;
  device_.functions_.CmdPipelineBarrier(command_buffer_, source.stages, destination.stages, 0, 0,
                                         nullptr, 0, nullptr, 1, &native);
  ++barriers_;
}

void VulkanCommandList::BeginRendering(const RenderingInfo &info) {
  if (submitted_ || rendering_ || info.width == 0 || info.height == 0)
    throw std::logic_error("invalid Vulkan BeginRendering");
  auto &target = device_.ValidateRenderTarget(info.color_target);
  framebuffer_ = device_.CreateFramebuffer(target,
                                            device_.RenderPassFor(target.descriptor.format),
                                            info.width, info.height);
  VkClearValue clear{};
  clear.color.float32[3] = 1.0F;
  VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  begin.renderPass = device_.RenderPassFor(target.descriptor.format);
  begin.framebuffer = framebuffer_;
  begin.renderArea.extent = {info.width, info.height};
  begin.clearValueCount = 1;
  begin.pClearValues = &clear;
  device_.functions_.CmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
  render_target_ = info.color_target;
  width_ = info.width;
  height_ = info.height;
  rendering_ = true;
  pipeline_bound_ = false;
}

void VulkanCommandList::BindPipeline(PipelineHandle pipeline) {
  if (submitted_ || !rendering_)
    throw std::logic_error("Vulkan pipeline binding requires rendering");
  auto &record = device_.ValidatePipeline(pipeline);
  const auto &target = device_.ValidateRenderTarget(render_target_);
  device_.Require(record.render_pass == device_.RenderPassFor(target.descriptor.format),
                  "Vulkan pipeline format does not match render target");
  device_.functions_.CmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     record.pipeline);
  const VkDescriptorSet descriptor_set = device_.descriptor_set_;
  device_.functions_.CmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                           device_.pipeline_layout_, 0, 1, &descriptor_set, 0,
                                           nullptr);
  const VkViewport viewport{0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_),
                            0.0F, 1.0F};
  const VkRect2D scissor{{0, 0}, {width_, height_}};
  device_.functions_.CmdSetViewport(command_buffer_, 0, 1, &viewport);
  device_.functions_.CmdSetScissor(command_buffer_, 0, 1, &scissor);
  pipeline_bound_ = true;
}

void VulkanCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  if (submitted_ || !rendering_ || !pipeline_bound_ || vertex_count == 0 || instance_count == 0)
    throw std::logic_error("invalid Vulkan draw");
  device_.functions_.CmdDraw(command_buffer_, vertex_count, instance_count, 0, 0);
  ++draws_;
}

void VulkanCommandList::EndRendering() {
  if (submitted_ || !rendering_)
    throw std::logic_error("Vulkan EndRendering without BeginRendering");
  device_.functions_.CmdEndRenderPass(command_buffer_);
  rendering_ = false;
}

void VulkanCommandList::Close() {
  if (closed_)
    return;
  if (rendering_)
    throw std::logic_error("cannot close a Vulkan command list during rendering");
  Check(device_.functions_.EndCommandBuffer(command_buffer_), "vkEndCommandBuffer(command list)");
  closed_ = true;
}


} // namespace

std::unique_ptr<Device> CreateVulkanDevice() {
  return std::make_unique<VulkanDevice>();
}
} // namespace nexora::rhi
