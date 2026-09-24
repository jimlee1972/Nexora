#if !defined(__APPLE__)
#error "MetalSurface.mm is only built on Apple platforms"
#endif
#include "Nexora/Presentation/Surface.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Nexora::Presentation {
namespace {
class MetalSurface final : public ISurface {
public:
  MetalSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows)
      : thread_(std::this_thread::get_id()), width_(descriptor.width), height_(descriptor.height) {
    auto *window = (__bridge NSWindow *)windows.NativeHandle(descriptor.window);
    device_ = MTLCreateSystemDefaultDevice();
    if (!window || !device_)
      return;
    layer_ = [CAMetalLayer layer];
    layer_.device = device_;
    layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer_.framebufferOnly = YES;
    layer_.displaySyncEnabled = descriptor.presentMode == PresentMode::VSync;
    layer_.drawableSize = CGSizeMake(width_, height_);
    [window contentView].wantsLayer = YES;
    [window contentView].layer = layer_;
    queue_ = [device_ newCommandQueue];
    valid_ = queue_ != nil && CreateUiResources();
    diagnostics_.negotiatedPresentMode = descriptor.presentMode;
  }
  ~MetalSurface() override { static_cast<void>(DrainAndDestroy()); }
  std::thread::id RenderThread() const noexcept override { return thread_; }
  SurfaceStatus NotifyWindowExtent(std::uint32_t width, std::uint32_t height) noexcept override {
    pendingWidth_.store(width);
    pendingHeight_.store(height);
    dirty_.store(true);
    return width && height ? SurfaceStatus::Ready : SurfaceStatus::ZeroExtent;
  }
  SurfaceStatus Acquire() override {
    if (thread_ != std::this_thread::get_id())
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
      layer_.drawableSize = CGSizeMake(width_, height_);
      ++diagnostics_.resizeGenerations;
    }
    drawable_ = [layer_ nextDrawable];
    if (!drawable_)
      return SurfaceStatus::OutOfDate;
    frame_ = (frame_ + 1) % kFrames;
    if (inflight_[frame_] != nil) {
      [inflight_[frame_] waitUntilCompleted];
      if (inflight_[frame_].status == MTLCommandBufferStatusError)
        return SurfaceStatus::DeviceLost;
      inflight_[frame_] = nil;
      retiredTextures_[frame_].clear();
    }
    commands_ = [queue_ commandBuffer];
    if (!commands_)
      return SurfaceStatus::DeviceLost;
    ++diagnostics_.acquiredFrames;
    return SurfaceStatus::Ready;
  }
  SurfaceStatus Present() override {
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    if (!drawable_)
      return SurfaceStatus::OutOfDate;
    if (!uiRendered_) {
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = drawable_.texture;
      pass.colorAttachments[0].loadAction = MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      pass.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.08, 0.16, 1.0);
      [[commands_ renderCommandEncoderWithDescriptor:pass] endEncoding];
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
  SurfaceStatus RenderUi(const UiDrawData &drawData) override {
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    if (!drawable_ || !commands_ || drawData.vertices.empty() || drawData.indices.empty())
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
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.08, 0.16, 1.0);
    id<MTLRenderCommandEncoder> encoder = [commands_ renderCommandEncoderWithDescriptor:pass];
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
      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
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
  SurfaceDiagnostics Diagnostics() const noexcept override { return diagnostics_; }
  SurfaceStatus DrainAndDestroy() override {
    if (destroyed_)
      return SurfaceStatus::Ready;
    if (thread_ != std::this_thread::get_id())
      return SurfaceStatus::WrongThread;
    id<MTLCommandBuffer> barrier = [queue_ commandBuffer];
    [barrier commit];
    [barrier waitUntilCompleted];
    drawable_ = nil;
    commands_ = nil;
    for (auto &inflight : inflight_)
      inflight = nil;
    uiTextures_.clear();
    uiPipeline_ = nil;
    uiSampler_ = nil;
    layer_ = nil;
    queue_ = nil;
    device_ = nil;
    destroyed_ = true;
    return SurfaceStatus::Ready;
  }

private:
  static constexpr std::size_t kFrames = 3;
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
    id<MTLLibrary> library =
        [device_ newLibraryWithSource:[NSString stringWithUTF8String:source]
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
    MTLTextureDescriptor *descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:upload.width
                                                          height:upload.height
                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:descriptor];
    if (!texture)
      return false;
    [texture replaceRegion:MTLRegionMake2D(0, 0, upload.width, upload.height)
               mipmapLevel:0
                 withBytes:upload.pixels.data()
               bytesPerRow:upload.rowPitch];
    if (const auto previous = uiTextures_.find(upload.textureId); previous != uiTextures_.end())
      retiredTextures_[frame_].push_back(previous->second);
    uiTextures_[upload.textureId] = texture;
    ++diagnostics_.nativeUiTextureUploads;
    return true;
  }
  std::thread::id thread_;
  std::uint32_t width_{}, height_{};
  std::atomic<std::uint32_t> pendingWidth_{}, pendingHeight_{};
  std::atomic_bool dirty_{};
  bool valid_{}, destroyed_{};
  bool uiRendered_{};
  std::size_t frame_{};
  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  id<MTLCommandBuffer> commands_ = nil;
  id<MTLRenderPipelineState> uiPipeline_ = nil;
  id<MTLSamplerState> uiSampler_ = nil;
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
