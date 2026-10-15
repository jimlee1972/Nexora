#include "CoreConsoleIngress.h"
#include "PlayGameplayModule.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
NexoraGameplayHostV3 host{};
bool fail_create{}, fail_start{}, retain{};
unsigned stops{}, destroys{};
editor::preview::PlayGameplayModule *owner{};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = host.allocate(host.context, 7, 64, 16);
  Require(*state && owner->Allocations().live_bytes == 64, "Create allocation not observed");
  return fail_create ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) { return fail_start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK; }
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
void Stop(void *) {
  ++stops;
  Require(owner->Allocations().live_bytes == 64, "Stop lost live allocation observation");
}
void Destroy(void *state) {
  ++destroys;
  Require(owner->Allocations().live_bytes == 64, "Destroy lost live allocation observation");
  static_cast<unsigned char *>(state)[63] = 0x5a;
  if (!retain)
    host.deallocate(host.context, 7, state, 64, 16);
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
} // namespace

int main() {
  try {
    runtime::World source;
    const auto scene = source.LoadScene("Allocation source");
    Require(source.Activate(scene), "activate source failed");
    source.CreateEntity(scene);
    const auto baseline = source.SaveScene(scene);
    runtime::PlaySession play(source);
    core::AsyncLogService logs{64};
    logs.Start();
    runtime::RuntimeConsole console{64};
    editor::preview::CoreConsoleIngress ingress{logs, console, "NexoraEditor"};
    unsigned warnings{};
    bool throwing_sink{};
    editor::preview::PlayGameplayModule module([&](std::uint32_t level, std::string message) {
      ++warnings;
      Require(!owner->Allocations().live_bytes &&
                  owner->LastTeardownAllocations().reclaimed_bytes == 64 &&
                  !host.allocate(host.context, 7, 16, 16),
              "warning ran before reclamation/revocation");
      Require(level == static_cast<std::uint32_t>(core::LogLevel::Warning) &&
                  message == "Play teardown reclaimed 1 host allocations (64 payload bytes) "
                             "left after module callbacks.",
              "warning lost exact bounded scope/counts");
      if (throwing_sink)
        throw std::runtime_error("test sink failure");
      logs.Write(static_cast<core::LogLevel>(level), "Gameplay", std::move(message));
    });
    owner = &module;
    // Success, retained success, failed Create, and failed Start repeat in the same owner.
    for (unsigned cycle = 0; cycle < 32; ++cycle) {
      fail_create = cycle % 4 == 2;
      fail_start = cycle % 4 == 3;
      retain = cycle % 4 != 0;
      stops = destroys = 0;
      const auto before = warnings;
      Require(play.Start(.25, [](runtime::World &, double) { return true; }), "Play start failed");
      const bool loaded = module.Load(*play.PlayWorld(), Loader);
      Require(loaded == (!fail_create && !fail_start), "failure load result changed");
      if (loaded) {
        const auto copy = module.Allocations();
        Require(copy.live_allocations == 1 && copy.live_bytes == 64 && copy.peak_bytes == 64 &&
                    !copy.rejected_allocations && !copy.rejected_deallocations,
                "new session retained prior counters");
        if (cycle == 0) {
          auto *extra = host.allocate(host.context, 8, 96, 32);
          Require(extra, "extra allocation failed");
          host.deallocate(host.context, 9, extra, 96, 32);
          host.deallocate(host.context, 8, extra, 95, 32);
          host.deallocate(host.context, 8, extra, 96, 16);
          Require(module.Allocations().live_bytes == 160 &&
                      module.Allocations().rejected_deallocations == 3,
                  "wrong ownership/size/alignment changed live allocation");
          host.deallocate(host.context, 8, extra, 96, 32);
          host.deallocate(host.context, 8, nullptr, 96, 32);
          constexpr std::uint64_t chunk = 16 * 1024 * 1024;
          std::array<void *, 3> large{};
          for (auto &pointer : large) {
            pointer = host.allocate(host.context, 8, chunk, 16);
            Require(pointer, "in-budget allocation failed");
          }
          Require(!host.allocate(host.context, 8, chunk, 16) &&
                      !host.allocate(host.context, 8, 0, 16) &&
                      !host.allocate(host.context, 8, chunk + 1, 16) &&
                      !host.allocate(host.context, 8, 16, 3) &&
                      !host.allocate(host.context, 8, 16, 8192),
                  "invalid/over-budget allocation admitted");
          for (auto pointer : large)
            host.deallocate(host.context, 8, pointer, chunk, 16);
          const auto observed = module.Allocations();
          Require(observed.live_allocations == 1 && observed.live_bytes == 64 &&
                      observed.peak_allocations == 4 && observed.peak_bytes == 3 * chunk + 64 &&
                      observed.rejected_allocations == 5 && observed.rejected_deallocations == 4 &&
                      copy.live_bytes == 64 && copy.peak_bytes == 64,
                  "payload/quota/owning snapshot accounting incorrect");
        }
      }
      module.Unload();
      const auto last = module.LastTeardownAllocations();
      Require(!module.Allocations().live_bytes && !module.Allocations().peak_bytes &&
                  !last.live_allocations && !last.live_bytes &&
                  last.reclaimed_allocations == (retain ? 1U : 0U) &&
                  last.reclaimed_bytes == (retain ? 64U : 0U) &&
                  warnings == before + (retain ? 1U : 0U),
              "teardown did not distinguish paired release from fallback reclamation");
      Require(stops == (loaded ? 1U : 0U) && destroys == (fail_create ? 0U : 1U),
              "module lifecycle order changed");
      module.Unload();
      Require(warnings == before + (retain ? 1U : 0U) &&
                  module.LastTeardownAllocations().reclaimed_bytes == last.reclaimed_bytes &&
                  !host.allocate(host.context, 7, 16, 16) && !module.Allocations().live_bytes,
              "repeated Unload duplicated report or unbound allocation admitted");
      Require(play.Stop() && source.SaveScene(scene) == baseline, "allocation escaped Play");
    }
    // Observe the same production producer/Console path, including severity and scope.
    logs.Flush();
    ingress.Poll();
    const auto records = console.Snapshot();
    Require(records.size() == 24 && console.DroppedCount() == 0 &&
                std::ranges::all_of(records,
                                    [](const auto &record) {
                                      return record.category == "Gameplay" &&
                                             record.severity ==
                                                 runtime::RuntimeLogSeverity::Warning &&
                                             record.message.find("64 payload bytes") !=
                                                 std::string::npos;
                                    }),
            "fallback warnings did not reach bounded graphical Console ingress");
    fail_create = fail_start = false;
    retain = throwing_sink = true;
    Require(play.Start(.25, [](runtime::World &, double) { return true; }) &&
                module.Load(*play.PlayWorld(), Loader),
            "throwing sink load failed");
    module.Unload();
    Require(!module.Allocations().live_bytes &&
                module.LastTeardownAllocations().reclaimed_bytes == 64 && play.Stop(),
            "throwing sink interrupted cleanup");
    std::cout << "Play host allocations: 32 paired/retained/failed lifecycles, quota, ownership, "
                 "Console warning and throwing sink passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
