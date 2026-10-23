#include "../../Apps/TwoDPlayer/UiInputGate.h"
#include "Nexora/Runtime/GameplayLifecycle.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
extern "C" int32_t NexoraGameModuleLoad(uint32_t, NexoraGameModuleV3 *);
extern "C" int64_t NexoraLifecycleControl(void *, uint32_t, uint32_t);
using namespace nexora::runtime;
struct Host {
  GameplayServices services;
  unsigned allocated{}, freed{};
};
int main() {
  // Exercise the exact Player UI boundary: a restored held button cannot submit a draft.
  nexora::player::UiInputGate gate;
  nexora::twod::GameplayUiInput ui;
  ui.activate = true;
  gate.Apply(ui, true, false);
  assert(!ui.activate && !ui.focused);
  for (unsigned i = 0; i < 4; ++i) {
    ui = {};
    ui.activate = true;
    ui.pointer_down = true;
    gate.Apply(ui, false, false);
    assert(!ui.activate && !ui.pointer_down && !ui.focused);
  }
  ui = {};
  gate.Apply(ui, false, true);
  assert(!ui.focused);
  ui = {};
  ui.activate = true;
  gate.Apply(ui, false, false);
  assert(ui.activate && ui.focused);

  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-sp06-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(p, e);
    }
  } cleanup{root};
  GameplayStorage storage(root, "sp06-diagnostic");
  GameplayLifecycle lifecycle;
  Host context;
  context.services.lifecycle = lifecycle.Api();
  auto &files = context.services.storage;
  files.struct_size = sizeof(files);
  files.abi_version = 1;
  assert(storage.GetApi(files) == 0);
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = 3;
  host.context = &context;
  host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API;
  host.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *out) {
    return static_cast<Host *>(p)->services.GetApi(out);
  };
  host.allocate = [](void *p, uint64_t, uint64_t n, uint64_t a) -> void * {
    ++static_cast<Host *>(p)->allocated;
    return ::operator new(n, std::align_val_t(a));
  };
  host.deallocate = [](void *p, uint64_t, void *v, uint64_t, uint64_t a) {
    ++static_cast<Host *>(p)->freed;
    ::operator delete(v, std::align_val_t(a));
  };
  NexoraGameModuleV3 module{};
  module.struct_size = sizeof(module);
  assert(NexoraGameModuleLoad(3, &module) == 0);
  void *state{};
  assert(module.create(&state, &host) == 0 && module.on_start(state) == 0);
  auto command = [&](uint32_t k, uint32_t v = 0) { return NexoraLifecycleControl(state, k, v); };
  auto tick = [&](double seconds = 1.0 / 60) {
    assert(module.fixed_update(state, seconds) == 0);
    assert(module.update(state, seconds) == 0);
  };
  auto resume = [&] {
    assert(command(1) == 1);
    const auto before = command(6);
    for (unsigned n = 0; n < 181; ++n)
      tick();
    assert(command(5) == 0 && command(6) <= before + 1);
  };
  tick();
  assert(command(6) == 1 && command(7) == 17);
  assert(lifecycle.Focus(false));
  assert(lifecycle.Focus(true)); // same batch still pauses before fixed tick
  tick();
  assert(command(5) == 1 && command(6) == 1 && command(7) == 17);
  std::array<unsigned char, 16> previous{};
  NexoraStorageInfoV1 info{sizeof(info), 1, 0, 0};
  assert(files.load(files.context, "sp06", 4, previous.data(), previous.size(), &info) == 0);
  assert(previous[0] == 1 && previous[8] == 17);
  for (unsigned i = 0; i < 120; ++i)
    tick();
  assert(command(6) == 1);
  resume();
  tick();
  const auto before = command(6);
  assert(command(8, 42) == 0); // Pending draft survives every interruption.
  std::filesystem::create_directory(root /
                                    "sp06.tmp"); // deterministic atomic-write failure even as root
  {
    std::ofstream blocker(root / "sp06.tmp" / "keep");
    blocker << "blocked";
  }
  assert(lifecycle.RequestClose());
  tick();
  assert(command(9) == 1 && command(6) == before && !lifecycle.ExitRequested());
  assert(command(4) == -3 && !lifecycle.ExitRequested());
  std::array<unsigned char, 16> retained{};
  assert(files.load(files.context, "sp06", 4, retained.data(), retained.size(), &info) == 0 &&
         retained == previous);
  assert(command(3) == 0);
  tick();
  assert(command(1) == 0); // Cancel close cannot bypass a failed snapshot.
  std::filesystem::remove_all(root / "sp06.tmp");
  assert(command(2) == 1);
  resume();
  assert(command(7) == 42);
  assert(lifecycle.Minimize(true));
  tick();
  const auto minimized = command(6);
  for (unsigned i = 0; i < 10; ++i)
    tick();
  assert(command(6) == minimized);
  assert(lifecycle.Minimize(false));
  tick();
  assert(command(5) == 1);
  resume();
  assert(lifecycle.Devices(1, 0));
  assert(command(12, 0) == 0);
  tick();
  assert(lifecycle.Devices(0, 0));
  tick();
  const auto removed = command(6);
  assert(command(1) == 0 && command(11, 50) == -2);
  assert(lifecycle.Devices(1, 0));
  tick();
  assert(command(6) == removed && command(5) == 1);
  resume();
  assert(command(1) == 0); // Running cannot restart the countdown.
  assert(lifecycle.Suspend(true));
  tick();
  const auto asleep = command(6);
  for (unsigned i = 0; i < 20; ++i)
    tick(600);
  assert(command(6) == asleep && command(1) == 0);
  assert(lifecycle.Suspend(false));
  tick(600);
  assert(command(6) == asleep && command(5) == 1);
  assert(command(1) == 1);
  tick(.1);
  assert(lifecycle.Focus(false));
  tick();
  assert(lifecycle.Focus(true));
  tick();
  assert(command(10) == 0 && command(5) == 1);
  resume();
  assert(lifecycle.RequestClose());
  tick();
  assert(command(4) == 0 && lifecycle.ExitRequested());
  const auto savedTicks = command(6);
  module.on_stop(state);
  module.destroy(state);
  // Fresh module reads an actual persisted snapshot, starts paused, retains uncommitted draft.
  assert(module.create(&state, &host) == 0);
  assert(command(5) == 1 && command(6) == savedTicks && command(7) == 42);
  module.destroy(state);
  assert(context.allocated == context.freed);
  std::cout << "SP-06 real Zig: before-fixed pause, pending draft, 3-second explicit resume, "
               "native-slot loss, suspend, failed atomic save preservation/retry, close "
               "cancellation/accept and reopen passed\n";
}
