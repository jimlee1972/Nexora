// Included inside MetalSurface. Targets and upload storage belong to completed frame slots.
private:
bool twoDLinearRecorded_{};
struct TwoDBloomTarget {
  id<MTLTexture> image = nil;
  std::uint32_t width{}, height{};
  std::uint64_t bytes{};
};
struct TwoDLinearFrame {
  id<MTLCounterSampleBuffer> effectsQueries = nil;
  TwoDEffectsFrameDiagnostics pending;
  std::chrono::steady_clock::time_point effectsSubmittedAt{};
  uint32_t profilingStatus{}, counterMode{};
  std::array<TwoDBloomTarget, 9> bloom;

  std::array<id<MTLTexture>, 2> colors{};
  id<MTLBuffer> upload = nil, shadowBuffer = nil;
  std::uint64_t shadowBytes{};
  std::size_t capacity{};
  std::uint32_t width{}, height{};
  std::uint64_t allocationBytes{};
};
std::array<TwoDLinearFrame, 3> twoDLinearFrames_;
id<MTLComputePipelineState> twoDShadowPipeline_ = nil;
id<MTLRenderPipelineState> twoDBloomPipeline_ = nil;
id<MTLRenderPipelineState> twoDLinearPipeline_ = nil;
id<MTLRenderPipelineState> twoDLinearInstancePipeline_ = nil;
id<MTLRenderPipelineState> twoDLinearCompositePipeline_ = nil;
void UpdateTwoDResourceStats() noexcept {
  uint64_t upload{};
  for (const auto &slot : twoDLinearFrames_)
    upload += slot.capacity;
  detail::TrackTwoDResources(diagnostics_, upload, uiTextures_.size());
}
void InitializeTwoDEffectsTiming(TwoDLinearFrame &f) {
  f.profilingStatus = !gpuTimingEnabled_ ? 0 : 3;
  if (!gpuTimingEnabled_)
    return;
  if (@available(macOS 11.0, *)) {
    const bool stage = [device_ supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
    const bool commands =
        [device_ supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary] &&
        [device_ supportsCounterSampling:MTLCounterSamplingPointAtDispatchBoundary];
    if (!stage && !commands)
      return;
    id<MTLCounterSet> timestampSet = nil;
    for (id<MTLCounterSet> set in device_.counterSets)
      if ([set.name isEqualToString:MTLCommonCounterSetTimestamp]) {
        timestampSet = set;
        break;
      }
    if (!timestampSet)
      return;
    auto descriptor = [MTLCounterSampleBufferDescriptor new];
    descriptor.counterSet = timestampSet;
    descriptor.sampleCount = 8;
    descriptor.storageMode = MTLStorageModeShared;
    NSError *error = nil;
    f.effectsQueries = [device_ newCounterSampleBufferWithDescriptor:descriptor error:&error];
    f.counterMode = stage ? 1 : 2;
    f.profilingStatus = f.effectsQueries ? 1 : 4;
  }
}
void CollectTwoDEffects(TwoDLinearFrame &f) noexcept {
  if (!f.pending.completedFrame)
    return;
  auto completed = f.pending;
  completed.timingSource = f.effectsQueries ? 3U : 0U;
  completed.timingStatus = f.profilingStatus;
  completed.timingMask = 0;
  if (@available(macOS 11.0, *)) {
    if (f.effectsQueries) {
      NSData *resolved = [f.effectsQueries resolveCounterRange:NSMakeRange(0, 8)];
      if (resolved && resolved.length == 8 * sizeof(MTLCounterResultTimestamp)) {
        std::array<MTLCounterResultTimestamp, 8> values{};
        std::memcpy(values.data(), resolved.bytes, sizeof(values));
        for (unsigned pass = 0; pass < 4; ++pass) {
          const auto begin = values[2 * pass].timestamp, end = values[2 * pass + 1].timestamp;
          if (!completed.work[pass])
            continue;
          if (!begin || !end || begin == MTLCounterErrorValue || end == MTLCounterErrorValue) {
            completed.timingStatus = 3;
            continue;
          }
          // Metal's timestamp counter values are native nanoseconds, not command-buffer CPU times.
          const auto measured = detail::TimestampMilliseconds(
              begin, end, 64, 1.0, detail::CompletionWallMilliseconds(f.effectsSubmittedAt));
          if (measured) {
            completed.milliseconds[pass] = *measured;
            completed.timingMask |= 1U << pass;
          } else
            completed.timingStatus = 3;
        }
      } else
        completed.timingStatus = 4;
    }
  }
  detail::PublishTwoDEffects(diagnostics_, completed);
  f.pending = {};
}
void AttachTwoDRenderTiming(MTLRenderPassDescriptor *pass, TwoDLinearFrame &f, NSUInteger begin,
                            NSUInteger end) {
  if (@available(macOS 11.0, *)) {
    if (f.effectsQueries && f.counterMode == 1) {
      auto attachment = pass.sampleBufferAttachments[0];
      attachment.sampleBuffer = f.effectsQueries;
      attachment.startOfVertexSampleIndex = begin;
      attachment.endOfVertexSampleIndex = MTLCounterDontSample;
      attachment.startOfFragmentSampleIndex = MTLCounterDontSample;
      attachment.endOfFragmentSampleIndex = end;
    }
  }
}
void SampleTwoDRenderTiming(id<MTLRenderCommandEncoder> encoder, TwoDLinearFrame &f,
                            NSUInteger marker) {
  if (@available(macOS 11.0, *)) {
    if (f.effectsQueries && f.counterMode == 2 && marker != MTLCounterDontSample)
      [encoder sampleCountersInBuffer:f.effectsQueries atSampleIndex:marker withBarrier:YES];
  }
}
void SampleTwoDComputeTiming(id<MTLComputeCommandEncoder> encoder, TwoDLinearFrame &f,
                             NSUInteger marker) {
  if (@available(macOS 11.0, *)) {
    if (f.effectsQueries && f.counterMode == 2)
      [encoder sampleCountersInBuffer:f.effectsQueries atSampleIndex:marker withBarrier:YES];
  }
}
void ResetTwoDLinearFrame(TwoDLinearFrame &f) {
  diagnostics_.twoDLinearTargetBytes -= f.allocationBytes;
  for (const auto &target : f.bloom)
    diagnostics_.twoDBloomTargetBytes -= target.bytes;
  diagnostics_.twoDShadowBufferBytes -= f.shadowBytes;
  f = {};
  UpdateTwoDResourceStats();
}
void ResetTwoDLinearFrames() {
  for (auto &f : twoDLinearFrames_)
    ResetTwoDLinearFrame(f);
}
bool CreateTwoDLinearPipelines() {
  if (twoDLinearPipeline_ && twoDLinearInstancePipeline_ && twoDLinearCompositePipeline_ &&
      twoDShadowPipeline_ && twoDBloomPipeline_)
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
  auto shadowLibrary = library(twod_linear_metal_shadow);
  auto bloomLibrary = library(twod_linear_metal_bloom);
  if (!vertexLibrary || !instanceLibrary || !fragmentLibrary || !compositeLibrary ||
      !shadowLibrary || !bloomLibrary)
    return false;
  auto vertex = [vertexLibrary newFunctionWithName:@"twoDVertexMain"];
  auto instance = [instanceLibrary newFunctionWithName:@"twoDInstanceMain"];
  auto fragment = [fragmentLibrary newFunctionWithName:@"twoDFragmentMain"];
  auto composite = [compositeLibrary newFunctionWithName:@"twoDCompositeMain"];
  auto shadow = [shadowLibrary newFunctionWithName:@"twoDShadowMain"];
  auto bloom = [bloomLibrary newFunctionWithName:@"twoDBloomMain"];
  if (shadow)
    twoDShadowPipeline_ = [device_ newComputePipelineStateWithFunction:shadow error:&error];
  if (!vertex || !instance || !fragment || !composite || !twoDShadowPipeline_ || !bloom)
    return false;
  for (unsigned mode = 0; mode < 4; ++mode) {
    auto desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = mode == 1 ? instance : vertex;
    desc.fragmentFunction = mode == 3 ? bloom : mode == 2 ? composite : fragment;
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
    for (unsigned i = 0; i < (mode >= 2 ? 1U : 2U); ++i) {
      auto color = desc.colorAttachments[i];
      color.pixelFormat = mode == 2 ? layer_.pixelFormat : MTLPixelFormatRGBA16Float;
      color.blendingEnabled = mode < 2;
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
    else if (mode == 2)
      twoDLinearCompositePipeline_ = pipeline;
    else
      twoDBloomPipeline_ = pipeline;
  }
  return true;
}
bool EnsureTwoDBloomTarget(TwoDBloomTarget &target, uint32_t width, uint32_t height) {
  if (target.image && target.width == width && target.height == height)
    return true;
  diagnostics_.twoDBloomTargetBytes -= target.bytes;
  target = {};
  auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                 width:width
                                                                height:height
                                                             mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModePrivate;
  target.image = [device_ newTextureWithDescriptor:desc];
  if (!target.image)
    return false;
  target.width = width;
  target.height = height;
  target.bytes = static_cast<uint64_t>(target.image.allocatedSize);
  diagnostics_.twoDBloomTargetBytes += target.bytes;
  return true;
}
bool DrawTwoDBloom(TwoDLinearFrame &f, const TwoDLinearDrawData &data, std::size_t quadOffset) {
  const auto plan =
      PrepareTwoDBloom(width_, height_, data.lighting ? &data.lighting->uniforms : nullptr);
  std::array<bool, 9> used{};
  for (const auto &step : plan.steps)
    used[unsigned(step.output)] = true;
  for (unsigned i = 0; i < used.size(); ++i)
    if (!used[i]) {
      diagnostics_.twoDBloomTargetBytes -= f.bloom[i].bytes;
      f.bloom[i] = {};
    }
  for (const auto &step : plan.steps) {
    auto &target = f.bloom[unsigned(step.output)];
    if (!EnsureTwoDBloomTarget(target, step.width, step.height))
      return false;
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target.image;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    const NSUInteger begin = &step == &plan.steps.front() ? 4 : MTLCounterDontSample;
    const NSUInteger end = &step == &plan.steps.back() ? 5 : MTLCounterDontSample;
    AttachTwoDRenderTiming(pass, f, begin, end);
    auto encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
    if (!encoder)
      return false;
    SampleTwoDRenderTiming(encoder, f, begin);
    [encoder setRenderPipelineState:twoDBloomPipeline_];
    TwoDShaderConstants constants{{2.F / width_, -2.F / height_, -1, 1}};
    constants.style[3] = step.up ? 1 : 0;
    constants.emission = {step.texel[0], step.texel[1], 0, 0};
    [encoder setVertexBytes:&constants length:sizeof(constants) atIndex:0];
    [encoder setFragmentBytes:&constants length:sizeof(constants) atIndex:0];
    [encoder setVertexBuffer:f.upload offset:quadOffset atIndex:1];
    [encoder setFragmentTexture:step.source < 0 ? f.colors[1] : f.bloom[unsigned(step.source)].image
                        atIndex:0];
    [encoder setFragmentTexture:step.detail < 0 ? f.colors[1] : f.bloom[unsigned(step.detail)].image
                        atIndex:1];
    [encoder setFragmentSamplerState:uiSampler_ atIndex:0];
    [encoder setFragmentSamplerState:uiSampler_ atIndex:1];
    [encoder setViewport:MTLViewport{0, 0, double(step.width), double(step.height), 0, 1}];
    [encoder setScissorRect:MTLScissorRect{0, 0, step.width, step.height}];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:f.upload
                 indexBufferOffset:quadOffset + sizeof(UiVertex) * 4];
    SampleTwoDRenderTiming(encoder, f, end);
    [encoder endEncoding];
    ++f.pending.work[2];
    ++diagnostics_.twoDBloomPasses;
    ++diagnostics_.nativeUiDrawCalls;
  }
  return true;
}
bool CreateTwoDLinearTargets(TwoDLinearFrame &f) {
  if (f.width == width_ && f.height == height_ && f.colors[0] && f.colors[1] && f.shadowBuffer)
    return true;
  for (const auto &target : f.bloom)
    diagnostics_.twoDBloomTargetBytes -= target.bytes;
  ResetTwoDLinearFrame(f);
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
  f.shadowBuffer = [device_ newBufferWithLength:8 * 1024 * sizeof(float)
                                        options:MTLResourceStorageModePrivate];
  if (!f.shadowBuffer)
    return false;
  f.shadowBytes = static_cast<std::uint64_t>(f.shadowBuffer.allocatedSize);
  diagnostics_.twoDShadowBufferBytes += f.shadowBytes;
  InitializeTwoDEffectsTiming(f);
  ++diagnostics_.twoDLinearTargetAllocations;
  return true;
}

public:
bool SupportsTwoDLinear() const noexcept override {
  return valid_ && !destroyed_ && srgbUiOutput_ && device_ != nil;
}
bool SupportsTwoDShadows() const noexcept override { return SupportsTwoDLinear(); }
bool SupportsTwoDBloom() const noexcept override { return SupportsTwoDLinear(); }

SurfaceStatus DrawTwoDLinear(const TwoDLinearDrawData &data) override {
  @autoreleasepool {
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    if (!SupportsTwoDLinear())
      return SurfaceStatus::Unsupported;
    if (!drawable_ || !commands_ || sceneDrawn_ || uiRendered_ ||
        !ValidateTwoDLinearDraws(data, width_, height_))
      return SurfaceStatus::InvalidDescriptor;
    twoDLinearRecorded_ = true;
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
    if (data.lighting) {
      for (const auto &style : data.lighting->styles) {
        if (!style.emissionTexture)
          continue;
        bool known = uiTextures_.contains(style.emissionTexture);
        bool nearest = false;
        if (known)
          nearest = uiTextureMetadata_.at(style.emissionTexture).nearest;
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
    f.pending = {};
    f.pending.effectsRevision = data.lighting ? data.lighting->effectsRevision : 0;
    f.effectsSubmittedAt = std::chrono::steady_clock::now();
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
    const auto edgeOffset =
        (uniformOffset + sizeof(TwoDLightingUniform) + 255U) & ~std::size_t(255);
    const auto edgeBytes =
        std::max<std::size_t>(1, data.lighting ? data.lighting->edges.size() : 0) *
        sizeof(TwoDShadowEdge);
    const auto required = edgeOffset + edgeBytes;
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
    std::memset(bytes + edgeOffset, 0, edgeBytes);
    if (data.lighting && !data.lighting->edges.empty())
      std::memcpy(bytes + edgeOffset, data.lighting->edges.data(),
                  data.lighting->edges.size_bytes());
    if (data.lighting && data.lighting->uniforms.counts[1]) {
      id<MTLComputeCommandEncoder> compute = nil;
      if (@available(macOS 11.0, *)) {
        if (f.effectsQueries && f.counterMode == 1) {
          auto pass = [MTLComputePassDescriptor computePassDescriptor];
          pass.sampleBufferAttachments[0].sampleBuffer = f.effectsQueries;
          pass.sampleBufferAttachments[0].startOfEncoderSampleIndex = 0;
          pass.sampleBufferAttachments[0].endOfEncoderSampleIndex = 1;
          compute = [commands_ computeCommandEncoderWithDescriptor:pass];
        } else
          compute = [commands_ computeCommandEncoder];
      } else
        compute = [commands_ computeCommandEncoder];

      if (!compute)
        return SurfaceStatus::DeviceLost;
      SampleTwoDComputeTiming(compute, f, 0);
      [compute setComputePipelineState:twoDShadowPipeline_];
      [compute setBuffer:f.upload offset:uniformOffset atIndex:1];
      [compute setBuffer:f.upload offset:edgeOffset atIndex:5];
      [compute setBuffer:f.shadowBuffer offset:0 atIndex:7];
      [compute dispatchThreadgroups:MTLSizeMake(16, data.lighting->uniforms.counts[1], 1)
              threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
      SampleTwoDComputeTiming(compute, f, 1);
      [compute endEncoding];
      ++f.pending.work[0];
      ++diagnostics_.twoDShadowDispatches;
      diagnostics_.twoDShadowSamples += uint64_t(data.lighting->uniforms.counts[1]) * 1024;
    }
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    for (unsigned i = 0; i < 2; ++i) {
      pass.colorAttachments[i].texture = f.colors[i];
      pass.colorAttachments[i].loadAction = MTLLoadActionClear;
      pass.colorAttachments[i].storeAction = MTLStoreActionStore;
      pass.colorAttachments[i].clearColor = MTLClearColorMake(0, 0, 0, 1);
    }
    AttachTwoDRenderTiming(pass, f, 2, 3);
    auto encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
    if (!encoder)
      return SurfaceStatus::DeviceLost;
    SampleTwoDRenderTiming(encoder, f, 2);
    TwoDShaderConstants transform{{2.F / width_, -2.F / height_, -1, 1}};
    [encoder setViewport:MTLViewport{0, 0, double(width_), double(height_), 0, 1}];
    [encoder setVertexBytes:&transform length:sizeof(transform) atIndex:0];
    [encoder setFragmentBuffer:f.upload offset:uniformOffset atIndex:1];
    [encoder setFragmentBuffer:f.shadowBuffer offset:0 atIndex:6];
    std::size_t commandIndex{};
    for (const auto &c : data.world.commands) {
      transform.SetStyle(data.lighting ? data.lighting->styles[commandIndex]
                                       : TwoDLightingStyle{UINT32_MAX, 0, 0, 0});
      ++commandIndex;
      [encoder setFragmentBytes:&transform length:sizeof(transform) atIndex:0];
      const auto clipped = TwoDLinearClip(c, data.viewport, width_, height_);
      if (!clipped[2] || !clipped[3])
        continue;
      const auto texture = uiTextures_.at(c.textureId);
      const auto &meta = uiTextureMetadata_.at(c.textureId);
      [encoder setFragmentTexture:texture atIndex:0];
      const auto maskId =
          data.lighting ? data.lighting->styles[commandIndex - 1].emissionTexture : 0;
      [encoder setFragmentTexture:uiTextures_.at(maskId ? maskId : c.textureId) atIndex:1];
      [encoder setFragmentSamplerState:uiSampler_ atIndex:1];
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
      ++f.pending.work[1];
      if (transform.emission[3] > 0)
        ++diagnostics_.twoDEmissionDraws;
      ++diagnostics_.nativeUiDrawCalls;
    }
    SampleTwoDRenderTiming(encoder, f, 3);
    [encoder endEncoding];
    if (!DrawTwoDBloom(f, data, quadOffset))
      return SurfaceStatus::DeviceLost;
    pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable_.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    AttachTwoDRenderTiming(pass, f, 6, 7);
    encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
    if (!encoder)
      return SurfaceStatus::DeviceLost;
    SampleTwoDRenderTiming(encoder, f, 6);
    [encoder setRenderPipelineState:twoDLinearCompositePipeline_];
    transform.SetCompositeViewport(width_, height_, data.viewport);
    [encoder setViewport:MTLViewport{0, 0, double(width_), double(height_), 0, 1}];
    [encoder setVertexBytes:&transform length:sizeof(transform) atIndex:0];
    [encoder setVertexBuffer:f.upload offset:quadOffset atIndex:1];
    [encoder setFragmentTexture:f.colors[0] atIndex:0];
    [encoder setFragmentBytes:&transform length:sizeof(transform) atIndex:0];
    [encoder setFragmentBuffer:f.upload offset:uniformOffset atIndex:1];
    [encoder setFragmentBuffer:f.shadowBuffer offset:0 atIndex:6];
    const auto bloomPlan =
        PrepareTwoDBloom(width_, height_, data.lighting ? &data.lighting->uniforms : nullptr);
    [encoder setFragmentTexture:bloomPlan.result < 0 ? f.colors[1]
                                                     : f.bloom[unsigned(bloomPlan.result)].image
                        atIndex:1];
    [encoder setFragmentSamplerState:uiSampler_ atIndex:1];
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
    SampleTwoDRenderTiming(encoder, f, 7);
    f.pending.work[3] = 1;
    [encoder endEncoding];
    sceneDrawn_ = true;
    ++diagnostics_.nativeUiDrawCalls;
    diagnostics_.spriteInstanceUploadBytes += instances.size();
    if (!data.overlay.commands.empty()) {
      const auto result = RenderUi(data.overlay);
      if (result != SurfaceStatus::Ready)
        return result;
    }
    f.pending.completedFrame = diagnostics_.twoDLinearFrames + 1;
    UpdateTwoDResourceStats();
    ++diagnostics_.twoDLinearFrames;
    return SurfaceStatus::Ready;
  }
}
