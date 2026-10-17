#if !defined(NEXORA_ENABLE_NATIVE_BACKENDS)
#error "VulkanDevice.cpp requires NEXORA_ENABLE_NATIVE_BACKENDS"
#endif

#define VK_NO_PROTOTYPES
#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/Device.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
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

template <typename Function, typename Pointer> Function FunctionCast(Pointer pointer) {
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

std::uint64_t Key(BufferHandle handle) {
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
  case TextureFormat::Rgba16Float:
    break; // External Presentation-owned HDR metadata; native triangle allocation is unsupported.
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
  PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties{};
  PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2{};
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
  PFN_vkCreateComputePipelines CreateComputePipelines{};
  PFN_vkDestroyPipeline DestroyPipeline{};
  PFN_vkCmdPipelineBarrier CmdPipelineBarrier{};
  PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer{};
  PFN_vkCmdBeginRenderPass CmdBeginRenderPass{};
  PFN_vkCmdEndRenderPass CmdEndRenderPass{};
  PFN_vkCmdBindPipeline CmdBindPipeline{};
  PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets{};
  PFN_vkCmdSetViewport CmdSetViewport{};
  PFN_vkCmdSetScissor CmdSetScissor{};
  PFN_vkCmdDraw CmdDraw{};
  PFN_vkCmdDrawIndirect CmdDrawIndirect{};
  PFN_vkCreateQueryPool CreateQueryPool{};
  PFN_vkDestroyQueryPool DestroyQueryPool{};
  PFN_vkGetQueryPoolResults GetQueryPoolResults{};
  PFN_vkCmdResetQueryPool CmdResetQueryPool{};
  PFN_vkCmdWriteTimestamp CmdWriteTimestamp{};
  PFN_vkCmdDispatch CmdDispatch{};
};

template <typename Function> Function RequireFunction(Function function, const char *name) {
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
  void TransitionBuffer(const BufferBarrier &barrier) override;
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) override;
  void BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset, std::uint32_t stride) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void DrawIndirect(std::uint32_t command_count) override;
  void Dispatch(std::uint32_t groups_x, std::uint32_t groups_y, std::uint32_t groups_z) override;
  void EndRendering() override;

  [[nodiscard]] bool IsClosed() const noexcept { return !rendering_; }
  [[nodiscard]] bool IsSubmitted() const noexcept { return submitted_; }
  [[nodiscard]] bool BelongsTo(const VulkanDevice &device) const noexcept {
    return &device_ == &device;
  }
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  [[nodiscard]] std::uint64_t IndirectDraws() const noexcept { return indirect_draws_; }
  [[nodiscard]] std::uint64_t Dispatches() const noexcept { return dispatches_; }
  void MarkSubmitted() noexcept { submitted_ = true; }
  void Close();

private:
  friend class VulkanDevice;
  [[nodiscard]] VkDescriptorSet SnapshotComputeBindings();
  static constexpr std::uint32_t ComputeSetsPerPool = 32;
  std::array<BufferHandle, 4> storage_buffers_{};
  std::array<bool, 4> storage_bound_{};
  std::vector<VkDescriptorPool> compute_descriptor_pools_;
  std::uint32_t compute_sets_in_last_pool_{ComputeSetsPerPool};
  VulkanDevice &device_;
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  VkQueryPool timestamps_{VK_NULL_HANDLE};
  struct TextureSnapshot final {
    TextureHandle handle;
    std::uint64_t revision;
    ResourceState state;
    TextureFormat format;
    std::uint32_t width;
    std::uint32_t height;
    VkImage image;
    VkImageView view;
  };
  TextureSnapshot &SnapshotTexture(TextureHandle texture);
  std::unordered_map<std::uint64_t, TextureSnapshot> texture_snapshots_;
  std::vector<VkFramebuffer> framebuffers_;
  bool rendering_{false};
  bool pipeline_bound_{false};
  PipelineType pipeline_type_{PipelineType::Graphics};
  bool submitted_{false};
  std::uint64_t completed_token_{};
  bool closed_{false};
  TextureHandle render_target_{};
  std::uint32_t width_{};
  std::uint32_t height_{};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
  std::uint64_t indirect_draws_{};
  std::uint64_t dispatches_{};
  VkBuffer indirect_buffer_{VK_NULL_HANDLE};
  VkDeviceMemory indirect_memory_{VK_NULL_HANDLE};
  VkBuffer bound_indirect_buffer_{VK_NULL_HANDLE};
  VkDeviceSize bound_indirect_offset_{};
  VkDeviceSize bound_indirect_size_{};
  std::uint32_t bound_indirect_stride_{DrawIndirectArgumentSize};
};

class VulkanDevice final : public Device {
  struct TextureRecord;
  struct BufferRecord;
  struct PipelineRecord;

public:
  explicit VulkanDevice(const DeviceCreationOptions &options);
  ~VulkanDevice() override;

  Backend GetBackend() const noexcept override { return Backend::Vulkan; }
  GraphicsDeviceCapabilities Capabilities() const noexcept override { return capabilities_; }
  TextureHandle CreateTexture(const TextureDescriptor &descriptor) override;
  void DestroyTexture(TextureHandle texture) override;
  BufferHandle CreateBuffer(const BufferDescriptor &descriptor) override;
  void WriteBuffer(BufferHandle buffer, std::uint64_t offset,
                   std::span<const std::byte> data) override;
  void ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                            std::span<std::byte> data) override;
  void ReadTextureForTesting(TextureHandle texture, std::span<std::byte> data) override;
  void DestroyBuffer(BufferHandle buffer) override;
  PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) override;
  void DestroyPipeline(PipelineHandle pipeline) override;
  std::unique_ptr<CommandList> CreateCommandList(QueueType queue) override;
  std::uint64_t Submit(CommandList &commands) override;
  [[nodiscard]] std::uint64_t CompletedSubmissionValue() const noexcept override;
  void WaitForSubmission(std::uint64_t value) override;
  void Present(TextureHandle texture) override;
  void WaitIdle() override;
  [[nodiscard]] DeviceDiagnostics Diagnostics() const noexcept override;

private:
  friend class VulkanCommandList;
  struct CommandAllocation final {
    VkCommandPool pool{VK_NULL_HANDLE};
    VkCommandBuffer commands{VK_NULL_HANDLE};
  };
  CommandAllocation AcquireCommandAllocation();
  void ReleaseCommandAllocation(CommandAllocation allocation, bool reusable) noexcept;
  // Fixed storage: retirement never allocates or grows an unbounded cache.
  std::array<CommandAllocation, 8> idle_command_allocations_{};
  std::size_t idle_command_allocation_count_{};
  void LoadInstanceFunctions();
  void LoadDeviceFunctions();
  void SelectPhysicalDevice();
  void CreateCoreObjects();
  void CreateRenderPasses();
  void CreateUniformResources();
  void LoadShaderModule();
  void TransitionTextureImmediately(TextureRecord &texture, ResourceState state);
  void Require(bool condition, const char *message);
  TextureRecord &ValidateTextureLocked(TextureHandle texture);
  PipelineRecord &ValidatePipeline(PipelineHandle pipeline);
  BufferRecord &ValidateBuffer(BufferHandle buffer);
  VkFramebuffer CreateFramebuffer(VkImageView view, VkRenderPass render_pass, std::uint32_t width,
                                  std::uint32_t height);
  void DestroyFramebuffer(VkFramebuffer framebuffer);
  VkRenderPass RenderPassFor(TextureFormat format) const;
  std::uint32_t FindMemoryType(std::uint32_t type_bits, VkMemoryPropertyFlags properties) const;
  void DestroyTextureRecord(TextureRecord &texture);
  void DestroyBufferRecord(BufferRecord &buffer);
  void DestroyPipelineRecord(PipelineRecord &pipeline);

  VulkanLoader loader_;
  VulkanFunctions functions_{};
  VkInstance instance_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  GraphicsDeviceCapabilities capabilities_;
  bool gpu_timing_requested_{};
  unsigned timestamp_bits_{};
  double timestamp_period_ns_{};
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
  VkShaderModule compute_shader_module_{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkDescriptorSetLayout compute_descriptor_set_layout_{VK_NULL_HANDLE};
  VkPipelineLayout compute_pipeline_layout_{VK_NULL_HANDLE};
  VkRenderPass rgba_render_pass_{VK_NULL_HANDLE};
  VkRenderPass bgra_render_pass_{VK_NULL_HANDLE};
  mutable std::mutex mutex_;
  core::HandlePool<TextureTag> texture_pool_;
  core::HandlePool<BufferTag> buffer_pool_;
  core::HandlePool<PipelineTag> pipeline_pool_;
  std::unordered_map<std::uint64_t, TextureRecord> textures_;
  std::unordered_map<std::uint64_t, BufferRecord> buffers_;
  std::unordered_map<std::uint64_t, PipelineRecord> pipelines_;
  DeviceDiagnostics diagnostics_{};
  std::uint64_t submitted_submission_{};
  std::uint64_t completed_submission_{};
};

struct VulkanDevice::BufferRecord final {
  BufferDescriptor descriptor;
  VkBuffer buffer{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
};

struct VulkanDevice::TextureRecord final {
  TextureDescriptor descriptor;
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkImageView view{VK_NULL_HANDLE};
  ResourceState logical_state{ResourceState::Undefined};
  std::uint64_t revision{};
};

struct VulkanDevice::PipelineRecord final {
  PipelineDescriptor descriptor;
  VkPipeline pipeline{VK_NULL_HANDLE};
  VkRenderPass render_pass{VK_NULL_HANDLE};
};

void VulkanDevice::LoadInstanceFunctions() {
  functions_.DestroyInstance = RequireFunction(
      loader_.Instance<PFN_vkDestroyInstance>(instance_, "vkDestroyInstance"), "vkDestroyInstance");
  functions_.EnumeratePhysicalDevices = RequireFunction(
      loader_.Instance<PFN_vkEnumeratePhysicalDevices>(instance_, "vkEnumeratePhysicalDevices"),
      "vkEnumeratePhysicalDevices");
  functions_.GetPhysicalDeviceQueueFamilyProperties =
      RequireFunction(loader_.Instance<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
                          instance_, "vkGetPhysicalDeviceQueueFamilyProperties"),
                      "vkGetPhysicalDeviceQueueFamilyProperties");
  functions_.GetPhysicalDeviceMemoryProperties =
      RequireFunction(loader_.Instance<PFN_vkGetPhysicalDeviceMemoryProperties>(
                          instance_, "vkGetPhysicalDeviceMemoryProperties"),
                      "vkGetPhysicalDeviceMemoryProperties");
  functions_.GetPhysicalDeviceProperties =
      RequireFunction(loader_.Instance<PFN_vkGetPhysicalDeviceProperties>(
                          instance_, "vkGetPhysicalDeviceProperties"),
                      "vkGetPhysicalDeviceProperties");
  functions_.GetPhysicalDeviceFeatures2 = RequireFunction(
      loader_.Instance<PFN_vkGetPhysicalDeviceFeatures2>(instance_, "vkGetPhysicalDeviceFeatures2"),
      "vkGetPhysicalDeviceFeatures2");
  functions_.CreateDevice = RequireFunction(
      loader_.Instance<PFN_vkCreateDevice>(instance_, "vkCreateDevice"), "vkCreateDevice");
  functions_.GetDeviceProcAddr =
      RequireFunction(loader_.Instance<PFN_vkGetDeviceProcAddr>(instance_, "vkGetDeviceProcAddr"),
                      "vkGetDeviceProcAddr");
}

void VulkanDevice::LoadDeviceFunctions() {
#define LOAD_DEVICE(member, symbol)                                                                \
  functions_.member = RequireFunction(                                                             \
      FunctionCast<decltype(functions_.member)>(functions_.GetDeviceProcAddr(device_, symbol)),    \
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
  LOAD_DEVICE(CreateComputePipelines, "vkCreateComputePipelines");
  LOAD_DEVICE(DestroyPipeline, "vkDestroyPipeline");
  LOAD_DEVICE(CmdPipelineBarrier, "vkCmdPipelineBarrier");
  LOAD_DEVICE(CmdCopyImageToBuffer, "vkCmdCopyImageToBuffer");
  LOAD_DEVICE(CmdBeginRenderPass, "vkCmdBeginRenderPass");
  LOAD_DEVICE(CmdEndRenderPass, "vkCmdEndRenderPass");
  LOAD_DEVICE(CmdBindPipeline, "vkCmdBindPipeline");
  LOAD_DEVICE(CmdBindDescriptorSets, "vkCmdBindDescriptorSets");
  LOAD_DEVICE(CmdSetViewport, "vkCmdSetViewport");
  LOAD_DEVICE(CmdSetScissor, "vkCmdSetScissor");
  LOAD_DEVICE(CmdDraw, "vkCmdDraw");
  LOAD_DEVICE(CmdDrawIndirect, "vkCmdDrawIndirect");
  LOAD_DEVICE(CreateQueryPool, "vkCreateQueryPool");
  LOAD_DEVICE(DestroyQueryPool, "vkDestroyQueryPool");
  LOAD_DEVICE(GetQueryPoolResults, "vkGetQueryPoolResults");
  LOAD_DEVICE(CmdResetQueryPool, "vkCmdResetQueryPool");
  LOAD_DEVICE(CmdWriteTimestamp, "vkCmdWriteTimestamp");
  LOAD_DEVICE(CmdDispatch, "vkCmdDispatch");
#undef LOAD_DEVICE
}

VulkanDevice::VulkanDevice(const DeviceCreationOptions &options)
    : gpu_timing_requested_(options.enable_gpu_timing) {
  capabilities_.recording_resource_reuse_enabled = options.enable_recording_resource_reuse;
  const auto *shader_path = std::getenv("NEXORA_SLANG_SPIRV_PATH");
  if (!shader_path || *shader_path == '\0')
    throw std::runtime_error("NEXORA_SLANG_SPIRV_PATH is required for the Vulkan backend");
  std::ifstream shader_file(shader_path, std::ios::binary | std::ios::ate);
  if (!shader_file)
    throw std::runtime_error(std::string("cannot open Vulkan shader artifact: ") + shader_path);
  functions_.GetInstanceProcAddr = RequireFunction(
      loader_.Global<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"), "vkGetInstanceProcAddr");
  const VkApplicationInfo application_info{VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                           nullptr,
                                           "Nexora",
                                           VK_MAKE_VERSION(0, 1, 0),
                                           "Nexora",
                                           VK_MAKE_VERSION(0, 1, 0),
                                           VK_API_VERSION_1_1};
  const VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                           nullptr,
                                           0,
                                           &application_info,
                                           0,
                                           nullptr,
                                           0,
                                           nullptr};
  const auto create_instance =
      RequireFunction(loader_.Global<PFN_vkCreateInstance>("vkCreateInstance"), "vkCreateInstance");
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
    VkPhysicalDeviceProperties properties{};
    functions_.GetPhysicalDeviceProperties(candidate, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_1)
      continue;
    VkPhysicalDeviceShaderDrawParametersFeatures draw_parameters{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, nullptr, VK_FALSE};
    VkPhysicalDeviceFeatures2 features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &draw_parameters, {}};
    functions_.GetPhysicalDeviceFeatures2(candidate, &features);
    if (!draw_parameters.shaderDrawParameters || !features.features.multiDrawIndirect)
      continue;
    std::uint32_t queue_count = 0;
    functions_.GetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queue_count);
    functions_.GetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());
    for (std::uint32_t index = 0; index < queue_count; ++index) {
      if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
        physical_device_ = candidate;
        graphics_queue_family_ = index;
        capabilities_.backend = Backend::Vulkan;
        capabilities_.software_device = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
        capabilities_.vendor_id = properties.vendorID;
        capabilities_.device_id = properties.deviceID;
        capabilities_.driver_version = properties.driverVersion;
        capabilities_.dxil_shader_model_support = CapabilitySupport::Unsupported;
        const bool compute = (queues[index].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
        capabilities_.independent_graphics_recording = true;
        capabilities_.shared_buffer_read_recording = true;
        timestamp_bits_ = queues[index].timestampValidBits;
        timestamp_period_ns_ = properties.limits.timestampPeriod;
        capabilities_.gpu_timing_support = timestamp_bits_ == 0 ? CapabilitySupport::Unsupported
                                           : timestamp_bits_ <= 64 &&
                                                   std::isfinite(timestamp_period_ns_) &&
                                                   timestamp_period_ns_ > 0
                                               ? CapabilitySupport::Supported
                                               : CapabilitySupport::Unknown;
        capabilities_.gpu_timing_enabled =
            gpu_timing_requested_ &&
            capabilities_.gpu_timing_support == CapabilitySupport::Supported;
        capabilities_.independent_compute_recording = compute;
        capabilities_.features[static_cast<std::size_t>(GraphicsFeature::Compute)] = {
            compute ? CapabilitySupport::Supported : CapabilitySupport::Unsupported,
            compute ? FeatureVersion{1, 0} : FeatureVersion{}, compute, true};
        capabilities_.features[static_cast<std::size_t>(GraphicsFeature::IndirectDraw)] = {
            CapabilitySupport::Supported, {1, 0}, true, true};
        const bool tessellation = features.features.tessellationShader == VK_TRUE;
        capabilities_.features[static_cast<std::size_t>(GraphicsFeature::Tessellation)] = {
            tessellation ? CapabilitySupport::Supported : CapabilitySupport::Unsupported,
            tessellation ? FeatureVersion{1, 0} : FeatureVersion{}, false, false};
        capabilities_
            .features[static_cast<std::size_t>(GraphicsFeature::ParallelCommandRecording)] = {
            CapabilitySupport::Supported, {1, 0}, false, false};
        return;
      }
    }
  }
  throw std::runtime_error("Vulkan requires a Vulkan 1.1 graphics device with shaderDrawParameters "
                           "and multiDrawIndirect");
}

void VulkanDevice::CreateCoreObjects() {
  constexpr float priority = 1.0F;
  const VkDeviceQueueCreateInfo queue_info{
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, graphics_queue_family_, 1, &priority};
  // Slang emits SPIR-V 1.3 with DrawParameters for the canonical native shaders.
  const VkPhysicalDeviceShaderDrawParametersFeatures draw_parameters{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, nullptr, VK_TRUE};
  VkPhysicalDeviceFeatures features{};
  features.multiDrawIndirect = VK_TRUE;
  const VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                       &draw_parameters,
                                       0,
                                       1,
                                       &queue_info,
                                       0,
                                       nullptr,
                                       0,
                                       nullptr,
                                       &features};
  Check(functions_.CreateDevice(physical_device_, &device_info, nullptr, &device_),
        "vkCreateDevice");
  LoadDeviceFunctions();
  functions_.GetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);

  const VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
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
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    const VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                  nullptr,
                                                  0,
                                                  1,
                                                  &attachment,
                                                  1,
                                                  &subpass,
                                                  1,
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
  const VkMemoryAllocateInfo allocate_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
      FindMemoryType(requirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
  Check(functions_.AllocateMemory(device_, &allocate_info, nullptr, &uniform_memory_),
        "vkAllocateMemory(uniform)");
  Check(functions_.BindBufferMemory(device_, uniform_buffer_, uniform_memory_, 0),
        "vkBindBufferMemory(uniform)");
  void *mapped = nullptr;
  Check(functions_.MapMemory(device_, uniform_memory_, 0, 64, 0, &mapped), "vkMapMemory(uniform)");
  const float identity[16] = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  std::memcpy(mapped, identity, sizeof(identity));
  functions_.UnmapMemory(device_, uniform_memory_);

  const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                             VK_SHADER_STAGE_VERTEX_BIT, nullptr};
  const VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &binding};
  Check(
      functions_.CreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set_layout_),
      "vkCreateDescriptorSetLayout");
  const VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
  const VkDescriptorPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &pool_size};
  Check(functions_.CreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
        "vkCreateDescriptorPool");
  const VkDescriptorSetAllocateInfo set_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             nullptr, descriptor_pool_, 1, &descriptor_set_layout_};
  Check(functions_.AllocateDescriptorSets(device_, &set_info, &descriptor_set_),
        "vkAllocateDescriptorSets");
  const VkDescriptorBufferInfo descriptor_buffer{uniform_buffer_, 0, 64};
  const VkWriteDescriptorSet write{
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set_,    0,      0, 1,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      nullptr, &descriptor_buffer, nullptr};
  functions_.UpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      nullptr,
      0,
      1,
      &descriptor_set_layout_,
      0,
      nullptr};
  Check(functions_.CreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout_),
        "vkCreatePipelineLayout");

  VkDescriptorSetLayoutBinding storage_bindings[4]{};
  for (std::uint32_t index = 0; index < 4; ++index) {
    storage_bindings[index].binding = index;
    storage_bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storage_bindings[index].descriptorCount = 1;
    storage_bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  }
  const VkDescriptorSetLayoutCreateInfo compute_layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 4, storage_bindings};
  Check(functions_.CreateDescriptorSetLayout(device_, &compute_layout_info, nullptr,
                                             &compute_descriptor_set_layout_),
        "vkCreateDescriptorSetLayout(compute)");
  const VkPipelineLayoutCreateInfo compute_pipeline_layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      nullptr,
      0,
      1,
      &compute_descriptor_set_layout_,
      0,
      nullptr};
  Check(functions_.CreatePipelineLayout(device_, &compute_pipeline_layout_info, nullptr,
                                        &compute_pipeline_layout_),
        "vkCreatePipelineLayout(compute)");
}

void VulkanDevice::LoadShaderModule() {
  const auto load = [this](const char *environment, VkShaderModule &module) {
    const auto *path = std::getenv(environment);
    if (!path || *path == '\0')
      throw std::runtime_error(std::string(environment) + " is required for the Vulkan backend");
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
      throw std::runtime_error(std::string("cannot open Vulkan shader artifact: ") + path);
    const auto size = file.tellg();
    if (size <= 0 || (size % static_cast<std::streamoff>(sizeof(std::uint32_t))) != 0)
      throw std::runtime_error("Vulkan shader artifact has an invalid size");
    std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(words.data()), size);
    if (!file || words.empty() || words.front() != 0x07230203U)
      throw std::runtime_error("Vulkan shader artifact has an invalid SPIR-V magic");
    const VkShaderModuleCreateInfo module_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr,
                                               0, words.size() * sizeof(std::uint32_t),
                                               words.data()};
    Check(functions_.CreateShaderModule(device_, &module_info, nullptr, &module),
          "vkCreateShaderModule");
  };
  load("NEXORA_SLANG_SPIRV_PATH", shader_module_);
  load("NEXORA_SLANG_COMPUTE_SPIRV_PATH", compute_shader_module_);
}

void VulkanDevice::TransitionTextureImmediately(TextureRecord &texture, ResourceState state) {
  if (state == ResourceState::Undefined)
    return;
  const VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                  nullptr, immediate_command_pool_,
                                                  VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  Check(functions_.AllocateCommandBuffers(device_, &allocate_info, &command_buffer),
        "vkAllocateCommandBuffers(texture transition)");
  const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
  VkFence fence = VK_NULL_HANDLE;
  try {
    Check(functions_.BeginCommandBuffer(command_buffer, &begin_info),
          "vkBeginCommandBuffer(texture transition)");
    const auto destination = StageFor(state);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
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
    Check(functions_.EndCommandBuffer(command_buffer), "vkEndCommandBuffer(texture transition)");
    const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    Check(functions_.CreateFence(device_, &fence_info, nullptr, &fence),
          "vkCreateFence(texture transition)");
    const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   nullptr,
                                   0,
                                   nullptr,
                                   nullptr,
                                   1,
                                   &command_buffer,
                                   0,
                                   nullptr};
    Check(functions_.QueueSubmit(graphics_queue_, 1, &submit_info, fence),
          "vkQueueSubmit(texture transition)");
    Check(functions_.WaitForFences(device_, 1, &fence, VK_TRUE,
                                   std::numeric_limits<std::uint64_t>::max()),
          "vkWaitForFences(texture transition)");
    functions_.DestroyFence(device_, fence, nullptr);
  } catch (...) {
    if (fence != VK_NULL_HANDLE)
      functions_.DestroyFence(device_, fence, nullptr);
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
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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
  try {
    Check(functions_.CreateImage(device_, &image_info, nullptr, &record.image), "vkCreateImage");
    VkMemoryRequirements requirements{};
    functions_.GetImageMemoryRequirements(device_, record.image, &requirements);
    const VkMemoryAllocateInfo allocate_info{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
        FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    Check(functions_.AllocateMemory(device_, &allocate_info, nullptr, &record.memory),
          "vkAllocateMemory(image)");
    Check(functions_.BindImageMemory(device_, record.image, record.memory, 0), "vkBindImageMemory");
    const VkImageViewCreateInfo view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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

BufferHandle VulkanDevice::CreateBuffer(const BufferDescriptor &descriptor) {
  if (descriptor.size == 0)
    throw std::invalid_argument("buffer size must be non-zero");
  BufferRecord record;
  record.descriptor = descriptor;
  const VkBufferCreateInfo info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      nullptr,
      0,
      descriptor.size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0,
      nullptr};
  try {
    Check(functions_.CreateBuffer(device_, &info, nullptr, &record.buffer), "vkCreateBuffer");
    VkMemoryRequirements requirements{};
    functions_.GetBufferMemoryRequirements(device_, record.buffer, &requirements);
    const VkMemoryAllocateInfo allocation{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
        FindMemoryType(requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    Check(functions_.AllocateMemory(device_, &allocation, nullptr, &record.memory),
          "vkAllocateMemory(buffer)");
    Check(functions_.BindBufferMemory(device_, record.buffer, record.memory, 0),
          "vkBindBufferMemory");
  } catch (...) {
    DestroyBufferRecord(record);
    throw;
  }
  std::lock_guard lock{mutex_};
  const auto handle = buffer_pool_.Create();
  buffers_.emplace(Key(handle), std::move(record));
  return handle;
}

void VulkanDevice::WriteBuffer(BufferHandle buffer, std::uint64_t offset,
                               std::span<const std::byte> data) {
  auto &record = ValidateBuffer(buffer);
  Require(offset <= record.descriptor.size && data.size() <= record.descriptor.size - offset,
          "Vulkan buffer write is out of bounds");
  void *mapped = nullptr;
  Check(functions_.MapMemory(device_, record.memory, offset, data.size(), 0, &mapped),
        "vkMapMemory(buffer write)");
  std::memcpy(mapped, data.data(), data.size());
  functions_.UnmapMemory(device_, record.memory);
}

void VulkanDevice::ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                                        std::span<std::byte> data) {
  auto &record = ValidateBuffer(buffer);
  Require(offset <= record.descriptor.size && data.size() <= record.descriptor.size - offset,
          "Vulkan buffer read is out of bounds");
  void *mapped = nullptr;
  Check(functions_.MapMemory(device_, record.memory, offset, data.size(), 0, &mapped),
        "vkMapMemory(buffer read)");
  std::memcpy(data.data(), mapped, data.size());
  functions_.UnmapMemory(device_, record.memory);
  ++diagnostics_.readbacks;
}

void VulkanDevice::ReadTextureForTesting(TextureHandle texture, std::span<std::byte> data) {
  WaitIdle();
  VkImage image{VK_NULL_HANDLE};
  ResourceState state{};
  TextureDescriptor descriptor;
  {
    std::lock_guard lock{mutex_};
    const auto found = textures_.find(Key(texture));
    Require(texture_pool_.Contains(texture) && found != textures_.end(),
            "reading invalid Vulkan texture");
    image = found->second.image;
    state = found->second.logical_state;
    descriptor = found->second.descriptor;
  }
  Require(descriptor.format != TextureFormat::Depth32Float,
          "depth texture readback is unsupported");
  Require(state != ResourceState::Undefined, "reading a Vulkan texture with undefined contents");
  const auto byte_count =
      static_cast<VkDeviceSize>(descriptor.width) * descriptor.height * std::uint64_t{4};
  Require(data.size() == byte_count, "texture readback size must be width * height * 4");

  VkBuffer staging{VK_NULL_HANDLE};
  VkDeviceMemory staging_memory{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  VkFence fence{VK_NULL_HANDLE};
  const auto release = [&] {
    if (fence != VK_NULL_HANDLE)
      functions_.DestroyFence(device_, fence, nullptr);
    if (command_buffer != VK_NULL_HANDLE)
      functions_.FreeCommandBuffers(device_, immediate_command_pool_, 1, &command_buffer);
    if (staging != VK_NULL_HANDLE)
      functions_.DestroyBuffer(device_, staging, nullptr);
    if (staging_memory != VK_NULL_HANDLE)
      functions_.FreeMemory(device_, staging_memory, nullptr);
  };
  try {
    const VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                         nullptr,
                                         0,
                                         byte_count,
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VK_SHARING_MODE_EXCLUSIVE,
                                         0,
                                         nullptr};
    Check(functions_.CreateBuffer(device_, &buffer_info, nullptr, &staging),
          "vkCreateBuffer(readback)");
    VkMemoryRequirements requirements{};
    functions_.GetBufferMemoryRequirements(device_, staging, &requirements);
    const VkMemoryAllocateInfo allocation{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
        FindMemoryType(requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    Check(functions_.AllocateMemory(device_, &allocation, nullptr, &staging_memory),
          "vkAllocateMemory(readback)");
    Check(functions_.BindBufferMemory(device_, staging, staging_memory, 0),
          "vkBindBufferMemory(readback)");

    const VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                    nullptr, immediate_command_pool_,
                                                    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    Check(functions_.AllocateCommandBuffers(device_, &allocate_info, &command_buffer),
          "vkAllocateCommandBuffers(readback)");
    const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                              VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    Check(functions_.BeginCommandBuffer(command_buffer, &begin_info),
          "vkBeginCommandBuffer(readback)");

    const auto original = StageFor(state);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = original.access;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = ToLayout(state);
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    functions_.CmdPipelineBarrier(command_buffer, original.stages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0, 0, nullptr, 0, nullptr, 1, &barrier);
    const VkBufferImageCopy region{0,         0,
                                   0,         {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                   {0, 0, 0}, {descriptor.width, descriptor.height, 1}};
    functions_.CmdCopyImageToBuffer(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    staging, 1, &region);
    // Restore the layout the render graph last recorded, so the readback leaves no trace.
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = original.access;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = ToLayout(state);
    functions_.CmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, original.stages,
                                  0, 0, nullptr, 0, nullptr, 1, &barrier);
    Check(functions_.EndCommandBuffer(command_buffer), "vkEndCommandBuffer(readback)");
    const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    Check(functions_.CreateFence(device_, &fence_info, nullptr, &fence), "vkCreateFence(readback)");
    const VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   nullptr,
                                   0,
                                   nullptr,
                                   nullptr,
                                   1,
                                   &command_buffer,
                                   0,
                                   nullptr};
    Check(functions_.QueueSubmit(graphics_queue_, 1, &submit_info, fence),
          "vkQueueSubmit(readback)");
    {
      std::lock_guard lock{mutex_};
      ++ValidateTextureLocked(texture).revision;
    }
    Check(functions_.WaitForFences(device_, 1, &fence, VK_TRUE,
                                   std::numeric_limits<std::uint64_t>::max()),
          "vkWaitForFences(readback)");
    void *mapped = nullptr;
    Check(functions_.MapMemory(device_, staging_memory, 0, byte_count, 0, &mapped),
          "vkMapMemory(readback)");
    std::memcpy(data.data(), mapped, data.size());
    functions_.UnmapMemory(device_, staging_memory);
  } catch (...) {
    release();
    throw;
  }
  release();
  std::lock_guard lock{mutex_};
  ++diagnostics_.readbacks;
}

void VulkanDevice::DestroyBufferRecord(BufferRecord &buffer) {
  if (buffer.buffer != VK_NULL_HANDLE)
    functions_.DestroyBuffer(device_, buffer.buffer, nullptr);
  if (buffer.memory != VK_NULL_HANDLE)
    functions_.FreeMemory(device_, buffer.memory, nullptr);
  buffer.buffer = VK_NULL_HANDLE;
  buffer.memory = VK_NULL_HANDLE;
}

void VulkanDevice::DestroyBuffer(BufferHandle buffer) {
  std::lock_guard lock{mutex_};
  const auto found = buffers_.find(Key(buffer));
  Require(buffer_pool_.Contains(buffer) && found != buffers_.end(),
          "destroying invalid Vulkan buffer");
  DestroyBufferRecord(found->second);
  buffers_.erase(found);
  Require(buffer_pool_.Destroy(buffer), "destroying stale Vulkan buffer");
}

PipelineHandle VulkanDevice::CreatePipeline(const PipelineDescriptor &descriptor) {
  RequireGraphicsCapabilities(Capabilities(), descriptor.graphics_requirements);
  if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0)
    throw std::invalid_argument("pipeline hashes must be non-zero");
  if (descriptor.type == PipelineType::Compute) {
    const VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                nullptr,
                                                0,
                                                VK_SHADER_STAGE_COMPUTE_BIT,
                                                compute_shader_module_,
                                                "computeMain",
                                                nullptr};
    const VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                           nullptr,
                                           0,
                                           stage,
                                           compute_pipeline_layout_,
                                           VK_NULL_HANDLE,
                                           -1};
    PipelineRecord record;
    record.descriptor = descriptor;
    Check(functions_.CreateComputePipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr,
                                            &record.pipeline),
          "vkCreateComputePipelines");
    std::lock_guard lock{mutex_};
    const auto handle = pipeline_pool_.Create();
    pipelines_.emplace(Key(handle), std::move(record));
    return handle;
  }
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
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      nullptr,
      0,
      0,
      nullptr,
      0,
      nullptr};
  const VkPipelineInputAssemblyStateCreateInfo input_assembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
  VkPipelineViewportStateCreateInfo viewport_state{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0F;
  const VkPipelineMultisampleStateCreateInfo multisample{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      nullptr,
      0,
      VK_SAMPLE_COUNT_1_BIT,
      VK_FALSE,
      1.0F,
      nullptr,
      VK_FALSE,
      VK_FALSE};
  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.blendEnable = VK_FALSE;
  blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  const VkPipelineColorBlendStateCreateInfo blend{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      nullptr,
      0,
      VK_FALSE,
      VK_LOGIC_OP_COPY,
      1,
      &blend_attachment,
      {0.0F, 0.0F, 0.0F, 0.0F}};
  const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamic_state{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dynamic_states};
  const VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
  // This device selects a single queue family that supports both graphics and
  // present (see SelectPhysicalDevice), which on every host this backend
  // targets also supports compute; VulkanCommandList's command pool is
  // created against that same family regardless of `queue`. There is no
  // separate transfer-only queue family selected, so QueueType::Copy is
  // genuinely unsupported today (V2-M3's open "native queue/timeline
  // integration" gate), unlike Compute.
  if (queue != QueueType::Graphics && queue != QueueType::Compute)
    throw std::invalid_argument("Vulkan backend only supports the graphics/compute queue");
  return std::make_unique<VulkanCommandList>(*this, queue);
}

VkRenderPass VulkanDevice::RenderPassFor(TextureFormat format) const {
  switch (format) {
  case TextureFormat::Rgba8Unorm:
    return rgba_render_pass_;
  case TextureFormat::Bgra8Unorm:
    return bgra_render_pass_;
  case TextureFormat::Depth32Float:
  case TextureFormat::Rgba16Float:
    break;
  }
  throw std::invalid_argument("Vulkan render pass requires a color format");
}

VkFramebuffer VulkanDevice::CreateFramebuffer(VkImageView view, VkRenderPass render_pass,
                                              std::uint32_t width, std::uint32_t height) {
  const VkImageView attachment = view;
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

VulkanDevice::TextureRecord &VulkanDevice::ValidateTextureLocked(TextureHandle texture) {
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "command list references invalid Vulkan texture");
  return found->second;
}

VulkanDevice::PipelineRecord &VulkanDevice::ValidatePipeline(PipelineHandle pipeline) {
  std::lock_guard lock{mutex_};
  const auto found = pipelines_.find(Key(pipeline));
  Require(pipeline_pool_.Contains(pipeline) && found != pipelines_.end(),
          "binding invalid Vulkan pipeline");
  return found->second;
}

VulkanDevice::BufferRecord &VulkanDevice::ValidateBuffer(BufferHandle buffer) {
  std::lock_guard lock{mutex_};
  const auto found = buffers_.find(Key(buffer));
  Require(buffer_pool_.Contains(buffer) && found != buffers_.end(),
          "binding invalid Vulkan buffer");
  return found->second;
}

std::uint64_t VulkanDevice::Submit(CommandList &commands) {
  auto *validated = dynamic_cast<VulkanCommandList *>(&commands);
  VkFence fence = VK_NULL_HANDLE;
  std::chrono::steady_clock::time_point submitted_at{};
  {
    std::lock_guard lock{mutex_};
    Require(validated != nullptr, "command list belongs to another device");
    Require(validated->BelongsTo(*this), "command list belongs to another device");
    Require(validated->IsClosed(), "cannot submit an open command list");
    Require(!validated->IsSubmitted(), "command list was already submitted");
    for (const auto &entry : validated->texture_snapshots_) {
      const auto &snapshot = entry.second;
      const auto &texture = ValidateTextureLocked(snapshot.handle);
      Require(texture.revision == snapshot.revision, "stale Vulkan texture recording snapshot");
    }
    validated->Close();
    const VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    Check(functions_.CreateFence(device_, &fence_info, nullptr, &fence), "vkCreateFence(submit)");
    const VkSubmitInfo submit_info{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0,      nullptr, nullptr, 1,
        &validated->command_buffer_,   0,       nullptr};
    try {
      if (capabilities_.gpu_timing_enabled)
        submitted_at = std::chrono::steady_clock::now();
      Check(functions_.QueueSubmit(graphics_queue_, 1, &submit_info, fence), "vkQueueSubmit");
    } catch (...) {
      functions_.DestroyFence(device_, fence, nullptr);
      throw;
    }
    // Commit only after QueueSubmit accepts the list. Abandoning recording changes nothing.
    for (const auto &entry : validated->texture_snapshots_) {
      auto &texture = textures_.at(entry.first);
      texture.logical_state = entry.second.state;
      ++texture.revision;
    }
    validated->MarkSubmitted();
    ++diagnostics_.submitted_command_lists;
    diagnostics_.barriers += validated->Barriers();
    diagnostics_.draw_calls += validated->DrawCalls();
    diagnostics_.indirect_draw_calls += validated->IndirectDraws();
    diagnostics_.compute_dispatches += validated->Dispatches();
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
  std::optional<double> measured;
  if (capabilities_.gpu_timing_enabled && validated->timestamps_ != VK_NULL_HANDLE) {
    // Query availability is read only after the existing completion fence; never add WAIT.
    std::array<std::uint64_t, 4> values{};
    const auto result = functions_.GetQueryPoolResults(
        device_, validated->timestamps_, 0, 2, sizeof(values), values.data(),
        2 * sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (result == VK_SUCCESS && values[1] && values[3])
      measured =
          detail::TimestampMilliseconds(values[0], values[2], timestamp_bits_, timestamp_period_ns_,
                                        detail::CompletionWallMilliseconds(submitted_at));
  }
  for (const auto framebuffer : validated->framebuffers_)
    DestroyFramebuffer(framebuffer);
  validated->framebuffers_.clear();
  std::lock_guard lock{mutex_};
  completed_submission_ = ++submitted_submission_;
  validated->completed_token_ = completed_submission_;
  if (capabilities_.gpu_timing_enabled)
    detail::PublishGpuTiming(diagnostics_.gpu_timing, GpuTimingSource::VulkanTimestamps,
                             submitted_submission_, measured);
  return submitted_submission_;
}

std::uint64_t VulkanDevice::CompletedSubmissionValue() const noexcept {
  std::lock_guard lock{mutex_};
  return completed_submission_;
}

void VulkanDevice::WaitForSubmission(std::uint64_t value) {
  std::lock_guard lock{mutex_};
  Require(value <= submitted_submission_, "waiting for an unknown Vulkan submission");
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
  for (std::size_t index = 0; index < idle_command_allocation_count_; ++index)
    functions_.DestroyCommandPool(device_, idle_command_allocations_[index].pool, nullptr);
  idle_command_allocation_count_ = 0;
  for (auto &[key, pipeline] : pipelines_)
    (void)key, DestroyPipelineRecord(pipeline);
  for (auto &[key, texture] : textures_)
    (void)key, DestroyTextureRecord(texture);
  for (auto &[key, buffer] : buffers_)
    (void)key, DestroyBufferRecord(buffer);
  if (shader_module_ != VK_NULL_HANDLE)
    functions_.DestroyShaderModule(device_, shader_module_, nullptr);
  if (compute_shader_module_ != VK_NULL_HANDLE)
    functions_.DestroyShaderModule(device_, compute_shader_module_, nullptr);
  if (pipeline_layout_ != VK_NULL_HANDLE)
    functions_.DestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  if (compute_pipeline_layout_ != VK_NULL_HANDLE)
    functions_.DestroyPipelineLayout(device_, compute_pipeline_layout_, nullptr);
  if (compute_descriptor_set_layout_ != VK_NULL_HANDLE)
    functions_.DestroyDescriptorSetLayout(device_, compute_descriptor_set_layout_, nullptr);
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

VulkanDevice::CommandAllocation VulkanDevice::AcquireCommandAllocation() {
  {
    std::lock_guard lock{mutex_};
    if (capabilities_.recording_resource_reuse_enabled && idle_command_allocation_count_) {
      auto allocation = idle_command_allocations_[--idle_command_allocation_count_];
      idle_command_allocations_[idle_command_allocation_count_] = {};
      diagnostics_.cached_command_pools = idle_command_allocation_count_;
      ++diagnostics_.command_pool_reuses;
      return allocation;
    }
  }
  CommandAllocation allocation;
  const VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                     graphics_queue_family_};
  Check(functions_.CreateCommandPool(device_, &info, nullptr, &allocation.pool),
        "vkCreateCommandPool(command list)");
  const VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                             nullptr, allocation.pool,
                                             VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
  try {
    Check(functions_.AllocateCommandBuffers(device_, &commands, &allocation.commands),
          "vkAllocateCommandBuffers(command list)");
  } catch (...) {
    functions_.DestroyCommandPool(device_, allocation.pool, nullptr);
    throw;
  }
  std::lock_guard lock{mutex_};
  ++diagnostics_.command_pool_creations;
  return allocation;
}

void VulkanDevice::ReleaseCommandAllocation(CommandAllocation allocation, bool reusable) noexcept {
  if (allocation.pool == VK_NULL_HANDLE)
    return;
  if (reusable && capabilities_.recording_resource_reuse_enabled &&
      functions_.ResetCommandPool(device_, allocation.pool, 0) == VK_SUCCESS) {
    // Reset drops every recorded reference before descriptors, query pools and framebuffers die.
    // The pool is exclusively list-owned until this point, even when its fence completed earlier.
    std::lock_guard lock{mutex_};
    if (idle_command_allocation_count_ < idle_command_allocations_.size()) {
      idle_command_allocations_[idle_command_allocation_count_++] = allocation;
      diagnostics_.cached_command_pools = idle_command_allocation_count_;
      return;
    }
  }
  if (allocation.commands != VK_NULL_HANDLE)
    functions_.FreeCommandBuffers(device_, allocation.pool, 1, &allocation.commands);
  functions_.DestroyCommandPool(device_, allocation.pool, nullptr);
}

VulkanCommandList::VulkanCommandList(VulkanDevice &device, QueueType queue) : device_(device) {
  (void)queue;
  const auto allocation = device_.AcquireCommandAllocation();
  command_pool_ = allocation.pool;
  command_buffer_ = allocation.commands;
  try {
    const VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                              VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    Check(device_.functions_.BeginCommandBuffer(command_buffer_, &begin_info),
          "vkBeginCommandBuffer(command list)");
    if (device_.capabilities_.gpu_timing_enabled) {
      const VkQueryPoolCreateInfo query_info{
          VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_TIMESTAMP, 2, 0};
      VkQueryPool queries = VK_NULL_HANDLE;
      if (device_.functions_.CreateQueryPool(device_.device_, &query_info, nullptr, &queries) ==
          VK_SUCCESS) {
        timestamps_ = queries;
        device_.functions_.CmdResetQueryPool(command_buffer_, timestamps_, 0, 2);
        device_.functions_.CmdWriteTimestamp(command_buffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                             timestamps_, 0);
      }
    }
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
  // A successful Submit sets completed_token_ only after its fence returns. Accepted but failed
  // submissions stay ineligible even if the caller later drains them. Never reset in-flight work.
  const bool reusable = !submitted_ || (completed_token_ != 0 &&
                                        device_.CompletedSubmissionValue() >= completed_token_);
  device_.ReleaseCommandAllocation({command_pool_, command_buffer_}, reusable);
  command_pool_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
  if (timestamps_ != VK_NULL_HANDLE)
    device_.functions_.DestroyQueryPool(device_.device_, timestamps_, nullptr);
  for (const auto framebuffer : framebuffers_)
    device_.DestroyFramebuffer(framebuffer);
  for (const auto pool : compute_descriptor_pools_)
    device_.functions_.DestroyDescriptorPool(device_.device_, pool, nullptr);
  if (indirect_buffer_ != VK_NULL_HANDLE)
    device_.functions_.DestroyBuffer(device_.device_, indirect_buffer_, nullptr);
  if (indirect_memory_ != VK_NULL_HANDLE)
    device_.functions_.FreeMemory(device_.device_, indirect_memory_, nullptr);
}

VulkanCommandList::TextureSnapshot &VulkanCommandList::SnapshotTexture(TextureHandle handle) {
  std::lock_guard lock{device_.mutex_};
  const auto &texture = device_.ValidateTextureLocked(handle);
  auto &snapshot =
      texture_snapshots_
          .try_emplace(Key(handle),
                       TextureSnapshot{handle, texture.revision, texture.logical_state,
                                       texture.descriptor.format, texture.descriptor.width,
                                       texture.descriptor.height, texture.image, texture.view})
          .first->second;
  device_.Require(snapshot.revision == texture.revision,
                  "Vulkan texture changed during command recording");
  return snapshot;
}

void VulkanCommandList::Transition(const Barrier &barrier) {
  if (submitted_ || closed_)
    throw std::logic_error("cannot record a submitted command list");
  if (rendering_)
    throw std::logic_error("Vulkan barriers cannot occur inside rendering");
  if (!IsValidQueue(barrier.source_queue) || !IsValidQueue(barrier.destination_queue) ||
      barrier.after == ResourceState::Undefined)
    throw std::invalid_argument("invalid Vulkan texture barrier");
  const auto source = StageFor(barrier.before);
  const auto destination = StageFor(barrier.after);
  const auto old_layout = ToLayout(barrier.before);
  const auto new_layout = ToLayout(barrier.after);
  auto &texture = SnapshotTexture(barrier.texture);
  if (texture.state != barrier.before) {
    std::lock_guard lock{device_.mutex_};
    device_.Require(false, "Vulkan barrier before-state mismatch");
  }
  if (barrier.before == barrier.after)
    return;
  texture.state = barrier.after;
  VkImageMemoryBarrier native{};
  native.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  native.srcAccessMask = source.access;
  native.dstAccessMask = destination.access;
  native.oldLayout = old_layout;
  native.newLayout = new_layout;
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

void VulkanCommandList::TransitionBuffer(const BufferBarrier &barrier) {
  if (submitted_ || closed_ || rendering_)
    throw std::logic_error("buffer barriers require an open command list outside rendering");
  if (!IsValidBufferBarrier(barrier))
    throw std::invalid_argument("invalid buffer barrier");
  auto &buffer = device_.ValidateBuffer(barrier.buffer);
  const auto scope = [](BufferAccess access) -> StageAccess {
    switch (access) {
    case BufferAccess::Undefined:
      return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
              VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    case BufferAccess::ShaderRead:
      return {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
              VK_ACCESS_SHADER_READ_BIT};
    case BufferAccess::StorageReadWrite:
      return {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
    case BufferAccess::IndirectRead:
      return {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    }
    throw std::invalid_argument("invalid buffer access");
  };
  const auto source = scope(barrier.before);
  const auto destination = scope(barrier.after);
  VkBufferMemoryBarrier native{};
  native.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  native.srcAccessMask = source.access;
  native.dstAccessMask = destination.access;
  // All logical queues currently share one native family and synchronous submission.
  native.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  native.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  native.buffer = buffer.buffer;
  native.size = VK_WHOLE_SIZE;
  device_.functions_.CmdPipelineBarrier(command_buffer_, source.stages, destination.stages, 0, 0,
                                        nullptr, 1, &native, 0, nullptr);
  ++barriers_;
}

void VulkanCommandList::BeginRendering(const RenderingInfo &info) {
  if (submitted_ || closed_ || rendering_ || info.width == 0 || info.height == 0)
    throw std::logic_error("invalid Vulkan BeginRendering");
  auto &target = SnapshotTexture(info.color_target);
  if (target.state != ResourceState::RenderTarget) {
    std::lock_guard lock{device_.mutex_};
    device_.Require(false, "Vulkan render target is not in RenderTarget state");
  }
  if (info.width > target.width || info.height > target.height)
    throw std::logic_error("Vulkan render area exceeds the target extent");
  const auto render_pass = device_.RenderPassFor(target.format);
  const auto framebuffer =
      device_.CreateFramebuffer(target.view, render_pass, info.width, info.height);
  try {
    framebuffers_.push_back(framebuffer);
  } catch (...) {
    device_.DestroyFramebuffer(framebuffer);
    throw;
  }
  VkClearValue clear{};
  clear.color.float32[3] = 1.0F;
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = render_pass;
  begin.framebuffer = framebuffer;
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
  if (submitted_ || closed_)
    throw std::logic_error("cannot bind a Vulkan pipeline after submission");
  auto &record = device_.ValidatePipeline(pipeline);
  if (record.descriptor.type == PipelineType::Compute) {
    if (rendering_)
      throw std::logic_error("Vulkan compute pipeline cannot be bound during rendering");
    device_.functions_.CmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                                       record.pipeline);
    pipeline_bound_ = true;
    pipeline_type_ = PipelineType::Compute;
    return;
  }
  if (!rendering_)
    throw std::logic_error("Vulkan graphics pipeline binding requires rendering");
  const auto &target = SnapshotTexture(render_target_);
  if (record.render_pass != device_.RenderPassFor(target.format)) {
    std::lock_guard lock{device_.mutex_};
    device_.Require(false, "Vulkan pipeline format does not match render target");
  }
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
  pipeline_type_ = PipelineType::Graphics;
}

void VulkanCommandList::BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) {
  if (submitted_ || closed_ || rendering_ || binding >= 4)
    throw std::logic_error("invalid Vulkan storage-buffer binding");
  (void)device_.ValidateBuffer(buffer);
  storage_buffers_[binding] = buffer;
  storage_bound_[binding] = true;
}

VkDescriptorSet VulkanCommandList::SnapshotComputeBindings() {
  // A recorded dispatch owns its descriptor contents. Updating a previously bound set would
  // change earlier dispatches and invalidate command buffers without update-after-bind support.
  std::array<VkDescriptorBufferInfo, 4> buffers{};
  for (std::size_t index = 0; index < buffers.size(); ++index) {
    if (!storage_bound_[index])
      throw std::logic_error("Vulkan dispatch requires all four storage-buffer bindings");
    const auto &record = device_.ValidateBuffer(storage_buffers_[index]);
    buffers[index] = {record.buffer, 0, record.descriptor.size};
  }
  if (compute_sets_in_last_pool_ == ComputeSetsPerPool) {
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * ComputeSetsPerPool};
    const VkDescriptorPoolCreateInfo info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, ComputeSetsPerPool, 1, &size};
    VkDescriptorPool pool = VK_NULL_HANDLE;
    Check(device_.functions_.CreateDescriptorPool(device_.device_, &info, nullptr, &pool),
          "vkCreateDescriptorPool(command list compute)");
    try {
      compute_descriptor_pools_.push_back(pool);
    } catch (...) {
      device_.functions_.DestroyDescriptorPool(device_.device_, pool, nullptr);
      throw;
    }
    compute_sets_in_last_pool_ = 0;
  }
  const VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                         compute_descriptor_pools_.back(), 1,
                                         &device_.compute_descriptor_set_layout_};
  VkDescriptorSet set = VK_NULL_HANDLE;
  Check(device_.functions_.AllocateDescriptorSets(device_.device_, &info, &set),
        "vkAllocateDescriptorSets(dispatch)");
  ++compute_sets_in_last_pool_;
  std::array<VkWriteDescriptorSet, 4> writes{};
  for (std::uint32_t index = 0; index < writes.size(); ++index)
    writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     nullptr,
                     set,
                     index,
                     0,
                     1,
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     nullptr,
                     &buffers[index],
                     nullptr};
  device_.functions_.UpdateDescriptorSets(
      device_.device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
  return set;
}

void VulkanCommandList::BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset,
                                           std::uint32_t stride) {
  if (submitted_ || closed_ || !rendering_ ||
      (stride != 0 && (stride < DrawIndirectArgumentSize || stride % alignof(std::uint32_t) != 0)))
    throw std::logic_error("indirect-buffer binding requires rendering");
  auto &record = device_.ValidateBuffer(buffer);
  if (offset > record.descriptor.size || DrawIndirectArgumentSize > record.descriptor.size - offset)
    throw std::logic_error("indirect-buffer binding is out of bounds");
  bound_indirect_buffer_ = record.buffer;
  bound_indirect_offset_ = offset;
  bound_indirect_size_ = record.descriptor.size;
  bound_indirect_stride_ = stride == 0 ? DrawIndirectArgumentSize : stride;
}

void VulkanCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ || vertex_count == 0 ||
      instance_count == 0)
    throw std::logic_error("invalid Vulkan draw");
  device_.functions_.CmdDraw(command_buffer_, vertex_count, instance_count, 0, 0);
  ++draws_;
}

void VulkanCommandList::DrawIndirect(std::uint32_t command_count) {
  if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ || command_count == 0 ||
      (bound_indirect_buffer_ == VK_NULL_HANDLE && indirect_buffer_ != VK_NULL_HANDLE))
    throw std::logic_error("invalid Vulkan indirect draw");
  if (bound_indirect_buffer_ != VK_NULL_HANDLE) {
    if (!IndirectDrawRangeFits(bound_indirect_size_, bound_indirect_offset_, bound_indirect_stride_,
                               command_count))
      throw std::logic_error("Vulkan indirect draw reads past the bound indirect buffer");
    device_.functions_.CmdDrawIndirect(command_buffer_, bound_indirect_buffer_,
                                       bound_indirect_offset_, command_count,
                                       bound_indirect_stride_);
    ++indirect_draws_;
    return;
  }
  const VkDeviceSize size = static_cast<VkDeviceSize>(DrawIndirectArgumentSize) * command_count;
  const VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                       nullptr,
                                       0,
                                       size,
                                       VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                       VK_SHARING_MODE_EXCLUSIVE,
                                       0,
                                       nullptr};
  Check(device_.functions_.CreateBuffer(device_.device_, &buffer_info, nullptr, &indirect_buffer_),
        "vkCreateBuffer(indirect)");
  VkMemoryRequirements requirements{};
  device_.functions_.GetBufferMemoryRequirements(device_.device_, indirect_buffer_, &requirements);
  const VkMemoryAllocateInfo allocation{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
      device_.FindMemoryType(requirements.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
  Check(device_.functions_.AllocateMemory(device_.device_, &allocation, nullptr, &indirect_memory_),
        "vkAllocateMemory(indirect)");
  Check(device_.functions_.BindBufferMemory(device_.device_, indirect_buffer_, indirect_memory_, 0),
        "vkBindBufferMemory(indirect)");
  void *mapped = nullptr;
  Check(device_.functions_.MapMemory(device_.device_, indirect_memory_, 0, size, 0, &mapped),
        "vkMapMemory(indirect)");
  auto *commands = static_cast<DrawIndirectArguments *>(mapped);
  for (std::uint32_t index = 0; index < command_count; ++index)
    commands[index] = {3, 1, 0, index};
  device_.functions_.UnmapMemory(device_.device_, indirect_memory_);
  device_.functions_.CmdDrawIndirect(command_buffer_, indirect_buffer_, 0, command_count,
                                     DrawIndirectArgumentSize);
  ++indirect_draws_;
}

void VulkanCommandList::Dispatch(std::uint32_t groups_x, std::uint32_t groups_y,
                                 std::uint32_t groups_z) {
  if (submitted_ || closed_ || rendering_ || !pipeline_bound_ ||
      pipeline_type_ != PipelineType::Compute || groups_x == 0 || groups_y == 0 || groups_z == 0)
    throw std::logic_error("invalid Vulkan dispatch");
  const auto descriptors = SnapshotComputeBindings();
  device_.functions_.CmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                                           device_.compute_pipeline_layout_, 0, 1, &descriptors, 0,
                                           nullptr);
  device_.functions_.CmdDispatch(command_buffer_, groups_x, groups_y, groups_z);
  ++dispatches_;
}

void VulkanCommandList::EndRendering() {
  if (submitted_ || closed_ || !rendering_)
    throw std::logic_error("Vulkan EndRendering without BeginRendering");
  device_.functions_.CmdEndRenderPass(command_buffer_);
  rendering_ = false;
}

void VulkanCommandList::Close() {
  if (closed_)
    return;
  if (rendering_)
    throw std::logic_error("cannot close a Vulkan command list during rendering");
  if (timestamps_ != VK_NULL_HANDLE)
    device_.functions_.CmdWriteTimestamp(command_buffer_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                         timestamps_, 1);
  Check(device_.functions_.EndCommandBuffer(command_buffer_), "vkEndCommandBuffer(command list)");
  closed_ = true;
}

} // namespace

std::unique_ptr<Device> CreateVulkanDevice(const DeviceCreationOptions &options) {
  return std::make_unique<VulkanDevice>(options);
}
} // namespace nexora::rhi
