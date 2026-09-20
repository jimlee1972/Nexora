#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cmath>
#include <cstddef>

namespace nexora::runtime {
namespace {

constexpr std::size_t kRequiredHostSize = sizeof(NexoraGameplayHostV1);
constexpr std::size_t kRequiredModuleSize = sizeof(NexoraGameModuleV1);

} // namespace

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV1 host) noexcept : host_(host) {}

GameplayModuleHost::~GameplayModuleHost() { Unload(); }

bool GameplayModuleHost::Create(NexoraGameModuleLoadFn load, NexoraGameModuleV1 &module) const {
  if (load == nullptr || host_.struct_size < kRequiredHostSize ||
      host_.abi_version != NEXORA_GAMEPLAY_ABI_VERSION)
    return false;

  module = {};
  module.struct_size = sizeof(module);
  module.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  if (load(NEXORA_GAMEPLAY_ABI_VERSION, &module) != 0 || module.struct_size < kRequiredModuleSize ||
      module.abi_version != NEXORA_GAMEPLAY_ABI_VERSION || module.initialize == nullptr ||
      module.update == nullptr || module.shutdown == nullptr)
    return false;

  void *state = nullptr;
  if (module.initialize(&state, &host_) != 0)
    return false;
  module.module_state = state;
  return true;
}

bool GameplayModuleHost::Load(NexoraGameModuleLoadFn load) {
  std::scoped_lock lock(mutex_);
  if (loaded_)
    return false;
  NexoraGameModuleV1 candidate{};
  if (!Create(load, candidate))
    return false;
  module_ = candidate;
  loaded_ = true;
  return true;
}

bool GameplayModuleHost::Reload(NexoraGameModuleLoadFn load) {
  std::scoped_lock lock(mutex_);
  if (!loaded_)
    return false;

  NexoraGameModuleV1 candidate{};
  if (!Create(load, candidate))
    return false;

  ShutdownLocked();
  module_ = candidate;
  loaded_ = true;
  return true;
}

bool GameplayModuleHost::Update(double delta_seconds) {
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0)
    return false;
  std::scoped_lock lock(mutex_);
  if (!loaded_)
    return false;
  module_.update(module_.module_state, delta_seconds);
  return true;
}

void GameplayModuleHost::ShutdownLocked() noexcept {
  if (loaded_)
    module_.shutdown(module_.module_state);
  module_ = {};
  loaded_ = false;
}

void GameplayModuleHost::Unload() noexcept {
  std::scoped_lock lock(mutex_);
  ShutdownLocked();
}

bool GameplayModuleHost::IsLoaded() const noexcept {
  std::scoped_lock lock(mutex_);
  return loaded_;
}

} // namespace nexora::runtime
