#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/EventBus.h"
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Core/Log.h"
#include "Nexora/Core/Memory.h"
#include "Nexora/Core/Time.h"
#include "Nexora/Core/Vfs.h"

#include <cstddef>
#include <filesystem>
#include <memory>

namespace nexora::core {
struct EngineConfiguration final {
  std::size_t worker_count{};
  std::size_t crash_ring_capacity{256};
  std::size_t frame_arena_capacity{1024 * 1024};
  std::filesystem::path content_root{"."};
};

struct EngineServices final {
  TrackingAllocator *memory{};
  JobSystem *jobs{};
  VirtualFileSystem *vfs{};
  AsyncLogService *log{};
  EventBus *events{};
};

class NEXORA_CORE_API Engine final {
public:
  Engine();
  ~Engine();
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  void Initialize(const EngineConfiguration &configuration = {});
  void Shutdown() noexcept;
  [[nodiscard]] bool IsInitialized() const noexcept;
  [[nodiscard]] EngineServices Services() noexcept;
  [[nodiscard]] FixedTickClock &Clock();
  void BeginFrame();

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::core
