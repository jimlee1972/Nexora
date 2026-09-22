#include "Nexora/Runtime/GameplayModuleHost.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <vector>

namespace nexora::runtime {
namespace {

constexpr std::size_t kRequiredHostSize = sizeof(NexoraGameplayHostV3);
constexpr std::size_t kRequiredModuleSize = sizeof(NexoraGameModuleV3);

} // namespace

GameplayModuleHost::GameplayModuleHost(NexoraGameplayHostV3 host) noexcept : host_(host) {}

GameplayModuleHost::~GameplayModuleHost() { Unload(); }

bool GameplayModuleHost::Create(NexoraGameModuleLoadV3Fn load, NexoraGameModuleV3 &module) const {
  if (load == nullptr || host_.struct_size < kRequiredHostSize ||
      host_.abi_version != NEXORA_GAMEPLAY_ABI_VERSION)
    return false;

  module = {};
  module.struct_size = sizeof(module);
  module.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  if (load(NEXORA_GAMEPLAY_ABI_VERSION, &module) != 0 || module.struct_size < kRequiredModuleSize ||
      module.abi_version != NEXORA_GAMEPLAY_ABI_VERSION || module.create == nullptr ||
      module.on_start == nullptr || module.update == nullptr || module.on_stop == nullptr ||
      module.destroy == nullptr)
    return false;

  void *state = nullptr;
  if (module.create(&state, &host_) != NEXORA_GAMEPLAY_OK)
    return false;
  module.module_state = state;
  if (module.on_start(state) != NEXORA_GAMEPLAY_OK) {
    module.destroy(state);
    module = {};
    return false;
  }
  return true;
}

bool GameplayModuleHost::Load(NexoraGameModuleLoadV3Fn load) {
  std::scoped_lock lock(mutex_);
  if (loaded_)
    return false;
  NexoraGameModuleV3 candidate{};
  if (!Create(load, candidate))
    return false;
  module_ = candidate;
  loaded_ = true;
  return true;
}

bool GameplayModuleHost::Reload(NexoraGameModuleLoadV3Fn load) {
  const auto started = std::chrono::steady_clock::now();
  std::scoped_lock lock(mutex_);
  if (!loaded_)
    return false;

  std::vector<std::byte> saved_state;
  if (module_.save_state != nullptr) {
    const auto required = module_.save_state(module_.module_state, nullptr, 0);
    if (required != 0) {
      saved_state.resize(required);
      if (module_.save_state(module_.module_state, saved_state.data(), required) != required)
        return false;
    }
  }

  NexoraGameModuleV3 candidate{};
  if (!Create(load, candidate))
    return false;

  if (!saved_state.empty()) {
    if (candidate.load_state == nullptr ||
        candidate.load_state(candidate.module_state, saved_state.data(),
                             static_cast<std::uint32_t>(saved_state.size())) != 0) {
      candidate.on_stop(candidate.module_state);
      candidate.destroy(candidate.module_state);
      return false;
    }
  }

  ShutdownLocked();
  module_ = candidate;
  loaded_ = true;
  ++reload_stats_.successful_reloads;
  reload_stats_.migrated_bytes = static_cast<std::uint32_t>(saved_state.size());
  reload_stats_.last_reload_duration = std::chrono::steady_clock::now() - started;
  return true;
}

bool GameplayModuleHost::Update(double delta_seconds) {
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0)
    return false;
  std::scoped_lock lock(mutex_);
  if (!loaded_)
    return false;
  return module_.update(module_.module_state, delta_seconds) == NEXORA_GAMEPLAY_OK;
}

bool GameplayModuleHost::FixedUpdate(double fixed_delta_seconds) {
  if (!std::isfinite(fixed_delta_seconds) || fixed_delta_seconds <= 0.0)
    return false;
  std::scoped_lock lock(mutex_);
  if (!loaded_ || module_.fixed_update == nullptr ||
      (module_.capabilities & NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE) == 0)
    return false;
  return module_.fixed_update(module_.module_state, fixed_delta_seconds) == NEXORA_GAMEPLAY_OK;
}

void GameplayModuleHost::ShutdownLocked() noexcept {
  if (loaded_) {
    module_.on_stop(module_.module_state);
    module_.destroy(module_.module_state);
  }
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

GameplayModuleHost::ReloadStats GameplayModuleHost::GetReloadStats() const noexcept {
  std::scoped_lock lock(mutex_);
  return reload_stats_;
}

} // namespace nexora::runtime
