#include "GameplayReloadFixture.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/Runtime.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

namespace {
using namespace nexora;
using namespace nexora::test::reload;

struct Wire final {
  std::uint64_t schema{1}, counter{}, scene{}, entity{};
};
struct State final {
  const NexoraGameplayHostV3 *host;
  Wire wire;
};

void Write(Context &context, runtime::Id entity, double x) {
  const game::GameplayTransformWire value{x, 0, 0};
  assert(game::WriteGameplayComponent(*context.world, entity, game::TransformComponentType(),
                                      &value, sizeof(value)) == 0);
}

int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  auto &context = Owner::Get(host->context);
  auto &owner = *context.owner;
  if (context.candidate) {
    Write(context, owner.entity, 999);
    if (owner.fail_create)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  auto state = std::make_unique<State>(State{host, {1, 0, owner.scene, owner.entity}});
  if (!context.candidate)
    owner.active = state.get();
  *output = state.release();
  ++owner.allocations;
  return 0;
}
int32_t Start(void *pointer) {
  auto &state = *static_cast<State *>(pointer);
  auto &context = Owner::Get(state.host->context);
  auto &owner = *context.owner;
  if (context.candidate) {
    Write(context, state.wire.entity, static_cast<double>(state.wire.counter));
    if (owner.fail_start)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  return 0;
}
int32_t Update(void *pointer, double) {
  auto &state = *static_cast<State *>(pointer);
  ++state.wire.counter;
  auto &context = Owner::Get(state.host->context);
  Write(context, state.wire.entity, static_cast<double>(state.wire.counter));
  context.owner->active = &state;
  return 0;
}
void Stop(void *pointer) {
  auto &state = *static_cast<State *>(pointer);
  auto &context = Owner::Get(state.host->context);
  runtime::WorldCommandBuffer commands;
  commands.DestroyEntity(state.wire.entity);
  assert(commands.Apply(*context.world));
  ++context.owner->stops;
}
void Destroy(void *pointer) {
  auto *state = static_cast<State *>(pointer);
  ++Owner::Get(state->host->context).owner->frees;
  delete state;
}
std::uint32_t Save(void *pointer, void *output, std::uint32_t size) {
  auto &state = *static_cast<State *>(pointer);
  auto &owner = *Owner::Get(state.host->context).owner;
  if (!owner.job_finished)
    owner.captured_before_job = true;
  if (owner.oversized)
    return Host::kMaximumStateBytes + 1;
  if (output && size == sizeof(Wire))
    std::memcpy(output, &state.wire, sizeof(Wire));
  return sizeof(Wire);
}
int32_t Restore(void *pointer, const void *input, std::uint32_t size) {
  auto &state = *static_cast<State *>(pointer);
  if (!input || size != sizeof(Wire))
    return -1;
  std::memcpy(&state.wire, input, sizeof(Wire));
  return Owner::Get(state.host->context).owner->fail_restore ? -3 : 0;
}
int32_t Load(std::uint32_t abi, NexoraGameModuleV3 *module) {
  if (abi != NEXORA_GAMEPLAY_ABI_VERSION)
    return -2;
  *module = {sizeof(*module), abi,    NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION,
             nullptr,         Create, Start,
             nullptr,         Update, Stop,
             Destroy,         Save,   Restore};
  return 0;
}
int32_t NoMigration(std::uint32_t abi, NexoraGameModuleV3 *module) {
  const auto result = Load(abi, module);
  module->capabilities = 0;
  module->save_state = nullptr;
  module->load_state = nullptr;
  return result;
}
int32_t LoadWithReloadLifecycle(std::uint32_t abi, NexoraGameModuleV3 *module) {
  const auto result = Load(abi, module);
  module->capabilities |= NEXORA_GAMEPLAY_CAPABILITY_RELOAD_LIFECYCLE;
  return result;
}
int32_t BadAbi(std::uint32_t abi, NexoraGameModuleV3 *module) {
  const auto result = Load(abi, module);
  ++module->abi_version;
  return result;
}

int32_t CreateBounded(void **state, const NexoraGameplayHostV3 *host) {
  assert(host->struct_size == sizeof(*host));
  return Create(state, host);
}
int32_t LoadBounded(std::uint32_t abi, NexoraGameModuleV3 *module) {
  const auto result = Load(abi, module);
  module->create = CreateBounded;
  return result;
}

void Verify(const Owner &owner, std::uint64_t counter) {
  const auto *entity = owner.world.FindEntity(owner.entity);
  assert(entity && entity->transform.x == static_cast<double>(counter));
  assert(owner.world.ActiveSceneCount() == 1 && owner.world.ActiveEntityCount() == 1);
}
} // namespace

int main() {
  static_assert(std::is_nothrow_move_assignable_v<runtime::World>);
  Owner owner;
  Host host(owner.Api(), owner.Barrier(), owner.Transaction());
  assert(host.Load(Load) && host.Update(0.1));
  Verify(owner, 1);
  for (auto flag : {&Owner::fail_begin, &Owner::fail_create, &Owner::fail_restore,
                    &Owner::fail_start, &Owner::oversized}) {
    owner.*flag = true;
    const auto generation = host.Generation();
    assert(!host.Reload(Load));
    assert(host.Generation() == generation && owner.commits == 0 && owner.stops == 0);
    assert(owner.allocations - owner.frees == 1);
    Verify(owner, 1);
    owner.*flag = false;
  }
  assert(!host.Reload(BadAbi));
  assert(host.GetReloadFailure() == Host::ReloadFailure::InvalidCandidate);
  assert(!host.Reload(NoMigration));
  assert(host.GetReloadFailure() == Host::ReloadFailure::RestartRequired);
  Verify(owner, 1);

  auto lease = host.AcquireWork(1);
  assert(lease && lease.Generation() == 1 && !host.AcquireWork(0));
  owner.job_finished = false;
  owner.barrier_entered = false;
  auto *old = static_cast<State *>(owner.active);
  std::thread job([&, ticket = std::move(lease)]() mutable {
    {
      std::unique_lock lock(owner.mutex);
      owner.changed.wait(lock, [&] { return owner.release_job; });
    }
    old->wire.counter += 100;
    owner.job_finished = true;
    ticket = {};
  });
  std::atomic<bool> reloaded{};
  std::thread release([&] {
    std::unique_lock lock(owner.mutex);
    owner.changed.wait(lock, [&] { return owner.barrier_entered; });
    assert(!reloaded && !host.AcquireWork(1));
    owner.release_job = true;
    owner.changed.notify_all();
  });
  assert(host.Reload(Load)); // World hooks stay on the original owner thread.
  reloaded = true;
  job.join();
  release.join();
  assert(!owner.captured_before_job && host.Generation() == 2 && owner.stops == 1);
  assert(!host.AcquireWork(1));
  Verify(owner, 101); // Retired Stop destroyed its own entity, not this transferred handle.
  assert(host.Update(0.1));
  Verify(owner, 102);
  host.Unload();
  assert(owner.allocations == owner.frees && !owner.world.FindEntity(owner.entity));
  assert(!host.AcquireWork(2));

  Owner no_isolation;
  Host strict(no_isolation.Api(), {}, {});
  assert(strict.Load(Load) && !strict.Reload(Load));
  assert(strict.GetReloadFailure() == Host::ReloadFailure::RestartRequired);
  assert(strict.Update(0.1));
  Verify(no_isolation, 1);

  Owner widened;
  auto api = widened.Api();
  api.struct_size = std::numeric_limits<std::uint32_t>::max();
  auto transaction = widened.Transaction();
  transaction.begin = [](void *pointer, NexoraGameplayHostV3 &candidate) noexcept {
    const auto hooks = static_cast<Owner *>(pointer)->Transaction();
    const bool begun = hooks.begin(pointer, candidate);
    candidate.struct_size = std::numeric_limits<std::uint32_t>::max();
    return begun;
  };
  Host bounded(api, {}, transaction);
  assert(bounded.Load(LoadBounded) && bounded.Update(0.1) && bounded.Reload(LoadBounded));
  Verify(widened, 1);

  // Candidate-only lifecycle admission must happen before candidate Create can mutate even
  // its staging World. Both generations must opt in; a rejected candidate leaves live state.
  for (const bool old_supports_reload : {false, true}) {
    Owner lifecycle;
    auto hooks = lifecycle.Transaction();
    hooks.begin = [](void *pointer, NexoraGameplayHostV3 &candidate) noexcept {
      auto owned = static_cast<Owner *>(pointer)->Transaction();
      const bool begun = owned.begin(pointer, candidate);
      candidate.capabilities |= NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE;
      return begun;
    };
    Host modern(lifecycle.Api(), {}, hooks);
    assert(modern.Load(old_supports_reload ? LoadWithReloadLifecycle : Load));
    assert(modern.Update(0.1));
    const auto generation = modern.Generation();
    const auto allocations = lifecycle.allocations;
    assert(!modern.Reload(old_supports_reload ? Load : LoadWithReloadLifecycle));
    assert(modern.GetReloadFailure() == Host::ReloadFailure::RestartRequired);
    assert(modern.Generation() == generation && lifecycle.commits == 0);
    assert(lifecycle.allocations == allocations && lifecycle.stops == 0);
    Verify(lifecycle, 1);
    assert(modern.Update(0.1));
    Verify(lifecycle, 2);
    if (old_supports_reload) {
      assert(modern.Reload(LoadWithReloadLifecycle));
      Verify(lifecycle, 2);
      assert(modern.Update(0.1));
      Verify(lifecycle, 3);
    }
    modern.Unload();
    assert(lifecycle.allocations == lifecycle.frees);
  }
}
