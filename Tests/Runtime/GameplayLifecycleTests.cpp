#include "Nexora/Runtime/GameplayLifecycle.h"
#include "Nexora/Runtime/GameplayServices.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
using namespace nexora::runtime;
struct Motor {
  bool fail{};
  unsigned writes{};
  float low{}, high{};
  static int32_t Write(void *p, uint32_t, float l, float h) {
    auto &s = *static_cast<Motor *>(p);
    ++s.writes;
    if (s.fail)
      return -3;
    s.low = l;
    s.high = h;
    return 0;
  }
};
int main() {
  Motor motor;
  GameplayLifecycle life(Motor::Write, &motor);
  auto api = life.Api();
  auto state = [&] {
    NexoraLifecycleStateV1 s{};
    s.struct_size = sizeof(s);
    assert(api.get_state(api.context, &s) == 0);
    return s;
  };
  assert(state().flags == 1 && !life.Guarded());
  assert(api.rumble(api.context, 0, 1, 1, 10) == -2);
  assert(life.Devices(3, 1));
  assert(api.rumble(api.context, 1, 1, 1, 10) == -2);
  for (float bad : {-1.F, 1.1F, std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()})
    assert(api.rumble(api.context, 0, bad, 0, 10) == -1);
  assert(api.rumble(api.context, 4, 0, 0, 0) == -1);
  assert(api.rumble(api.context, 0, 1, 0, 0) == -1);
  assert(api.rumble(api.context, 0, 1, 0, 5001) == -1);
  assert(api.rumble(api.context, 0, .5F, 1, 10) == 0);
  assert(motor.low == .5F && motor.high == 1);
  assert(life.Tick(GameplayLifecycle::Clock::now() + std::chrono::seconds(1)) && motor.low == 0);
  assert(api.rumble(api.context, 0, 1, 1, 5000) == 0);
  motor.fail = true;
  assert(!life.Focus(false));
  assert(state().flags == 0 && state().interruption_sequence == 1 && motor.low == 1);
  assert(api.rumble(api.context, 0, 1, 0, 1) == -3);
  motor.fail = false;
  assert(life.Tick() && motor.low == 0); // native failure retries zero, never stale gain
  assert(life.Focus(true) && life.Minimize(true) && life.Minimize(false));
  assert(life.Suspend(true) && life.Suspend(true) && life.Suspend(false));
  auto s = state();
  assert(s.flags == 1 && s.interruption_sequence == 3 && s.suspend_sequence == 1 &&
         s.resume_sequence == 1);
  assert(life.Devices(2, 0) && life.Devices(3, 1));
  assert(state().disconnect_sequence[0] == 1);
  assert(life.Disconnected(1) && state().disconnect_sequence[1] == 1);
  assert(api.set_close_guard(api.context, 1) == 0 && life.RequestClose());
  auto token = state().close_token;
  assert(token && !life.ExitRequested() && life.RequestClose() && state().close_token == token);
  assert(api.set_close_guard(api.context, 0) == -3);
  assert(api.resolve_close(api.context, token + 1, 1) == -1);
  assert(api.resolve_close(api.context, token, 0) == 0 && !life.ExitRequested());
  assert(life.RequestClose() && state().close_token > token);
  assert(api.resolve_close(api.context, token, 1) == -1);
  assert(api.resolve_close(api.context, state().close_token, 1) == 0 && life.ExitRequested());
  NexoraLifecycleStateV1 bad{};
  bad.struct_size = 1;
  bad.flags = 123;
  assert(api.get_state(api.context, &bad) == -2 && bad.flags == 123);
  assert(api.get_state(nullptr, &bad) == -1);
  std::thread thread([&] {
    assert(api.get_state(api.context, &bad) == -3 && bad.flags == 123);
    assert(api.set_close_guard(api.context, 1) == -3);
    assert(api.resolve_close(api.context, 1, 0) == -3);
    assert(api.rumble(api.context, 0, 0, 0, 0) == -3);
    assert(!life.Focus(false) && !life.Tick());
  });
  thread.join();
  struct Future {
    NexoraLifecycleStateV1 state{};
    uint64_t canary = 0x12345678;
  } future;
  future.state.struct_size = sizeof(future);
  assert(api.get_state(api.context, &future.state) == 0 &&
         future.state.struct_size == sizeof(future) && future.canary == 0x12345678);
  GameplayServices services;
  services.lifecycle = api;
  alignas(NexoraGameplayServicesApiV1)
      std::array<std::byte, sizeof(NexoraGameplayServicesApiV1) + 16>
          memory;
  for (size_t size = 56; size <= sizeof(NexoraGameplayServicesApiV1); ++size) {
    memory.fill(std::byte{0x5a});
    uint32_t header[]{uint32_t(size), 1};
    std::memcpy(memory.data(), header, sizeof(header));
    auto *table = reinterpret_cast<NexoraGameplayServicesApiV1 *>(memory.data());
    assert(services.GetApi(table) == 0);
    assert(table->struct_size == 56 + (size - 56) / 8 * 8);
    for (size_t i = table->struct_size; i < memory.size(); ++i)
      assert(memory[i] == std::byte{0x5a});
  }
  GameplayLifecycle legacy;
  assert(legacy.RequestClose() && legacy.ExitRequested());
  {
    GameplayLifecycle stopped(Motor::Write, &motor);
    assert(stopped.Devices(1, 1));
    auto a = stopped.Api();
    assert(a.rumble(a.context, 0, 1, 1, 5000) == 0);
  }
  assert(motor.low == 0 && motor.high == 0);
  std::cout << "SP-06 lifecycle tokens, prefix canaries, retained interruptions and bounded motor "
               "safety passed\n";
}
