#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/Cancellation.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace nexora::core {

enum class JobPriority : std::uint8_t { Low, Normal, High };
enum class JobStatus : std::uint8_t { Queued, Running, Completed, Cancelled, Failed };

class NEXORA_CORE_API JobHandle final {
public:
  JobHandle() = default;
  [[nodiscard]] bool IsValid() const noexcept;
  [[nodiscard]] JobStatus Status() const noexcept;

private:
  struct State;
  explicit JobHandle(std::shared_ptr<State> state) : state_(std::move(state)) {}
  std::shared_ptr<State> state_;
  friend class JobSystem;
};

struct JobDescriptor final {
  std::function<void(const CancellationToken &)> function;
  JobPriority priority{JobPriority::Normal};
  CancellationToken cancellation;
  std::string debug_name;
};

class NEXORA_CORE_API JobSystem final {
public:
  explicit JobSystem(std::size_t worker_count = 0);
  ~JobSystem();
  JobSystem(const JobSystem &) = delete;
  JobSystem &operator=(const JobSystem &) = delete;

  void Start();
  void Stop();
  [[nodiscard]] JobHandle Submit(JobDescriptor descriptor,
                                 std::span<const JobHandle> dependencies = {});
  void Wait(const JobHandle &handle);
  [[nodiscard]] std::size_t WorkerCount() const noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};

} // namespace nexora::core
