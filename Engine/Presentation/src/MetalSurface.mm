#if !defined(__APPLE__)
#error "MetalSurface.mm is only built on Apple platforms"
#endif
#include "Nexora/Presentation/Surface.h"
#include "SceneInstanceUpload.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Nexora::Presentation {
namespace {
class MetalSurface final : public ISurface {
public:
  MetalSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows)
      : thread_(std::this_thread::get_id()), width_(descriptor.width), height_(descriptor.height) {
    @autoreleasepool {
      if (descriptor.framesInFlight < 2 || descriptor.framesInFlight > kFrames ||
          descriptor.colorSpace != ColorSpace::Srgb || !width_ || !height_)
        return;
      frames_ = descriptor.framesInFlight;
      auto *window = (__bridge NSWindow *)windows.NativeHandle(descriptor.window);
      device_ = MTLCreateSystemDefaultDevice();
      if (!window || !device_)
        return;
      layer_ = [CAMetalLayer layer];
      layer_.device = device_;
      layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
      // CompositeScene blits into the drawable, so framebuffer-only storage is insufficient.
      layer_.framebufferOnly = NO;
      layer_.maximumDrawableCount = frames_;
      layer_.displaySyncEnabled = descriptor.presentMode == PresentMode::VSync;
      layer_.contentsScale = window.backingScaleFactor;
      layer_.drawableSize = CGSizeMake(width_, height_);
      [window contentView].wantsLayer = YES;
      [window contentView].layer = layer_;
      queue_ = [device_ newCommandQueue];
      valid_ = queue_ != nil && CreateUiResources() && CreateSceneResources();
      diagnostics_.negotiatedPresentMode = descriptor.presentMode;
    }
  }
  ~MetalSurface() override { static_cast<void>(DrainAndDestroy()); }
  std::thread::id RenderThread() const noexcept override { return thread_; }
  SurfaceStatus NotifyWindowExtent(std::uint32_t width, std::uint32_t height) noexcept override {
    std::lock_guard lock(extentMutex_);
    pendingWidth_ = width;
    pendingHeight_ = height;
    dirty_ = true;
    return width && height ? SurfaceStatus::Ready : SurfaceStatus::ZeroExtent;
  }
  SurfaceStatus Acquire() override {
    @autoreleasepool {
      if (thread_ != std::this_thread::get_id())
        return SurfaceStatus::WrongThread;
      if (destroyed_)
        return SurfaceStatus::SurfaceLost;
      if (!valid_)
        return SurfaceStatus::Unsupported;
      if (drawable_ || commands_)
        return SurfaceStatus::InvalidDescriptor;
      {
        std::lock_guard lock(extentMutex_);
        if (dirty_) {
          width_ = pendingWidth_;
          height_ = pendingHeight_;
          dirty_ = false;
          if (!DrainFrames())
            return SurfaceStatus::DeviceLost;
          if (width_ && height_) {
            layer_.drawableSize = CGSizeMake(width_, height_);
            ++diagnostics_.resizeGenerations;
          }
        }
      }
      // Minimization persists until another nonzero extent is published.
      if (!width_ || !height_)
        return SurfaceStatus::ZeroExtent;
      frame_ = (frame_ + 1) % frames_;
      if (inflight_[frame_] != nil) {
        [inflight_[frame_] waitUntilCompleted];
        ++diagnostics_.fenceWaits;
        if (inflight_[frame_].status == MTLCommandBufferStatusError)
          return SurfaceStatus::DeviceLost;
        inflight_[frame_] = nil;
        retiredTextures_[frame_].clear();
      }
      drawable_ = [layer_ nextDrawable];
      if (!drawable_)
        return SurfaceStatus::OutOfDate;
      commands_ = [queue_ commandBuffer];
      if (!commands_)
        return SurfaceStatus::DeviceLost;
      uiRendered_ = sceneDrawn_ = sceneOffscreen_ = sceneComposited_ = false;
      ++diagnostics_.acquiredFrames;
      return SurfaceStatus::Ready;
    }
  }
  SurfaceStatus Present() override {
    @autoreleasepool {
      if (thread_ != std::this_thread::get_id())
        return SurfaceStatus::WrongThread;
      if (!drawable_)
        return SurfaceStatus::OutOfDate;
      if (sceneOffscreen_ && !sceneComposited_)
        return SurfaceStatus::InvalidDescriptor;
      if (!uiRendered_ && !sceneDrawn_) {
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable_.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.08, 0.16, 1.0);
        id<MTLRenderCommandEncoder> encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
        if (!encoder)
          return SurfaceStatus::DeviceLost;
        [encoder endEncoding];
      }
      [commands_ presentDrawable:drawable_];
      [commands_ commit];
      inflight_[frame_] = commands_;
      commands_ = nil;
      drawable_ = nil;
      uiRendered_ = false;
      ++diagnostics_.presentedFrames;
      return SurfaceStatus::Ready;
    }
  }
  SurfaceStatus RenderUi(const UiDrawData &drawData) override {
    @autoreleasepool {
      if (thread_ != std::this_thread::get_id())
        return SurfaceStatus::WrongThread;
      if (!drawable_ || !commands_ || uiRendered_ || (sceneOffscreen_ && !sceneComposited_) ||
          drawData.vertices.empty() || drawData.indices.empty())
        return SurfaceStatus::InvalidDescriptor;
      if (!ValidateUi(drawData))
        return SurfaceStatus::InvalidDescriptor;
      for (const auto &upload : drawData.textureUploads)
        if (!UploadUiTexture(upload))
          return SurfaceStatus::DeviceLost;
      const auto vertices = std::as_bytes(drawData.vertices);
      const auto required = vertices.size() + drawData.indices.size();
      if (uiCapacity_[frame_] < required) {
        uiCapacity_[frame_] = 4096;
        while (uiCapacity_[frame_] < required)
          uiCapacity_[frame_] *= 2;
        uiUploads_[frame_] = [device_ newBufferWithLength:uiCapacity_[frame_]
                                                  options:MTLResourceStorageModeShared];
        if (!uiUploads_[frame_])
          return SurfaceStatus::DeviceLost;
        ++diagnostics_.nativeUiBufferReallocations;
      }
      std::memcpy(uiUploads_[frame_].contents, vertices.data(), vertices.size());
      std::memcpy(static_cast<std::byte *>(uiUploads_[frame_].contents) + vertices.size(),
                  drawData.indices.data(), drawData.indices.size());
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = drawable_.texture;
      pass.colorAttachments[0].loadAction = sceneDrawn_ ? MTLLoadActionLoad : MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      pass.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.08, 0.16, 1.0);
      id<MTLRenderCommandEncoder> encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
      if (!encoder)
        return SurfaceStatus::DeviceLost;
      [encoder setRenderPipelineState:uiPipeline_];
      [encoder setCullMode:MTLCullModeNone];
      [encoder setVertexBuffer:uiUploads_[frame_] offset:0 atIndex:0];
      const float transform[4] = {2.0F / static_cast<float>(width_),
                                  -2.0F / static_cast<float>(height_), -1.0F, 1.0F};
      [encoder setVertexBytes:transform length:sizeof(transform) atIndex:1];
      [encoder setFragmentSamplerState:uiSampler_ atIndex:0];
      for (const auto &command : drawData.commands) {
        const auto texture = uiTextures_.find(command.textureId);
        if (texture == uiTextures_.end()) {
          ++diagnostics_.nativeUiRejectedTextures;
          continue;
        }
        [encoder setFragmentTexture:texture->second atIndex:0];
        const MTLScissorRect scissor{static_cast<NSUInteger>(command.clipX),
                                     static_cast<NSUInteger>(command.clipY), command.clipWidth,
                                     command.clipHeight};
        [encoder setScissorRect:scissor];
        [encoder
            drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexCount:command.elementCount
                        indexType:drawData.indices32Bit ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16
                      indexBuffer:uiUploads_[frame_]
                indexBufferOffset:vertices.size() +
                                  command.indexOffset *
                                      (drawData.indices32Bit ? 4U : 2U)instanceCount:1
                       baseVertex:command.vertexOffset
                     baseInstance:0];
        ++diagnostics_.nativeUiDrawCalls;
      }
      [encoder endEncoding];
      uiRendered_ = true;
      return SurfaceStatus::Ready;
    }
  }
  SurfaceStatus DrawScene(const SceneDrawData &drawData) override {
    @autoreleasepool {
      if (thread_ != std::this_thread::get_id())
        return SurfaceStatus::WrongThread;
      if (!drawable_ || !commands_ || sceneDrawn_ || !scenePipeline_)
        return SurfaceStatus::InvalidDescriptor;
      const auto viewport = ResolveSceneViewport(drawData.viewport, width_, height_);
      const bool defaultViewport = drawData.viewport.x == 0 && drawData.viewport.y == 0 &&
                                   drawData.viewport.width == 0 && drawData.viewport.height == 0;
      if (!viewport || (uiRendered_ && (drawData.offscreen || defaultViewport)) ||
          (drawData.offscreen && !defaultViewport) || !ValidateScene(drawData))
        return SurfaceStatus::InvalidDescriptor;
      const auto textureId = drawData.textureId ? drawData.textureId : UINT64_MAX;
      std::size_t additional =
          textureId == UINT64_MAX && !sceneTextures_.contains(textureId) ? 1 : 0;
      for (const auto &upload : drawData.textureUploads)
        additional += sceneTextures_.contains(upload.textureId) ? 0 : 1;
      if (sceneTextures_.size() + additional > 64)
        return SurfaceStatus::Unsupported;
      for (const auto &upload : drawData.textureUploads)
        if (!sceneTextures_.contains(upload.textureId)) {
          const auto texture = CreateTexture(upload);
          if (!texture)
            return SurfaceStatus::DeviceLost;
          sceneTextures_[upload.textureId] = texture;
          ++diagnostics_.sceneTextureUploads;
        }
      const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                           std::byte{255}};
      if (!sceneTextures_.contains(textureId)) {
        const auto texture = CreateTexture({UINT64_MAX, 1, 1, 4, white});
        if (!texture)
          return SurfaceStatus::DeviceLost;
        sceneTextures_[textureId] = texture;
        ++diagnostics_.sceneTextureUploads;
      }
      const SceneInstance identity{};
      const auto instances = drawData.instances.empty()
                                 ? std::span<const SceneInstance>(&identity, 1)
                                 : drawData.instances;
      const auto vertices = std::as_bytes(drawData.vertices);
      const auto indices = std::as_bytes(drawData.indices);
      const auto packedInstances = PackSceneInstances(instances);
      if (!packedInstances)
        return SurfaceStatus::InvalidDescriptor;
      const auto instanceBytes =
          std::as_bytes(std::span<const SceneInstanceUpload>(*packedInstances));
      const auto instanceOffset = (vertices.size() + indices.size() + 3) & ~std::size_t{3};
      const auto required = instanceOffset + instanceBytes.size();
      if (sceneCapacity_[frame_] < required) {
        auto capacity = std::size_t{4096};
        while (capacity < required)
          capacity *= 2;
        auto buffer = [device_ newBufferWithLength:capacity options:MTLResourceStorageModeShared];
        if (!buffer)
          return SurfaceStatus::DeviceLost;
        sceneUploads_[frame_] = buffer;
        sceneCapacity_[frame_] = capacity;
      }
      auto *storage = static_cast<std::byte *>(sceneUploads_[frame_].contents);
      std::memcpy(storage, vertices.data(), vertices.size());
      std::memcpy(storage + vertices.size(), indices.data(), indices.size());
      std::memcpy(storage + instanceOffset, instanceBytes.data(), instanceBytes.size());
      if (!EnsureSceneTargets(drawData.offscreen))
        return SurfaceStatus::DeviceLost;
      struct SceneConstants final {
        float mvp[16];
        float direction[4];
        float light[4];
        float color[4];
      } constants{};
      std::memcpy(constants.mvp, drawData.model_view_projection, sizeof(constants.mvp));
      std::memcpy(constants.direction, drawData.light_direction, sizeof(drawData.light_direction));
      std::memcpy(constants.light, drawData.light_color, sizeof(drawData.light_color));
      std::memcpy(constants.color, drawData.base_color, sizeof(constants.color));
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture =
          drawData.offscreen ? sceneColors_[frame_] : drawable_.texture;
      pass.colorAttachments[0].loadAction = uiRendered_ ? MTLLoadActionLoad : MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      pass.colorAttachments[0].clearColor = MTLClearColorMake(0.025, 0.045, 0.09, 1);
      pass.depthAttachment.texture = sceneDepths_[frame_];
      pass.depthAttachment.loadAction = MTLLoadActionClear;
      pass.depthAttachment.storeAction = MTLStoreActionDontCare;
      pass.depthAttachment.clearDepth = 1;
      id<MTLRenderCommandEncoder> encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
      if (!encoder)
        return SurfaceStatus::DeviceLost;
      [encoder setRenderPipelineState:scenePipeline_];
      [encoder setDepthStencilState:depthState_];
      [encoder setCullMode:MTLCullModeNone];
      [encoder setViewport:MTLViewport{static_cast<double>(viewport->x),
                                       static_cast<double>(viewport->y),
                                       static_cast<double>(viewport->width),
                                       static_cast<double>(viewport->height), 0, 1}];
      [encoder setScissorRect:MTLScissorRect{viewport->x, viewport->y, viewport->width,
                                             viewport->height}];
      [encoder setVertexBuffer:sceneUploads_[frame_] offset:0 atIndex:0];
      [encoder setVertexBuffer:sceneUploads_[frame_] offset:instanceOffset atIndex:1];
      [encoder setVertexBytes:&constants length:sizeof(constants) atIndex:2];
      [encoder setFragmentTexture:sceneTextures_.at(textureId) atIndex:0];
      [encoder setFragmentSamplerState:uiSampler_ atIndex:0];
      const SceneMeshBatch whole{0, static_cast<std::uint32_t>(drawData.indices.size()), 0,
                                 static_cast<std::uint32_t>(instances.size())};
      const auto batches =
          drawData.batches.empty() ? std::span<const SceneMeshBatch>(&whole, 1) : drawData.batches;
      for (const auto &batch : batches)
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:batch.indexCount
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:sceneUploads_[frame_]
                     indexBufferOffset:vertices.size() + batch.firstIndex * sizeof(std::uint16_t)
                         instanceCount:batch.instanceCount
                            baseVertex:0
                          baseInstance:batch.firstInstance];
      [encoder endEncoding];
      sceneDrawn_ = true;
      sceneOffscreen_ = drawData.offscreen;
      ++diagnostics_.sceneDrawCalls;
      diagnostics_.sceneInstances += instances.size();
      if (drawData.offscreen)
        ++diagnostics_.sceneOffscreenDrawCalls;
      return SurfaceStatus::Ready;
    }
  }
  SurfaceStatus CompositeScene() override {
    @autoreleasepool {
      if (thread_ != std::this_thread::get_id())
        return SurfaceStatus::WrongThread;
      if (!drawable_ || !commands_ || !sceneOffscreen_ || sceneComposited_ || uiRendered_)
        return SurfaceStatus::InvalidDescriptor;
      id<MTLBlitCommandEncoder> encoder = [commands_ blitCommandEncoder];
      if (!encoder)
        return SurfaceStatus::DeviceLost;
      [encoder copyFromTexture:sceneColors_[frame_]
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width_, height_, 1)
                     toTexture:drawable_.texture
              destinationSlice:0
              destinationLevel:0
             destinationOrigin:MTLOriginMake(0, 0, 0)];
      [encoder endEncoding];
      sceneComposited_ = true;
      ++diagnostics_.sceneComposites;
      return SurfaceStatus::Ready;
    }
  }
#if defined(NEXORA_METAL_SCENE_TESTING)
  // Compiled only into the native pixel test, never exported by the production surface.
  std::vector<std::byte> ReadScenePixelsForTesting() {
    const auto pitch = (static_cast<std::size_t>(width_) * 4 + 255) & ~std::size_t{255};
    auto buffer = [device_ newBufferWithLength:pitch * height_
                                       options:MTLResourceStorageModeShared];
    auto command = [queue_ commandBuffer];
    auto encoder = [command blitCommandEncoder];
    if (!buffer || !encoder || !sceneColors_[frame_])
      return {};
    [encoder copyFromTexture:sceneColors_[frame_]
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(width_, height_, 1)
                        toBuffer:buffer
               destinationOffset:0
          destinationBytesPerRow:pitch
        destinationBytesPerImage:pitch * height_];
    [encoder endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if (command.status == MTLCommandBufferStatusError)
      return {};
    std::vector<std::byte> pixels(static_cast<std::size_t>(width_) * height_ * 4);
    for (std::size_t y = 0; y < height_; ++y)
      std::memcpy(pixels.data() + y * width_ * 4,
                  static_cast<const std::byte *>(buffer.contents) + y * pitch, width_ * 4);
    return pixels;
  }
#endif
  SurfaceDiagnostics Diagnostics() const noexcept override { return diagnostics_; }
  SurfaceStatus DrainAndDestroy() override {
    @autoreleasepool {
      if (destroyed_)
        return SurfaceStatus::Ready;
      if (thread_ != std::this_thread::get_id())
        return SurfaceStatus::WrongThread;
      // Drop an abandoned recording before releasing its storage. Submitted recordings are drained.
      drawable_ = nil;
      commands_ = nil;
      valid_ = false;
      const bool drained = DrainFrames();
      for (std::size_t i = 0; i < kFrames; ++i) {
        uiUploads_[i] = nil;
        sceneUploads_[i] = nil;
        sceneDepths_[i] = nil;
        sceneColors_[i] = nil;
      }
      uiTextures_.clear();
      sceneTextures_.clear();
      uiPipeline_ = nil;
      scenePipeline_ = nil;
      depthState_ = nil;
      uiSampler_ = nil;
      layer_ = nil;
      queue_ = nil;
      device_ = nil;
      destroyed_ = true;
      return drained ? SurfaceStatus::Ready : SurfaceStatus::DeviceLost;
    }
  }

private:
  static constexpr std::size_t kFrames = 3;
  bool DrainFrames() {
    bool success = true;
    for (std::size_t i = 0; i < kFrames; ++i) {
      if (inflight_[i]) {
        [inflight_[i] waitUntilCompleted];
        ++diagnostics_.fenceWaits;
        if (inflight_[i].status == MTLCommandBufferStatusError) {
          diagnostics_.lastPlatformResult = static_cast<std::int64_t>(inflight_[i].error.code);
          success = false;
        }
        inflight_[i] = nil;
      }
      retiredTextures_[i].clear();
    }
    return success;
  }
  bool ValidateScene(const SceneDrawData &data) const {
    if (data.vertices.empty() || data.vertices.size() > 65535 || data.indices.empty() ||
        data.indices.size() > 1048576 || data.indices.size() % 3 != 0 ||
        data.instances.size() > 4096 || data.textureUploads.size() > 16 ||
        data.textureId == UINT64_MAX ||
        !ValidateSceneMeshBatches(data.batches, data.indices.size(),
                                  std::max<std::size_t>(data.instances.size(), 1)))
      return false;
    for (const auto index : data.indices)
      if (index >= data.vertices.size())
        return false;
    const auto finite = [](const auto &values) {
      return std::all_of(std::begin(values), std::end(values),
                         [](float v) { return std::isfinite(v); });
    };
    for (const auto &vertex : data.vertices)
      if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.uv))
        return false;
    if (!finite(data.model_view_projection) || !finite(data.light_direction) ||
        !finite(data.light_color) || !finite(data.base_color))
      return false;
    for (const auto &instance : data.instances) {
      SceneInstanceUpload packed;
      if (!PackSceneInstance(instance, packed))
        return false;
    }
    for (std::size_t i = 0; i < data.textureUploads.size(); ++i) {
      const auto &upload = data.textureUploads[i];
      if (!ValidTexture(upload) || upload.textureId == UINT64_MAX || upload.width > 1024 ||
          upload.height > 1024)
        return false;
      for (std::size_t j = 0; j < i; ++j)
        if (data.textureUploads[j].textureId == upload.textureId)
          return false;
    }
    return !data.textureId || sceneTextures_.contains(data.textureId) ||
           std::any_of(data.textureUploads.begin(), data.textureUploads.end(),
                       [&data](const auto &upload) { return upload.textureId == data.textureId; });
  }
  static bool ValidTexture(const UiTextureUpload &upload) {
    return upload.textureId != 0 && upload.width != 0 && upload.width <= 4096 &&
           upload.height != 0 && upload.height <= 4096 && upload.rowPitch == upload.width * 4U &&
           upload.pixels.size() == static_cast<std::size_t>(upload.rowPitch) * upload.height;
  }
  bool ValidateUi(const UiDrawData &data) const {
    const std::size_t indexSize = data.indices32Bit ? 4 : 2;
    if (data.vertices.size() > 1048576 || data.indices.size() > 4194304 ||
        data.indices.size() % indexSize != 0 || data.commands.size() > 65536 ||
        data.textureUploads.size() > 64)
      return false;
    for (const auto &upload : data.textureUploads)
      if (!ValidTexture(upload))
        return false;
    for (const auto &command : data.commands) {
      if (command.clipX < 0 || command.clipY < 0 || command.clipWidth == 0 ||
          command.clipHeight == 0 || static_cast<std::uint32_t>(command.clipX) >= width_ ||
          static_cast<std::uint32_t>(command.clipY) >= height_ ||
          command.clipWidth > width_ - static_cast<std::uint32_t>(command.clipX) ||
          command.clipHeight > height_ - static_cast<std::uint32_t>(command.clipY) ||
          command.indexOffset > data.indices.size() / indexSize ||
          command.elementCount > data.indices.size() / indexSize - command.indexOffset)
        return false;
      for (std::size_t i = command.indexOffset; i < command.indexOffset + command.elementCount;
           ++i) {
        std::uint32_t index = 0;
        std::memcpy(&index, data.indices.data() + i * indexSize, indexSize);
        const auto vertex = static_cast<std::int64_t>(index) + command.vertexOffset;
        if (vertex < 0 || static_cast<std::uint64_t>(vertex) >= data.vertices.size())
          return false;
      }
    }
    return true;
  }
  bool EnsureSceneTargets(bool offscreen) {
    const auto ensure = [&](id<MTLTexture> __strong &texture, MTLPixelFormat format) {
      if (texture && texture.width == width_ && texture.height == height_)
        return true;
      auto *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                            width:width_
                                                                           height:height_
                                                                        mipmapped:NO];
      descriptor.storageMode = MTLStorageModePrivate;
      descriptor.usage = MTLTextureUsageRenderTarget;
      texture = [device_ newTextureWithDescriptor:descriptor];
      return texture != nil;
    };
    return ensure(sceneDepths_[frame_], MTLPixelFormatDepth32Float) &&
           (!offscreen || ensure(sceneColors_[frame_], MTLPixelFormatBGRA8Unorm));
  }
  id<MTLTexture> CreateTexture(const UiTextureUpload &upload) {
    auto *descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:upload.width
                                                          height:upload.height
                                                       mipmapped:NO];
    // Managed storage supports discrete Intel/AMD Macs as well as Apple Silicon's shared storage.
    descriptor.storageMode =
        device_.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
    descriptor.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:descriptor];
    if (texture)
      [texture replaceRegion:MTLRegionMake2D(0, 0, upload.width, upload.height)
                 mipmapLevel:0
                   withBytes:upload.pixels.data()
                 bytesPerRow:upload.rowPitch];
    return texture;
  }
  bool CreateSceneResources() {
    constexpr const char *source = R"(
      #include <metal_stdlib>
      using namespace metal;
      struct Input {
        float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; float2 uv [[attribute(2)]];
        float4 model0 [[attribute(3)]]; float4 model1 [[attribute(4)]]; float4 model2 [[attribute(5)]];
        float4 normal0 [[attribute(6)]]; float4 normal1 [[attribute(7)]]; float4 normal2 [[attribute(8)]];
        float4 color [[attribute(9)]];
      };
      struct Scene { float4 rows[4]; float4 direction; float4 light; float4 color; };
      struct Output { float4 position [[position]]; float3 illumination; float2 uv; };
      float3 safeNormal(float3 value) {
        float magnitude = max(max(abs(value.x), abs(value.y)), abs(value.z));
        return magnitude > 0.0 ? normalize(value / magnitude) : float3(0);
      }
      vertex Output sceneVertex(Input input [[stage_in]], constant Scene &scene [[buffer(2)]]) {
        float4 local = float4(input.position, 1.0);
        float4 p = float4(dot(input.model0, local), dot(input.model1, local), dot(input.model2, local), 1.0);
        Output output;
        // Explicit row dot products preserve the public row-major matrix; Metal's Y axis matches DX12.
        output.position = float4(dot(scene.rows[0], p), dot(scene.rows[1], p), dot(scene.rows[2], p), dot(scene.rows[3], p));
        float3 localNormal = safeNormal(input.normal);
        float3 n = float3(dot(input.normal0.xyz, localNormal), dot(input.normal1.xyz, localNormal), dot(input.normal2.xyz, localNormal));
        float diffuse = max(dot(safeNormal(n), safeNormal(-scene.direction.xyz)), 0.0);
        output.illumination = scene.color.rgb * input.color.rgb * (float3(0.18) + scene.light.rgb * diffuse * 0.82);
        output.uv = input.uv; return output;
      }
      fragment float4 sceneFragment(Output input [[stage_in]], texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]]) {
        return float4(input.illumination, 1.0) * image.sample(imageSampler, input.uv);
      }
    )";
    NSError *error = nil;
    id<MTLLibrary> library = [device_ newLibraryWithSource:[NSString stringWithUTF8String:source]
                                                   options:nil
                                                     error:&error];
    if (!library)
      return false;
    auto *pipeline = [MTLRenderPipelineDescriptor new];
    pipeline.vertexFunction = [library newFunctionWithName:@"sceneVertex"];
    pipeline.fragmentFunction = [library newFunctionWithName:@"sceneFragment"];
    pipeline.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipeline.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    auto *vertices = [MTLVertexDescriptor vertexDescriptor];
    const MTLVertexFormat formats[]{
        MTLVertexFormatFloat3, MTLVertexFormatFloat3, MTLVertexFormatFloat2, MTLVertexFormatFloat4,
        MTLVertexFormatFloat4, MTLVertexFormatFloat4, MTLVertexFormatFloat4, MTLVertexFormatFloat4,
        MTLVertexFormatFloat4, MTLVertexFormatFloat4};
    const NSUInteger offsets[]{offsetof(SceneVertex, position),
                               offsetof(SceneVertex, normal),
                               offsetof(SceneVertex, uv),
                               offsetof(SceneInstanceUpload, model),
                               offsetof(SceneInstanceUpload, model) + 16,
                               offsetof(SceneInstanceUpload, model) + 32,
                               offsetof(SceneInstanceUpload, normal),
                               offsetof(SceneInstanceUpload, normal) + 16,
                               offsetof(SceneInstanceUpload, normal) + 32,
                               offsetof(SceneInstanceUpload, color)};
    for (NSUInteger i = 0; i < 10; ++i) {
      vertices.attributes[i].format = formats[i];
      vertices.attributes[i].offset = offsets[i];
      vertices.attributes[i].bufferIndex = i < 3 ? 0 : 1;
    }
    vertices.layouts[0].stride = sizeof(SceneVertex);
    vertices.layouts[1].stride = sizeof(SceneInstanceUpload);
    vertices.layouts[1].stepFunction = MTLVertexStepFunctionPerInstance;
    vertices.layouts[1].stepRate = 1;
    pipeline.vertexDescriptor = vertices;
    scenePipeline_ = [device_ newRenderPipelineStateWithDescriptor:pipeline error:&error];
    auto *depth = [MTLDepthStencilDescriptor new];
    depth.depthCompareFunction = MTLCompareFunctionLessEqual;
    depth.depthWriteEnabled = YES;
    depthState_ = [device_ newDepthStencilStateWithDescriptor:depth];
    return scenePipeline_ != nil && depthState_ != nil;
  }
  bool CreateUiResources() {
    constexpr const char *source = R"(
      #include <metal_stdlib>
      using namespace metal;
      struct Input { float2 position [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 color [[attribute(2)]]; };
      struct Output { float4 position [[position]]; float2 uv; float4 color; };
      vertex Output uiVertex(Input input [[stage_in]], constant float4 &transform [[buffer(1)]]) {
        Output output; output.position = float4(input.position * transform.xy + transform.zw, 0.0, 1.0); output.uv = input.uv; output.color = input.color; return output;
      }
      fragment float4 uiFragment(Output input [[stage_in]], texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]]) { return input.color * image.sample(imageSampler, input.uv); }
    )";
    NSError *error = nil;
    id<MTLLibrary> library = [device_ newLibraryWithSource:[NSString stringWithUTF8String:source]
                                                   options:nil
                                                     error:&error];
    if (!library)
      return false;
    MTLRenderPipelineDescriptor *pipeline = [MTLRenderPipelineDescriptor new];
    pipeline.vertexFunction = [library newFunctionWithName:@"uiVertex"];
    pipeline.fragmentFunction = [library newFunctionWithName:@"uiFragment"];
    pipeline.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipeline.colorAttachments[0].blendingEnabled = YES;
    pipeline.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipeline.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipeline.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    MTLVertexDescriptor *vertices = [MTLVertexDescriptor vertexDescriptor];
    vertices.attributes[0].format = MTLVertexFormatFloat2;
    vertices.attributes[0].offset = offsetof(UiVertex, position);
    vertices.attributes[0].bufferIndex = 0;
    vertices.attributes[1].format = MTLVertexFormatFloat2;
    vertices.attributes[1].offset = offsetof(UiVertex, uv);
    vertices.attributes[1].bufferIndex = 0;
    vertices.attributes[2].format = MTLVertexFormatUChar4Normalized;
    vertices.attributes[2].offset = offsetof(UiVertex, color);
    vertices.attributes[2].bufferIndex = 0;
    vertices.layouts[0].stride = sizeof(UiVertex);
    pipeline.vertexDescriptor = vertices;
    uiPipeline_ = [device_ newRenderPipelineStateWithDescriptor:pipeline error:&error];
    MTLSamplerDescriptor *sampler = [MTLSamplerDescriptor new];
    sampler.minFilter = MTLSamplerMinMagFilterLinear;
    sampler.magFilter = MTLSamplerMinMagFilterLinear;
    sampler.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler.tAddressMode = MTLSamplerAddressModeClampToEdge;
    uiSampler_ = [device_ newSamplerStateWithDescriptor:sampler];
    return uiPipeline_ != nil && uiSampler_ != nil;
  }
  bool UploadUiTexture(const UiTextureUpload &upload) {
    if (upload.textureId == 0 || upload.width == 0 || upload.height == 0 ||
        upload.rowPitch != upload.width * 4U ||
        upload.pixels.size() != static_cast<std::size_t>(upload.rowPitch) * upload.height)
      return false;
    id<MTLTexture> texture = CreateTexture(upload);
    if (!texture)
      return false;
    if (const auto previous = uiTextures_.find(upload.textureId); previous != uiTextures_.end())
      retiredTextures_[frame_].push_back(previous->second);
    uiTextures_[upload.textureId] = texture;
    ++diagnostics_.nativeUiTextureUploads;
    return true;
  }
  std::thread::id thread_;
  std::uint32_t width_{}, height_{};
  std::mutex extentMutex_;
  std::uint32_t pendingWidth_{}, pendingHeight_{};
  bool dirty_{};
  bool valid_{}, destroyed_{};
  bool uiRendered_{}, sceneDrawn_{}, sceneOffscreen_{}, sceneComposited_{};
  std::size_t frames_ = kFrames;
  std::size_t frame_{};
  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  id<MTLCommandBuffer> commands_ = nil;
  id<MTLRenderPipelineState> uiPipeline_ = nil;
  id<MTLSamplerState> uiSampler_ = nil;
  id<MTLRenderPipelineState> scenePipeline_ = nil;
  id<MTLDepthStencilState> depthState_ = nil;
  std::array<id<MTLBuffer>, kFrames> sceneUploads_{};
  std::array<std::size_t, kFrames> sceneCapacity_{};
  std::array<id<MTLTexture>, kFrames> sceneDepths_{};
  std::array<id<MTLTexture>, kFrames> sceneColors_{};
  std::unordered_map<std::uint64_t, id<MTLTexture>> sceneTextures_;
  std::array<id<MTLCommandBuffer>, kFrames> inflight_{};
  std::array<id<MTLBuffer>, kFrames> uiUploads_{};
  std::array<std::size_t, kFrames> uiCapacity_{};
  std::array<std::vector<id<MTLTexture>>, kFrames> retiredTextures_;
  std::unordered_map<std::uint64_t, id<MTLTexture>> uiTextures_;
  CAMetalLayer *layer_ = nil;
  id<CAMetalDrawable> drawable_ = nil;
  SurfaceDiagnostics diagnostics_{};
};
} // namespace
std::unique_ptr<ISurface> CreateMetalSurface(const SurfaceDescriptor &descriptor,
                                             Window::IWindowSystem &windows) {
  return std::make_unique<MetalSurface>(descriptor, windows);
}
} // namespace Nexora::Presentation
