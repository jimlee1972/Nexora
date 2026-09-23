#include "GameplayConformanceVectors.h"
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <new>
#include <string_view>

extern "C" int32_t NexoraGameModuleLoad(uint32_t requested_abi, NexoraGameModuleV3 *module);
extern "C" uint32_t NexoraGameModuleUpdateCount();
extern "C" double NexoraGameModuleElapsedSeconds();
extern "C" uint32_t NexoraGameModuleFixedUpdateCount();
extern "C" uint32_t NexoraGameModuleStartCount();

namespace {

struct HostState final {
  bool received_log{};
  std::uint32_t allocations{};
  std::uint32_t deallocations{};
  std::uint64_t allocation_owner{};
  std::uint64_t deallocation_owner{};
  nexora::game::GameplayTransformWire transform{1.0, 2.0, 3.0};
};

void Log(void *context, uint32_t, const char *message, uint32_t length) {
  auto &state = *static_cast<HostState *>(context);
  state.received_log = std::string_view(message, length) == "Zig gameplay initialized";
}

int32_t ReadComponent(void *context, uint64_t entity, uint64_t type, void *data, uint32_t size) {
  if (entity != 0 || type != nexora::game::TransformComponentType() ||
      size != sizeof(nexora::game::GameplayTransformWire))
    return -1;
  *static_cast<nexora::game::GameplayTransformWire *>(data) =
      static_cast<HostState *>(context)->transform;
  return 0;
}

int32_t WriteComponent(void *context, uint64_t entity, uint64_t type, const void *data,
                       uint32_t size) {
  if (entity != 0 || type != nexora::game::TransformComponentType() ||
      size != sizeof(nexora::game::GameplayTransformWire))
    return -1;
  static_cast<HostState *>(context)->transform =
      *static_cast<const nexora::game::GameplayTransformWire *>(data);
  return 0;
}

void *Allocate(void *context, std::uint64_t owner, std::uint64_t size, std::uint64_t alignment) {
  auto &state = *static_cast<HostState *>(context);
  ++state.allocations;
  state.allocation_owner = owner;
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)});
}

void Deallocate(void *context, std::uint64_t owner, void *allocation, std::uint64_t,
                std::uint64_t alignment) {
  auto &state = *static_cast<HostState *>(context);
  ++state.deallocations;
  state.deallocation_owner = owner;
  ::operator delete(allocation, std::align_val_t{static_cast<std::size_t>(alignment)});
}

} // namespace

int main() {
  HostState state;
  NexoraGameplayHostV3 api{sizeof(NexoraGameplayHostV3),
                           NEXORA_GAMEPLAY_ABI_VERSION,
                           NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR,
                           &state,
                           Log,
                           ReadComponent,
                           WriteComponent,
                           Allocate,
                           Deallocate};
  nexora::runtime::GameplayModuleHost host(api);

  assert(host.Load(NexoraGameModuleLoad));
  assert(state.received_log);
  assert(nexora::test::RunGameplayConformanceVectors(host));
  assert(NexoraGameModuleStartCount() == 1);
  assert(NexoraGameModuleFixedUpdateCount() == nexora::test::kExpectedFixedUpdates);
  assert(NexoraGameModuleUpdateCount() == nexora::test::kExpectedUpdates);
  assert(std::abs(NexoraGameModuleElapsedSeconds() - nexora::test::kExpectedElapsedSeconds) <
         0.000001);
  assert(std::abs(state.transform.x - (1.0 + nexora::test::kExpectedElapsedSeconds)) < 0.000001);
  assert(host.Reload(NexoraGameModuleLoad));
  assert(NexoraGameModuleUpdateCount() == nexora::test::kExpectedUpdates);
  assert(host.GetReloadStats().migrated_bytes > 0);
  host.Unload();
  assert(state.allocations == 2);
  assert(state.deallocations == 2);
  assert(state.allocation_owner == NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE);
  assert(state.deallocation_owner == NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE);
}
