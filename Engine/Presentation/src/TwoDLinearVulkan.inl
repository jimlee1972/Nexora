// Included inside VulkanSurface; uses only its existing device, frame and texture owners.
private:
struct TwoDLinearFrame {
  std::array<VkImage, 2> images{};
  std::array<VkDeviceMemory, 2> memory{};
  std::array<VkImageView, 2> views{};
  VkFramebuffer worldFramebuffer{}, compositeFramebuffer{};
  VkDescriptorSet descriptor{};
  VkBuffer uiUpload{};
  VkDeviceMemory uiMemory{};
  VkDeviceSize uiCapacity{}, allocationBytes{};
  bool initialized{};
  std::uint32_t width{}, height{};
};
std::array<TwoDLinearFrame, 3> twoDLinearFrames_{};
VkRenderPass twoDLinearPass_{};
VkPipeline twoDLinearPipeline_{}, twoDLinearInstancePipeline_{}, twoDLinearCompositePipeline_{};
bool CreateTwoDLinearProgram(bool instanced, bool composite, VkPipeline &output) {
  const auto makeShader = [&](const std::uint32_t *code, std::size_t bytes, VkShaderModule &out) {
    const VkShaderModuleCreateInfo create{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                          bytes, code};
    return vkCreateShaderModule(device_, &create, nullptr, &out) == VK_SUCCESS;
  };
  VkShaderModule vertex{}, fragment{};
  if (!makeShader(instanced ? twod_linear_spirv_instance : twod_linear_spirv_vert,
                  instanced ? sizeof(twod_linear_spirv_instance) : sizeof(twod_linear_spirv_vert),
                  vertex) ||
      !makeShader(composite ? twod_linear_spirv_composite : twod_linear_spirv_frag,
                  composite ? sizeof(twod_linear_spirv_composite) : sizeof(twod_linear_spirv_frag),
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
  if (composite)
    blends[0].blendEnable = VK_FALSE;
  blending.attachmentCount = composite ? 1 : 2;
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
  pipeline.layout = uiPipelineLayout_;
  pipeline.renderPass = composite ? uiRenderPass_ : twoDLinearPass_;
  const auto result =
      vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &output);
  vkDestroyShaderModule(device_, fragment, nullptr);
  vkDestroyShaderModule(device_, vertex, nullptr);
  return Initialized(result, "vkCreateGraphicsPipelines (linear 2D)");
}
void DestroyTwoDLinearFrame(TwoDLinearFrame &f) {
  if (f.compositeFramebuffer)
    vkDestroyFramebuffer(device_, f.compositeFramebuffer, nullptr);
  if (f.worldFramebuffer)
    vkDestroyFramebuffer(device_, f.worldFramebuffer, nullptr);
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
  for (auto pipeline :
       {twoDLinearPipeline_, twoDLinearInstancePipeline_, twoDLinearCompositePipeline_})
    if (pipeline)
      vkDestroyPipeline(device_, pipeline, nullptr);
  if (twoDLinearPass_)
    vkDestroyRenderPass(device_, twoDLinearPass_, nullptr);
  twoDLinearPipeline_ = twoDLinearInstancePipeline_ = twoDLinearCompositePipeline_ = VK_NULL_HANDLE;
  twoDLinearPass_ = VK_NULL_HANDLE;
}
bool CreateTwoDLinearPipelines() {
  if (twoDLinearPipeline_ && twoDLinearInstancePipeline_ && twoDLinearCompositePipeline_)
    return true;
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
  if (vkCreateRenderPass(device_, &pass, nullptr, &twoDLinearPass_) != VK_SUCCESS)
    return false;
  if (CreateTwoDLinearProgram(false, false, twoDLinearPipeline_) &&
      CreateTwoDLinearProgram(true, false, twoDLinearInstancePipeline_) &&
      CreateTwoDLinearProgram(false, true, twoDLinearCompositePipeline_))
    return true;
  DestroyTwoDLinear();
  return false;
}
bool CreateTwoDLinearTargets(TwoDLinearFrame &f) {
  if (f.width == width_ && f.height == height_ && f.worldFramebuffer && f.descriptor)
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
SurfaceStatus DrawTwoDLinear(const TwoDLinearDrawData &data) override {
  if (!OnThread())
    return SurfaceStatus::WrongThread;
  if (!SupportsTwoDLinear())
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
  const auto required = quadOffset + sizeof(quad) + sizeof(indices);
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
  vkUnmapMemory(device_, f.uiMemory);
  std::array<VkImageMemoryBarrier, 2> barriers{};
  for (unsigned i = 0; i < 2; ++i) {
    auto &b = barriers[i];
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = !f.initialized ? VK_IMAGE_LAYOUT_UNDEFINED
                                 : (i == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                           : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b.srcAccessMask = !f.initialized ? 0
                                     : (i == 0 ? VK_ACCESS_SHADER_READ_BIT
                                               : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
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
  const float transform[] = {2.F / width_, 2.F / height_, -1, -1};
  vkCmdPushConstants(frame.commands, uiPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(transform), transform);
  vkCmdBindIndexBuffer(frame.commands, f.uiUpload, vertices.size(),
                       data.world.indices32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
  for (const auto &c : data.world.commands) {
    const auto clipped = TwoDLinearClip(c, data.viewport, width_, height_);
    if (!clipped[2] || !clipped[3])
      continue;
    const auto &texture = uiTextures_.at(c.textureId);
    vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipelineLayout_, 0,
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
    ++diagnostics_.nativeUiDrawCalls;
  }
  vkCmdEndRenderPass(frame.commands);
  auto barrier = barriers[0];
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(frame.commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
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
  vkCmdBindDescriptorSets(frame.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipelineLayout_, 0, 1,
                          &f.descriptor, 0, nullptr);
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
