#pragma once

#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/Api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace nexora::renderer {
struct GraphTexture final {
  std::uint32_t id{};
  friend bool operator==(GraphTexture, GraphTexture) = default;
};
struct TextureUse final {
  GraphTexture texture;
  rhi::ResourceState state;
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
  };
  struct Statistics final {
    std::size_t pass_count{};
    std::size_t transient_texture_count{};
    std::size_t barrier_count{};
  };

  [[nodiscard]] GraphTexture ImportTexture(rhi::TextureHandle texture,
                                           const rhi::TextureDescriptor &descriptor);
  [[nodiscard]] GraphTexture CreateTransientTexture(const rhi::TextureDescriptor &descriptor);
  [[nodiscard]] std::size_t AddPass(PassDescriptor descriptor);
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
  };
  std::vector<TextureRecord> textures_;
  std::vector<PassDescriptor> passes_;
  std::vector<std::vector<std::size_t>> explicit_edges_;
  std::vector<std::size_t> execution_order_;
  Statistics statistics_;
  bool compiled_{false};
};
} // namespace nexora::renderer
