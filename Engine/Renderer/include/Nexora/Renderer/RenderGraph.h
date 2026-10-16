#pragma once

#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/Api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::renderer {
struct GraphTexture final {
  std::uint32_t id{};
  friend bool operator==(GraphTexture, GraphTexture) = default;
};
struct GraphBuffer final {
  std::uint32_t id{};
  friend bool operator==(GraphBuffer, GraphBuffer) = default;
};
struct BufferUse final {
  GraphBuffer buffer;
  rhi::BufferAccess access;
};
struct TextureUse final {
  GraphTexture texture;
  rhi::ResourceState state;
};

// Native owners keep physical resources private; callbacks materialize requested transitions.
// Context spans and names are borrowed only for the callback. These are logical graph IDs,
// never RHI texture handles or native images/devices.
struct ExternalTextureTransition final {
  GraphTexture texture;
  rhi::ResourceState before;
  rhi::ResourceState after;
};
struct ExternalPassContext final {
  std::string_view name;
  std::span<const TextureUse> reads;
  std::span<const TextureUse> writes;
  std::span<const ExternalTextureTransition> transitions;
};

class NEXORA_RENDERER_API RenderGraph final {
public:
  using ExecuteFunction =
      std::function<void(rhi::CommandList &, std::span<const rhi::TextureHandle>)>;
  struct PassDescriptor final {
    std::string name;
    rhi::QueueType queue{rhi::QueueType::Graphics};
    std::vector<TextureUse> reads;
    std::vector<TextureUse> writes;
    ExecuteFunction execute;
    std::function<void(const ExternalPassContext &)> external_execute{};
    std::vector<BufferUse> buffer_reads{};
    std::vector<BufferUse> buffer_writes{};
  };
  struct Statistics final {
    std::size_t pass_count{};
    std::size_t transient_texture_count{};
    std::size_t barrier_count{};
    std::size_t queue_transfer_count{};
    std::size_t external_transition_count{};
    std::size_t completed_pass_count{};
    std::size_t buffer_barrier_count{};
    std::size_t buffer_queue_transfer_count{};
  };

  // Borrowed buffers must remain alive through execution. No transient buffers are allocated.
  [[nodiscard]] GraphBuffer
  ImportBuffer(rhi::BufferHandle buffer,
               rhi::BufferAccess initial_access = rhi::BufferAccess::Undefined,
               rhi::QueueType initial_queue = rhi::QueueType::Graphics);
  [[nodiscard]] GraphTexture ImportTexture(rhi::TextureHandle texture,
                                           const rhi::TextureDescriptor &descriptor);
  [[nodiscard]] GraphTexture CreateTransientTexture(const rhi::TextureDescriptor &descriptor);
  [[nodiscard]] std::size_t AddPass(PassDescriptor descriptor);
  [[nodiscard]] GraphTexture ImportExternalTexture(const rhi::TextureDescriptor &descriptor);
  [[nodiscard]] std::size_t
  AddExternalPass(std::string name, std::vector<TextureUse> reads, std::vector<TextureUse> writes,
                  std::function<void(const ExternalPassContext &)> execute);
  void ExecuteExternal();
  void AddDependency(std::size_t before, std::size_t after);
  void Compile();
  void Execute(rhi::Device &device);
  [[nodiscard]] std::span<const std::size_t> ExecutionOrder() const noexcept {
    return execution_order_;
  }
  [[nodiscard]] Statistics GetStatistics() const noexcept { return statistics_; }

private:
  struct TextureRecord final {
    rhi::TextureDescriptor descriptor;
    rhi::TextureHandle imported;
    bool transient{};
    std::size_t first_use{};
    std::size_t last_use{};
    bool external{};
  };
  struct BufferRecord final {
    rhi::BufferHandle imported;
    rhi::BufferAccess initial_access;
    rhi::QueueType initial_queue;
  };
  struct PlannedTextureBarrier final {
    GraphTexture texture;
    rhi::ResourceState before;
    rhi::ResourceState after;
    rhi::QueueType source_queue;
    rhi::QueueType destination_queue;
  };
  std::vector<TextureRecord> textures_;
  std::vector<BufferRecord> buffers_;
  std::vector<std::vector<PlannedTextureBarrier>> texture_barriers_;
  std::vector<std::vector<rhi::BufferBarrier>> buffer_barriers_;
  std::vector<PassDescriptor> passes_;
  std::vector<std::vector<std::size_t>> explicit_edges_;
  std::vector<std::size_t> execution_order_;
  Statistics statistics_;
  bool compiled_{false};
};
} // namespace nexora::renderer
