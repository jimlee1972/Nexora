// Included inside MetalSurface. Targets and upload storage belong to completed frame slots.
private:
struct TwoDLinearFrame {
  std::array<id<MTLTexture>, 2> colors{};
  id<MTLBuffer> upload = nil;
  std::size_t capacity{};
  std::uint32_t width{}, height{};
  std::uint64_t allocationBytes{};
};
std::array<TwoDLinearFrame, 3> twoDLinearFrames_;
id<MTLRenderPipelineState> twoDLinearPipeline_ = nil;
id<MTLRenderPipelineState> twoDLinearInstancePipeline_ = nil;
id<MTLRenderPipelineState> twoDLinearCompositePipeline_ = nil;
void ResetTwoDLinearFrames() {
  for (auto &f : twoDLinearFrames_) {
    diagnostics_.twoDLinearTargetBytes -= f.allocationBytes;
    f = {};
  }
}
bool CreateTwoDLinearPipelines() {
  if (twoDLinearPipeline_ && twoDLinearInstancePipeline_ && twoDLinearCompositePipeline_)
    return true;
  NSError *error = nil;
  const auto library = [&](const char *source) -> id<MTLLibrary> {
    NSError *compileError = nil;
    auto result = [device_ newLibraryWithSource:[NSString stringWithUTF8String:source]
                                        options:nil
                                          error:&compileError];
    if (!result)
      NSLog(@"linear 2D shader compile: %@", compileError);
    return result;
  };
  auto vertexLibrary = library(twod_linear_metal_vert);
  auto instanceLibrary = library(twod_linear_metal_instance);
  auto fragmentLibrary = library(twod_linear_metal_frag);
  auto compositeLibrary = library(twod_linear_metal_composite);
  if (!vertexLibrary || !instanceLibrary || !fragmentLibrary || !compositeLibrary)
    return false;
  auto vertex = [vertexLibrary newFunctionWithName:@"twoDVertexMain"];
  auto instance = [instanceLibrary newFunctionWithName:@"twoDInstanceMain"];
  auto fragment = [fragmentLibrary newFunctionWithName:@"twoDFragmentMain"];
  auto composite = [compositeLibrary newFunctionWithName:@"twoDCompositeMain"];
  if (!vertex || !instance || !fragment || !composite)
    return false;
  for (unsigned mode = 0; mode < 3; ++mode) {
    auto desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = mode == 1 ? instance : vertex;
    desc.fragmentFunction = mode == 2 ? composite : fragment;
    auto attributes = [MTLVertexDescriptor vertexDescriptor];
    if (mode == 1) {
      const std::size_t offsets[] = {
          offsetof(SpriteGpuInstance, origin), offsetof(SpriteGpuInstance, axisX),
          offsetof(SpriteGpuInstance, axisY), offsetof(SpriteGpuInstance, uv),
          offsetof(SpriteGpuInstance, color)};
      for (unsigned i = 0; i < 5; ++i) {
        attributes.attributes[i].format = i < 3    ? MTLVertexFormatFloat2
                                          : i == 3 ? MTLVertexFormatFloat4
                                                   : MTLVertexFormatUChar4Normalized;
        attributes.attributes[i].offset = offsets[i];
        attributes.attributes[i].bufferIndex = 1;
      }
      attributes.layouts[1].stride = sizeof(SpriteGpuInstance);
      attributes.layouts[1].stepFunction = MTLVertexStepFunctionPerInstance;
      attributes.layouts[1].stepRate = 1;
    } else {
      const std::size_t offsets[] = {offsetof(UiVertex, position), offsetof(UiVertex, uv),
                                     offsetof(UiVertex, color)};
      for (unsigned i = 0; i < 3; ++i) {
        attributes.attributes[i].format =
            i < 2 ? MTLVertexFormatFloat2 : MTLVertexFormatUChar4Normalized;
        attributes.attributes[i].offset = offsets[i];
        attributes.attributes[i].bufferIndex = 1;
      }
      attributes.layouts[1].stride = sizeof(UiVertex);
      attributes.layouts[1].stepFunction = MTLVertexStepFunctionPerVertex;
    }
    desc.vertexDescriptor = attributes;
    for (unsigned i = 0; i < (mode == 2 ? 1U : 2U); ++i) {
      auto color = desc.colorAttachments[i];
      color.pixelFormat = mode == 2 ? layer_.pixelFormat : MTLPixelFormatRGBA16Float;
      color.blendingEnabled = mode != 2;
      color.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
      color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      color.rgbBlendOperation = MTLBlendOperationAdd;
      color.sourceAlphaBlendFactor = MTLBlendFactorOne;
      color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      color.alphaBlendOperation = MTLBlendOperationAdd;
    }
    auto pipeline = [device_ newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline) {
      NSLog(@"linear 2D pipeline: %@", error);
      twoDLinearPipeline_ = twoDLinearInstancePipeline_ = twoDLinearCompositePipeline_ = nil;
      return false;
    }
    if (mode == 0)
      twoDLinearPipeline_ = pipeline;
    else if (mode == 1)
      twoDLinearInstancePipeline_ = pipeline;
    else
      twoDLinearCompositePipeline_ = pipeline;
  }
  return true;
}
bool CreateTwoDLinearTargets(TwoDLinearFrame &f) {
  if (f.width == width_ && f.height == height_ && f.colors[0] && f.colors[1])
    return true;
  diagnostics_.twoDLinearTargetBytes -= f.allocationBytes;
  f = {};
  f.width = width_;
  f.height = height_;
  auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                 width:width_
                                                                height:height_
                                                             mipmapped:NO];
  desc.storageMode = MTLStorageModePrivate;
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  for (auto &texture : f.colors) {
    texture = [device_ newTextureWithDescriptor:desc];
    if (!texture)
      return false;
    const auto bytes = static_cast<std::uint64_t>(texture.allocatedSize);
    f.allocationBytes += bytes;
    diagnostics_.twoDLinearTargetBytes += bytes;
  }
  ++diagnostics_.twoDLinearTargetAllocations;
  return true;
}

public:
bool SupportsTwoDLinear() const noexcept override {
  return valid_ && !destroyed_ && srgbUiOutput_ && device_ != nil;
}
SurfaceStatus DrawTwoDLinear(const TwoDLinearDrawData &data) override {
  @autoreleasepool {
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    if (!SupportsTwoDLinear())
      return SurfaceStatus::Unsupported;
    if (!drawable_ || !commands_ || sceneDrawn_ || uiRendered_ ||
        !ValidateTwoDLinearDraws(data, width_, height_))
      return SurfaceStatus::InvalidDescriptor;
    for (const auto *packet : {&data.world, &data.overlay})
      for (const auto &upload : packet->textureUploads) {
        const auto old = uiTextureMetadata_.find(upload.textureId);
        if (old != uiTextureMetadata_.end() && old->second.immutable &&
            (!upload.immutable || old->second.width != upload.width ||
             old->second.height != upload.height || old->second.nearest != upload.nearest))
          return SurfaceStatus::InvalidDescriptor;
      }
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
    auto &f = twoDLinearFrames_[frame_];
    if (!CreateTwoDLinearTargets(f))
      return SurfaceStatus::DeviceLost;
    for (const auto *packet : {&data.world, &data.overlay})
      for (const auto &upload : packet->textureUploads)
        if (!UploadUiTexture(upload))
          return SurfaceStatus::DeviceLost;
    for (const auto *packet : {&data.world, &data.overlay})
      for (const auto &c : packet->commands)
        if (!uiTextures_.contains(c.textureId))
          return SurfaceStatus::InvalidDescriptor;
    const auto vertices = std::as_bytes(data.world.vertices);
    const auto instances = std::as_bytes(data.world.spriteInstances);
    const auto instanceOffset =
        (vertices.size() + data.world.indices.size() + 3U) & ~std::size_t(3);
    const auto quadOffset = (instanceOffset + instances.size() + 3U) & ~std::size_t(3);
    const UiVertex quad[] = {{{0, 0}, {0, 0}, 0xffffffff},
                             {{float(width_), 0}, {1, 0}, 0xffffffff},
                             {{float(width_), float(height_)}, {1, 1}, 0xffffffff},
                             {{0, float(height_)}, {0, 1}, 0xffffffff}};
    const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    const auto uniformOffset =
        (quadOffset + sizeof(quad) + sizeof(indices) + 255U) & ~std::size_t(255);
    const auto required = uniformOffset + sizeof(TwoDLightingUniform);
    if (f.capacity < required) {
      f.capacity = 4096;
      while (f.capacity < required)
        f.capacity *= 2;
      f.upload = [device_ newBufferWithLength:f.capacity options:MTLResourceStorageModeShared];
      if (!f.upload)
        return SurfaceStatus::DeviceLost;
      ++diagnostics_.nativeUiBufferReallocations;
    }
    auto *bytes = static_cast<std::byte *>(f.upload.contents);
    std::memcpy(bytes, vertices.data(), vertices.size());
    std::memcpy(bytes + vertices.size(), data.world.indices.data(), data.world.indices.size());
    if (!instances.empty())
      std::memcpy(bytes + instanceOffset, instances.data(), instances.size());
    std::memcpy(bytes + quadOffset, quad, sizeof(quad));
    std::memcpy(bytes + quadOffset + sizeof(quad), indices, sizeof(indices));
    const TwoDLightingUniform fallback{};
    std::memcpy(bytes + uniformOffset, data.lighting ? &data.lighting->uniforms : &fallback,
                sizeof(fallback));
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    for (unsigned i = 0; i < 2; ++i) {
      pass.colorAttachments[i].texture = f.colors[i];
      pass.colorAttachments[i].loadAction = MTLLoadActionClear;
      pass.colorAttachments[i].storeAction = MTLStoreActionStore;
      pass.colorAttachments[i].clearColor = MTLClearColorMake(0, 0, 0, 1);
    }
    auto encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
    if (!encoder)
      return SurfaceStatus::DeviceLost;
    TwoDShaderConstants transform{{2.F / width_, -2.F / height_, -1, 1}};
    [encoder setViewport:MTLViewport{0, 0, double(width_), double(height_), 0, 1}];
    [encoder setVertexBytes:&transform length:sizeof(transform) atIndex:0];
    [encoder setFragmentBuffer:f.upload offset:uniformOffset atIndex:1];
    std::size_t commandIndex{};
    for (const auto &c : data.world.commands) {
      transform.style = data.lighting ? data.lighting->styles[commandIndex]
                                      : TwoDLightingStyle{UINT32_MAX, 0, 0, 0};
      ++commandIndex;
      [encoder setFragmentBytes:&transform length:sizeof(transform) atIndex:0];
      const auto clipped = TwoDLinearClip(c, data.viewport, width_, height_);
      if (!clipped[2] || !clipped[3])
        continue;
      const auto texture = uiTextures_.at(c.textureId);
      const auto &meta = uiTextureMetadata_.at(c.textureId);
      [encoder setFragmentTexture:texture atIndex:0];
      [encoder setFragmentSamplerState:meta.nearest ? uiNearestSampler_ : uiSampler_ atIndex:0];
      [encoder setScissorRect:MTLScissorRect{clipped[0], clipped[1], clipped[2], clipped[3]}];
      if (c.instanceCount) {
        [encoder setRenderPipelineState:twoDLinearInstancePipeline_];
        [encoder setVertexBuffer:f.upload offset:instanceOffset atIndex:1];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:6
                  instanceCount:c.instanceCount
                   baseInstance:c.firstInstance];
        ++diagnostics_.spriteInstancedDrawCalls;
        diagnostics_.spriteGpuInstances += c.instanceCount;
      } else {
        const auto indexOffset =
            vertices.size() + c.indexOffset * (data.world.indices32Bit ? 4U : 2U);
        [encoder setRenderPipelineState:twoDLinearPipeline_];
        [encoder setVertexBuffer:f.upload offset:0 atIndex:1];
        [encoder
            drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexCount:c.elementCount
                        indexType:data.world.indices32Bit ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16
                      indexBuffer:f.upload
                indexBufferOffset:indexOffset
                    instanceCount:1
                       baseVertex:c.vertexOffset
                     baseInstance:0];
      }
      ++diagnostics_.nativeUiDrawCalls;
    }
    [encoder endEncoding];
    pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable_.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
    if (!encoder)
      return SurfaceStatus::DeviceLost;
    [encoder setRenderPipelineState:twoDLinearCompositePipeline_];
    [encoder setViewport:MTLViewport{0, 0, double(width_), double(height_), 0, 1}];
    [encoder setVertexBytes:&transform length:sizeof(transform) atIndex:0];
    [encoder setVertexBuffer:f.upload offset:quadOffset atIndex:1];
    [encoder setFragmentTexture:f.colors[0] atIndex:0];
    [encoder setFragmentSamplerState:uiSampler_ atIndex:0];
    const auto &v = data.viewport;
    const auto clip = v == std::array<std::uint32_t, 4>{} ? MTLScissorRect{0, 0, width_, height_}
                                                          : MTLScissorRect{v[0], v[1], v[2], v[3]};
    [encoder setScissorRect:clip];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:f.upload
                 indexBufferOffset:quadOffset + sizeof(quad)];
    [encoder endEncoding];
    sceneDrawn_ = true;
    ++diagnostics_.nativeUiDrawCalls;
    diagnostics_.spriteInstanceUploadBytes += instances.size();
    if (!data.overlay.commands.empty()) {
      const auto result = RenderUi(data.overlay);
      if (result != SurfaceStatus::Ready)
        return result;
    }
    ++diagnostics_.twoDLinearFrames;
    return SurfaceStatus::Ready;
  }
}
