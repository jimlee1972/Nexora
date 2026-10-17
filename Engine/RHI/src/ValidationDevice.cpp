#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/Device.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace nexora::rhi {
namespace {
class ValidationDevice;

class ValidationCommandList final : public CommandList {
public:
  explicit ValidationCommandList(ValidationDevice &device) : device_(device) {}
  void Transition(const Barrier &barrier) override;
  void TransitionBuffer(const BufferBarrier &barrier) override;
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void BindVertexBuffer(BufferHandle buffer, std::uint64_t offset) override;
  void BindIndexBuffer(BufferHandle buffer, IndexFormat format, std::uint64_t offset) override;
  void BindTexture(std::uint32_t binding, TextureHandle texture) override;
  void BindConstantBuffer(std::uint32_t binding, BufferHandle buffer,
                          std::uint64_t offset) override;
  void BindStorageBuffer(std::uint32_t binding, BufferHandle buffer) override;
  void BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset, std::uint32_t stride) override;
  void SetScissor(const ScissorRect &rect) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count,
                   std::uint32_t first_index, std::int32_t vertex_offset,
                   std::uint32_t first_instance) override;
  void Dispatch(std::uint32_t groups_x, std::uint32_t groups_y, std::uint32_t groups_z) override;
  void DrawIndirect(std::uint32_t command_count) override;
  void EndRendering() override;
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  [[nodiscard]] std::uint64_t Dispatches() const noexcept { return dispatches_; }
  [[nodiscard]] std::uint64_t IndirectDraws() const noexcept { return indirect_draws_; }
  [[nodiscard]] bool IsClosed() const noexcept { return !rendering_; }
  [[nodiscard]] bool IsSubmitted() const noexcept { return submitted_; }
  [[nodiscard]] bool BelongsTo(const ValidationDevice &device) const noexcept {
    return &device_ == &device;
  }
  void MarkSubmitted() noexcept { submitted_ = true; }

private:
  friend class ValidationDevice;
  struct TextureSnapshot final {
    TextureHandle handle;
    std::uint64_t revision;
    ResourceState state;
    std::uint32_t width;
    std::uint32_t height;
  };
  TextureSnapshot &SnapshotTexture(TextureHandle texture);
  std::unordered_map<std::uint64_t, TextureSnapshot> texture_snapshots_;
  ValidationDevice &device_;
  bool rendering_{false};
  bool pipeline_bound_{false};
  bool vertex_buffer_bound_{false};
  bool index_buffer_bound_{false};
  bool texture_bound_{false};
  bool compute_pipeline_bound_{false};
  bool storage_buffer_bound_{false};
  bool scissor_set_{false};
  bool submitted_{false};
  // The bound indirect buffer's extent, checked at DrawIndirect like every native backend.
  bool indirect_buffer_bound_{false};
  std::uint64_t indirect_size_{};
  std::uint64_t indirect_offset_{};
  std::uint32_t indirect_stride_{DrawIndirectArgumentSize};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
  std::uint64_t dispatches_{};
  std::uint64_t indirect_draws_{};
};

class ValidationDevice final : public Device {
public:
  Backend GetBackend() const noexcept override { return Backend::Null; }
  GraphicsDeviceCapabilities Capabilities() const noexcept override {
    GraphicsDeviceCapabilities result;
    result.validation_only = true;
    result.gpu_timing_support = CapabilitySupport::Unsupported;
    result.independent_compute_recording = true;
    result.independent_graphics_recording = true;
    result.shared_buffer_read_recording = true;
    result.software_device = true;
    result.dxil_shader_model_support = CapabilitySupport::Unsupported;
    for (auto &feature : result.features)
      feature.support = CapabilitySupport::Unsupported;
    for (const auto feature : {GraphicsFeature::Compute, GraphicsFeature::IndirectDraw})
      result.features[static_cast<std::size_t>(feature)] = {
          CapabilitySupport::Supported, {1, 0}, true, true};
    return result;
  }
  TextureHandle CreateTexture(const TextureDescriptor &descriptor) override {
    if (descriptor.width == 0 || descriptor.height == 0)
      throw std::invalid_argument("invalid texture extent");
    std::lock_guard lock{mutex_};
    const auto handle = textures_.Create();
    texture_states_.emplace(Key(handle), descriptor.initial_state);
    texture_revisions_.emplace(Key(handle), 0);
    texture_dimensions_.emplace(Key(handle), std::array{descriptor.width, descriptor.height});
    texture_sizes_.emplace(Key(handle),
                           static_cast<std::uint64_t>(descriptor.width) * descriptor.height * 4U);
    texture_row_pitches_.emplace(Key(handle), descriptor.width * 4U);
    return handle;
  }
  void WriteTextureRgba8(TextureHandle texture, std::span<const std::byte> data,
                         std::uint32_t row_pitch) override {
    std::lock_guard lock{mutex_};
    Require(textures_.Contains(texture), "writing invalid texture");
    Require(row_pitch == texture_row_pitches_.at(Key(texture)) &&
                data.size() == texture_sizes_.at(Key(texture)),
            "texture upload size is invalid");
    ++texture_revisions_.at(Key(texture));
  }
  void DestroyTexture(TextureHandle texture) override {
    std::lock_guard lock{mutex_};
    Require(textures_.Destroy(texture), "destroying invalid texture");
    texture_states_.erase(Key(texture));
    texture_revisions_.erase(Key(texture));
    texture_dimensions_.erase(Key(texture));
    texture_sizes_.erase(Key(texture));
    texture_row_pitches_.erase(Key(texture));
  }
  BufferHandle CreateBuffer(const BufferDescriptor &descriptor) override {
    if (descriptor.size == 0)
      throw std::invalid_argument("buffer size must be non-zero");
    std::lock_guard lock{mutex_};
    const auto handle = buffers_.Create();
    buffer_sizes_.emplace(Key(handle), descriptor.size);
    buffer_data_.emplace(Key(handle), std::vector<std::byte>(descriptor.size));
    return handle;
  }
  void WriteBuffer(BufferHandle buffer, std::uint64_t offset,
                   std::span<const std::byte> data) override {
    std::lock_guard lock{mutex_};
    Require(buffers_.Contains(buffer), "writing invalid buffer");
    const auto size = buffer_sizes_.at(Key(buffer));
    Require(offset <= size && data.size() <= size - offset, "buffer write is out of bounds");
    std::ranges::copy(data,
                      buffer_data_.at(Key(buffer)).begin() + static_cast<std::ptrdiff_t>(offset));
  }
  void DestroyBuffer(BufferHandle buffer) override {
    std::lock_guard lock{mutex_};
    Require(buffers_.Destroy(buffer), "destroying invalid buffer");
    buffer_sizes_.erase(Key(buffer));
    buffer_data_.erase(Key(buffer));
  }
  void ReadBufferForTesting(BufferHandle buffer, std::uint64_t offset,
                            std::span<std::byte> data) override {
    std::lock_guard lock{mutex_};
    Require(buffers_.Contains(buffer), "reading invalid buffer");
    const auto &source = buffer_data_.at(Key(buffer));
    Require(offset <= source.size() && data.size() <= source.size() - offset,
            "buffer read is out of bounds");
    std::ranges::copy_n(source.begin() + static_cast<std::ptrdiff_t>(offset), data.size(),
                        data.begin());
    ++diagnostics_.readbacks;
  }
  PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) override {
    RequireGraphicsCapabilities(Capabilities(), descriptor.graphics_requirements);
    if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0) {
      throw std::invalid_argument("pipeline hashes must be non-zero");
    }
    std::lock_guard lock{mutex_};
    const auto handle = pipelines_.Create();
    pipeline_types_.emplace(Key(handle), descriptor.type);
    return handle;
  }
  void DestroyPipeline(PipelineHandle pipeline) override {
    std::lock_guard lock{mutex_};
    Require(pipelines_.Destroy(pipeline), "destroying invalid pipeline");
    pipeline_types_.erase(Key(pipeline));
  }
  std::unique_ptr<CommandList> CreateCommandList(QueueType) override {
    return std::make_unique<ValidationCommandList>(*this);
  }
  std::uint64_t Submit(CommandList &commands) override {
    auto *validated = dynamic_cast<ValidationCommandList *>(&commands);
    std::lock_guard lock{mutex_};
    Require(validated != nullptr, "command list belongs to another device");
    Require(validated->BelongsTo(*this), "command list belongs to another device");
    Require(validated->IsClosed(), "cannot submit an open command list");
    Require(!validated->IsSubmitted(), "command list was already submitted");
    for (const auto &entry : validated->texture_snapshots_) {
      Require(textures_.Contains(entry.second.handle), "submitting a stale texture handle");
      Require(texture_revisions_.at(entry.first) == entry.second.revision,
              "stale validation texture recording snapshot");
    }
    for (const auto &entry : validated->texture_snapshots_) {
      texture_states_.at(entry.first) = entry.second.state;
      ++texture_revisions_.at(entry.first);
    }
    validated->MarkSubmitted();
    ++diagnostics_.submitted_command_lists;
    diagnostics_.barriers += validated->Barriers();
    diagnostics_.draw_calls += validated->DrawCalls();
    diagnostics_.compute_dispatches += validated->Dispatches();
    diagnostics_.indirect_draw_calls += validated->IndirectDraws();
    completed_submission_ = ++submitted_submission_;
    return submitted_submission_;
  }
  std::uint64_t CompletedSubmissionValue() const noexcept override {
    std::lock_guard lock{mutex_};
    return completed_submission_;
  }
  void WaitForSubmission(std::uint64_t value) override {
    std::lock_guard lock{mutex_};
    Require(value <= submitted_submission_, "waiting for an unknown submission");
    completed_submission_ = std::max(completed_submission_, value);
  }
  void Present(TextureHandle texture) override {
    std::lock_guard lock{mutex_};
    Require(textures_.Contains(texture), "presenting invalid texture");
    Require(texture_states_.at(Key(texture)) == ResourceState::Present,
            "present texture is not in Present state");
    ++diagnostics_.presents;
  }
  void WaitIdle() override {}
  DeviceDiagnostics Diagnostics() const noexcept override {
    std::lock_guard lock{mutex_};
    return diagnostics_;
  }

  PipelineType ValidatePipeline(PipelineHandle pipeline) {
    std::lock_guard lock{mutex_};
    Require(pipelines_.Contains(pipeline), "binding invalid pipeline");
    return pipeline_types_.at(Key(pipeline));
  }
  [[nodiscard]] std::uint64_t BufferSize(BufferHandle buffer) {
    std::lock_guard lock{mutex_};
    Require(buffers_.Contains(buffer), "binding invalid buffer");
    return buffer_sizes_.at(Key(buffer));
  }
  void ValidateBuffer(BufferHandle buffer, std::uint64_t offset) {
    std::lock_guard lock{mutex_};
    Require(buffers_.Contains(buffer), "binding invalid buffer");
    Require(offset < buffer_sizes_.at(Key(buffer)), "buffer binding offset is out of bounds");
  }

private:
  friend class ValidationCommandList;
  template <typename Handle> static std::uint64_t Key(Handle handle) {
    return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.index;
  }
  void Require(bool condition, const char *message) {
    if (condition)
      return;
    ++diagnostics_.validation_errors;
    throw std::logic_error(message);
  }
  mutable std::mutex mutex_;
  core::HandlePool<TextureTag> textures_;
  core::HandlePool<BufferTag> buffers_;
  core::HandlePool<PipelineTag> pipelines_;
  std::unordered_map<std::uint64_t, ResourceState> texture_states_;
  std::unordered_map<std::uint64_t, std::uint64_t> texture_revisions_;
  std::unordered_map<std::uint64_t, std::array<std::uint32_t, 2>> texture_dimensions_;
  std::unordered_map<std::uint64_t, std::uint64_t> texture_sizes_;
  std::unordered_map<std::uint64_t, std::uint32_t> texture_row_pitches_;
  std::unordered_map<std::uint64_t, std::uint64_t> buffer_sizes_;
  std::unordered_map<std::uint64_t, std::vector<std::byte>> buffer_data_;
  std::unordered_map<std::uint64_t, PipelineType> pipeline_types_;
  DeviceDiagnostics diagnostics_;
  std::uint64_t submitted_submission_{};
  std::uint64_t completed_submission_{};
};

ValidationCommandList::TextureSnapshot &
ValidationCommandList::SnapshotTexture(TextureHandle handle) {
  std::lock_guard lock{device_.mutex_};
  device_.Require(device_.textures_.Contains(handle), "command list references invalid texture");
  const auto key = ValidationDevice::Key(handle);
  const auto &dimensions = device_.texture_dimensions_.at(key);
  auto &snapshot = texture_snapshots_
                       .try_emplace(key, TextureSnapshot{handle, device_.texture_revisions_.at(key),
                                                         device_.texture_states_.at(key),
                                                         dimensions[0], dimensions[1]})
                       .first->second;
  device_.Require(snapshot.revision == device_.texture_revisions_.at(key),
                  "texture changed during command recording");
  return snapshot;
}

void ValidationCommandList::Transition(const Barrier &barrier) {
  if (submitted_)
    throw std::logic_error("cannot record a submitted command list");
  if (rendering_)
    throw std::logic_error("barriers cannot occur inside rendering");
  if (!IsValidTextureBarrier(barrier))
    throw std::invalid_argument("invalid texture barrier");
  auto &texture = SnapshotTexture(barrier.texture);
  if (texture.state != barrier.before)
    throw std::logic_error("barrier before-state mismatch");
  texture.state = barrier.after;
  ++barriers_;
}
void ValidationCommandList::TransitionBuffer(const BufferBarrier &barrier) {
  if (submitted_ || rendering_)
    throw std::logic_error("buffer barriers require an open command list outside rendering");
  if (!IsValidBufferBarrier(barrier))
    throw std::invalid_argument("invalid buffer barrier");
  device_.ValidateBuffer(barrier.buffer, 0);
  ++barriers_;
}

void ValidationCommandList::BeginRendering(const RenderingInfo &info) {
  if (submitted_ || rendering_ || info.width == 0 || info.height == 0)
    throw std::logic_error("invalid BeginRendering");
  const auto &target = SnapshotTexture(info.color_target);
  if (target.state != ResourceState::RenderTarget || info.width > target.width ||
      info.height > target.height)
    throw std::logic_error("invalid render target state/extent");
  rendering_ = true;
  pipeline_bound_ = false;
  vertex_buffer_bound_ = false;
  index_buffer_bound_ = false;
  texture_bound_ = false;
  scissor_set_ = false;
}
void ValidationCommandList::BindVertexBuffer(BufferHandle buffer, std::uint64_t offset) {
  if (submitted_ || !rendering_)
    throw std::logic_error("vertex-buffer binding requires rendering");
  device_.ValidateBuffer(buffer, offset);
  vertex_buffer_bound_ = true;
}
void ValidationCommandList::BindIndexBuffer(BufferHandle buffer, IndexFormat,
                                            std::uint64_t offset) {
  if (submitted_ || !rendering_)
    throw std::logic_error("index-buffer binding requires rendering");
  device_.ValidateBuffer(buffer, offset);
  index_buffer_bound_ = true;
}
void ValidationCommandList::BindTexture(std::uint32_t, TextureHandle texture) {
  if (submitted_ || !rendering_)
    throw std::logic_error("texture binding requires rendering");
  if (SnapshotTexture(texture).state != ResourceState::ShaderRead)
    throw std::logic_error("sampled texture is not in ShaderRead state");
  texture_bound_ = true;
}
void ValidationCommandList::BindConstantBuffer(std::uint32_t, BufferHandle buffer,
                                               std::uint64_t offset) {
  if (submitted_ || !rendering_)
    throw std::logic_error("constant-buffer binding requires rendering");
  device_.ValidateBuffer(buffer, offset);
}
void ValidationCommandList::BindStorageBuffer(std::uint32_t, BufferHandle buffer) {
  if (submitted_ || rendering_)
    throw std::logic_error("storage-buffer binding requires a compute command list");
  device_.ValidateBuffer(buffer, 0);
  storage_buffer_bound_ = true;
}
void ValidationCommandList::BindIndirectBuffer(BufferHandle buffer, std::uint64_t offset,
                                               std::uint32_t stride) {
  if (submitted_ || !rendering_ || (stride != 0 && stride < DrawIndirectArgumentSize))
    throw std::logic_error("indirect-buffer binding requires rendering");
  const auto size = device_.BufferSize(buffer);
  if (offset > size || size - offset < DrawIndirectArgumentSize)
    throw std::logic_error("indirect-buffer binding is out of bounds");
  indirect_buffer_bound_ = true;
  indirect_size_ = size;
  indirect_offset_ = offset;
  indirect_stride_ = stride == 0 ? DrawIndirectArgumentSize : stride;
}
void ValidationCommandList::SetScissor(const ScissorRect &rect) {
  if (submitted_ || !rendering_ || rect.width == 0 || rect.height == 0)
    throw std::logic_error("invalid scissor rectangle");
  scissor_set_ = true;
}
void ValidationCommandList::BindPipeline(PipelineHandle pipeline) {
  if (submitted_)
    throw std::logic_error("cannot bind a pipeline after submission");
  const auto type = device_.ValidatePipeline(pipeline);
  if (type == PipelineType::Graphics && !rendering_)
    throw std::logic_error("graphics pipeline binding requires rendering");
  if (type == PipelineType::Compute && rendering_)
    throw std::logic_error("compute pipeline binding cannot occur during rendering");
  pipeline_bound_ = type == PipelineType::Graphics;
  compute_pipeline_bound_ = type == PipelineType::Compute;
}
void ValidationCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  if (submitted_ || !rendering_ || !pipeline_bound_ || vertex_count == 0 || instance_count == 0) {
    throw std::logic_error("invalid draw");
  }
  ++draws_;
}
void ValidationCommandList::DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count,
                                        std::uint32_t, std::int32_t, std::uint32_t) {
  if (submitted_ || !rendering_ || !pipeline_bound_ || !vertex_buffer_bound_ ||
      !index_buffer_bound_ || !texture_bound_ || !scissor_set_ || index_count == 0 ||
      instance_count == 0)
    throw std::logic_error("invalid indexed draw");
  ++draws_;
}
void ValidationCommandList::Dispatch(std::uint32_t groups_x, std::uint32_t groups_y,
                                     std::uint32_t groups_z) {
  if (submitted_ || rendering_ || groups_x == 0 || groups_y == 0 || groups_z == 0)
    throw std::logic_error("invalid dispatch");
  ++dispatches_;
}
void ValidationCommandList::DrawIndirect(std::uint32_t command_count) {
  if (submitted_ || !rendering_ || !pipeline_bound_ || command_count == 0)
    throw std::logic_error("invalid indirect draw");
  // Without a bound buffer the backend supplies its own one-command arguments (FramePipeline's
  // triangle path); with one, the whole range must fit, as on every native backend.
  if (indirect_buffer_bound_ &&
      !IndirectDrawRangeFits(indirect_size_, indirect_offset_, indirect_stride_, command_count))
    throw std::logic_error("indirect draw reads past the bound indirect buffer");
  ++indirect_draws_;
  ++draws_;
}
void ValidationCommandList::EndRendering() {
  if (submitted_ || !rendering_)
    throw std::logic_error("EndRendering without BeginRendering");
  rendering_ = false;
}
} // namespace

std::unique_ptr<Device> CreateValidationDevice() { return std::make_unique<ValidationDevice>(); }
} // namespace nexora::rhi
