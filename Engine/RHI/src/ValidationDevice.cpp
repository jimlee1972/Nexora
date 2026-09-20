#include "Nexora/RHI/Device.h"

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
  void BeginRendering(const RenderingInfo &info) override;
  void BindPipeline(PipelineHandle pipeline) override;
  void Draw(std::uint32_t vertex_count, std::uint32_t instance_count) override;
  void EndRendering() override;
  [[nodiscard]] std::uint64_t Barriers() const noexcept { return barriers_; }
  [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return draws_; }
  [[nodiscard]] bool IsClosed() const noexcept { return !rendering_; }

private:
  ValidationDevice &device_;
  bool rendering_{false};
  bool pipeline_bound_{false};
  std::uint64_t barriers_{};
  std::uint64_t draws_{};
};

class ValidationDevice final : public Device {
public:
  Backend GetBackend() const noexcept override { return Backend::Null; }
  TextureHandle CreateTexture(const TextureDescriptor &descriptor) override {
    if (descriptor.width == 0 || descriptor.height == 0)
      throw std::invalid_argument("invalid texture extent");
    std::lock_guard lock{mutex_};
    const auto handle = textures_.Create();
    texture_states_.emplace(Key(handle), descriptor.initial_state);
    return handle;
  }
  void DestroyTexture(TextureHandle texture) override {
    std::lock_guard lock{mutex_};
    Require(textures_.Destroy(texture), "destroying invalid texture");
    texture_states_.erase(Key(texture));
  }
  PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) override {
    if (descriptor.layout_hash == 0 || descriptor.shader_hash == 0) {
      throw std::invalid_argument("pipeline hashes must be non-zero");
    }
    std::lock_guard lock{mutex_};
    return pipelines_.Create();
  }
  void DestroyPipeline(PipelineHandle pipeline) override {
    std::lock_guard lock{mutex_};
    Require(pipelines_.Destroy(pipeline), "destroying invalid pipeline");
  }
  std::unique_ptr<CommandList> CreateCommandList(QueueType) override {
    return std::make_unique<ValidationCommandList>(*this);
  }
  void Submit(CommandList &commands) override {
    auto *validated = dynamic_cast<ValidationCommandList *>(&commands);
    if (validated == nullptr || !validated->IsClosed())
      throw std::logic_error("invalid command submission");
    std::lock_guard lock{mutex_};
    ++diagnostics_.submitted_command_lists;
    diagnostics_.barriers += validated->Barriers();
    diagnostics_.draw_calls += validated->DrawCalls();
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

  void Transition(const Barrier &barrier) {
    std::lock_guard lock{mutex_};
    Require(textures_.Contains(barrier.texture), "barrier references invalid texture");
    auto &state = texture_states_.at(Key(barrier.texture));
    Require(state == barrier.before, "barrier before-state mismatch");
    state = barrier.after;
  }
  void ValidateTarget(TextureHandle texture) {
    std::lock_guard lock{mutex_};
    Require(textures_.Contains(texture), "rendering references invalid texture");
    Require(texture_states_.at(Key(texture)) == ResourceState::RenderTarget,
            "render target is not in RenderTarget state");
  }
  void ValidatePipeline(PipelineHandle pipeline) {
    std::lock_guard lock{mutex_};
    Require(pipelines_.Contains(pipeline), "binding invalid pipeline");
  }

private:
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
  core::HandlePool<PipelineTag> pipelines_;
  std::unordered_map<std::uint64_t, ResourceState> texture_states_;
  DeviceDiagnostics diagnostics_;
};

void ValidationCommandList::Transition(const Barrier &barrier) {
  if (rendering_)
    throw std::logic_error("barriers cannot occur inside rendering");
  device_.Transition(barrier);
  ++barriers_;
}
void ValidationCommandList::BeginRendering(const RenderingInfo &info) {
  if (rendering_ || info.width == 0 || info.height == 0)
    throw std::logic_error("invalid BeginRendering");
  device_.ValidateTarget(info.color_target);
  rendering_ = true;
  pipeline_bound_ = false;
}
void ValidationCommandList::BindPipeline(PipelineHandle pipeline) {
  if (!rendering_)
    throw std::logic_error("pipeline binding requires rendering");
  device_.ValidatePipeline(pipeline);
  pipeline_bound_ = true;
}
void ValidationCommandList::Draw(std::uint32_t vertex_count, std::uint32_t instance_count) {
  if (!rendering_ || !pipeline_bound_ || vertex_count == 0 || instance_count == 0) {
    throw std::logic_error("invalid draw");
  }
  ++draws_;
}
void ValidationCommandList::EndRendering() {
  if (!rendering_)
    throw std::logic_error("EndRendering without BeginRendering");
  rendering_ = false;
}
} // namespace

std::unique_ptr<Device> CreateValidationDevice() { return std::make_unique<ValidationDevice>(); }
} // namespace nexora::rhi
