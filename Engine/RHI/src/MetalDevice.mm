#if !defined(__APPLE__)
#error "MetalDevice.mm is only built on Apple platforms"
#endif

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/Device.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace nexora::rhi {
namespace {

std::uint64_t Key(TextureHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

std::uint64_t Key(BufferHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

std::uint64_t Key(PipelineHandle handle) {
  return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
}

MTLPixelFormat ToPixelFormat(TextureFormat format) {
  switch (format) {
  case TextureFormat::Rgba8Unorm:
    return MTLPixelFormatRGBA8Unorm;
  case TextureFormat::Bgra8Unorm:
    return MTLPixelFormatBGRA8Unorm;
  case TextureFormat::Depth32Float:
    throw std::invalid_argument("Metal triangle backend only supports color textures");
  case TextureFormat::Rgba16Float:
    break; // External Presentation-owned HDR metadata; native triangle allocation is unsupported.
  }
  throw std::invalid_argument("unsupported Metal texture format");
}

std::string ErrorDescription(NSError *error) {
  if (!error)
    return "unknown Metal error";
  const auto *message = error.localizedDescription.UTF8String;
  return message ? message : "unknown Metal error";
}

constexpr char kFallbackMetalShader[] = R"(
#include <metal_stdlib>
using namespace metal;
struct FrameConstants { float4x4 transform; };
struct VertexOutput {
  float4 position [[position]];
  float3 color;
};
vertex VertexOutput vertexMain(uint id [[vertex_id]],
                               constant FrameConstants &frame [[buffer(0)]]) {
  const float2 positions[3] = {float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5)};
  const float3 colors[3] = {float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
                            float3(0.0, 0.0, 1.0)};
  VertexOutput output;
  output.position = frame.transform * float4(positions[id], 0.0, 1.0);
  output.color = colors[id];
  return output;
}
fragment float4 fragmentMain(VertexOutput input [[stage_in]]) {
  return float4(input.color, 1.0);
}
kernel void computeMain(uint3 id [[thread_position_in_grid]]) { (void)id; }
)";

class MetalDevice;

class MetalCommandList final : public CommandList {
public:
  MetalCommandList(MetalDevice &device, QueueType queue);
  ~MetalCommandList() override;

  void Transition(const Barrier &barrier) override;
  void TransitionBuffer(const BufferBarrier &barrier) override;
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) override;
  void BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset, std::uint32_t stride) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void Dispatch(std::uint32_t groups_x, std::uint32_t groups_y, std::uint32_t groups_z) override;
  void DrawIndirect(std::uint32_t command_count) override;
  void EndRendering() override;

  [[nodiscard]] bool IsClosed() const noexcept { return !rendering_; }
  [[nodiscard]] bool IsSubmitted() const noexcept { return submitted_; }
  [[nodiscard]] bool BelongsTo(const MetalDevice &device) const noexcept {
    return &device_ == &device;
  }
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  [[nodiscard]] std::uint64_t Dispatches() const noexcept { return dispatches_; }
  [[nodiscard]] std::uint64_t IndirectDraws() const noexcept { return indirect_draws_; }
  void MarkSubmitted() noexcept { submitted_ = true; }
  void Close();

private:
  friend class MetalDevice;
  struct TextureSnapshot final {
    TextureHandle handle;
    std::uint64_t revision;
    ResourceState state;
    TextureFormat format;
    std::uint32_t width;
    std::uint32_t height;
    id<MTLTexture> texture;
  };
  TextureSnapshot &SnapshotTexture(TextureHandle texture);
  std::unordered_map<std::uint64_t, TextureSnapshot> texture_snapshots_;
  MetalDevice &device_;
  id<MTLCommandBuffer> command_buffer_{};
  id<MTLRenderCommandEncoder> encoder_{};
  id<MTLComputeCommandEncoder> compute_encoder_{};
  bool rendering_{false};
  bool pipeline_bound_{false};
  PipelineType pipeline_type_{PipelineType::Graphics};
  bool submitted_{false};
  bool closed_{false};
  TextureHandle render_target_{};
  std::uint32_t width_{};
  std::uint32_t height_{};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
  std::uint64_t dispatches_{};
  std::uint64_t indirect_draws_{};
  id<MTLBuffer> indirect_buffer_{};
  std::uint64_t indirect_offset_{};
  std::uint64_t indirect_size_{};
  std::uint32_t indirect_stride_{};
  std::array<BufferHandle, 4> storage_buffers_{};
  std::array<bool, 4> storage_bound_{};
};

class MetalDevice final : public Device {
  struct TextureRecord;
  struct PipelineRecord;
  struct BufferRecord;

public:
  MetalDevice();
  ~MetalDevice() override = default;

  Backend GetBackend() const noexcept override { return Backend::Metal; }
  GraphicsDeviceCapabilities Capabilities() const noexcept override {
    GraphicsDeviceCapabilities result;
    result.backend = Backend::Metal;
    result.independent_compute_recording = true;
    result.independent_graphics_recording = true;
    result.dxil_shader_model_support = CapabilitySupport::Unsupported;
    for (const auto feature : {GraphicsFeature::Compute, GraphicsFeature::IndirectDraw})
      result.features[static_cast<std::size_t>(feature)] = {
          CapabilitySupport::Supported, {1, 0}, true, true};
    result.features[static_cast<std::size_t>(GraphicsFeature::ParallelCommandRecording)] = {
        CapabilitySupport::Supported, {1, 0}, false, false};
    return result;
  }
  TextureHandle CreateTexture(const TextureDescriptor &descriptor) override;
  void DestroyTexture(TextureHandle texture) override;
  void ReadTextureForTesting(TextureHandle texture, std::span<std::byte> data) override;
  BufferHandle CreateBuffer(const BufferDescriptor &descriptor) override;
  void WriteBuffer(BufferHandle buffer, std::uint64_t offset,
                   std::span<const std::byte> data) override;
  void ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                            std::span<std::byte> data) override;
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
  friend class MetalCommandList;
  void Require(bool condition, const char *message);
  TextureRecord &ValidateTextureLocked(TextureHandle texture);
  PipelineRecord &ValidatePipeline(PipelineHandle pipeline);
  BufferRecord &ValidateBuffer(BufferHandle buffer);
  BufferRecord &ValidateBufferLocked(BufferHandle buffer);
  void LoadShaderLibrary();

  id<MTLDevice> device_{};
  id<MTLCommandQueue> queue_{};
  id<MTLLibrary> library_{};
  id<MTLLibrary> compute_library_{};
  bool canonical_compute_{};
  id<MTLFunction> vertex_function_{};
  id<MTLFunction> fragment_function_{};
  id<MTLFunction> compute_function_{};
  id<MTLBuffer> uniform_buffer_{};
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

struct MetalDevice::TextureRecord final {
  TextureDescriptor descriptor;
  id<MTLTexture> texture{};
  ResourceState logical_state{ResourceState::Undefined};
  std::uint64_t revision{};
};

struct MetalDevice::PipelineRecord final {
  PipelineDescriptor descriptor;
  id<MTLRenderPipelineState> pipeline{};
  id<MTLComputePipelineState> compute_pipeline{};
};

struct MetalDevice::BufferRecord final {
  BufferDescriptor descriptor;
  id<MTLBuffer> buffer{};
};

MetalDevice::MetalDevice() {
  @autoreleasepool {
    device_ = MTLCreateSystemDefaultDevice();
    if (!device_)
      throw std::runtime_error("Metal has no default device");
    queue_ = [device_ newCommandQueue];
    if (!queue_)
      throw std::runtime_error("Metal command queue creation failed");
    uniform_buffer_ = [device_ newBufferWithLength:64 options:MTLResourceStorageModeShared];
    if (!uniform_buffer_)
      throw std::runtime_error("Metal uniform buffer creation failed");
    const float identity[16] = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    std::memcpy(uniform_buffer_.contents, identity, sizeof(identity));
    LoadShaderLibrary();
  }
}

void MetalDevice::LoadShaderLibrary() {
  std::string source = kFallbackMetalShader;
  if (const auto *path = std::getenv("NEXORA_SLANG_METAL_PATH"); path && *path != '\0') {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
      throw std::runtime_error(std::string("cannot open Metal shader artifact: ") + path);
    const auto size = file.tellg();
    if (size <= 0)
      throw std::runtime_error("Metal shader artifact is empty");
    source.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(source.data(), size);
  }
  NSString *source_string = [[NSString alloc] initWithBytes:source.data()
                                                     length:source.size()
                                                   encoding:NSUTF8StringEncoding];
  NSError *error = nil;
  library_ = [device_ newLibraryWithSource:source_string options:nil error:&error];
  if (!library_)
    throw std::runtime_error("Metal shader library compilation failed: " + ErrorDescription(error));
  vertex_function_ = [library_ newFunctionWithName:@"vertexMain"];
  fragment_function_ = [library_ newFunctionWithName:@"fragmentMain"];
  compute_function_ = [library_ newFunctionWithName:@"computeMain"];
  if (!vertex_function_ || !fragment_function_)
    throw std::runtime_error("Metal shader library has no canonical triangle entry points");
  if (const auto *path = std::getenv("NEXORA_SLANG_COMPUTE_METAL_PATH"); path && *path != '\0') {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0)
      throw std::runtime_error("Metal compute artifact is missing or empty");
    std::string compute_source(static_cast<std::size_t>(file.tellg()), '\0');
    file.seekg(0);
    if (!file.read(compute_source.data(), static_cast<std::streamsize>(compute_source.size())))
      throw std::runtime_error("cannot read Metal compute artifact");
    NSString *text = [[NSString alloc] initWithBytes:compute_source.data()
                                              length:compute_source.size()
                                            encoding:NSUTF8StringEncoding];
    if (!text)
      throw std::runtime_error("Metal compute artifact is not UTF-8");
    NSError *compute_error = nil;
    compute_library_ = [device_ newLibraryWithSource:text options:nil error:&compute_error];
    if (!compute_library_)
      throw std::runtime_error("Metal GPU-driven shader compilation failed: " +
                               ErrorDescription(compute_error));
    compute_function_ = [compute_library_ newFunctionWithName:@"computeMain"];
    if (!compute_function_)
      throw std::runtime_error("Metal GPU-driven artifact has no computeMain entry point");
    canonical_compute_ = true;
  }
  if (!compute_function_) {
    NSString *compute_source = @"#include <metal_stdlib>\nusing namespace metal;\n"
                                "kernel void computeMain(uint3 id [[thread_position_in_grid]]) "
                                "{ (void)id; }";
    NSError *compute_error = nil;
    id<MTLLibrary> compute_library = [device_ newLibraryWithSource:compute_source
                                                           options:nil
                                                             error:&compute_error];
    if (!compute_library)
      throw std::runtime_error("Metal compute shader compilation failed: " +
                               ErrorDescription(compute_error));
    compute_function_ = [compute_library newFunctionWithName:@"computeMain"];
    if (!compute_function_)
      throw std::runtime_error("Metal fallback compute shader has no computeMain entry point");
  }
}

TextureHandle MetalDevice::CreateTexture(const TextureDescriptor &descriptor) {
  @autoreleasepool {
    if (descriptor.width == 0 || descriptor.height == 0)
      throw std::invalid_argument("invalid texture extent");
    MTLTextureDescriptor *texture_descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:ToPixelFormat(descriptor.format)
                                                           width:descriptor.width
                                                          height:descriptor.height
                                                       mipmapped:NO];
    texture_descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    texture_descriptor.storageMode = MTLStorageModePrivate;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:texture_descriptor];
    if (!texture)
      throw std::runtime_error("Metal texture creation failed");
    TextureRecord record{descriptor, texture, descriptor.initial_state};
    std::lock_guard lock{mutex_};
    const auto handle = texture_pool_.Create();
    textures_.emplace(Key(handle), std::move(record));
    return handle;
  }
}

void MetalDevice::ReadTextureForTesting(TextureHandle texture, std::span<std::byte> data) {
  @autoreleasepool {
    WaitIdle();
    id<MTLBuffer> staging = nil;
    id<MTLCommandBuffer> commands = nil;
    std::size_t row_bytes{};
    std::size_t row_pitch{};
    std::uint32_t height{};
    {
      std::lock_guard lock{mutex_};
      auto &record = ValidateTextureLocked(texture);
      Require(record.descriptor.format == TextureFormat::Rgba8Unorm ||
                  record.descriptor.format == TextureFormat::Bgra8Unorm,
              "Metal texture readback requires a four-byte color format");
      Require(record.logical_state != ResourceState::Undefined,
              "reading a Metal texture with undefined contents");
      row_bytes = static_cast<std::size_t>(record.descriptor.width) * 4;
      height = record.descriptor.height;
      Require(data.size() == row_bytes * height,
              "texture readback size must be width * height * 4");
      row_pitch = (row_bytes + 255) & ~std::size_t{255};
      staging = [device_ newBufferWithLength:row_pitch * height
                                     options:MTLResourceStorageModeShared];
      commands = [queue_ commandBuffer];
      if (!staging || !commands)
        throw std::runtime_error("Metal texture readback allocation failed");
      id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
      if (!blit)
        throw std::runtime_error("Metal texture readback encoder creation failed");
      [blit copyFromTexture:record.texture
                       sourceSlice:0
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:MTLSizeMake(record.descriptor.width, height, 1)
                          toBuffer:staging
                 destinationOffset:0
            destinationBytesPerRow:row_pitch
          destinationBytesPerImage:row_pitch * height];
      [blit endEncoding];
      [commands commit];
      ++record.revision;
    }
    [commands waitUntilCompleted];
    if (commands.status == MTLCommandBufferStatusError)
      throw std::runtime_error("Metal texture readback failed: " +
                               ErrorDescription(commands.error));
    for (std::uint32_t row = 0; row < height; ++row)
      std::memcpy(data.data() + row * row_bytes,
                  static_cast<const std::byte *>(staging.contents) + row * row_pitch, row_bytes);
    std::lock_guard lock{mutex_};
    ++diagnostics_.readbacks;
  }
}

void MetalDevice::DestroyTexture(TextureHandle texture) {
  std::lock_guard lock{mutex_};
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "destroying invalid Metal texture");
  textures_.erase(found);
  Require(texture_pool_.Destroy(texture), "destroying stale Metal texture");
}

BufferHandle MetalDevice::CreateBuffer(const BufferDescriptor &descriptor) {
  if (descriptor.size == 0)
    throw std::invalid_argument("invalid Metal buffer size");
  id<MTLBuffer> buffer = [device_ newBufferWithLength:descriptor.size
                                              options:MTLResourceStorageModeShared];
  if (!buffer)
    throw std::runtime_error("Metal buffer creation failed");
  std::lock_guard lock{mutex_};
  const auto handle = buffer_pool_.Create();
  buffers_.emplace(Key(handle), BufferRecord{descriptor, buffer});
  return handle;
}

void MetalDevice::WriteBuffer(BufferHandle buffer, std::uint64_t offset,
                              std::span<const std::byte> data) {
  std::lock_guard lock{mutex_};
  auto &record = ValidateBufferLocked(buffer);
  Require(offset <= record.descriptor.size && data.size() <= record.descriptor.size - offset,
          "Metal buffer upload exceeds allocation");
  std::memcpy(static_cast<std::byte *>(record.buffer.contents) + offset, data.data(), data.size());
}

void MetalDevice::ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                                       std::span<std::byte> data) {
  WaitIdle();
  std::lock_guard lock{mutex_};
  auto &record = ValidateBufferLocked(buffer);
  Require(offset <= record.descriptor.size && data.size() <= record.descriptor.size - offset,
          "Metal buffer readback exceeds allocation");
  if (data.empty())
    return;
  // These buffers are shared, hazard-tracked allocations. Queue completion makes GPU writes
  // visible to the CPU; no managed/private-storage synchronization or production readback.
  std::memcpy(data.data(), static_cast<const std::byte *>(record.buffer.contents) + offset,
              data.size());
  ++diagnostics_.readbacks;
}

void MetalDevice::DestroyBuffer(BufferHandle buffer) {
  std::lock_guard lock{mutex_};
  const auto found = buffers_.find(Key(buffer));
  Require(buffer_pool_.Contains(buffer) && found != buffers_.end(),
          "destroying invalid Metal buffer");
  buffers_.erase(found);
  Require(buffer_pool_.Destroy(buffer), "destroying stale Metal buffer");
}

PipelineHandle MetalDevice::CreatePipeline(const PipelineDescriptor &descriptor) {
  @autoreleasepool {
    RequireGraphicsCapabilities(Capabilities(), descriptor.graphics_requirements);
    if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0)
      throw std::invalid_argument("pipeline hashes must be non-zero");
    NSError *error = nil;
    PipelineRecord record{descriptor};
    if (descriptor.type == PipelineType::Compute) {
      record.compute_pipeline = [device_ newComputePipelineStateWithFunction:compute_function_
                                                                       error:&error];
      if (!record.compute_pipeline)
        throw std::runtime_error("Metal compute pipeline creation failed: " +
                                 ErrorDescription(error));
    } else {
      MTLRenderPipelineDescriptor *pipeline_descriptor = [MTLRenderPipelineDescriptor new];
      pipeline_descriptor.vertexFunction = vertex_function_;
      pipeline_descriptor.fragmentFunction = fragment_function_;
      pipeline_descriptor.colorAttachments[0].pixelFormat = ToPixelFormat(descriptor.color_format);
      pipeline_descriptor.colorAttachments[0].blendingEnabled = NO;
      record.pipeline = [device_ newRenderPipelineStateWithDescriptor:pipeline_descriptor
                                                                error:&error];
      if (!record.pipeline)
        throw std::runtime_error("Metal pipeline creation failed: " + ErrorDescription(error));
    }
    std::lock_guard lock{mutex_};
    const auto handle = pipeline_pool_.Create();
    pipelines_.emplace(Key(handle), std::move(record));
    return handle;
  }
}

void MetalDevice::DestroyPipeline(PipelineHandle pipeline) {
  std::lock_guard lock{mutex_};
  const auto found = pipelines_.find(Key(pipeline));
  Require(pipeline_pool_.Contains(pipeline) && found != pipelines_.end(),
          "destroying invalid Metal pipeline");
  pipelines_.erase(found);
  Require(pipeline_pool_.Destroy(pipeline), "destroying stale Metal pipeline");
}

std::unique_ptr<CommandList> MetalDevice::CreateCommandList(QueueType queue) {
  if (queue != QueueType::Graphics && queue != QueueType::Compute)
    throw std::invalid_argument("Metal backend only supports graphics and compute queues");
  return std::make_unique<MetalCommandList>(*this, queue);
}

void MetalDevice::Require(bool condition, const char *message) {
  if (condition)
    return;
  ++diagnostics_.validation_errors;
  throw std::logic_error(message);
}

MetalDevice::TextureRecord &MetalDevice::ValidateTextureLocked(TextureHandle texture) {
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "command list references invalid Metal texture");
  return found->second;
}

MetalDevice::PipelineRecord &MetalDevice::ValidatePipeline(PipelineHandle pipeline) {
  std::lock_guard lock{mutex_};
  const auto found = pipelines_.find(Key(pipeline));
  Require(pipeline_pool_.Contains(pipeline) && found != pipelines_.end(),
          "binding invalid Metal pipeline");
  return found->second;
}

MetalDevice::BufferRecord &MetalDevice::ValidateBuffer(BufferHandle buffer) {
  std::lock_guard lock{mutex_};
  return ValidateBufferLocked(buffer);
}

MetalDevice::BufferRecord &MetalDevice::ValidateBufferLocked(BufferHandle buffer) {
  const auto found = buffers_.find(Key(buffer));
  Require(buffer_pool_.Contains(buffer) && found != buffers_.end(),
          "operation references invalid Metal buffer");
  return found->second;
}

std::uint64_t MetalDevice::Submit(CommandList &commands) {
  @autoreleasepool {
    auto *validated = dynamic_cast<MetalCommandList *>(&commands);
    {
      std::lock_guard lock{mutex_};
      Require(validated != nullptr, "command list belongs to another device");
      Require(validated->BelongsTo(*this), "command list belongs to another device");
      Require(validated->IsClosed(), "cannot submit an open command list");
      Require(!validated->IsSubmitted(), "command list was already submitted");
      for (const auto &entry : validated->texture_snapshots_) {
        Require(ValidateTextureLocked(entry.second.handle).revision == entry.second.revision,
                "stale Metal texture recording snapshot");
      }
      validated->Close();
      [validated->command_buffer_ commit];
      for (const auto &entry : validated->texture_snapshots_) {
        auto &texture = textures_.at(entry.first);
        texture.logical_state = entry.second.state;
        ++texture.revision;
      }
      validated->MarkSubmitted();
      ++diagnostics_.submitted_command_lists;
      diagnostics_.barriers += validated->Barriers();
      diagnostics_.draw_calls += validated->DrawCalls();
      diagnostics_.compute_dispatches += validated->Dispatches();
      diagnostics_.indirect_draw_calls += validated->IndirectDraws();
    }
    [validated->command_buffer_ waitUntilCompleted];
    if (validated->command_buffer_.status == MTLCommandBufferStatusError) {
      throw std::runtime_error("Metal command buffer failed: " +
                               ErrorDescription(validated->command_buffer_.error));
    }
    std::lock_guard lock{mutex_};
    completed_submission_ = ++submitted_submission_;
    return submitted_submission_;
  }
}

std::uint64_t MetalDevice::CompletedSubmissionValue() const noexcept {
  std::lock_guard lock{mutex_};
  return completed_submission_;
}

void MetalDevice::WaitForSubmission(std::uint64_t value) {
  std::lock_guard lock{mutex_};
  Require(value <= submitted_submission_, "waiting for an unknown Metal submission");
}

void MetalDevice::Present(TextureHandle texture) {
  std::lock_guard lock{mutex_};
  const auto found = textures_.find(Key(texture));
  Require(texture_pool_.Contains(texture) && found != textures_.end(),
          "presenting invalid Metal texture");
  Require(found->second.logical_state == ResourceState::Present,
          "present texture is not in Present state");
  ++diagnostics_.presents;
}

void MetalDevice::WaitIdle() {
  @autoreleasepool {
    id<MTLCommandBuffer> command_buffer = [queue_ commandBuffer];
    if (!command_buffer)
      throw std::runtime_error("Metal idle command buffer creation failed");
    [command_buffer commit];
    [command_buffer waitUntilCompleted];
    if (command_buffer.status == MTLCommandBufferStatusError)
      throw std::runtime_error("Metal idle command buffer failed: " +
                               ErrorDescription(command_buffer.error));
  }
}

DeviceDiagnostics MetalDevice::Diagnostics() const noexcept {
  std::lock_guard lock{mutex_};
  return diagnostics_;
}

MetalCommandList::MetalCommandList(MetalDevice &device, QueueType queue) : device_(device) {
  @autoreleasepool {
    (void)queue;
    command_buffer_ = [device_.queue_ commandBuffer];
    if (!command_buffer_)
      throw std::runtime_error("Metal command buffer creation failed");
  }
}

MetalCommandList::~MetalCommandList() {
  @autoreleasepool {
    if (encoder_)
      [encoder_ endEncoding];
    if (compute_encoder_)
      [compute_encoder_ endEncoding];
    encoder_ = nil;
    compute_encoder_ = nil;
    command_buffer_ = nil;
    indirect_buffer_ = nil;
    texture_snapshots_.clear();
  }
}

MetalCommandList::TextureSnapshot &MetalCommandList::SnapshotTexture(TextureHandle handle) {
  std::lock_guard lock{device_.mutex_};
  const auto &texture = device_.ValidateTextureLocked(handle);
  const auto key = Key(handle);
  auto entry = texture_snapshots_.find(key);
  if (entry == texture_snapshots_.end())
    entry = texture_snapshots_
                .emplace(key, TextureSnapshot{handle, texture.revision, texture.logical_state,
                                              texture.descriptor.format, texture.descriptor.width,
                                              texture.descriptor.height, texture.texture})
                .first;
  device_.Require(entry->second.revision == texture.revision,
                  "Metal texture changed during command recording");
  return entry->second;
}

void MetalCommandList::Transition(const Barrier &barrier) {
  @autoreleasepool {
    if (submitted_ || closed_)
      throw std::logic_error("cannot record a submitted command list");
    if (rendering_)
      throw std::logic_error("Metal barriers cannot occur inside rendering");
    if (!IsValidTextureBarrier(barrier))
      throw std::invalid_argument("invalid Metal texture barrier");
    auto &texture = SnapshotTexture(barrier.texture);
    if (texture.state != barrier.before) {
      std::lock_guard lock{device_.mutex_};
      device_.Require(false, "Metal barrier before-state mismatch");
    }
    if (barrier.before == barrier.after)
      return;
    texture.state = barrier.after;
    // Metal's hazard tracking orders accesses across encoders and command buffers. The
    // render-graph state transition remains explicit for validation and diagnostics.
    ++barriers_;
  }
}

void MetalCommandList::TransitionBuffer(const BufferBarrier &barrier) {
  @autoreleasepool {
    if (submitted_ || closed_ || rendering_)
      throw std::logic_error("buffer barriers require an open command list outside rendering");
    if (!IsValidBufferBarrier(barrier))
      throw std::invalid_argument("invalid buffer barrier");
    (void)device_.ValidateBuffer(barrier.buffer);
    // Tracked shared buffers are synchronized across encoder boundaries on the native queue.
    if (compute_encoder_) {
      [compute_encoder_ endEncoding];
      compute_encoder_ = nil;
      pipeline_bound_ = false;
    }
    ++barriers_;
  }
}

void MetalCommandList::BeginRendering(const RenderingInfo &info) {
  @autoreleasepool {
    if (submitted_ || closed_ || rendering_ || info.width == 0 || info.height == 0)
      throw std::logic_error("invalid Metal BeginRendering");
    if (compute_encoder_) {
      [compute_encoder_ endEncoding];
      compute_encoder_ = nil;
    }
    const auto &target = SnapshotTexture(info.color_target);
    if (target.state != ResourceState::RenderTarget || info.width > target.width ||
        info.height > target.height)
      throw std::logic_error("invalid Metal render target state/extent");
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    encoder_ = [command_buffer_ renderCommandEncoderWithDescriptor:pass];
    if (!encoder_)
      throw std::runtime_error("Metal render encoder creation failed");
    render_target_ = info.color_target;
    width_ = info.width;
    height_ = info.height;
    rendering_ = true;
    pipeline_bound_ = false;
  }
}

void MetalCommandList::BindPipeline(PipelineHandle pipeline) {
  @autoreleasepool {
    if (submitted_ || closed_)
      throw std::logic_error("cannot bind a pipeline on a submitted Metal command list");
    auto &record = device_.ValidatePipeline(pipeline);
    pipeline_type_ = record.descriptor.type;
    if ((pipeline_type_ == PipelineType::Graphics) != rendering_)
      throw std::logic_error("Metal pipeline type does not match rendering state");
    if (pipeline_type_ == PipelineType::Compute) {
      if (!compute_encoder_)
        compute_encoder_ = [command_buffer_ computeCommandEncoder];
      if (!compute_encoder_)
        throw std::runtime_error("Metal compute encoder creation failed");
      [compute_encoder_ setComputePipelineState:record.compute_pipeline];
      for (std::uint32_t index = 0; index < storage_buffers_.size(); ++index) {
        if (storage_bound_[index]) {
          auto &buffer = device_.ValidateBuffer(storage_buffers_[index]);
          [compute_encoder_ setBuffer:buffer.buffer offset:0 atIndex:index];
        }
      }
    } else {
      const auto &target = SnapshotTexture(render_target_);
      if (record.descriptor.color_format != target.format) {
        std::lock_guard lock{device_.mutex_};
        device_.Require(false, "Metal pipeline format does not match render target");
      }
      [encoder_ setRenderPipelineState:record.pipeline];
      [encoder_ setVertexBuffer:device_.uniform_buffer_ offset:0 atIndex:0];
    }
    pipeline_bound_ = true;
  }
}

void MetalCommandList::BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) {
  @autoreleasepool {
    if (submitted_ || closed_ || rendering_ || binding >= storage_buffers_.size())
      throw std::logic_error("invalid Metal storage-buffer binding");
    auto &record = device_.ValidateBuffer(buffer);
    storage_buffers_[binding] = buffer;
    storage_bound_[binding] = true;
    if (compute_encoder_)
      [compute_encoder_ setBuffer:record.buffer offset:0 atIndex:binding];
  }
}

void MetalCommandList::BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset,
                                          std::uint32_t stride) {
  @autoreleasepool {
    if (submitted_ || closed_ || stride != GPUDrivenIndirectCommandStride ||
        offset % alignof(std::uint32_t) != 0)
      throw std::logic_error("invalid Metal indirect-buffer binding");
    auto &record = device_.ValidateBuffer(buffer);
    if (offset > record.descriptor.size ||
        record.descriptor.size - offset < DrawIndirectArgumentSize)
      throw std::logic_error("Metal indirect-buffer binding exceeds allocation");
    indirect_buffer_ = record.buffer;
    indirect_offset_ = offset;
    indirect_size_ = record.descriptor.size;
    indirect_stride_ = stride;
  }
}

void MetalCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  @autoreleasepool {
    if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ || vertex_count == 0 ||
        instance_count == 0)
      throw std::logic_error("invalid Metal draw");
    [encoder_ drawPrimitives:MTLPrimitiveTypeTriangle
                 vertexStart:0
                 vertexCount:vertex_count
               instanceCount:instance_count
                baseInstance:0];
    ++draws_;
  }
}

void MetalCommandList::Dispatch(std::uint32_t groups_x, std::uint32_t groups_y,
                                std::uint32_t groups_z) {
  @autoreleasepool {
    if (submitted_ || closed_ || rendering_ || !pipeline_bound_ ||
        pipeline_type_ != PipelineType::Compute || !compute_encoder_ || groups_x == 0 ||
        groups_y == 0 || groups_z == 0)
      throw std::logic_error("invalid Metal dispatch");
    if (device_.canonical_compute_) {
      for (std::size_t index = 0; index < storage_bound_.size(); ++index) {
        if (!storage_bound_[index])
          throw std::logic_error("Metal GPU-driven dispatch requires all storage bindings");
        (void)device_.ValidateBuffer(storage_buffers_[index]);
      }
    }
    const MTLSize threadsPerGroup = MTLSizeMake(1, 1, 1);
    const MTLSize threadgroups = MTLSizeMake(groups_x, groups_y, groups_z);
    [compute_encoder_ dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerGroup];
    ++dispatches_;
  }
}

void MetalCommandList::DrawIndirect(std::uint32_t command_count) {
  @autoreleasepool {
    if (submitted_ || closed_ || !rendering_ || !pipeline_bound_ ||
        pipeline_type_ != PipelineType::Graphics || !indirect_buffer_ ||
        indirect_stride_ != GPUDrivenIndirectCommandStride || command_count == 0)
      throw std::logic_error("invalid Metal indirect draw");
    if (!IndirectDrawRangeFits(indirect_size_, indirect_offset_, indirect_stride_, command_count))
      throw std::logic_error("Metal indirect draw reads past the bound indirect buffer");
    for (std::uint32_t command = 0; command < command_count; ++command) {
      [encoder_ drawPrimitives:MTLPrimitiveTypeTriangle
                indirectBuffer:indirect_buffer_
          indirectBufferOffset:indirect_offset_ +
                               static_cast<std::uint64_t>(command) * indirect_stride_];
    }
    draws_ += command_count;
    ++indirect_draws_;
  }
}

void MetalCommandList::EndRendering() {
  @autoreleasepool {
    if (submitted_ || closed_ || !rendering_)
      throw std::logic_error("Metal EndRendering without BeginRendering");
    [encoder_ endEncoding];
    encoder_ = nil;
    rendering_ = false;
  }
}

void MetalCommandList::Close() {
  if (closed_)
    return;
  if (rendering_)
    throw std::logic_error("cannot close a Metal command list during rendering");
  if (compute_encoder_) {
    [compute_encoder_ endEncoding];
    compute_encoder_ = nil;
  }
  closed_ = true;
}

} // namespace

std::unique_ptr<Device> CreateMetalDevice() { return std::make_unique<MetalDevice>(); }
} // namespace nexora::rhi
