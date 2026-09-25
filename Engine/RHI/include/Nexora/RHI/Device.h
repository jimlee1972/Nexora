#pragma once

#include "Nexora/RHI/Api.h"
#include "Nexora/RHI/IndirectCommand.h"
#include "Nexora/RHI/Types.h"

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

namespace nexora::rhi {
struct DeviceDiagnostics final {
  std::uint64_t submitted_command_lists{};
  std::uint64_t barriers{};
  std::uint64_t draw_calls{};
  std::uint64_t presents{};
  std::uint64_t validation_errors{};
  std::uint64_t compute_dispatches{};
  std::uint64_t indirect_draw_calls{};
  std::uint64_t readbacks{};
};
class NEXORA_RHI_API CommandList {
public:
  virtual ~CommandList() = default;
  virtual void Transition(const Barrier &barrier) = 0;
  virtual void BeginRendering(const RenderingInfo &info) = 0;
  virtual void BindPipeline(PipelineHandle pipeline) = 0;
  virtual void BindVertexBuffer(BufferHandle, std::uint64_t = 0) {
    throw std::logic_error("vertex-buffer binding is unsupported");
  }
  virtual void BindIndexBuffer(BufferHandle, IndexFormat, std::uint64_t = 0) {
    throw std::logic_error("index-buffer binding is unsupported");
  }
  virtual void BindTexture(std::uint32_t, TextureHandle) {
    throw std::logic_error("texture binding is unsupported");
  }
  virtual void BindStorageBuffer(std::uint32_t, BufferHandle) {
    throw std::logic_error("storage-buffer binding is unsupported");
  }
  virtual void BindIndirectBuffer(BufferHandle, std::uint64_t = 0, std::uint32_t = 0) {
    throw std::logic_error("indirect-buffer binding is unsupported");
  }
  virtual void SetScissor(const ScissorRect &) {
    throw std::logic_error("scissor rectangles are unsupported");
  }
  virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1) = 0;
  virtual void DrawIndexed(std::uint32_t, std::uint32_t = 1, std::uint32_t = 0, std::int32_t = 0,
                           std::uint32_t = 0) {
    throw std::logic_error("indexed drawing is unsupported");
  }
  virtual void Dispatch(std::uint32_t, std::uint32_t = 1, std::uint32_t = 1) {
    throw std::logic_error("compute dispatch is unsupported");
  }
  virtual void DrawIndirect(std::uint32_t) {
    throw std::logic_error("indirect drawing is unsupported");
  }
  virtual void EndRendering() = 0;
};

class NEXORA_RHI_API Device {
public:
  virtual ~Device() = default;
  [[nodiscard]] virtual Backend GetBackend() const noexcept = 0;
  [[nodiscard]] virtual TextureHandle CreateTexture(const TextureDescriptor &descriptor) = 0;
  virtual void WriteTextureRgba8(TextureHandle, std::span<const std::byte>, std::uint32_t) {
    throw std::logic_error("texture uploads are unsupported");
  }
  virtual void DestroyTexture(TextureHandle texture) = 0;
  [[nodiscard]] virtual BufferHandle CreateBuffer(const BufferDescriptor &) {
    throw std::logic_error("buffer creation is unsupported");
  }
  virtual void WriteBuffer(BufferHandle, std::uint64_t, std::span<const std::byte>) {
    throw std::logic_error("buffer uploads are unsupported");
  }
  virtual void DestroyBuffer(BufferHandle) {
    throw std::logic_error("buffer destruction is unsupported");
  }
  // Explicitly test-only: production rendering must not introduce synchronous readback.
  virtual void ReadBufferForTesting(BufferHandle, std::uint64_t, std::span<std::byte>) {
    throw std::logic_error("buffer readback is unsupported");
  }
  [[nodiscard]] virtual PipelineHandle CreatePipeline(const PipelineDescriptor &descriptor) = 0;
  virtual void DestroyPipeline(PipelineHandle pipeline) = 0;
  [[nodiscard]] virtual std::unique_ptr<CommandList> CreateCommandList(QueueType queue) = 0;
  // Returns a monotonically increasing completion value. Resources referenced by the command list
  // remain in use until CompletedSubmissionValue reaches that value.
  virtual std::uint64_t Submit(CommandList &commands) = 0;
  [[nodiscard]] virtual std::uint64_t CompletedSubmissionValue() const noexcept = 0;
  virtual void WaitForSubmission(std::uint64_t value) = 0;
  virtual void Present(TextureHandle texture) = 0;
  virtual void WaitIdle() = 0;
  [[nodiscard]] virtual DeviceDiagnostics Diagnostics() const noexcept = 0;
};

[[nodiscard]] NEXORA_RHI_API std::unique_ptr<Device> CreateValidationDevice();
[[nodiscard]] NEXORA_RHI_API std::unique_ptr<Device> CreateDevice(Backend backend);
[[nodiscard]] NEXORA_RHI_API bool IsBackendAvailable(Backend backend) noexcept;
} // namespace nexora::rhi
