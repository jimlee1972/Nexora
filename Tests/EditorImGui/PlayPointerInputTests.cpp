#include "PlayInputForwarding.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace Nexora::Window;
using Snapshot = NexoraPointerInputSnapshotV1;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
WindowEvent Event(WindowEventType type, int x = 0, int y = 0) {
  WindowEvent e;
  e.type = type;
  e.value0 = x;
  e.value1 = y;
  return e;
}
bool Neutral(const Snapshot &s) {
  return s.struct_size == sizeof(s) && s.abi_version == 1 && !s.delta_x && !s.delta_y &&
         !s.wheel_x && !s.wheel_y && !s.buttons && !s.reserved;
}
NexoraGameplayHostV3 host{};
Snapshot fixed{}, updated{};
unsigned creates{}, starts{}, stops{}, destroys{};
Snapshot Capture(std::uint32_t user = 0) {
  Snapshot s{};
  s.struct_size = sizeof(s);
  Require(NexoraCapturePointerInputV1(&host, user, &s) == NEXORA_GAMEPLAY_OK,
          "provider capture failed");
  return s;
}
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  Require(Neutral(Capture()), "Create input was not neutral");
  ++creates;
  *state = &host;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) {
  Require(Neutral(Capture()), "Start input was not neutral");
  ++starts;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *, double) {
  fixed = Capture();
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double) {
  updated = Capture();
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {
  Require(Neutral(Capture()), "Stop lost provider or retained input");
  ++stops;
}
void Destroy(void *) {
  Require(Neutral(Capture()), "Destroy lost provider or retained input");
  ++destroys;
}
int32_t Load(uint32_t version, NexoraGameModuleV3 *module) {
  if (version != 3)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->capabilities = NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE;
  module->create = Create;
  module->on_start = Start;
  module->fixed_update = Fixed;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
void StateContracts() {
  using editor::preview::PlayInputState;
  for (int cycle = 0; cycle < 32; ++cycle) {
    PlayInputState input;
    input.SetFocused(true);
    input.Process(std::array{Event(WindowEventType::Pointer, 100, 200),
                             Event(WindowEventType::Wheel, 120, -120),
                             Event(WindowEventType::PointerButton, 4, 1)});
    Require(Neutral(input.PointerSnapshot()), "acquisition leaked impulses or buttons");
    input.Process(std::array{
        Event(WindowEventType::Pointer, 110, 190), Event(WindowEventType::Pointer, 115, 205),
        Event(WindowEventType::Wheel, -120, 240), Event(WindowEventType::Wheel, 240, -120),
        Event(WindowEventType::PointerButton, 4, 1), Event(WindowEventType::PointerButton, 0, 1)});
    const auto copy = input.PointerSnapshot();
    Require(copy.delta_x == 15 && copy.delta_y == 5 && copy.wheel_x == 120 && copy.wheel_y == 120 &&
                copy.buttons == 17 && copy.sequence == input.Snapshot().sequence &&
                input.Snapshot().buttons == 2,
            "batch arithmetic, raw buttons, or legacy sequence changed");
    const auto repeat = input.PointerSnapshot();
    Require(std::memcmp(&copy, &repeat, sizeof(copy)) == 0, "capture consumed displacement");
    input.Process({});
    const auto empty = input.PointerSnapshot();
    Require(!empty.delta_x && !empty.delta_y && !empty.wheel_x && !empty.wheel_y &&
                empty.buttons == 17 && empty.sequence > copy.sequence && copy.delta_x == 15,
            "empty batch replayed impulses, lost held buttons, or mutated owned copy");
    for (const auto domain :
         {WindowEventType::Resized, WindowEventType::DpiChanged, WindowEventType::DisplayChanged}) {
      for (const bool last : {false, true}) {
        const auto pointer = Event(WindowEventType::Pointer, -10000, 30000);
        const auto change = Event(domain);
        input.Process(last ? std::array{pointer, change} : std::array{change, pointer});
        Require(!input.PointerSnapshot().delta_x && !input.PointerSnapshot().wheel_y,
                "coordinate-domain change leaked a displacement");
        input.Process(std::array{Event(WindowEventType::Pointer, -9990, 30020)});
        Require(input.PointerSnapshot().delta_x == (last ? 0 : 10) &&
                    input.PointerSnapshot().delta_y == (last ? 0 : 20),
                "coordinate-domain event order retained an invalid anchor");
      }
    }
    input.Process(std::array{Event(WindowEventType::Pointer, std::numeric_limits<int>::min(),
                                   std::numeric_limits<int>::max())});
    input.Process(std::array{Event(WindowEventType::Pointer, std::numeric_limits<int>::max(),
                                   std::numeric_limits<int>::min()),
                             Event(WindowEventType::Wheel, std::numeric_limits<int>::max(),
                                   std::numeric_limits<int>::min())});
    Require(input.PointerSnapshot().delta_x == PlayInputState::kMaximumPointerDelta &&
                input.PointerSnapshot().delta_y == -PlayInputState::kMaximumPointerDelta &&
                input.PointerSnapshot().wheel_x == PlayInputState::kMaximumWheelUnits &&
                input.PointerSnapshot().wheel_y == -PlayInputState::kMaximumWheelUnits,
            "extreme native coordinates overflowed or exceeded bounds");
    for (const auto release : {Event(WindowEventType::FocusChanged),
                               Event(WindowEventType::Key, static_cast<int>(Key::Escape), 1)}) {
      for (const bool last : {false, true}) {
        input.SetFocused(true);
        input.Process({});
        const auto pointer = Event(WindowEventType::Pointer, 123, 456);
        input.Process(last ? std::array{pointer, release} : std::array{release, pointer});
        input.Process(std::array{pointer, Event(WindowEventType::Wheel, 120, 240)});
        Require(Neutral(input.PointerSnapshot()), "release did not win the full batch");
      }
    }
    input.SetFocused(true);
    input.Process(
        std::array{Event(WindowEventType::Pointer, 10, 20), Event(WindowEventType::Resized)});
    input.Process(std::array{Event(WindowEventType::Pointer, 30, 40)});
    Require(Neutral(input.PointerSnapshot()), "acquisition retained an old-domain anchor");
    editor::PlayInputBindings rebound;
    rebound.controls[1][0] = editor::PlayInputControl::B;
    Require(input.SetBindings(rebound) && Neutral(input.PointerSnapshot()),
            "profile change did not revoke impulses/buttons");
    input.Process(std::array{Event(WindowEventType::Pointer, 200, 300)});
    Require(Neutral(input.PointerSnapshot()), "profile acquisition admitted motion");
  }
}
void ProviderContracts() {
  runtime::World world;
  const auto scene = world.LoadScene("source");
  Require(world.Activate(scene), "source activation failed");
  const auto target = world.CreateEntity(scene).id;
  runtime::PlaySession play(world);
  editor::preview::PlayGameplayModule module;
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    Require(play.Start(.25, [&](runtime::World &, double dt) { return module.FixedUpdate(dt); }) &&
                module.Load(*play.PlayWorld(), Load),
            "Play module startup failed");
    for (uint32_t user = 0; user < 4; ++user) {
      editor::preview::ForwardPlayInput(
          play, module, true, std::array{Event(WindowEventType::Pointer, 10, 20)}, {}, user);
      editor::preview::ForwardPlayInput(play, module, true,
                                        std::array{Event(WindowEventType::Pointer, 17, 11),
                                                   Event(WindowEventType::Wheel, -120, 240),
                                                   Event(WindowEventType::PointerButton, 2, 1)},
                                        {}, user);
      const auto selected = Capture(user);
      Require(selected.delta_x == 7 && selected.delta_y == -9 && selected.wheel_x == -120 &&
                  selected.wheel_y == 240 && selected.buttons == 4,
              "selected user lost copied motion/wheel/raw buttons");
      for (uint32_t other = 0; other < 4; ++other)
        if (other != user) {
          const auto neutral = Capture(other);
          Require(Neutral(neutral) && neutral.sequence == selected.sequence,
                  "logical user leaked pointer input");
        }
      if (user == 0) {
        Require(play.Tick() && module.Update(.125), "gameplay callbacks failed");
        Require(std::memcmp(&selected, &fixed, sizeof(fixed)) == 0 &&
                    std::memcmp(&fixed, &updated, sizeof(fixed)) == 0,
                "Fixed/Update consumed or changed the shared batch");
      }
      auto output = selected;
      Require(NexoraCapturePointerInputV1(&host, 4, &output) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  std::memcmp(&output, &selected, sizeof(output)) == 0 &&
                  NexoraCapturePointerInputV1(&host, user, nullptr) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
              "invalid capture changed output");
      output.struct_size = sizeof(output) - 1;
      const auto before = output;
      Require(NexoraCapturePointerInputV1(&host, user, &output) ==
                      NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
                  std::memcmp(&before, &output, sizeof(output)) == 0,
              "short output was partially written");
      struct Future {
        Snapshot value;
        std::uint64_t suffix;
      } future{selected, 0x123456789abcdef0};
      future.value.struct_size = sizeof(future);
      Require(NexoraCapturePointerInputV1(&host, user, &future.value) == NEXORA_GAMEPLAY_OK &&
                  future.suffix == 0x123456789abcdef0,
              "unknown caller suffix was overwritten");
    }
    Require(play.Pause(), "Pause failed");
    editor::preview::ForwardPlayInput(play, module, true,
                                      std::array{Event(WindowEventType::Pointer, 90, 80)});
    Require(Neutral(Capture()) && play.Step() && Neutral(fixed), "Pause/Step admitted motion");
    module.Unload();
    auto output = updated;
    const auto before = output;
    Require(NexoraCapturePointerInputV1(&host, 0, &output) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                std::memcmp(&before, &output, sizeof(output)) == 0 && play.Stop(),
            "unbound owner capture wrote output or stop failed");
    Require(world.FindEntity(target)->transform.x == 0 &&
                world.FindEntity(target)->transform.y == 0,
            "Play changed the source World");
  }
  Require(creates == 32 && starts == 32 && stops == 32 && destroys == 32,
          "lifecycle callbacks were skipped or duplicated");
}
} // namespace
int main() {
  try {
    StateContracts();
    ProviderContracts();
    std::cout << "32 pointer state/provider lifecycles passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
