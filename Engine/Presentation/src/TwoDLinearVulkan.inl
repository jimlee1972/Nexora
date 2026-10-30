// Included inside VulkanSurface; uses only its existing device, frame and texture owners.
private:
struct TwoDBloomTarget {
  VkImage image{};
  VkDeviceMemory memory{};
  VkImageView view{};
  VkFramebuffer framebuffer{};
  VkDescriptorSet descriptor{};
  VkDeviceSize bytes{};
  std::uint32_t width{}, height{};
  bool initialized{};
};
struct TwoDLinearFrame {
  std::array<TwoDBloomTarget, 9> bloom;
  VkDescriptorSet emissionDescriptor{};
  std::array<VkImage, 2> images{};
  std::array<VkDeviceMemory, 2> memory{};
  std::array<VkImageView, 2> views{};
  VkFramebuffer worldFramebuffer{}, compositeFramebuffer{};
  VkDescriptorSet descriptor{}, lightingDescriptor{};
  VkBuffer uiUpload{}, shadowBuffer{};
  VkDeviceMemory shadowMemory{};
  VkDeviceSize shadowBytes{};
  VkDeviceMemory uiMemory{};
  VkDeviceSize uiCapacity{}, allocationBytes{};
  bool initialized{};
  std::uint32_t width{}, height{};
};
std::array<TwoDLinearFrame, 3> twoDLinearFrames_{};
bool twoDComputeQueue_{};
VkPipeline twoDShadowPipeline_{};
VkRenderPass twoDLinearPass_{}, twoDBloomPass_{};
VkPipeline twoDBloomPipeline_{};
VkDescriptorSetLayout twoDLightingLayout_{};
VkDescriptorPool twoDLightingPool_{};
VkPipelineLayout twoDLinearLayout_{};
VkPipeline twoDLinearPipeline_{}, twoDLinearInstancePipeline_{}, twoDLinearCompositePipeline_{};
bool CreateTwoDLinearProgram(bool instanced, bool composite, VkPipeline &output,
                             bool filter = false) {
  const auto makeShader = [&](const std::uint32_t *code, std::size_t bytes, VkShaderModule &out) {
    const VkShaderModuleCreateInfo create{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                          bytes, code};
    return vkCreateShaderModule(device_, &create, nullptr, &out) == VK_SUCCESS;
  };
  VkShaderModule vertex{}, fragment{};
  if (!makeShader(instanced ? twod_linear_spirv_instance : twod_linear_spirv_vert,
                  instanced ? sizeof(twod_linear_spirv_instance) : sizeof(twod_linear_spirv_vert),
                  vertex) ||
      !makeShader(filter      ? twod_linear_spirv_bloom
                  : composite ? twod_linear_spirv_composite
                              : twod_linear_spirv_frag,
                  filter      ? sizeof(twod_linear_spirv_bloom)
                  : composite ? sizeof(twod_linear_spirv_composite)
                              : sizeof(twod_linear_spirv_frag),
                  fragment)) {
    if (vertex)
      vkDestroyShaderModule(device_, vertex, nullptr);
    if (fragment)
      vkDestroyShaderModule(device_, fragment, nullptr);
    return false;
  }
  VkPipelineShaderStageCreateInfo stages[] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
       vertex, "main", nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr}};
  VkVertexInputBindingDescription vertexBinding{0, sizeof(UiVertex), VK_VERTEX_INPUT_RATE_VERTEX};
  const VkVertexInputAttributeDescription attributes[] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UiVertex, position)},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UiVertex, uv)},
      {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(UiVertex, color)}};
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &vertexBinding;
  vertexInput.vertexAttributeDescriptionCount = 3;
  vertexInput.pVertexAttributeDescriptions = attributes;
  const VkVertexInputAttributeDescription instanceAttributes[] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteGpuInstance, origin)},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteGpuInstance, axisX)},
      {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteGpuInstance, axisY)},
      {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SpriteGpuInstance, uv)},
      {4, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(SpriteGpuInstance, color)}};
  if (instanced) {
    vertexBinding.stride = sizeof(SpriteGpuInstance);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    vertexInput.vertexAttributeDescriptionCount = 5;
    vertexInput.pVertexAttributeDescriptions = instanceAttributes;
  }
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
  std::array<VkPipelineColorBlendAttachmentState, 2> blends{blend, blend};
  if (composite || filter)
    blends[0].blendEnable = VK_FALSE;
  blending.attachmentCount = composite || filter ? 1 : 2;
  blending.pAttachments = blends.data();
  const VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                     VK_DYNAMIC_STATE_BLEND_CONSTANTS};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 3;
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
  pipeline.layout = twoDLinearLayout_;
  pipeline.renderPass = filter ? twoDBloomPass_ : composite ? uiRenderPass_ : twoDLinearPass_;
  const auto result =
      vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &output);
  vkDestroyShaderModule(device_, fragment, nullptr);
  vkDestroyShaderModule(device_, vertex, nullptr);
  return Initialized(result, "vkCreateGraphicsPipelines (linear 2D)");
}
void DestroyTwoDBloomTarget(TwoDBloomTarget &t) {
  if (t.framebuffer)
    vkDestroyFramebuffer(device_, t.framebuffer, nullptr);
  if (t.descriptor)
    vkFreeDescriptorSets(device_, uiDescriptorPool_, 1, &t.descriptor);
  if (t.view)
    vkDestroyImageView(device_, t.view, nullptr);
  if (t.image)
    vkDestroyImage(device_, t.image, nullptr);
  if (t.memory)
    vkFreeMemory(device_, t.memory, nullptr);
  diagnostics_.twoDBloomTargetBytes -= t.bytes;
  t = {};
}
bool EnsureTwoDBloomTarget(TwoDBloomTarget &t, uint32_t width, uint32_t height) {
  if (t.width == width && t.height == height && t.framebuffer && t.descriptor)
    return true;
  DestroyTwoDBloomTarget(t);
  t.width = width;
  t.height = height;
  VkImageCreateInfo image{};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  image.extent = {width, height, 1};
  image.mipLevels = image.arrayLayers = 1;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateImage(device_, &image, nullptr, &t.image) != VK_SUCCESS)
    return false;
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(device_, t.image, &requirements);
  const auto type =
      FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (type == std::numeric_limits<uint32_t>::max())
    return false;
  const VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                        requirements.size, type};
  if (vkAllocateMemory(device_, &allocation, nullptr, &t.memory) != VK_SUCCESS)
    return false;
  t.bytes = requirements.size;
  diagnostics_.twoDBloomTargetBytes += t.bytes;
  if (vkBindImageMemory(device_, t.image, t.memory, 0) != VK_SUCCESS)
    return false;
  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = t.image;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = image.format;
  view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (vkCreateImageView(device_, &view, nullptr, &t.view) != VK_SUCCESS)
    return false;
  VkFramebufferCreateInfo framebuffer{};
  framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer.renderPass = twoDBloomPass_;
  framebuffer.attachmentCount = 1;
  framebuffer.pAttachments = &t.view;
  framebuffer.width = width;
  framebuffer.height = height;
  framebuffer.layers = 1;
  if (vkCreateFramebuffer(device_, &framebuffer, nullptr, &t.framebuffer) != VK_SUCCESS)
    return false;
  const VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             nullptr, uiDescriptorPool_, 1, &uiDescriptorLayout_};
  if (vkAllocateDescriptorSets(device_, &allocate, &t.descriptor) != VK_SUCCESS)
    return false;
  const VkDescriptorImageInfo info{uiSampler_, t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = t.descriptor;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  return true;
}
bool DrawTwoDBloom(TwoDLinearFrame &f, VkCommandBuffer commands, const TwoDLinearDrawData &data,
                   VkDeviceSize quadOffset) {
  const auto plan =
      PrepareTwoDBloom(width_, height_, data.lighting ? &data.lighting->uniforms : nullptr);
  std::array<bool, 9> used{};
  for (const auto &step : plan.steps)
    used[unsigned(step.output)] = true;
  for (unsigned i = 0; i < used.size(); ++i)
    if (!used[i])
      DestroyTwoDBloomTarget(f.bloom[i]);
  const auto descriptor = [&](int index) {
    return index < 0 ? f.emissionDescriptor : f.bloom[unsigned(index)].descriptor;
  };
  for (const auto &step : plan.steps) {
    auto &target = f.bloom[unsigned(step.output)];
    if (!EnsureTwoDBloomTarget(target, step.width, step.height))
      return false;
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout =
        target.initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = target.initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = target.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);
    const VkClearValue clear{{{0, 0, 0, 1}}};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = twoDBloomPass_;
    begin.framebuffer = target.framebuffer;
    begin.renderArea.extent = {step.width, step.height};
    begin.clearValueCount = 1;
    begin.pClearValues = &clear;
    vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDBloomPipeline_);
    const auto first = descriptor(step.source), second = descriptor(step.detail);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 0, 1,
                            &first, 0, nullptr);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 2, 1,
                            &second, 0, nullptr);
    TwoDShaderConstants constants{{2.F / width_, 2.F / height_, -1, -1}};
    constants.style[3] = step.up ? 1 : 0;
    constants.emission = {step.texel[0], step.texel[1], 0, 0};
    vkCmdPushConstants(commands, twoDLinearLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(constants), &constants);
    const VkViewport viewport{0, 0, float(step.width), float(step.height), 0, 1};
    const VkRect2D scissor{{0, 0}, {step.width, step.height}};
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    vkCmdBindVertexBuffers(commands, 0, 1, &f.uiUpload, &quadOffset);
    vkCmdBindIndexBuffer(commands, f.uiUpload, quadOffset + sizeof(UiVertex) * 4,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commands, 6, 1, 0, 0, 0);
    vkCmdEndRenderPass(commands);
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
    target.initialized = true;
    ++diagnostics_.twoDBloomPasses;
    ++diagnostics_.nativeUiDrawCalls;
  }
  // Restore the full-output transform and viewport for final SDR composition.
  const VkViewport viewport{0, 0, float(width_), float(height_), 0, 1};
  vkCmdSetViewport(commands, 0, 1, &viewport);
  const TwoDShaderConstants constants{{2.F / width_, 2.F / height_, -1, -1}};
  vkCmdPushConstants(commands, twoDLinearLayout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(constants), &constants);
  return true;
}
void DestroyTwoDLinearFrame(TwoDLinearFrame &f) {
  for (auto &target : f.bloom)
    DestroyTwoDBloomTarget(target);
  if (f.emissionDescriptor)
    vkFreeDescriptorSets(device_, uiDescriptorPool_, 1, &f.emissionDescriptor);
  if (f.shadowBuffer)
    vkDestroyBuffer(device_, f.shadowBuffer, nullptr);
  if (f.shadowMemory)
    vkFreeMemory(device_, f.shadowMemory, nullptr);
  diagnostics_.twoDShadowBufferBytes -= f.shadowBytes;
  if (f.compositeFramebuffer)
    vkDestroyFramebuffer(device_, f.compositeFramebuffer, nullptr);
  if (f.worldFramebuffer)
    vkDestroyFramebuffer(device_, f.worldFramebuffer, nullptr);
  if (f.lightingDescriptor)
    vkFreeDescriptorSets(device_, twoDLightingPool_, 1, &f.lightingDescriptor);
  if (f.descriptor)
    vkFreeDescriptorSets(device_, uiDescriptorPool_, 1, &f.descriptor);
  for (unsigned i = 0; i < 2; ++i) {
    if (f.views[i])
      vkDestroyImageView(device_, f.views[i], nullptr);
    if (f.images[i])
      vkDestroyImage(device_, f.images[i], nullptr);
    if (f.memory[i])
      vkFreeMemory(device_, f.memory[i], nullptr);
  }
  if (f.uiUpload)
    vkDestroyBuffer(device_, f.uiUpload, nullptr);
  if (f.uiMemory)
    vkFreeMemory(device_, f.uiMemory, nullptr);
  diagnostics_.twoDLinearTargetBytes -= f.allocationBytes;
  f = {};
}
void DestroyTwoDLinear() {
  for (auto &f : twoDLinearFrames_)
    DestroyTwoDLinearFrame(f);
  for (auto pipeline : {twoDLinearPipeline_, twoDLinearInstancePipeline_,
                        twoDLinearCompositePipeline_, twoDShadowPipeline_, twoDBloomPipeline_})
    if (pipeline)
      vkDestroyPipeline(device_, pipeline, nullptr);
  if (twoDLinearPass_)
    vkDestroyRenderPass(device_, twoDLinearPass_, nullptr);
  twoDLinearPipeline_ = twoDLinearInstancePipeline_ = twoDLinearCompositePipeline_ = VK_NULL_HANDLE;
  twoDShadowPipeline_ = twoDBloomPipeline_ = VK_NULL_HANDLE;
  if (twoDBloomPass_)
    vkDestroyRenderPass(device_, twoDBloomPass_, nullptr);
  twoDBloomPass_ = VK_NULL_HANDLE;
  twoDLinearPass_ = VK_NULL_HANDLE;
  if (twoDLinearLayout_)
    vkDestroyPipelineLayout(device_, twoDLinearLayout_, nullptr);
  if (twoDLightingPool_)
    vkDestroyDescriptorPool(device_, twoDLightingPool_, nullptr);
  if (twoDLightingLayout_)
    vkDestroyDescriptorSetLayout(device_, twoDLightingLayout_, nullptr);
  twoDLinearLayout_ = VK_NULL_HANDLE;
  twoDLightingPool_ = VK_NULL_HANDLE;
  twoDLightingLayout_ = VK_NULL_HANDLE;
}
bool CreateTwoDLinearPipelines() {
  if (twoDLinearPipeline_ && twoDLinearInstancePipeline_ && twoDLinearCompositePipeline_ &&
      twoDBloomPipeline_)
    return true;
  const VkDescriptorSetLayoutBinding bindings[]{
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
      {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
  const VkDescriptorSetLayoutCreateInfo setCreate{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 4, bindings};
  const VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9}};
  const VkDescriptorPoolCreateInfo poolCreate{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              nullptr,
                                              VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                              3,
                                              2,
                                              sizes};
  const VkDescriptorSetLayout layouts[]{uiDescriptorLayout_, VK_NULL_HANDLE};
  if (vkCreateDescriptorSetLayout(device_, &setCreate, nullptr, &twoDLightingLayout_) !=
          VK_SUCCESS ||
      vkCreateDescriptorPool(device_, &poolCreate, nullptr, &twoDLightingPool_) != VK_SUCCESS) {
    DestroyTwoDLinear();
    return false;
  }
  const VkDescriptorSetLayout allLayouts[]{layouts[0], twoDLightingLayout_, layouts[0]};
  const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                 sizeof(TwoDShaderConstants)};
  const VkPipelineLayoutCreateInfo layoutCreate{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 3, allLayouts, 1, &push};
  if (vkCreatePipelineLayout(device_, &layoutCreate, nullptr, &twoDLinearLayout_) != VK_SUCCESS) {
    DestroyTwoDLinear();
    return false;
  }
  std::array<VkAttachmentDescription, 2> attachments{};
  for (auto &a : attachments) {
    a.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    a.samples = VK_SAMPLE_COUNT_1_BIT;
    a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a.initialLayout = a.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  const VkAttachmentReference refs[] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                                        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 2;
  sub.pColorAttachments = refs;
  VkRenderPassCreateInfo pass{};
  pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass.attachmentCount = 2;
  pass.pAttachments = attachments.data();
  pass.subpassCount = 1;
  pass.pSubpasses = &sub;
  if (vkCreateRenderPass(device_, &pass, nullptr, &twoDLinearPass_) != VK_SUCCESS) {
    DestroyTwoDLinear();
    return false;
  }
  pass.attachmentCount = sub.colorAttachmentCount = 1;
  if (vkCreateRenderPass(device_, &pass, nullptr, &twoDBloomPass_) != VK_SUCCESS) {
    DestroyTwoDLinear();
    return false;
  }
  if (CreateTwoDLinearProgram(false, false, twoDLinearPipeline_) &&
      CreateTwoDLinearProgram(true, false, twoDLinearInstancePipeline_) &&
      CreateTwoDLinearProgram(false, true, twoDLinearCompositePipeline_) &&
      CreateTwoDLinearProgram(false, false, twoDBloomPipeline_, true)) {
    if (!twoDComputeQueue_)
      return true;
    const VkShaderModuleCreateInfo shaderCreate{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                nullptr, 0, sizeof(twod_linear_spirv_shadow),
                                                twod_linear_spirv_shadow};
    VkShaderModule shader{};
    if (vkCreateShaderModule(device_, &shaderCreate, nullptr, &shader) != VK_SUCCESS) {
      DestroyTwoDLinear();
      return false;
    }
    VkComputePipelineCreateInfo compute{};
    compute.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     nullptr,
                     0,
                     VK_SHADER_STAGE_COMPUTE_BIT,
                     shader,
                     "main",
                     nullptr};
    compute.layout = twoDLinearLayout_;
    const auto result = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &compute, nullptr,
                                                 &twoDShadowPipeline_);
    vkDestroyShaderModule(device_, shader, nullptr);
    if (result == VK_SUCCESS)
      return true;
  }
  DestroyTwoDLinear();
  return false;
}
bool CreateTwoDLinearTargets(TwoDLinearFrame &f) {
  if (f.width == width_ && f.height == height_ && f.worldFramebuffer && f.descriptor &&
      f.lightingDescriptor && f.emissionDescriptor && f.shadowBuffer && f.shadowMemory)
    return true;
  DestroyTwoDLinearFrame(f); // protecting slot fence was consumed by Acquire
  f.width = width_;
  f.height = height_;
  for (unsigned i = 0; i < 2; ++i) {
    VkImageCreateInfo image{};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image.extent = {width_, height_, 1};
    image.mipLevels = image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device_, &image, nullptr, &f.images[i]) != VK_SUCCESS)
      return false;
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device_, f.images[i], &req);
    const auto type = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type == std::numeric_limits<std::uint32_t>::max())
      return false;
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, type};
    if (vkAllocateMemory(device_, &alloc, nullptr, &f.memory[i]) != VK_SUCCESS)
      return false;
    f.allocationBytes += req.size;
    diagnostics_.twoDLinearTargetBytes += req.size;
    if (vkBindImageMemory(device_, f.images[i], f.memory[i], 0) != VK_SUCCESS)
      return false;
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = f.images[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = image.format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device_, &view, nullptr, &f.views[i]) != VK_SUCCESS)
      return false;
  }
  VkFramebufferCreateInfo fb{};
  fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb.renderPass = twoDLinearPass_;
  fb.attachmentCount = 2;
  fb.pAttachments = f.views.data();
  fb.width = width_;
  fb.height = height_;
  fb.layers = 1;
  if (vkCreateFramebuffer(device_, &fb, nullptr, &f.worldFramebuffer) != VK_SUCCESS)
    return false;
  VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                       uiDescriptorPool_, 1, &uiDescriptorLayout_};
  if (vkAllocateDescriptorSets(device_, &allocate, &f.descriptor) != VK_SUCCESS)
    return false;
  const VkDescriptorImageInfo info{uiSampler_, f.views[0],
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = f.descriptor;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  if (vkAllocateDescriptorSets(device_, &allocate, &f.emissionDescriptor) != VK_SUCCESS)
    return false;
  const VkDescriptorImageInfo emissionInfo{uiSampler_, f.views[1],
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  write.dstSet = f.emissionDescriptor;
  write.pImageInfo = &emissionInfo;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  const VkDescriptorSetAllocateInfo uniformAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                    nullptr, twoDLightingPool_, 1,
                                                    &twoDLightingLayout_};
  if (vkAllocateDescriptorSets(device_, &uniformAllocate, &f.lightingDescriptor) != VK_SUCCESS)
    return false;
  VkBufferCreateInfo shadowCreate{};
  shadowCreate.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  shadowCreate.size = 8 * 1024 * sizeof(float);
  shadowCreate.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  shadowCreate.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device_, &shadowCreate, nullptr, &f.shadowBuffer) != VK_SUCCESS)
    return false;
  VkMemoryRequirements shadowRequirements{};
  vkGetBufferMemoryRequirements(device_, f.shadowBuffer, &shadowRequirements);
  const auto shadowType =
      FindMemoryType(shadowRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (shadowType == std::numeric_limits<std::uint32_t>::max())
    return false;
  const VkMemoryAllocateInfo shadowAllocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                            shadowRequirements.size, shadowType};
  if (vkAllocateMemory(device_, &shadowAllocate, nullptr, &f.shadowMemory) != VK_SUCCESS)
    return false;
  f.shadowBytes = shadowRequirements.size;
  diagnostics_.twoDShadowBufferBytes += f.shadowBytes;
  if (vkBindBufferMemory(device_, f.shadowBuffer, f.shadowMemory, 0) != VK_SUCCESS)
    return false;
  ++diagnostics_.twoDLinearTargetAllocations;
  return true;
}

public:
bool SupportsTwoDLinear() const noexcept override {
  if (destroyed_ || !valid_ || !srgbUiOutput_)
    return false;
  VkFormatProperties format{};
  vkGetPhysicalDeviceFormatProperties(physical_, VK_FORMAT_R16G16B16A16_SFLOAT, &format);
  const auto need =
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  return (format.optimalTilingFeatures & need) == need;
}
bool SupportsTwoDShadows() const noexcept override {
  return SupportsTwoDLinear() && twoDComputeQueue_;
}
bool SupportsTwoDBloom() const noexcept override { return SupportsTwoDLinear(); }

SurfaceStatus DrawTwoDLinear(const TwoDLinearDrawData &data) override {
  if (!OnThread())
    return SurfaceStatus::WrongThread;
  if (!SupportsTwoDLinear() ||
      (data.lighting && data.lighting->uniforms.counts[1] && !SupportsTwoDShadows()))
    return SurfaceStatus::Unsupported;
  auto &frame = frames_[frame_];
  auto &f = twoDLinearFrames_[frame_];
  if (!acquired_ || transferTarget_ || sceneRendered_ || sceneOffscreen_ || frame.uiFramebuffer ||
      !ValidateTwoDLinearDraws(data, width_, height_))
    return SurfaceStatus::InvalidDescriptor;
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &command : packet->commands) {
      bool known = uiTextures_.contains(command.textureId);
      for (const auto *source : {&data.world, &data.overlay})
        for (const auto &upload : source->textureUploads)
          known = known || upload.textureId == command.textureId;
      if (!known)
        return SurfaceStatus::InvalidDescriptor;
    }
  if (data.lighting) {
    for (const auto &style : data.lighting->styles) {
      if (!style.emissionTexture)
        continue;
      bool known = uiTextures_.contains(style.emissionTexture);
      bool nearest = false;
      if (known)
        nearest = uiTextures_.at(style.emissionTexture).nearest;
      for (const auto *packet : {&data.world, &data.overlay})
        for (const auto &upload : packet->textureUploads)
          if (upload.textureId == style.emissionTexture) {
            known = true;
            nearest = upload.nearest;
          }
      if (!known || nearest)
        return SurfaceStatus::InvalidDescriptor;
    }
  }
  if (!CreateTwoDLinearPipelines())
    return SurfaceStatus::Unsupported;
  if (!CreateTwoDLinearTargets(f))
    return SurfaceStatus::DeviceLost;
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &upload : packet->textureUploads) {
      const auto old = uiTextures_.find(upload.textureId);
      if (old != uiTextures_.end() && old->second.immutable &&
          (!upload.immutable || old->second.width != upload.width ||
           old->second.height != upload.height || old->second.nearest != upload.nearest))
        return SurfaceStatus::InvalidDescriptor;
    }
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &upload : packet->textureUploads)
      if (!UploadUiTexture(frame, upload))
        return SurfaceStatus::DeviceLost;
  for (const auto &c : data.world.commands)
    if (!uiTextures_.contains(c.textureId))
      return SurfaceStatus::InvalidDescriptor;
  const auto vertices = std::as_bytes(data.world.vertices);
  const auto instances = std::as_bytes(data.world.spriteInstances);
  const auto instanceOffset = (vertices.size() + data.world.indices.size() + 3U) & ~std::size_t(3);
  const auto quadOffset = (instanceOffset + instances.size() + 3U) & ~std::size_t(3);
  const UiVertex quad[] = {{{0, 0}, {0, 0}, 0xffffffff},
                           {{float(width_), 0}, {1, 0}, 0xffffffff},
                           {{float(width_), float(height_)}, {1, 1}, 0xffffffff},
                           {{0, float(height_)}, {0, 1}, 0xffffffff}};
  const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_, &properties);
  const auto alignment =
      std::max<VkDeviceSize>(16, std::max(properties.limits.minUniformBufferOffsetAlignment,
                                          properties.limits.minStorageBufferOffsetAlignment));
  const auto uniformOffset =
      (quadOffset + sizeof(quad) + sizeof(indices) + alignment - 1) & ~(alignment - 1);
  const auto edgeOffset =
      (uniformOffset + sizeof(TwoDLightingUniform) + alignment - 1) & ~(alignment - 1);
  const auto edgeBytes = std::max<std::size_t>(1, data.lighting ? data.lighting->edges.size() : 0) *
                         sizeof(TwoDShadowEdge);
  const auto required = edgeOffset + edgeBytes;
  if (!EnsureUiUpload(f, required))
    return SurfaceStatus::DeviceLost;
  void *mapped{};
  if (vkMapMemory(device_, f.uiMemory, 0, required, 0, &mapped) != VK_SUCCESS)
    return SurfaceStatus::DeviceLost;
  auto *bytes = static_cast<std::byte *>(mapped);
  std::memcpy(bytes, vertices.data(), vertices.size());
  std::memcpy(bytes + vertices.size(), data.world.indices.data(), data.world.indices.size());
  if (!instances.empty())
    std::memcpy(bytes + instanceOffset, instances.data(), instances.size());
  std::memcpy(bytes + quadOffset, quad, sizeof(quad));
  std::memcpy(bytes + quadOffset + sizeof(quad), indices, sizeof(indices));
  const TwoDLightingUniform fallback{};
  std::memcpy(bytes + uniformOffset, data.lighting ? &data.lighting->uniforms : &fallback,
              sizeof(fallback));
  std::memset(bytes + edgeOffset, 0, edgeBytes);
  if (data.lighting && !data.lighting->edges.empty())
    std::memcpy(bytes + edgeOffset, data.lighting->edges.data(), data.lighting->edges.size_bytes());
  vkUnmapMemory(device_, f.uiMemory);
  const VkDescriptorBufferInfo uniform{f.uiUpload, uniformOffset, sizeof(TwoDLightingUniform)};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = f.lightingDescriptor;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.pBufferInfo = &uniform;
  const VkDescriptorBufferInfo edgeInfo{f.uiUpload, edgeOffset, edgeBytes};
  const VkDescriptorBufferInfo shadowInfo{f.shadowBuffer, 0, 8 * 1024 * sizeof(float)};
  std::array<VkWriteDescriptorSet, 4> writes{write, {}, {}, {}};
  for (unsigned binding = 1; binding < 4; ++binding) {
    auto &out = writes[binding];
    out.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    out.dstSet = f.lightingDescriptor;
    out.dstBinding = binding;
    out.descriptorCount = 1;
    out.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out.pBufferInfo = binding == 1 ? &edgeInfo : &shadowInfo;
  }
  vkUpdateDescriptorSets(device_, uint32_t(writes.size()), writes.data(), 0, nullptr);
  if (data.lighting && data.lighting->uniforms.counts[1]) {
    VkBufferMemoryBarrier before{};
    before.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    before.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    before.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    before.srcQueueFamilyIndex = before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.buffer = f.shadowBuffer;
    before.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(
        frame.commands,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &before, 0, nullptr);
    vkCmdBindPipeline(frame.commands, VK_PIPELINE_BIND_POINT_COMPUTE, twoDShadowPipeline_);
    vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_COMPUTE, twoDLinearLayout_, 1, 1,
                            &f.lightingDescriptor, 0, nullptr);
    vkCmdDispatch(frame.commands, 16, data.lighting->uniforms.counts[1], 1);
    before.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    before.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &before, 0,
                         nullptr);
    ++diagnostics_.twoDShadowDispatches;
    diagnostics_.twoDShadowSamples += uint64_t(data.lighting->uniforms.counts[1]) * 1024;
  }
  std::array<VkImageMemoryBarrier, 2> barriers{};
  for (unsigned i = 0; i < 2; ++i) {
    auto &b = barriers[i];
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout =
        !f.initialized ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b.srcAccessMask = !f.initialized ? 0 : VK_ACCESS_SHADER_READ_BIT;
    b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = f.images[i];
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  }
  vkCmdPipelineBarrier(
      frame.commands,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());
  const VkClearValue clears[] = {{{{0, 0, 0, 1}}}, {{{0, 0, 0, 1}}}};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = twoDLinearPass_;
  begin.framebuffer = f.worldFramebuffer;
  begin.renderArea.extent = {width_, height_};
  begin.clearValueCount = 2;
  begin.pClearValues = clears;
  vkCmdBeginRenderPass(frame.commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
  const VkViewport viewport{0, 0, float(width_), float(height_), 0, 1};
  vkCmdSetViewport(frame.commands, 0, 1, &viewport);
  TwoDShaderConstants transform{{2.F / width_, 2.F / height_, -1, -1}};
  vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 1, 1,
                          &f.lightingDescriptor, 0, nullptr);
  vkCmdPushConstants(frame.commands, twoDLinearLayout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(transform), &transform);
  vkCmdBindIndexBuffer(frame.commands, f.uiUpload, vertices.size(),
                       data.world.indices32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
  std::size_t commandIndex{};
  for (const auto &c : data.world.commands) {
    transform.SetStyle(data.lighting ? data.lighting->styles[commandIndex]
                                     : TwoDLightingStyle{UINT32_MAX, 0, 0, 0});
    ++commandIndex;
    vkCmdPushConstants(frame.commands, twoDLinearLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(transform), &transform);
    const auto clipped = TwoDLinearClip(c, data.viewport, width_, height_);
    if (!clipped[2] || !clipped[3])
      continue;
    const auto &texture = uiTextures_.at(c.textureId);
    const auto maskId = data.lighting ? data.lighting->styles[commandIndex - 1].emissionTexture : 0;
    const auto &mask = uiTextures_.at(maskId ? maskId : c.textureId);
    vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 2,
                            1, &mask.descriptor, 0, nullptr);
    vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 0,
                            1, &texture.descriptor, 0, nullptr);
    const VkRect2D clip{{std::int32_t(clipped[0]), std::int32_t(clipped[1])},
                        {clipped[2], clipped[3]}};
    vkCmdSetScissor(frame.commands, 0, 1, &clip);
    const VkDeviceSize offset = c.instanceCount ? instanceOffset : 0;
    vkCmdBindVertexBuffers(frame.commands, 0, 1, &f.uiUpload, &offset);
    vkCmdBindPipeline(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      c.instanceCount ? twoDLinearInstancePipeline_ : twoDLinearPipeline_);
    if (c.instanceCount) {
      vkCmdDraw(frame.commands, 6, c.instanceCount, 0, c.firstInstance);
      ++diagnostics_.spriteInstancedDrawCalls;
      diagnostics_.spriteGpuInstances += c.instanceCount;
    } else
      vkCmdDrawIndexed(frame.commands, c.elementCount, 1, c.indexOffset, c.vertexOffset, 0);
    if (transform.emission[3] > 0)
      ++diagnostics_.twoDEmissionDraws;
    ++diagnostics_.nativeUiDrawCalls;
  }
  vkCmdEndRenderPass(frame.commands);
  for (auto &barrier : barriers) {
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  }
  vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                       uint32_t(barriers.size()), barriers.data());
  if (!DrawTwoDBloom(f, frame.commands, data, quadOffset))
    return SurfaceStatus::DeviceLost;
  f.initialized = true;
  if (f.compositeFramebuffer)
    vkDestroyFramebuffer(device_, f.compositeFramebuffer, nullptr);
  VkFramebufferCreateInfo fb{};
  fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb.renderPass = uiRenderPass_;
  fb.attachmentCount = 1;
  fb.pAttachments = &imageViews_[imageIndex_];
  fb.width = width_;
  fb.height = height_;
  fb.layers = 1;
  if (vkCreateFramebuffer(device_, &fb, nullptr, &f.compositeFramebuffer) != VK_SUCCESS)
    return SurfaceStatus::DeviceLost;
  begin.renderPass = uiRenderPass_;
  begin.framebuffer = f.compositeFramebuffer;
  begin.clearValueCount = 0;
  begin.pClearValues = nullptr;
  vkCmdBeginRenderPass(frame.commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
  const VkClearAttachment black{VK_IMAGE_ASPECT_COLOR_BIT, 0, VkClearValue{{{0, 0, 0, 1}}}};
  const VkClearRect full{{{0, 0}, {width_, height_}}, 0, 1};
  vkCmdClearAttachments(frame.commands, 1, &black, 1, &full);
  vkCmdBindPipeline(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearCompositePipeline_);
  vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 0, 1,
                          &f.descriptor, 0, nullptr);
  const auto bloomPlan =
      PrepareTwoDBloom(width_, height_, data.lighting ? &data.lighting->uniforms : nullptr);
  const auto bloomDescriptor =
      bloomPlan.result < 0 ? f.emissionDescriptor : f.bloom[unsigned(bloomPlan.result)].descriptor;
  vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, twoDLinearLayout_, 2, 1,
                          &bloomDescriptor, 0, nullptr);
  const auto &v = data.viewport;
  const VkRect2D clip = v == std::array<std::uint32_t, 4>{}
                            ? VkRect2D{{0, 0}, {width_, height_}}
                            : VkRect2D{{std::int32_t(v[0]), std::int32_t(v[1])}, {v[2], v[3]}};
  vkCmdSetScissor(frame.commands, 0, 1, &clip);
  const VkDeviceSize offset = quadOffset;
  vkCmdBindVertexBuffers(frame.commands, 0, 1, &f.uiUpload, &offset);
  vkCmdBindIndexBuffer(frame.commands, f.uiUpload, quadOffset + sizeof(quad), VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(frame.commands, 6, 1, 0, 0, 0);
  vkCmdEndRenderPass(frame.commands);
  sceneRendered_ = true;
  transferTarget_ = false;
  ++diagnostics_.nativeUiDrawCalls;
  diagnostics_.spriteInstanceUploadBytes += instances.size();
  if (!data.overlay.commands.empty()) {
    const VkMemoryBarrier overlayBarrier{
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT,
                         1, &overlayBarrier, 0, nullptr, 0, nullptr);
    const auto status = RenderUi(data.overlay);
    if (status != SurfaceStatus::Ready)
      return status;
  }
  ++diagnostics_.twoDLinearFrames;
  return SurfaceStatus::Ready;
}
