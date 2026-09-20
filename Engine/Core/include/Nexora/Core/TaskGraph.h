#pragma once

#include "Nexora/Core/Api.h"

#include "Nexora/Core/JobSystem.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace nexora::core {
using TaskResource = std::uint64_t;

struct TaskDescriptor final {
  std::string name;
  std::vector<TaskResource> reads;
  std::vector<TaskResource> writes;
  std::function<void(const CancellationToken &)> function;
  JobPriority priority{JobPriority::Normal};
};

class NEXORA_CORE_API TaskGraph final {
public:
  [[nodiscard]] std::size_t Add(TaskDescriptor descriptor);
  void Compile();
  [[nodiscard]] std::vector<JobHandle> Execute(JobSystem &jobs,
                                               const CancellationToken &cancellation = {}) const;
  [[nodiscard]] std::span<const std::size_t> Dependencies(std::size_t task) const;

private:
  std::vector<TaskDescriptor> tasks_;
  std::vector<std::vector<std::size_t>> dependencies_;
  bool compiled_{false};
};
} // namespace nexora::core
