#pragma once

#include "Nexora/Core/JobSystem.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/Api.h"

#include <memory>

namespace nexora::renderer {
class NEXORA_RENDERER_API PipelineFuture final {
public:
  PipelineFuture() = default;
  [[nodiscard]] bool IsReady() const noexcept;
  void Wait() const;
  [[nodiscard]] rhi::PipelineHandle Get() const;

private:
  struct State;
  PipelineFuture(std::shared_ptr<State> state, core::JobHandle job)
      : state_(std::move(state)), job_(std::move(job)) {}
  std::shared_ptr<State> state_;
  core::JobHandle job_;
  friend class PipelineCache;
};

class NEXORA_RENDERER_API PipelineCache final {
public:
  PipelineCache(rhi::Device &device, core::JobSystem &jobs);
  ~PipelineCache();
  PipelineCache(const PipelineCache &) = delete;
  PipelineCache &operator=(const PipelineCache &) = delete;
  [[nodiscard]] PipelineFuture Request(const rhi::PipelineDescriptor &descriptor);
  [[nodiscard]] std::size_t Size() const noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::renderer
