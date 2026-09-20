#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/Types.h"

#include <memory>
#include <span>
#include <string_view>

namespace nexora::rhi {
struct DeviceDiagnostics final {
  std::uint64_t submitted_command_lists{};
  std::uint64_t barriers{};
  std::uint64_t draw_calls{};
  std::uint64_t presents{};
  std::uint64_t validation_errors{};
};
class NEXORA_RHI_API CommandList {
public:
  virtual ~CommandList() = default;
  virtual void Transition(const Barrier &barrier) = 0;
  virtual void BeginRendering(const RenderingInfo &info) = 0;
  virtual void BindPipeline(PipelineHandle pipeline) = 0;
  virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1) = 0;
  virtual void EndRendering() = 0;
};

class NEXORA_RHI_API Device {
public:
  virtual ~Device() = default;
  [[nodiscard]] virtual Backend GetBackend() const noexcept = 0;
  [[nodiscard]] virtual TextureHandle CreateTexture(const TextureDescriptor &descriptor) = 0;
  virtual void DestroyTexture(TextureHandle texture) = 0;
  [[nodiscard]] virtual PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) = 0;
  virtual void DestroyPipeline(PipelineHandle pipeline) = 0;
  [[nodiscard]] virtual std::unique_ptr<CommandList> CreateCommandList(QueueType queue) = 0;
  virtual void Submit(CommandList &commands) = 0;
  virtual void Present(TextureHandle texture) = 0;
  virtual void WaitIdle() = 0;
  [[nodiscard]] virtual DeviceDiagnostics Diagnostics() const noexcept = 0;
};

[[nodiscard]] NEXORA_RHI_API std::unique_ptr<Device> CreateValidationDevice();
[[nodiscard]] NEXORA_RHI_API std::unique_ptr<Device> CreateDevice(Backend backend);
[[nodiscard]] NEXORA_RHI_API bool IsBackendAvailable(Backend backend) noexcept;
} // namespace nexora::rhi
